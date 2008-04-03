// Date.cpp:  ActionScript class for date and time, for Gnash.
// 
//  Copyright (C) 2005, 2006, 2007, 2008 Free Software Foundation, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//

// Date.cpp
//
// Implements methods of the ActionScript "Date" class for Gnash
//
// TODO: What does Flash setTime() do/return if you hand it 0 parameters?
//
// Flash player handles a huge range of dates, including
// thousands of years BC. The timestamp value is correspondingly
// large: it is a double, which has a minimum size of 8 bytes
// in the C++ standard. Methods provided by ctime and sys/time.h
// generally rely on time_t whose size varies according to platform.
// It is not big enough to deal with all valid flash timestamps,
// so this class uses its own methods to convert to and from 
// a time struct and the time stamp.
// 
//
// FEATURES:
// Flash Player does not seem to respect TZ or the zoneinfo database;
// It changes to/from daylight saving time according to its own rules.
// We use the operating system's localtime routines.
//
// Flash player does bizarre things for some argument combinations,
// returning datestamps of /6.*e+19  We don't bother doing this...
//
// Boost date-time handles a larger range of correct dates than
// the usual C and system functions. However, it is still limited
// to POSIX to 1 Jan 1400 - 31 Dec 9999 and will not handle
// dates at all outside this range. Flash isn't really that
// bothered by correctness; rather, it needs to handle a vast
// range of dates consistently. See http://www.boost.org/doc/html/date_time.html
//
// Pros:
//
// *  OS portability is done by libboost, not here;
//
// Cons:
//
// *  It doesn't handle fractions of milliseconds (and who cares?);
// *  Mapping between boost's coherent date_time methods and Flash's
//    idiosyncratic ones to implement this class' methods is more tricky;
// *  It brings the need to handle all boundary cases and exceptions
//    explicitly (e.g. mapping of 38 Nov to 8 Dec, mapping negative
//    month/day-of-month/hours/min/secs/millisecs into the previous
//    year/month/day/hour/min/sec and so on).
// *  It doesn't do what ActionScript wants. This is the best reason
//    not to use it for time and date functions (though for portable
//    timing it might well be useful).

#ifdef HAVE_CONFIG_H
#include "gnashconfig.h"
#endif

#include "log.h"
#include "utility.h"
#include "Date.h"
#include "fn_call.h"
#include "GnashException.h"
#include "builtin_function.h"
#include "Object.h" // for getObjectInterface
#include "tu_timer.h"

#include <ctime>
#include <cmath>
#include <boost/format.hpp>

# include <sys/time.h>

// Declaration for replacement timezone functions
// In the absence of gettimeofday() we use ftime() to get milliseconds,
// but not for timezone offset because ftime's TZ stuff is unreliable.
// For that we use tzset()/timezone if it is available.

// The structure of these ifdefs mimics the structure of the code below
// where these things are used if available.

#if !defined(HAVE_GETTIMEOFDAY) || (!defined(HAVE_TM_GMTOFF) && !defined(HAVE_TZSET))
#ifdef HAVE_FTIME
extern "C" {
#  include <sys/types.h>    // for ftime()
#  include <sys/timeb.h>    // for ftime()
}
#endif
#endif

#if !defined(HAVE_TM_GMTOFF)
# ifdef HAVE_LONG_TIMEZONE
extern long timezone;   // for tzset()/long timezone;
# endif
#endif

