// 
//   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Free Software
//   Foundation, Inc
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


////////////////////////////////////////////////////////////
//
// Implementation of Netscape entry points (NPN_*)
//
#include "plugin.h"
#include "npapi.h"
#include "npfunctions.h"

#ifndef HIBYTE
#define HIBYTE(x) ((((uint32_t)(x)) & 0xff00) >> 8)
#endif

#ifndef LOBYTE
#define LOBYTE(W) ((W) & 0xFF)
#endif

extern NPNetscapeFuncs NPNFuncs;

void
NPN_Version(int* plugin_major, int* plugin_minor,
            int* netscape_major, int* netscape_minor)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    *plugin_major   = NP_VERSION_MAJOR;
    *plugin_minor   = NP_VERSION_MINOR;
    *netscape_major = HIBYTE(NPNFuncs.version);
    *netscape_minor = LOBYTE(NPNFuncs.version);
}

NPError
NPN_GetURLNotify(NPP instance,
                 const char *url, const char *target, void* notifyData)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    int navMinorVers = NPNFuncs.version & 0xFF;
    NPError rv = NPERR_NO_ERROR;
    
    if( navMinorVers >= NPVERS_HAS_NOTIFICATION ) {
        rv = NPNFuncs.geturlnotify(instance, url, target, notifyData);
    } else {
        rv = NPERR_INCOMPATIBLE_VERSION_ERROR;
    }
    
    return rv;
}

NPError
NPN_GetURL(NPP instance, const char *url, const char *target)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    NPError rv = NPNFuncs.geturl(instance, url, target);
    return rv;
}

NPError
NPN_PostURLNotify(NPP instance, const char* url, const char* window,
                  uint32_t len, const char* buf, NPBool file, void* notifyData)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    int navMinorVers = NPNFuncs.version & 0xFF;
    NPError rv = NPERR_NO_ERROR;
    
    if( navMinorVers >= NPVERS_HAS_NOTIFICATION ) {
        rv = NPNFuncs.posturlnotify(instance, url, window, len, buf, file,
                                    notifyData);
    } else {
        rv = NPERR_INCOMPATIBLE_VERSION_ERROR;
    }
    
    return rv;
}

NPError
NPN_PostURL(NPP instance, const char* url, const char* window,
            uint32_t len, const char* buf, NPBool file)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    NPError rv = NPNFuncs.posturl(instance, url, window, len, buf, file);
    return rv;
} 

NPError
NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    NPError rv = NPNFuncs.requestread(stream, rangeList);
    return rv;
}

NPError
NPN_NewStream(NPP instance, NPMIMEType type, const char* target,
                      NPStream** stream)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    int navMinorVersion = NPNFuncs.version & 0xFF;

    NPError rv = NPERR_NO_ERROR;
    
    if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT ) {
        rv = NPNFuncs.newstream(instance, type, target, stream);
    } else {
        rv = NPERR_INCOMPATIBLE_VERSION_ERROR;
    }
    
    return rv;
}

int32_t
NPN_Write(NPP instance, NPStream *stream, int32_t len, void *buffer)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    int navMinorVersion = NPNFuncs.version & 0xFF;
    int32_t rv = 0;
    
    if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT ) {
        rv = NPNFuncs.write(instance, stream, len, buffer);
    } else {
        rv = -1;
    }
    
    return rv;
}

NPError
NPN_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    int navMinorVersion = NPNFuncs.version & 0xFF;
    NPError rv = NPERR_NO_ERROR;
    
    if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT ) {
        rv = NPNFuncs.destroystream(instance, stream, reason);
    } else {
        rv = NPERR_INCOMPATIBLE_VERSION_ERROR;
    }
    
    return rv;
}

void
NPN_Status(NPP instance, const char *message)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    NPNFuncs.status(instance, message);
}

const char *
NPN_UserAgent(NPP instance)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    const char * rv = NULL;
    rv = NPNFuncs.uagent(instance);
    return rv;
}

void *
NPN_MemAlloc(uint32_t size)
{
    void * rv = NULL;
    rv = NPNFuncs.memalloc(size);
    return rv;
}

void
NPN_MemFree(void* ptr)
{
    NPNFuncs.memfree(ptr);
}

uint32_t
NPN_MemFlush(uint32_t size)
{
    uint32_t rv = NPNFuncs.memflush(size);
    return rv;
}

void
NPN_ReloadPlugins(NPBool reloadPages)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    NPNFuncs.reloadplugins(reloadPages);
}

NPError
NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    NPError rv = NPNFuncs.getvalue(instance, variable, value);
    return rv;
}

NPError
NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    NPError rv = NPNFuncs.setvalue(instance, variable, value);
    return rv;
}

void
NPN_InvalidateRect(NPP instance, NPRect *invalidRect)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    NPNFuncs.invalidaterect(instance, invalidRect);
}

