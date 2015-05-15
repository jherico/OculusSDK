/********************************************************************************//**

\file OVR_CAPI_GL.h
\brief GL specific structures used by the CAPI interface.
\date November 7, 2013
\author Lee Cooper

\copyright Copyright 2013 Oculus VR, LLC. All Rights reserved.
\n
Use of this software is subject to the terms of the Oculus Inc license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

************************************************************************************/

#ifndef OVR_CAPI_GL_h
#define OVR_CAPI_GL_h

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

/// Used to pass GL eye texture data to ovrHmd_EndFrame.
typedef struct ovrGLTextureData_s
{
    ovrTextureHeader Header;    ///< General device settings.
    GLuint           TexId;     ///< The OpenGL name for this texture.
} ovrGLTextureData;

#if defined(__cplusplus)
    static_assert(sizeof(ovrTexture) >= sizeof(ovrGLTextureData), "Insufficient size.");
    static_assert(sizeof(ovrGLTextureData) == sizeof(ovrTextureHeader) + 4, "size mismatch");
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



/// Creates a Texture Set suitable for use with OpenGL.
///
/// \param[in]  hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in]  format Specifies the texture format.
/// \param[in]  width Specifies the requested texture width.
/// \param[in]  height Specifies the requested texture height.
/// \param[out] outTextureSet Specifies the created ovrSwapTextureSet, which will be valid only upon a successful return value.
///             This texture set must be eventually destroyed via ovrHmd_DestroySwapTextureSet before destroying the HMD with ovrHmd_Destroy.
///
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information.
///
/// \see ovrHmd_DestroySwapTextureSet
///
OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_CreateSwapTextureSetGL(ovrHmd hmd, GLuint format,
                                                             int width, int height,
                                                             ovrSwapTextureSet** outTextureSet);


/// Creates a Mirror Texture which is auto-refreshed to mirror Rift contents produced by this application.
///
/// \param[in]  hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in]  format Specifies the texture format.
/// \param[in]  width Specifies the requested texture width.
/// \param[in]  height Specifies the requested texture height.
/// \param[out] outMirrorTexture Specifies the created ovrTexture, which will be valid only upon a successful return value.
///             This texture must be eventually destroyed via ovrHmd_DestroyMirrorTexture before destroying the HMD with ovrHmd_Destroy.
///
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information.
///
/// \see ovrHmd_DestroyMirrorTexture
///
OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_CreateMirrorTextureGL(ovrHmd hmd, GLuint format,
                                                            int width, int height,
                                                            ovrTexture** outMirrorTexture);


#endif    // OVR_CAPI_GL_h