namespace gnash {

// A time struct to contain the broken-down time.
struct GnashTime
{
    boost::int32_t millisecond;
    boost::int32_t second;
    boost::int32_t minute;
    boost::int32_t hour;
    boost::int32_t monthday;
    boost::int32_t weekday;
    boost::int32_t month;
    boost::int32_t year;
    boost::int32_t timezoneOffset;
};

static const int daysInMonth[2][12] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

// forward declarations
static void fillGnashTime(double time, GnashTime& gt);
static double makeTimeValue(GnashTime& gt);
static void getLocalTime(const double& time, GnashTime& gt);
static void getUniversalTime(const double& time, GnashTime& gt);

static double rogue_date_args(const fn_call& fn, unsigned maxargs);
static int minutes_east_of_gmt(struct tm &tm);

// Helper macros for calendar algorithms
#define IS_LEAP_YEAR(n) ( !((n + 1900) % 400) || ( !((n + 1900) % 4) && ((n + 1900) % 100)) )

// Count the leap years. This needs some adjustment
// to get the actual number
#define COUNT_LEAP_YEARS(n)   ( (n - 70) / 4 - (n - 70) / 100 + (n - 70) / 400 )

static void
getLocalTime(const double& time, GnashTime& gt)
{
    // Not yet correct - no time zone.
    fillGnashTime(time, gt);
}

static void
getUniversalTime(const double& time, GnashTime& gt)
{
    // No time zone needed.
    fillGnashTime(time, gt);
}

// Seconds and milliseconds should be exactly the same whether in UTC
// or in localtime, so we always use localtime.

// forward declarations
static as_value date_new(const fn_call& fn);
static as_value date_gettime(const fn_call& fn); 
static as_value date_settime(const fn_call& fn);
static as_value date_gettimezoneoffset(const fn_call& fn);
static as_value date_getyear(const fn_call& fn);
static as_value date_getfullyear(const fn_call& fn);
static as_value date_getmonth(const fn_call& fn);
static as_value date_getdate(const fn_call& fn);
static as_value date_getday(const fn_call& fn);
static as_value date_gethours(const fn_call& fn);
static as_value date_getminutes(const fn_call& fn);
static as_value date_getseconds(const fn_call& fn);
static as_value date_getmilliseconds(const fn_call& fn);
static as_value date_getutcfullyear(const fn_call& fn);
static as_value date_getutcmonth(const fn_call& fn);
static as_value date_getutcdate(const fn_call& fn);
static as_value date_getutcday(const fn_call& fn);
static as_value date_getutchours(const fn_call& fn);
static as_value date_getutcminutes(const fn_call& fn);
static as_value date_setdate(const fn_call& fn);
static as_value date_setfullyear(const fn_call& fn);
static as_value date_sethours(const fn_call& fn);
static as_value date_setmilliseconds(const fn_call& fn);
static as_value date_setminutes(const fn_call& fn);
static as_value date_setmonth(const fn_call& fn);
static as_value date_setseconds(const fn_call& fn);
static as_value date_setutcdate(const fn_call& fn);
static as_value date_setutcfullyear(const fn_call& fn);
static as_value date_setutchours(const fn_call& fn);
static as_value date_setutcminutes(const fn_call& fn);
static as_value date_setutcmonth(const fn_call& fn);
static as_value date_setyear(const fn_call& fn);
static as_value date_tostring(const fn_call& fn);
static as_value date_valueof(const fn_call& fn);

// Static AS methods
static as_value date_utc(const fn_call& fn);

static as_object* getDateInterface();
static void attachDateInterface(as_object& o);
static void attachDateStaticInterface(as_object& o);

static void
attachDateInterface(as_object& o)
{
  o.init_member("getDate", new builtin_function(date_getdate));
  o.init_member("getDay", new builtin_function(date_getday));
  o.init_member("getFullYear", new builtin_function(date_getfullyear));
  o.init_member("getHours", new builtin_function(date_gethours));
  o.init_member("getMilliseconds", new builtin_function(date_getmilliseconds));
  o.init_member("getMinutes", new builtin_function(date_getminutes));
  o.init_member("getMonth", new builtin_function(date_getmonth));
  o.init_member("getSeconds", new builtin_function(date_getseconds));
  o.init_member("getTime", new builtin_function(date_gettime));
  o.init_member("getTimezoneOffset", new builtin_function(date_gettimezoneoffset));
  o.init_member("getUTCDate", new builtin_function(date_getutcdate));
  o.init_member("getUTCDay", new builtin_function(date_getutcday));
  o.init_member("getUTCFullYear", new builtin_function(date_getutcfullyear));
  o.init_member("getUTCHours", new builtin_function(date_getutchours));
  o.init_member("getUTCMilliseconds", new builtin_function(date_getmilliseconds)); // same
  o.init_member("getUTCMinutes", new builtin_function(date_getutcminutes));
  o.init_member("getUTCMonth", new builtin_function(date_getutcmonth));
  o.init_member("getUTCSeconds", new builtin_function(date_getseconds));
  o.init_member("getYear", new builtin_function(date_getyear));
  o.init_member("setDate", new builtin_function(date_setdate));
  o.init_member("setFullYear", new builtin_function(date_setfullyear));
  o.init_member("setHours", new builtin_function(date_sethours));
  o.init_member("setMilliseconds", new builtin_function(date_setmilliseconds));
  o.init_member("setMinutes", new builtin_function(date_setminutes));
  o.init_member("setMonth", new builtin_function(date_setmonth));
  o.init_member("setSeconds", new builtin_function(date_setseconds));
  o.init_member("setTime", new builtin_function(date_settime));
  o.init_member("setUTCDate", new builtin_function(date_setutcdate));
  o.init_member("setUTCFullYear", new builtin_function(date_setutcfullyear));
  o.init_member("setUTCHours", new builtin_function(date_setutchours));
  o.init_member("setUTCMilliseconds", new builtin_function(date_setmilliseconds)); // same
  o.init_member("setUTCMinutes", new builtin_function(date_setutcminutes));
  o.init_member("setUTCMonth", new builtin_function(date_setutcmonth));
  o.init_member("setUTCSeconds", new builtin_function(date_setseconds));
  o.init_member("setYear", new builtin_function(date_setyear));
  o.init_member("toString", new builtin_function(date_tostring));
  o.init_member("valueOf", new builtin_function(date_valueof));
}

static void
attachDateStaticInterface(as_object& o)
{
  // This should *only* be available when SWF version is > 6
  // Are you sure? The online reference say it's in from v5 -martin
  o.init_member("UTC", new builtin_function(date_utc));
}

static as_object*
getDateInterface()
{
  static boost::intrusive_ptr<as_object> o;
  if ( o == NULL )
  {
    o = new as_object(getObjectInterface());
    attachDateInterface(*o);
  }
  return o.get();
}

class date_as_object : public as_object
{
public:
    // value is the master field and the Date's value, and holds the
    // date as the number of milliseconds since midnight 1 Jan 1970.
    // All other "fields" are calculated from this.
    double value;   // milliseconds UTC since the epoch

    date_as_object()
            :
        as_object(getDateInterface())
    {
    }

    as_value toString();

    bool isDateObject() { return true; }

private:

};


as_value
date_as_object::toString()
{
    const char* monthname[12] = { "Jan", "Feb", "Mar",
                                  "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep",
                                  "Oct", "Nov", "Dec" };
                                        
    const char* dayweekname[7] = { "Sun", "Mon", "Tue", "Wed",
                                   "Thu", "Fri", "Sat" };
  
    /// NAN and infinities all print as "Invalid Date"
    if (isnan(value) || isinf(value)) {
        return as_value("Invalid Date");
    }
  
    // The date value split out to year, month, day, hour etc and millisecs
    struct tm tm;
    GnashTime gt;
    // Time zone offset (including DST) as hours and minutes east of GMT
    int tzhours = 0;
    int tzminutes = 0; 

    getLocalTime(value, gt);
  
    // At the meridian we need to print "GMT+0100" when Daylight Saving
    // Time is in force, "GMT+0000" when it isn't, and other values for
    // other places around the globe when DST is/isn't in force there.
  
    // Split offset into hours and minutes
//    tzminutes = minutes_east_of_gmt(tm);
//    tzhours = tzminutes / 60, tzminutes %= 60;
  
    // If timezone is negative, both hours and minutes will be negative
    // but for the purpose of printing a string, only the hour needs to
    // produce a minus sign.
//    if (tzminutes < 0) tzminutes = - tzminutes;
  
    boost::format dateFormat("%s %s %d %02d:%02d:%02d GMT%+03d%02d %d");
    dateFormat % dayweekname[gt.weekday] % monthname[gt.month]
               % gt.monthday % gt.hour % gt.minute % gt.second
               % tzhours % tzminutes % (gt.year + 1900);
  
    return as_value(dateFormat.str());
  
}

/// \brief Date constructor
//
/// The constructor has three forms: 0 args, 1 arg and 2-7 args.
/// new Date() sets the Date to the current time of day
/// new Date(undefined[,*]) does the same.
/// new Date(timeValue:Number) sets the date to a number of milliseconds since
/// 1 Jan 1970 UTC
/// new Date(year, month[,date[,hour[,minute[,second[,millisecond]]]]])
/// creates a Date date object and sets it to a specified year/month etc
/// in local time.
/// year 0-99 means 1900-1999, other positive values are gregorian years
/// and negative values are years prior to 1900. Thus the only way to
/// specify the year 50AD is as -1850.
/// Defaults are 0 except for date (day of month) whose default it 1.

as_value
date_new(const fn_call& fn)
{
    // TODO: just make date_as_object constructor
    //       register the exported interface, don't
    //       replicate all functions !!

    date_as_object *date = new date_as_object;

    // Reject all date specifications containing Infinities and NaNs.
    // The commercial player does different things according to which
    // args are NaNs or Infinities:
    // for now, we just use rogue_date_args' algorithm
    double foo;
    if ((foo = rogue_date_args(fn, 7)) != 0.0) {
        date->value = foo;
        return as_value(date);
    }

    // TODO: move this to date_as_object constructor
    if (fn.nargs < 1 || fn.arg(0).is_undefined()) {
        // Set from system clock
        date->value = tu_timer::get_ticks();
    }
    else if (fn.nargs == 1) {
        // Set the value in milliseconds since 1970 UTC
        date->value = fn.arg(0).to_number();
    }
    else {
        // Create a time from the supplied (at least 2) arguments.
        GnashTime gt;
    
        gt.millisecond = 0;            
        gt.second = 0;
        gt.minute = 0;
        gt.hour = 0;
        gt.monthday = 1;
        gt.month = static_cast<int>(fn.arg(1).to_number());
    
        int year = static_cast<int>(fn.arg(0).to_number());
        
        // GnashTime.year is the value since 1900 (like struct tm)
        // negative value is a year before 1900
        // A year between 0 and 99 is the year since 1900
        if (year < 100) gt.year = year;
        // A value of 100 or more is a full year and must be
        // converted to years since 1900
        else gt.year = year - 1900;

        switch (fn.nargs) {
            default:
                IF_VERBOSE_ASCODING_ERRORS(
                log_aserror(_("Date constructor called with more than 7 arguments"));
                )
            case 7:
                // fractions of milliseconds are ignored
                gt.millisecond = (int)fn.arg(6).to_number();
            case 6:
                gt.second = (int)fn.arg(5).to_number();
            case 5:
                gt.minute = (int)fn.arg(4).to_number();
            case 4:
                gt.hour = (int)fn.arg(3).to_number();
            case 3:
                gt.monthday = (int)fn.arg(2).to_number();
            case 2:
                break;
                // Done already
            }

            double val = makeTimeValue(gt); // convert from local time
            if (val == -1) {
                // mktime could not represent the time
                log_error(_("Date() failed to initialise from arguments"));
                date->value = 0;  // or undefined?
            } else {
                date->value = val;
            }
    }
    
    return as_value(date);
}

//
//    =========    Functions to get dates in various ways    ========
//

// Date.getTime() is implemented by Date.valueOf()

// Functions to return broken-out elements of the date and time.

// We use a prototype macro to generate the function bodies because the many
// individual functions are small and almost identical.

// This calls _gmtime_r and _localtime_r, which are defined above into
// gmtime_r and localtime_r or our own local equivalents.

#define date_get_proto(function, timefn, element) \
  static as_value function(const fn_call& fn) { \
    boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr); \
    if (isnan(date->value) || isinf(date->value)) { as_value rv; rv.set_nan(); return rv; } \
    GnashTime gt; \
    timefn(date->value, gt); \
    return as_value(gt.element); \
  }

/// \brief Date.getYear
/// returns a Date's Gregorian year minus 1900 according to local time.

date_get_proto(date_getyear, getLocalTime, year)

/// \brief Date.getFullYear
/// returns a Date's Gregorian year according to local time.

date_get_proto(date_getfullyear, getLocalTime, year + 1900)

/// \brief Date.getMonth
/// returns a Date's month in the range 0 to 11.

date_get_proto(date_getmonth, getLocalTime, month)

/// \brief Date.getDate
/// returns a Date's day-of-month, from 1 to 31 according to local time.

date_get_proto(date_getdate, getLocalTime, monthday)

/// \brief Date.getDay
/// returns the day of the week for a Date according to local time,
/// where 0 is Sunday and 6 is Saturday.

date_get_proto(date_getday, getLocalTime, weekday)

/// \brief Date.getHours
/// Returns the hour number for a Date, from 0 to 23, according to local time.

date_get_proto(date_gethours, getLocalTime, hour)

/// \brief Date.getMinutes
/// returns a Date's minutes, from 0-59, according to localtime.
// (Yes, some places do have a fractions of an hour's timezone offset
// or daylight saving time!)

date_get_proto(date_getminutes, getLocalTime, minute)

/// \brief Date.getSeconds
/// returns a Date's seconds, from 0-59.
/// Localtime should be irrelevant.

date_get_proto(date_getseconds, getLocalTime, second)

/// \brief Date.getMilliseconds
/// returns a Date's millisecond component as an integer from 0 to 999.
/// Localtime is irrelevant!
//
// Also implements Date.getUTCMilliseconds
date_get_proto(date_getmilliseconds, getLocalTime, millisecond)


// The same functions for universal time.
//
date_get_proto(date_getutcfullyear, getUniversalTime, year + 1900)
date_get_proto(date_getutcmonth,    getUniversalTime, month)
date_get_proto(date_getutcdate,     getUniversalTime, monthday)
date_get_proto(date_getutcday,      getUniversalTime, weekday)
date_get_proto(date_getutchours,    getUniversalTime, hour)
date_get_proto(date_getutcminutes,  getUniversalTime, minute)