void
NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    NPNFuncs.invalidateregion(instance, invalidRegion);
}

void
NPN_ForceRedraw(NPP instance)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    NPNFuncs.forceredraw(instance);
}

NPIdentifier
NPN_GetStringIdentifier(const NPUTF8 *name)
{
    return NPNFuncs.getstringidentifier(name);
}

void
NPN_GetStringIdentifiers(const NPUTF8 **names, uint32_t nameCount,
                              NPIdentifier *identifiers)
{
    return NPNFuncs.getstringidentifiers(names, nameCount, identifiers);
}

NPIdentifier
NPN_GetStringIdentifier(int32_t intid)
{
    return NPNFuncs.getintidentifier(intid);
}

bool
NPN_IdentifierIsString(NPIdentifier identifier)
{
    return NPNFuncs.identifierisstring(identifier);
}

NPUTF8 *
NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
    return NPNFuncs.utf8fromidentifier(identifier);
}

int32_t
NPN_IntFromIdentifier(NPIdentifier identifier)
{
    return NPNFuncs.intfromidentifier(identifier);
}

NPObject *
NPN_CreateObject(NPP npp, NPClass *aClass)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.createobject(npp, aClass);
}

NPObject *
NPN_RetainObject(NPObject *obj)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.retainobject(obj);
}

void
NPN_ReleaseObject(NPObject *obj)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.releaseobject(obj);
}

bool
NPN_Invoke(NPP npp, NPObject* obj, NPIdentifier methodName,
           const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.invoke(npp, obj, methodName, args, argCount, result);
}

bool
NPN_InvokeDefault(NPP npp, NPObject* obj, const NPVariant *args,
                  uint32_t argCount, NPVariant *result)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.invokeDefault(npp, obj, args, argCount, result);
}

bool
NPN_Evaluate(NPP npp, NPObject* obj, NPString *script,
             NPVariant *result)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.evaluate(npp, obj, script, result);
}

bool
NPN_GetProperty(NPP npp, NPObject* obj, NPIdentifier propertyName,
                NPVariant *result)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.getproperty(npp, obj, propertyName, result);
}

bool
NPN_SetProperty(NPP npp, NPObject* obj, NPIdentifier propertyName,
                const NPVariant *value)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.setproperty(npp, obj, propertyName, value);
}

bool
NPN_RemoveProperty(NPP npp, NPObject* obj, NPIdentifier propertyName)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.removeproperty(npp, obj, propertyName);
}

bool
NPN_Enumerate(NPP npp, NPObject *obj, NPIdentifier **identifier,
              uint32_t *count)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.enumerate(npp, obj, identifier, count);
}

bool
NPN_Construct(NPP npp, NPObject *obj, const NPVariant *args,
              uint32_t argCount, NPVariant *result)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.construct(npp, obj, args, argCount, result);
}

bool
NPN_HasProperty(NPP npp, NPObject* obj, NPIdentifier propertyName)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.hasproperty(npp, obj, propertyName);
}

bool
NPN_HasMethod(NPP npp, NPObject* obj, NPIdentifier methodName)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.hasmethod(npp, obj, methodName);
}

void
NPN_ReleaseVariantValue(NPVariant *variant)
{
    NPNFuncs.releasevariantvalue(variant);
}

void
NPN_SetException(NPObject* obj, const NPUTF8 *message)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    NPNFuncs.setexception(obj, message);
}

NPError
NPN_GetValueForURL(NPP instance, NPNURLVariable variable,
                   const char *url, char **value, uint32_t *len)
{
    gnash::log_debug("%s: %s", __PRETTY_FUNCTION__, url);
    
    return NPNFuncs.getvalueforurl(instance, variable, url, value, len);
}

NPError
NPN_SetValueForURL(NPP instance, NPNURLVariable variable,
                   const char *url, const char *value, uint32_t len)
{
    gnash::log_debug("%s: %s, %s", __PRETTY_FUNCTION__, url, value);

    return NPNFuncs.setvalueforurl(instance, variable, url, value, len);
}

NPError
NPN_GetAuthenticationInfo(NPP instance, const char *protocol,
                          const char *host, int32_t port,
                          const char *scheme, const char *realm,
                          char **username, uint32_t *ulen,
                          char **password, uint32_t *plen)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    return NPNFuncs.getauthenticationinfo(instance, protocol, host, port,
                                          scheme, realm, username, ulen,
                                          password, plen);
}
  
void
NPN_PluginThreadAsyncCall(NPP plugin, void (*func)(void *), void *userData)
{
    gnash::log_debug(__PRETTY_FUNCTION__);

    NPNFuncs.pluginthreadasynccall(plugin, func, userData);
}

// Local Variables:
// mode: C++
// indent-tabs-mode: nil
// End:
