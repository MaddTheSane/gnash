//
//   Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
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
//

/* $Id: gtk_glue_agg.cpp,v 1.20 2007/06/27 17:05:31 udog Exp $ */


/// \page gtk_shm_support GTK shared memory extension support
/// 
/// The GTK-AGG combination supports the use of the X11 MIT-SHM extension.
/// This extension allows passing image data to the X server in it's native
/// format (defined by the graphics mode). This prevents CPU intensive pixel
/// format conversions for the X server.
///
/// Not all X servers support this extension and it's available for local
/// (not networked) X connections anyway. So the GTK GUI will first *try*
/// to use the extension and on failure provide automatic fallback to standard
/// pixmaps.
///
/// You won't notice this fallback unless you check the log messages (aside
/// from potential performance difference.)
///
/// The macro ENABLE_MIT_SHM must be defined in gtk_glue_agg.h to enable
/// support for the MIT-SHM extension.
///
/// For more information about the extension, have a look at these URLs:
/// http://en.wikipedia.org/wiki/MIT-SHM
/// http://www.xfree86.org/current/mit-shm.html  


// Also worth checking: http://en.wikipedia.org/wiki/X_video_extension


#include <cstdio>
#include <cerrno>
#include <cstring>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gnash.h"
#include "log.h"
#include "render_handler.h"
#include "render_handler_agg.h"
#include "gtk_glue_agg.h"

#ifdef ENABLE_MIT_SHM
#include <X11/Xlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <gdk/gdkprivate.h>
#endif


namespace gnash
{

GtkAggGlue::GtkAggGlue() :
	_offscreenbuf(NULL),
	_offscreenbuf_size(0),
	_agg_renderer(NULL),
	_width(0),
	_height(0),
	_bpp(0)
{
}

GtkAggGlue::~GtkAggGlue()
{
  if (_offscreenbuf) {
    free(_offscreenbuf);
    _offscreenbuf=NULL;
  }
  
  destroy_shm_image();
}

bool
GtkAggGlue::init(int /*argc*/, char **/*argv*/[])
{
    gdk_rgb_init();
    
    _have_shm = check_mit_shm(gdk_display);
    
#ifdef PIXELFORMAT_RGB565
    _bpp = 16;
#else
    // GDK's gdk_draw_rgb_image() needs 24-bit RGB data, so we initialize the
    // AGG renderer with RGB24 and let GTK take care of the proper pixel format.
    _bpp = 24;
#endif
    return true;
}

bool 
GtkAggGlue::check_mit_shm(Display *display) 
{
#ifdef ENABLE_MIT_SHM
  int major, minor, dummy;
  Bool pixmaps;
  
  log_msg("Checking support for MIT-SHM...");
  
  if (!XQueryExtension(display, "MIT-SHM", &dummy, &dummy, &dummy)) 
  {
    log_msg("WARNING: No MIT-SHM extension available, using standard XLib "
      "calls (slower)");
    return false;
  }
  
  if (XShmQueryVersion(display, &major, &minor, &pixmaps )!=True)
	{
    log_msg("WARNING: MIT-SHM not ready (network link?), using standard XLib "
      "calls (slower)");
    return false;
	}
	
	log_msg("NOTICE: MIT-SHM available (version %d.%d)!", major, minor);
	
	return true;
	
	
#else
	return false; // !ifdef ENABLE_MIT_SHM
#endif
  
}

void 
GtkAggGlue::create_shm_image(unsigned int width, unsigned int height)
{

  // destroy any already existing structures
  destroy_shm_image();

#ifdef ENABLE_MIT_SHM
  GdkVisual* visual = gdk_drawable_get_visual(_drawing_area->window);
  Visual* xvisual = GDK_VISUAL_XVISUAL(visual); 
  
  // prepare segment info (populated by XShmCreateImage)
  _shm_info = (XShmSegmentInfo*) malloc(sizeof(XShmSegmentInfo));  
  assert(_shm_info != NULL);
  
  // create shared memory XImage
  _shm_image = XShmCreateImage(gdk_display, xvisual, visual->depth, 
    ZPixmap, NULL, _shm_info, width, height);
    
  if (!_shm_image) {
    log_msg("Failed creating the shared memory XImage!");
    destroy_shm_image();
    return;
  }
  
  // create shared memory segment
  _shm_info->shmid = shmget(IPC_PRIVATE, 
    _shm_image->bytes_per_line * _shm_image->height, IPC_CREAT|0777);
    
  if (_shm_info->shmid == -1) {
    log_msg("Failed requesting shared memory segment (%s). Perhaps the "
      "required memory size is bigger than the limit set by the kernel.",
      strerror(errno));
    destroy_shm_image();
    return;
  }
  
  // attach the shared memory segment to our process
  _shm_info->shmaddr = _shm_image->data = (char*) shmat(_shm_info->shmid, 0, 0);
  
  if (_shm_info->shmaddr == (char*) -1) {
    log_msg("Failed attaching to shared memory segment: %s", strerror(errno));
    destroy_shm_image();
    return;
  }
  
  // Give the server full access to our memory segment. We just follow
  // the documentation which recommends this, but we could also give him
  // just read-only access since we don't need XShmGetImage...
  _shm_info->readOnly = False;
  
  // Finally, tell the server to attach to our shared memory segment  
  if (!XShmAttach(gdk_display, _shm_info)) {
    log_msg("Server failed attaching to the shared memory segment");
    destroy_shm_image();
    return;
  }
  
  //log_msg("create_shm_image() OK"); // <-- remove this
#endif
   
}

void 
GtkAggGlue::destroy_shm_image()
{
#ifdef ENABLE_MIT_SHM
  if (_shm_image) {  
    XDestroyImage(_shm_image);
    _shm_image=NULL;
  }
  
  if (_shm_info) {
    free(_shm_info);
    _shm_info=NULL;
  }
  
#endif
}

void
GtkAggGlue::prepDrawingArea(GtkWidget *drawing_area)
{
    _drawing_area = drawing_area;
}

render_handler*
GtkAggGlue::create_shm_handler()
{
#ifdef ENABLE_MIT_SHM
  GdkVisual *visual = gdk_drawable_get_visual(_drawing_area->window);
  
  // Create a dummy SHM image to detect it's pixel format (we can't use the 
  // info from "visual"). 
  // Is there a better way??
  
  create_shm_image(256,256);
  
  if (!_shm_image) return NULL;
  
  unsigned int red_shift, red_prec;
  unsigned int green_shift, green_prec;
  unsigned int blue_shift, blue_prec;

  decode_mask(_shm_image->red_mask,   &red_shift,   &red_prec);
  decode_mask(_shm_image->green_mask, &green_shift, &green_prec);
  decode_mask(_shm_image->blue_mask,  &blue_shift,  &blue_prec);
  
  
  log_msg("X server pixel format is (R%d:%d, G%d:%d, B%d:%d, %d bpp)",
    red_shift, red_prec,
    green_shift, green_prec,
    blue_shift, blue_prec,
    _shm_image->bits_per_pixel);
  
  
  char *pixelformat = agg_detect_pixel_format(
    red_shift, red_prec,
    green_shift, green_prec,
    blue_shift, blue_prec,
    _shm_image->bits_per_pixel);
    
  destroy_shm_image();

  if (!pixelformat) {
    log_msg("Pixel format of X server not recognized!");
    return NULL; 
  }

  log_msg("X server is using %s pixel format", pixelformat);
  
  render_handler* res = create_render_handler_agg(pixelformat);
  
  if (!res) {
    log_msg("Failed creating a renderer instance for this pixel format. "
      "Most probably Gnash has not compiled in (configured) support "
      "for this pixel format - using standard pixmaps instead");
      
    // disable use of shared memory pixmaps
    _have_shm = false;
  }      
  
  
  return res;
    
#else
  return NULL;
#endif
}

render_handler*
GtkAggGlue::createRenderHandler()
{

  // try with MIT-SHM
  if (_have_shm) {
    _agg_renderer = create_shm_handler();
    if (_agg_renderer) return _agg_renderer;
  }

#ifdef PIXELFORMAT_RGB565
#warning A pixel format of RGB565; you must have a (hacked) GTK which supports \
         this format (e.g., GTK on the OLPC).
    _agg_renderer = create_render_handler_agg("RGB565");
#else
    _agg_renderer = create_render_handler_agg("RGB24");
#endif
    return _agg_renderer;
}

void
GtkAggGlue::setRenderHandlerSize(int width, int height)
{
	assert(width>0);
	assert(height>0);
	assert(_agg_renderer!=NULL);

	#define CHUNK_SIZE (100*100*(_bpp/8))
	
	if (width == _width && height == _height)
	   return;
	   
  _width = width;
	_height = height;
	   
	   
	// try shared image first
	if (_have_shm)
	  create_shm_image(width, height);
	  
#ifdef ENABLE_MIT_SHM
	if (_shm_image) {
	
	  // ==> use shared memory image (faster)
	  
	  log_msg("GTK-AGG: Using shared memory image");
    
    if (_offscreenbuf) {  
      free(_offscreenbuf);
      _offscreenbuf = NULL;
    }
	  
  	static_cast<render_handler_agg_base *>(_agg_renderer)->init_buffer(
  	  (unsigned char*) _shm_info->shmaddr,
  		_shm_image->bytes_per_line * _shm_image->height,
  		_width,
  		_height
  	);
	
  } else {
#endif  	
  
    // ==> use standard pixmaps (slower, but should work in any case)

  	int new_bufsize = width*height*(_bpp/8);
  	
  	// TODO: At the moment we only increase the buffer and never decrease it. Should be
  	// changed sometime.
  	if (new_bufsize > _offscreenbuf_size) {
  		new_bufsize = static_cast<int>(new_bufsize / CHUNK_SIZE + 1) * CHUNK_SIZE;
  		// TODO: C++ conform alternative to realloc?
  		_offscreenbuf	= static_cast<unsigned char *>( realloc(_offscreenbuf, new_bufsize) );
  
  		if (!_offscreenbuf) {
  		  log_msg("Could not allocate %i bytes for offscreen buffer: %s\n",
  				new_bufsize, strerror(errno)
  			);
  			return;
  		}
  
  		log_msg("GTK-AGG: %i bytes offscreen buffer allocated\n", new_bufsize);
  
  		_offscreenbuf_size = new_bufsize;
  		memset(_offscreenbuf, 0, new_bufsize);
  	}
  	
  	// Only the AGG renderer has the function init_buffer, which is *not* part of
  	// the renderer api. It allows us to change the renderers movie size (and buffer
  	// address) during run-time.
  	static_cast<render_handler_agg_base *>(_agg_renderer)->init_buffer(
  	  _offscreenbuf,
  		_offscreenbuf_size,
  		_width,
  		_height
  	);
  	
  	
#ifdef ENABLE_MIT_SHM
  }
#endif  
	
}

void
GtkAggGlue::render()
{

#ifdef ENABLE_MIT_SHM
  if (_shm_image) {
  
    XShmPutImage(
      gdk_display, 
      GDK_WINDOW_XWINDOW(_drawing_area->window), 
      GDK_GC_XGC(_drawing_area->style->fg_gc[GTK_STATE_NORMAL]),  // ???
      _shm_image,
      0, 0,
      0, 0,
      _width, _height,
      False); 
      
      // <Udo>:
      // The shared memory buffer is copied in background(!) since the X 
      // calls are executed asynchroneously. This is dangerous because it
      // may happen that the renderer updates the buffer while the X server
      // still copies the data to the VRAM (flicker can occurr).
      // Normally this is avoided using the XShmCompletionEvent which is sent
      // to the client once the buffer has been copied. The last argument to
      // XShmPutImage must be set to True for this. 
      // We'd need to wait for this event before calling the renderer again.
      // I know nothing about X event handling and so I just call XSync here
      // to wait until all commands have been executed. This has the 
      // disadvantage that we can't leave the X server some time till the core 
      // is ready to *render* the next frame. I don't think it would make a 
      // significant performance difference unless data to video ram is very 
      // slow (could be the case for old / embedded computers, though).       
      XSync(gdk_display, False);

  } else {
#endif  

  	// Update the entire screen
  	gdk_draw_rgb_image (
  		_drawing_area->window,
  		_drawing_area->style->fg_gc[GTK_STATE_NORMAL],
  		0,
  		0,
  		_width,
  		_height,
  		GDK_RGB_DITHER_NONE,
  		_offscreenbuf,
  		(int)(_width*_bpp/8)
  	);  	

#ifdef ENABLE_MIT_SHM
  }
#endif  
	
}

void
GtkAggGlue::render(int minx, int miny, int maxx, int maxy)
{

#ifdef ENABLE_MIT_SHM
  if (_shm_image) {
  
    XShmPutImage(
      gdk_display, 
      GDK_WINDOW_XWINDOW(_drawing_area->window), 
      GDK_GC_XGC(_drawing_area->style->fg_gc[GTK_STATE_NORMAL]),  // ???
      _shm_image,
      minx, miny,
      minx, miny,
  		maxx-minx+1, maxy-miny+1,
      False);
      
    XSync(gdk_display, False); // see GtkAggGlue::render(void)
  
  } else {
#endif       

  	// Update only the invalidated rectangle
  	gdk_draw_rgb_image (
  		_drawing_area->window,
  		_drawing_area->style->fg_gc[GTK_STATE_NORMAL],
  		minx,
    	miny,
  		maxx-minx+1,
  		maxy-miny+1,
  		GDK_RGB_DITHER_NORMAL,
  		_offscreenbuf + miny*(_width*(_bpp/8)) + minx*(_bpp/8),
  		(int)((_width)*_bpp/8)
  	);
  	
#ifdef ENABLE_MIT_SHM
  }
#endif  
}

void
GtkAggGlue::configure(GtkWidget *const /*widget*/, GdkEventConfigure *const event)
{
	if (_agg_renderer)
		setRenderHandlerSize(event->width, event->height);
}


void 
GtkAggGlue::decode_mask(unsigned long mask, unsigned int *shift, unsigned int *size)
{
  *shift = 0;
  *size = 0;
  
  if (mask==0) return; // invalid mask
  
  while (!(mask & 1)) {
    (*shift)++;
    mask = mask >> 1;
  }
  
  while (mask & 1) {
    (*size)++;
    mask = mask >> 1;
  }
}

} // namespace gnash