// Return the difference between UTC and localtime+DST for a given date/time
// as the number of minutes east of GMT.
static int minutes_east_of_gmt(struct tm& /* tm*/)
{
    return 0;
#ifdef HAVE_TM_GMTOFF
  // tm_gmtoff is in seconds east of GMT; convert to minutes.
//  return((int) (tm.tm_gmtoff / 60));
    return 0;
#else
  // Find the geographical system timezone offset and add an hour if
  // DST applies to the date.
  // To get it really right I guess we should call both gmtime()
  // and localtime() and look at the difference.
  //
  // The range of standard time is GMT-11 to GMT+14.
  // The most extreme with DST is Chatham Island GMT+12:45 +1DST

  int minutes_east;

  // Find out system timezone offset...

#if 0

# if defined(HAVE_TZSET) && defined(HAVE_LONG_TIMEZONE)
  tzset();
  minutes_east = -timezone/60; // timezone is seconds west of GMT
# elif defined(HAVE_GETTIMEOFDAY)
  // gettimeofday(3):
  // "The use of the timezone structure is obsolete; the tz argument
  // should normally be specified as NULL. The tz_dsttime field has
  // never been used under Linux; it has not been and will not be
  // supported by libc or glibc."
  // Still, mancansa d'asu, t'acuma i buoi.
  struct timeval tv;
  struct timezone tz;
  gettimeofday(&tv,&tz);
  minutes_east = -tz.tz_minuteswest;

# elif defined(HAVE_FTIME)
  // ftime(3): "These days the contents of the timezone and dstflag
  // fields are undefined."
  // In practice, timezone is -120 in Italy when it should be -60.
  struct timeb tb;
    
  ftime (&tb);
  // tb.timezone is number of minutes west of GMT
  minutes_east = -tb.timezone;

# else
  minutes_east = 0; // No idea.
# endif

  // ...and adjust by one hour if DST was in force at that time.
  //
  // According to http://www.timeanddate.com/time/, the only place that
  // uses DST != +1 hour is Lord Howe Island with half an hour. Tough.

  if (tm.tm_isdst == 0) {
    // DST exists and is not in effect
  } else if (tm.tm_isdst > 0) {
    // DST exists and was in effect
    minutes_east += 60;
  } else {
    // tm_isdst is negative: cannot get TZ info.
    // Convert and print in UTC instead.
    log_error("Cannot get timezone information");
    minutes_east = 0;
  }
#endif

  return minutes_east;
#endif // HAVE_TM_OFFSET
}


/// \brief Date.getTimezoneOffset
/// returns the difference between localtime and UTC that was in effect at the
/// time specified by a Date object, according to local timezone and DST.
/// For example, if you are in GMT+0100, the offset is -60

static as_value date_gettimezoneoffset(const fn_call& fn) {
  boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr);

    log_unimpl("getTimeZoneOffset() unimplemented");
//  struct tm tm;
//  double msec;

//  if (fn.nargs > 0) {
//      IF_VERBOSE_ASCODING_ERRORS(
//    log_aserror("Date.getTimezoneOffset was called with parameters");
//      )
//  }

//  // Turn Flash datestamp into tm structure...
//  local_dateToGnashTime(date->value, tm, msec);

//  // ...and figure out the timezone/DST offset from that.
//  return as_value(-minutes_east_of_gmt(tm));
    return 0;

}


//
//    =========    Functions to set dates in various ways    ========
//

/// \brief Date.setTime
/// sets a Date in milliseconds after January 1, 1970 00:00 UTC.
/// The return value is the same as the parameter.
static as_value
date_settime(const fn_call& fn)
{
    boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr);

    if (fn.nargs < 1) {
        IF_VERBOSE_ASCODING_ERRORS(
            log_aserror(_("Date.setTime needs one argument"));
        )
    }
    else {
        // returns a double
        date->value = fn.arg(0).to_number();
    }

    if (fn.nargs > 1) {
        IF_VERBOSE_ASCODING_ERRORS(
            log_aserror(_("Date.setTime was called with more than one argument"));
        )
    }

    return as_value(date->value);
}

//
// Functions to set just some components of a Date.
//
// We do this by exploding the datestamp into the calendar components,
// setting the fields that are to be changed, then converting back.
//

// The Adobe player 9 behaves strangely. e.g., after "new date = Date(0)":
// date.setYear(1970); date.setMonth(1); date.setDate(29); gives Mar 1 but
// date.setYear(1970); date.setDate(29); date.setMonth(1); gives Feb 28
//
// We need two sets of the same functions: those that take localtime values
// and those that take UTC (GMT) values.
// Since there are a lot of them and they are hairy, we write one set that,
// if an additional extra parameter is passed, switch to working in UTC
// instead. Apart from the bottom-level conversions they are identical.

static void
gnashTimeToDate(GnashTime& gt, date_as_object& date, bool utc)
{
    // Needs timezone.
    if (utc) date.value = makeTimeValue(gt);
    else date.value = makeTimeValue(gt);
}

static void
dateToGnashTime(date_as_object& date, GnashTime& gt, bool utc)
{
    // Needs timezone.
    if (utc) getUniversalTime(date.value, gt);
    else getLocalTime(date.value, gt);
}

//
// Compound functions that can set one, two, three or four fields at once.
//
// There are two flavours: those that work with localtime and those that do so
// in UTC (except for setYear, which has no UTC version). We avoid duplication
// by passing an extra parameter "utc": if true, we use the UTC conversion
// functions, otherwise the localtime ones.
//
// All non-UTC functions take dates/times to be in local time and their return
// value is the new date in UTC milliseconds after 1/1/1970 00:00 UTC.
//

/// \brief Date.setFullYear(year[,month[,day]])
//
/// If the month and date parameters are specified, they are set in local time.
/// year: A four-digit number specifying a year.
/// Two-digit numbers do not represent four-digit years;
/// for example, 99 is not the year 1999, but the year 99.
/// month: An integer from 0 (January) to 11 (December). [optional]
/// day: An integer from 1 to 31. [optional]
///
/// If the month and/or day are omitted, they are left at their current values.
/// If changing the year or month results in an impossible date, it is
/// normalised: 29 Feb becomes 1 Mar, 31 April becomes 1 May etc.

// When changing the year/month/date from a date in Daylight Saving Time to a
// date not in DST or vice versa, with setYear and setFullYear the hour of day
// remains the same in *local time* not in UTC.
// So if a date object is set to midnight in January and you change the date
// to June, it will still be midnight localtime.
//
// When using setUTCFullYear instead, the time of day remains the same *in UTC*
// so, in the northern hemisphere, changing midnight from Jan to June gives
// 01:00 localtime.
//
// Heaven knows what happens if it is 1.30 localtime and you change the date
// to the day the clocks go forward.

