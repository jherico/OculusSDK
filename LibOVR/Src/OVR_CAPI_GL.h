/************************************************************************************

Filename    :   OVR_CAPI_GL.h
Content     :   GL specific structures used by the CAPI interface.
Created     :   November 7, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2013 Oculus VR, LLC. All Rights reserved.

Use of this software is subject to the terms of the Oculus Inc license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

************************************************************************************/
#ifndef OVR_CAPI_GL_h
#define OVR_CAPI_GL_h

/// @file OVR_CAPI_GL.h
/// OpenGL rendering support.

#include "OVR_CAPI.h"
#if defined(__APPLE__)
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif


/// Used to configure slave GL rendering (i.e. for devices created externally).
typedef struct OVR_ALIGNAS(8) ovrGLConfigData_s
{
    /// General device settings.
    ovrRenderAPIConfigHeader Header;

#if defined(OVR_OS_WIN32)
    /// The optional window handle. If unset, rendering will use the current window.
    HWND Window;
    /// The optional device context. If unset, rendering will use a new context.
    HDC  DC;
#elif defined (OVR_OS_LINUX)
    /// Optional display. If unset, will issue glXGetCurrentDisplay when context
    /// is current.
    struct _XDisplay* Disp;
#endif
} ovrGLConfigData;

/// Contains OpenGL-specific rendering information.
union ovrGLConfig
{
    /// General device settings.
    ovrRenderAPIConfig Config;
    /// OpenGL-specific settings.
    ovrGLConfigData    OGL;
};

/// Used to pass GL eye texture data to ovrHmd_EndFrame.
typedef struct OVR_ALIGNAS(8) ovrGLTextureData_s
{
    /// General device settings.
    ovrTextureHeader Header;
    /// The OpenGL name for this texture.
    GLuint           TexId;       
} ovrGLTextureData;

static_assert(offsetof(ovrGLTextureData, TexId) == offsetof(ovrTexture, PlatformData), "Mismatch of structs that are presumed binary equivalents.");

/// Contains OpenGL-specific texture information.
typedef union ovrGLTexture_s
{
    /// General device settings.
    ovrTexture       Texture;
    /// OpenGL-specific settings.
    ovrGLTextureData OGL;
} ovrGLTexture;

#endif	// OVR_CAPI_GL_h
