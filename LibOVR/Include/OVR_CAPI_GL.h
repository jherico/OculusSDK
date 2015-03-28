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

#if defined(OVR_OS_WIN32)
    #include <Windows.h>
    #include <gl/GL.h>
#elif defined(__APPLE__)
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif


#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4324) // structure was padded due to __declspec(align())
#endif


/// Used to configure slave GL rendering (i.e. for devices created externally).
typedef struct ovrGLConfigData_s
{
    ovrRenderAPIConfigHeader Header;    ///< General device settings.

#if defined(OVR_OS_WIN32)
    HWND Window;                ///< The optional window handle. If unset, rendering will use the current window.
    HDC  DC;                    ///< The optional device context. If unset, rendering will use a new context.
#elif defined (OVR_OS_LINUX)
    struct _XDisplay* Disp;     ///< Optional display. If unset, will issue glXGetCurrentDisplay when context is current.
#endif
} ovrGLConfigData;

#if defined(__cplusplus)
    static_assert(sizeof(ovrRenderAPIConfig) >= sizeof(ovrGLConfigData), "Insufficient size.");
#endif

/// Contains OpenGL-specific rendering information.
union ovrGLConfig
{
    ovrRenderAPIConfig Config;  ///< General device settings.
    ovrGLConfigData    OGL;     ///< OpenGL-specific settings.
};

/// Used to pass GL eye texture data to ovrHmd_EndFrame.
typedef struct ovrGLTextureData_s
{
    ovrTextureHeader Header;    ///< General device settings.
    GLuint           TexId;     ///< The OpenGL name for this texture.
} ovrGLTextureData;

#if defined(__cplusplus)
    static_assert(sizeof(ovrTexture) >= sizeof(ovrGLTextureData), "Insufficient size.");
#endif

/// Contains OpenGL-specific texture information.
typedef union ovrGLTexture_s
{
    ovrTexture       Texture;   ///< General device settings.
    ovrGLTextureData OGL;       ///< OpenGL-specific settings.
} ovrGLTexture;


#if defined(_MSC_VER)
    #pragma warning(pop)
#endif


#endif    // OVR_CAPI_GL_h