static as_value _date_setfullyear(const fn_call& fn, bool utc) {
  boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr);

  if (fn.nargs < 1) {
      IF_VERBOSE_ASCODING_ERRORS(
    log_aserror(_("Date.setFullYear needs one argument"));
      )
      date->value = NAN;
  } else if (rogue_date_args(fn, 3) != 0.0) {
      date->value = NAN;
  } else {
      GnashTime gt;

      dateToGnashTime(*date, gt, utc);
      gt.year = (int) fn.arg(0).to_number() - 1900;
      if (fn.nargs >= 2)
        gt.month = (int) fn.arg(1).to_number();
      if (fn.nargs >= 3)
        gt.monthday = (int) fn.arg(2).to_number();
      if (fn.nargs > 3) {
    IF_VERBOSE_ASCODING_ERRORS(
        log_aserror(_("Date.setFullYear was called with more than three arguments"));
    )
      }
      gnashTimeToDate(gt, *date, utc);
  }
  return as_value(date->value);
}

/// \brief Date.setYear(year[,month[,day]])
/// if year is 0-99, this means 1900-1999, otherwise it is a Gregorian year.
/// Negative values for year set negative years (years BC).
/// This means that you cannot set a Date to the years 0-99 AD using setYear().
/// "month" is 0 - 11 and day 1 - 31 as usual.
///
/// If month and/or day are omitted, they are left unchanged except:
/// - when the day is 29, 30 or 31 and changing to a month that has less days,
///   the month gets set to the following one and the date should wrap,
///   becoming 1, 2 or 3.
/// - when changing from 29 Feb in a leap year to a non-leap year, the date
///   should end up at March 1st of the same year.
//
// There is no setUTCYear() function.
static as_value
date_setyear(const fn_call& fn)
{
    boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr);

    // assert(fn.nargs == 1);
    if (fn.nargs < 1) {
        IF_VERBOSE_ASCODING_ERRORS(
            log_aserror(_("Date.setYear needs one argument"));
        )
        date->value = NAN;
    }
    else if (rogue_date_args(fn, 3) != 0.0) {
        date->value = NAN;
    }
    else {
        GnashTime gt;

        dateToGnashTime(*date, gt, false);
        gt.year = (int) fn.arg(0).to_number();
        // tm_year is number of years since 1900, so if they gave a
        // full year spec, we must adjust it.
        if (gt.year >= 100) gt.year -= 1900;

        if (fn.nargs >= 2) gt.month = fn.arg(1).to_int();
        if (fn.nargs >= 3) gt.monthday = fn.arg(2).to_int();
        if (fn.nargs > 3) {
            IF_VERBOSE_ASCODING_ERRORS(
                log_aserror(_("Date.setYear was called with more than three arguments"));
            )
        }
        gnashTimeToDate(gt, *date, false); // utc=false: use localtime
    }
    return as_value(date->value);
}

/// \brief Date.setMonth(month[,day])
/// sets the month (0-11) and day-of-month (1-31) components of a Date.
///
/// If the day argument is omitted, the new month has less days than the
/// old one and the new day is beyond the end of the month,
/// the day should be set to the last day of the specified month.
/// This implementation currently wraps it into the next month, which is wrong.

// If no arguments are given or if an invalid type is given,
// the commercial player sets the month to January in the same year.
// Only if the second parameter is present and has a non-numeric value,
// the result is NAN.
// We do not do the same because it's a bugger to code.

static as_value
_date_setmonth(const fn_call& fn, bool utc)
{
    boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr);

    // assert(fn.nargs >= 1 && fn.nargs <= 2);
    if (fn.nargs < 1) {
        IF_VERBOSE_ASCODING_ERRORS(
            log_aserror(_("Date.setMonth needs one argument"));
        )
        date->value = NAN;
    }
    else if (rogue_date_args(fn, 2) != 0.0) {
        date->value = NAN;
    }
    else {

        GnashTime gt;

        dateToGnashTime(*date, gt, utc);

        // It seems odd, but FlashPlayer takes all bad month values to mean
        // January
        double monthvalue =  fn.arg(0).to_number();
        if (isnan(monthvalue) || isinf(monthvalue)) monthvalue = 0.0;
        gt.month = (int) monthvalue;

        // If the day-of-month value is invalid instead, the result is NAN.
        if (fn.nargs >= 2) {
            double mdayvalue = fn.arg(1).to_number();
            if (isnan(mdayvalue) || isinf(mdayvalue)) {
                date->value = NAN;
                return as_value(date->value);
            }
            else {
                gt.monthday = (int) mdayvalue;
            }
        }
        if (fn.nargs > 2) {
            IF_VERBOSE_ASCODING_ERRORS(
                log_aserror(_("Date.setMonth was called with more than three arguments"));
            )
        }
        gnashTimeToDate(gt, *date, utc);
    }
    return as_value(date->value);
}

/// \brief Date.setDate(day)
/// Set the day-of-month (1-31) for a Date object.
/// If the day-of-month is beyond the end of the current month, it wraps into
/// the first days of the following  month.  This also happens if you set the
/// day > 31. Example: setting the 35th in January results in Feb 4th.

static as_value
_date_setdate(const fn_call& fn, bool utc) {
  boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr);

  if (fn.nargs < 1) {
      IF_VERBOSE_ASCODING_ERRORS(
    log_aserror(_("Date.setDate needs one argument"));
      )
      date->value = NAN;  // Is what FlashPlayer sets
  } else if (rogue_date_args(fn, 1) != 0.0) {
      date->value = NAN;
  } else {
    GnashTime gt;

    dateToGnashTime(*date, gt, utc);
    gt.monthday = fn.arg(0).to_int();
    gnashTimeToDate(gt, *date, utc);
  }
  if (fn.nargs > 1) {
      IF_VERBOSE_ASCODING_ERRORS(
    log_aserror(_("Date.setDate was called with more than one argument"));
      )
  }
  return as_value(date->value);
}

/// \brief Date.setHours(hour[,min[,sec[,millisec]]])
/// change the time-of-day in a Date object. If optional fields are omitted,
/// their values in the Date object are left the same as they were.
///
/// If hour>23 or min/sec>59, these are accepted and wrap into the following
/// minute, hour or calendar day.
/// Similarly, negative values carry you back into the previous minute/hour/day.
//
/// Only the integer part of millisec is used, truncating it, not rounding it.
/// The only way to set a fractional number of milliseconds is to use
/// setTime(n) or call the constructor with one argument.

static as_value _date_sethours(const fn_call& fn, bool utc) {
  boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr);

  // assert(fn.nargs >= 1 && fn.nargs <= 4);
  if (fn.nargs < 1) {
      IF_VERBOSE_ASCODING_ERRORS(
    log_aserror(_("Date.setHours needs one argument"));
      )
      date->value = NAN;  // Is what FlashPlayer sets
  } else if (rogue_date_args(fn, 4) != 0.0) {
      date->value = NAN;
  } else {
      
      GnashTime gt;

      dateToGnashTime(*date, gt, utc);
      gt.hour = fn.arg(0).to_int();
      if (fn.nargs >= 2)
        gt.minute = fn.arg(1).to_int();
      if (fn.nargs >= 3)
        gt.second = fn.arg(2).to_int();
      if (fn.nargs >= 4)
        gt.millisecond = fn.arg(3).to_int();
      if (fn.nargs > 4) {
    IF_VERBOSE_ASCODING_ERRORS(
        log_aserror(_("Date.setHours was called with more than four arguments"));
    )
      }
      gnashTimeToDate(gt, *date, utc);
  }
  return as_value(date->value);
}

/// \brief Date.setMinutes(minutes[,secs[,millisecs]])
/// change the time-of-day in a Date object. If optional fields are omitted,
/// their values in the Date object are left the same as they were.
///
/// If min/sec>59, these are accepted and wrap into the following minute, hour
/// or calendar day.
/// Similarly, negative values carry you back into the previous minute/hour/day.

static as_value
_date_setminutes(const fn_call& fn, bool utc)
{
    boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr);

    //assert(fn.nargs >= 1 && fn.nargs <= 3);
    if (fn.nargs < 1) {
        IF_VERBOSE_ASCODING_ERRORS(
            log_aserror(_("Date.setMinutes needs one argument"));
        )
        date->value = NAN;  // FlashPlayer instead leaves the date set to
        // a random value such as 9th December 2077 BC
    }
    else if (rogue_date_args(fn, 3) != 0.0) {
        date->value = NAN;
    }
    else {
        GnashTime gt;

        dateToGnashTime(*date, gt, utc);
        gt.minute = (int) fn.arg(0).to_number();
        if (fn.nargs >= 2) gt.second = (int) fn.arg(1).to_number();
        if (fn.nargs >= 3) gt.millisecond = (int) fn.arg(2).to_number();
        if (fn.nargs > 3) {
            IF_VERBOSE_ASCODING_ERRORS(
                log_aserror(_("Date.setMinutes was called with more than three arguments"));
            )
        }
        gnashTimeToDate(gt, *date, utc);
    }
    return as_value(date->value);
}

/// \brief Date.setSeconds(secs[,millisecs])
/// set the "seconds" component in a date object.
///
/// Values <0, >59 for secs or >999 for millisecs take the date back to the
/// previous minute (or hour or calendar day) or on to the following ones.

static as_value _date_setseconds(const fn_call& fn, bool utc) {
  boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr);

  // assert(fn.nargs >= 1 && fn.nargs <= 2);
  if (fn.nargs < 1) {
      IF_VERBOSE_ASCODING_ERRORS(
    log_aserror(_("Date.setSeconds needs one argument"));
      )
      date->value = NAN;  // Same as commercial player
  } else if (rogue_date_args(fn, 2) != 0.0) {
      date->value = NAN;
  } else {
      // We *could* set seconds [and milliseconds] without breaking the
      // structure out and reasembling it. We do it the same way as the
      // rest for simplicity and in case anyone's date routines ever
      // take account of leap seconds.
      GnashTime gt;

      dateToGnashTime(*date, gt, utc);
      gt.second = fn.arg(0).to_int();
      if (fn.nargs >= 2)
        gt.millisecond = fn.arg(1).to_int();
      if (fn.nargs > 2) {
    IF_VERBOSE_ASCODING_ERRORS(
        log_aserror(_("Date.setMinutes was called with more than three arguments"));
    )
      }
      // This is both setSeconds and setUTCSeconds.
      // Use utc to avoid needless worrying about timezones.
      gnashTimeToDate(gt, *date, utc);
  }
  return as_value(date->value);
}

static as_value
date_setmilliseconds(const fn_call& fn)
{
    boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr);

    // assert(fn.nargs == 1);
    if (fn.nargs < 1) {
        IF_VERBOSE_ASCODING_ERRORS(
            log_aserror(_("Date.setMilliseconds needs one argument"));
        )
        date->value = NAN;
    }
    else if (rogue_date_args(fn, 1) != 0.0) {
        date->value = NAN;
    }
    else {
        // Zero the milliseconds and set them from the argument.
        date->value = date->value - std::fmod(date->value, 1000.0) + fn.arg(0).to_int();
        if (fn.nargs > 1) {
            IF_VERBOSE_ASCODING_ERRORS(
                log_aserror(_("Date.setMilliseconds was called with more than one argument"));
            )
        }
    }
    return as_value(date->value);
}

// Bindings for localtime versions
#define local_proto(item) \
  static as_value date_set##item(const fn_call& fn) { \
    _date_set##item(fn, false); \
    return as_value(); \
  }
local_proto(fullyear)
local_proto(month)
local_proto(date)
local_proto(hours)
local_proto(minutes)
local_proto(seconds)
#undef local_proto

// The same things for UTC.
#define utc_proto(item) \
  static as_value date_setutc##item(const fn_call& fn) { \
    _date_set##item(fn, true); \
    return as_value(); \
  }
utc_proto(fullyear)
utc_proto(month)
utc_proto(date)
utc_proto(hours)
utc_proto(minutes)
#undef utc_proto


/// \brief Date.toString()
/// convert a Date to a printable string.
/// The format is "Thu Jan 1 00:00:00 GMT+0000 1970" and it is displayed in
/// local time.

static as_value date_tostring(const fn_call& fn) {

    boost::intrusive_ptr<date_as_object> date = 
                    ensureType<date_as_object>(fn.this_ptr);
    return date->toString();
}

// Date.UTC(year:Number,month[,day[,hour[,minute[,second[,millisecond]]]]]
//
// Convert a UTC date/time specification to number of milliseconds since
// 1 Jan 1970 00:00 UTC.
//
// unspecified optional arguments default to 0 except for day-of-month,
// which defaults to 1.
//
// year is a Gregorian year; special values 0 to 99 mean 1900 to 1999 so it is
// impossible to specify the year 55 AD using this interface.
//
// Any fractional part in the number of milliseconds is ignored (truncated)
//
// If 0 or 1 argument are passed, the result is the "undefined" value.
//
// This probably doesn't handle exceptional cases such as NaNs and infinities
// the same as the commercial player. What that does is:
// - if any argument is NaN, the result is NaN
// - if one or more of the optional arguments are +Infinity,
//  the result is +Infinity
// - if one or more of the optional arguments are -Infinity,
//  the result is -Infinity
// - if both +Infinity and -Infinity are present in the optional args,
//  or if one of the first two arguments is not numeric (including Inf),
//  the result is NaN.
// Actually, given a first parameter of Infinity,-Infinity or NAN,
// it returns -6.77681005679712e+19 but that's just crazy.
//
// We test for < 2 parameters and return undefined, but given any other
// non-numeric arguments we give NAN.


static as_value
date_utc(const fn_call& fn) {

    GnashTime gt; // Date structure for values down to milliseconds

    if (fn.nargs < 2) {
        IF_VERBOSE_ASCODING_ERRORS(
            log_aserror(_("Date.UTC needs one argument"));
        )
        return as_value();  // undefined
    }

    double result;

    // Check for presence of NaNs and Infinities in the arguments 
    // and return the appropriate value if so.
    if ( (result = rogue_date_args(fn, 7)) != 0.0) {
        return as_value(NAN);
    }

    // Preset default values
    // Year and month are always given explicitly
    gt.monthday = 1;
    gt.hour = 0;
    gt.minute = 0;
    gt.second = 0;
    gt.millisecond = 0;

    switch (fn.nargs) {
        default:  // More than 7
            IF_VERBOSE_ASCODING_ERRORS(
                log_aserror(_("Date.UTC was called with more than 7 arguments"));
            )
        case 7:
            // millisecs is double, but fractions of millisecs are ignored.
            gt.millisecond = fn.arg(6).to_int();
        case 6:
            gt.second = fn.arg(5).to_int();
        case 5:
            gt.minute = fn.arg(4).to_int();
        case 4:
            gt.hour = fn.arg(3).to_int();
        case 3:
            gt.monthday = fn.arg(2).to_int();
        case 2:   // these last two are always performed
            gt.month = fn.arg(1).to_int();
            {
                int year = static_cast<int>(fn.arg(0).to_number());
                if (year < 100) gt.year = year;
                else gt.year = year - 1900;
            }
    }

    result = makeTimeValue(gt);
    return as_value(result);
}

// Auxillary function checks for Infinities and NaN in a function's args and
// returns 0.0 if there are none,
// plus (or minus) infinity if positive (or negative) infinites are present,
// NAN is there are NANs present, or a mixture of positive and negative infs.
static double
rogue_date_args(const fn_call& fn, unsigned maxargs)
{
  // Two flags: Did we find any +Infinity (or -Infinity) values in the
  // argument list? If so, "infinity" must be set to the kind that we
  // found.
    bool plusinf = false;
    bool minusinf = false;
    double infinity = 0.0;  // The kind of infinity we found.
                            // 0.0 == none yet.

    // Only check the present parameters, up to the stated maximum number
    if (fn.nargs < maxargs) maxargs = fn.nargs;

    for (unsigned int i = 0; i < maxargs; i++) {
        double arg = fn.arg(i).to_number();

        if (isnan(arg)) return(NAN);

        if (isinf(arg)) {
            if (arg > 0) {  // Plus infinity
                plusinf = true;
            }
            else {  // Minus infinity
                minusinf = true;
            }
            // Remember the kind of infinity we found
            infinity = arg;
        }
    }
    // If both kinds of infinity were present in the args,
    // the result is NaN.
    if (plusinf && minusinf) return(NAN);

    // If only one kind of infinity was in the args, return that.
    if (plusinf || minusinf) return(infinity);
  
    // Otherwise indicate that the function arguments contained
    // no rogue values
    return(0.0);
}

/// \brief Date.valueOf() returns the number of milliseconds since midnight
/// January 1, 1970 00:00 UTC, for a Date. The return value can be a fractional
/// number of milliseconds.
static as_value
date_valueof(const fn_call& fn)
{
    boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr);
    return as_value(date->value);
}


static as_value date_gettime(const fn_call& fn)
{
    boost::intrusive_ptr<date_as_object> date = ensureType<date_as_object>(fn.this_ptr);
    return as_value(date->value);
}

// extern (used by Global.cpp)
void date_class_init(as_object& global)
{
    // This is going to be the global Date "class"/"function"
    static boost::intrusive_ptr<builtin_function> cl;

    if ( cl == NULL ) {
        cl=new builtin_function(&date_new, getDateInterface());
        // replicate all interface to class, to be able to access
        // all methods as static functions
        attachDateStaticInterface(*cl);
    }

    // Register _global.Date
    global.init_member("Date", cl.get());

}

///
///
/// Date conversion functions
///
///

// Converts a time struct into a flash timestamp. Similar to
// mktime, but not limited by the size of time_t. The mathematical
// algorithm looks nicer, but does not cope with large dates.
// Bumping up the int size or using doubles more might help - I 
// haven't really looked at it.
// The first algorithm appears to mimic flash behaviour for
// all dates, though it's a bit ugly.
static double
makeTimeValue(GnashTime& t)
{

#if 1

    // First, adjust years to deal with strange month
    // values.
    
    // Add or substract more than 12 months from the year
    // and adjust months to a valid value.
    t.year += t.month / 12;
    t.month %= 12;

    // Any negative remainder rolls back to the previous year.
    if (t.month < 0) {
        t.year--;
        t.month += 12;
    }

    // Now work out the years from 1970 in days.
    boost::int32_t day = t.monthday;    

    // This works but is a bit clunky.
    if (t.year < 70) {
        day = COUNT_LEAP_YEARS(t.year - 2) + ((t.year - 70) * 365);
        // Adds an extra leap year for the year 0.
        if (t.year <= 0) day++;
    }
    else {
        day = COUNT_LEAP_YEARS(t.year + 1) + ((t.year - 70) * 365);
    }
    
    // Add days for each month. Month must be 0 - 11;
    for (int i = 0; i < t.month; i++)
    {
        assert (t.month < 12);
        day += daysInMonth[IS_LEAP_YEAR (t.year)][i];
    }
    
    // Add the days of the month
    day += t.monthday - 1;

    /// Work out the timestamp
    double ret = static_cast<double>(day) * 86400000.0;
    ret += t.hour * 3600000.0;
    ret += t.minute * 60000.0;
    ret += t.second * 1000.0;
    ret += t.millisecond;
    return ret;
#else

  boost::int32_t d = t.monthday;
  boost::int32_t m = t.month + 1;
  boost::int32_t ya = t.year;  /* Years since 1900 */
  boost::int32_t k;  /* day number since 1 Jan 1900 */

  // For calculation, convert to a year starting on 1 March
  if (m > 2) m -= 3;
  else {
    m += 9;
    ya--;
  }

  k = (1461 * ya) / 4 + (153 * m + 2) / 5 + d + 58;

  /* K is now the day number since 1 Jan 1900.
   * Convert to minutes since 1 Jan 1970 */
  /* 25567 is the number of days from 1 Jan 1900 to 1 Jan 1970 */
  k = ((k - 25567) * 24 + t.hour) * 60 + t.minute;
  
  // Converting to double after minutes allows for +/- 4082 years with
  // 32-bit signed integers.
  return  (k * 60.0 + t.second) * 1000.0 + t.millisecond;
#endif
}

/// Helper function for getYearMathematical
static double
daysSinceUTCForYear(double year)
{
    return (
        365 * (year - 1970) +
        std::floor ((year - 1969) / 4.0f) -
        std::floor ((year - 1901) / 100.0f) +
        std::floor ((year - 1601) / 400.0f)
    );
}

// The algorithm used by swfdec. It iterates only a small number of
// times and is reliable to within a few milliseconds in 
// +- 100000 years. However, it appears to get the year wrong for
// midnight on January 1 of some years (as well as a few milliseconds
// before the end of other years, though that seems less serious).
static boost::int32_t
getYearMathematical(double days)
{

    boost::int32_t low = std::floor ((days >= 0 ? days / 366.0 : days / 365.0)) + 1970;
    boost::int32_t high = std::ceil ((days >= 0 ? days / 365.0 : days / 366.0)) + 1970;

    while (low < high) {
        boost::int32_t pivot = (low + high) / 2;

        if (daysSinceUTCForYear (pivot) <= days) {
            if (daysSinceUTCForYear (pivot + 1) > days) {
                return pivot;
            }
            else {
                low = pivot + 1;
            }
        }
        else {
        high = pivot - 1;
        }
    }

    return low;
}

// Another mathematical way of working out the year, which
// appears to be less reliable than swfdec's way. Adjusts
// days as well as returning the year.
static boost::int32_t
getYearApproximate(boost::int32_t& days)
{
    boost::int32_t year = ((days - 16) - COUNT_LEAP_YEARS((days - 16) / 365)) / 365 + 70;
    if (time < 0) year--;

    days -= (year - 70) * 365;
    if (year < 70) {
        days -= COUNT_LEAP_YEARS(year - 2);
        if (year <= 0) days--;
    }
    else {
        days -= COUNT_LEAP_YEARS(year + 1);
    }
    
    return year;
}

// The brute force way of converting days into years since the epoch.
// This also reduces the number of days accurately. Its disadvantage is,
// of course, that it iterates; its advantage that it's always correct.
boost::int32_t
getYearBruteForce(boost::int32_t& days)
{
    boost::int32_t year = 1970;

    // Handle 400-year blocks - which always have the same
    // number of days (14097) - to cut down on iterations.
    year += (days / 146097) * 400;
    days %= 146097;

    if (days >= 0)
    {
        for (;;)
	    {
            bool isleap = IS_LEAP_YEAR(year - 1900);
            if (days < (isleap ? 366 : 365)) break;
	        year++;
	        days -= isleap ? 366 : 365;
	    }
    }
    else
    {
        do
	    {
	        --year;
	        bool isleap = IS_LEAP_YEAR(year - 1900);
	        days += isleap ? 366 : 365;
	    } while (days < 0);
    }
    return year - 1900;
}

void fillGnashTime (double time, GnashTime& gt)
{

    gt.millisecond = std::fmod(time, 1000.0);
    time /= 1000.0;
    
    // Get the sub-day part of the time, if any and reduce time
    // to number of complete days.
    boost::int32_t remainder = static_cast<boost::int32_t>(std::fmod(time, 86400.0));
    boost::int32_t days = static_cast<boost::int32_t>(time / 86400.0); // complete days
   
    gt.second = remainder % 60;
    remainder /= 60;

    gt.minute = remainder % 60;
    remainder /= 60;

    gt.hour = remainder % 24;
 
    if (time < 0)
    {
        if (gt.millisecond < 0) { gt.millisecond += 1000; gt.second--; }
        if (gt.second < 0) { gt.second += 60; gt.minute--; }
        if (gt.minute < 0) { gt.minute += 60; gt.hour--; }
        if (gt.hour < 0) { gt.hour += 24; days--; }
    }

    if (days >= -4) gt.weekday = (days + 4) % 7;
    else gt.weekday = 6 - (((-5) - days ) % 7);

    // approximate way:
    //gt.year = getYearApproximate(days);

    /// swfdec way:
    //gt.year = getYearMathematical(static_cast<double>(days));
    //days -= 365 * (gt.year - 1970) + COUNT_LEAP_YEARS(gt.year - 1900);
    //gt.year -= 1900;

    // brute force way:
    gt.year = getYearBruteForce(days);
            
    gt.month = 0;
    for (int i = 0; i < 12; ++i)
    {
        if (days - daysInMonth[IS_LEAP_YEAR (gt.year)][i] < 0)
        {
            gt.month = i;
            break;
        }
        days -= daysInMonth[IS_LEAP_YEAR (gt.year)][i];
    }
    
    gt.monthday = days + 1;

}

} // end of gnash namespace
