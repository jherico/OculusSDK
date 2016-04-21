/************************************************************************************

Filename    :   Render_GL_Win32 Device.cpp
Content     :   Win32 OpenGL Device implementation
Created     :   September 10, 2012
Authors     :   Andrew Reisse, Michael Antonov, David Borel

Copyright   :   Copyright 2012 Oculus VR, LLC. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "Render_GL_Win32_Device.h"
#include "OVR_CAPI_GL.h"

#include <stdlib.h>
#include <dwmapi.h>

namespace OVR { namespace Render { namespace GL { namespace Win32 {

#pragma warning(disable : 4995) // The compiler encountered a function that was marked with pragma deprecated.



// ***** GL::Win32::RenderDevice
    
RenderDevice::RenderDevice(ovrSession session, const Render::RendererParams& p, HWND win, HGLRC gl)
    : GL::RenderDevice(session, p)
    , Window(win)
    , WglContext(gl)
{
    OVR_UNUSED(p);
}


// Implement static initializer function to create this class.
Render::RenderDevice* RenderDevice::CreateDevice(ovrSession session, const RendererParams& rp, void* oswnd, ovrGraphicsLuid luid)
{
    // FIXME: Figure out how to best match luid's in OpenGL
    OVR_UNUSED(luid);

    HWND hwnd = (HWND)oswnd;
	HDC dc = GetDC(hwnd);
    
	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARBFunc = NULL;
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARBFunc = NULL;

    {
        // First create a context for the purpose of getting access to wglChoosePixelFormatARB / wglCreateContextAttribsARB.
	    PIXELFORMATDESCRIPTOR pfd;
	    memset(&pfd, 0, sizeof(pfd));

	    pfd.nSize       = sizeof(pfd);
	    pfd.nVersion    = 1;
	    pfd.iPixelType  = PFD_TYPE_RGBA;
	    pfd.dwFlags     = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
	    pfd.cColorBits  = 32;
	    pfd.cDepthBits  = 16;

	    int pf = ChoosePixelFormat(dc, &pfd);
	    if (!pf)
	    {
		    ReleaseDC(hwnd, dc);
		    return NULL;
	    }
		
	    if (!SetPixelFormat(dc, pf, &pfd))
	    {
		    ReleaseDC(hwnd, dc);
		    return NULL;
	    }

	    HGLRC context = wglCreateContext(dc);
	    if (!wglMakeCurrent(dc, context))
	    {
		    wglDeleteContext(context);
		    ReleaseDC(hwnd, dc);
		    return NULL;
	    }

	    wglChoosePixelFormatARBFunc = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
	    wglCreateContextAttribsARBFunc = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
        OVR_ASSERT(wglChoosePixelFormatARBFunc && wglCreateContextAttribsARBFunc);

	    wglDeleteContext(context);
    }

    // Now create the real context that we will be using.
	int iAttributes[] = {
		//WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_COLOR_BITS_ARB, 32,
        WGL_DEPTH_BITS_ARB, 16,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
		WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
        0, 0};

    float fAttributes[] = {0,0};
	int   pf = 0;
	UINT  numFormats = 0;

	if (!wglChoosePixelFormatARBFunc(dc, iAttributes, fAttributes, 1, &pf, &numFormats))
    {
        ReleaseDC(hwnd, dc);
        return NULL;
    }

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));

    if (!SetPixelFormat(dc, pf, &pfd))
    {
        ReleaseDC(hwnd, dc);
        return NULL;
    }

    GLint attribs[16];
    int   attribCount = 0;
    int   flags = 0;
    int   profileFlags = 0;

    // Version
    if(rp.GLMajorVersion)
    {
        attribs[attribCount++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
        attribs[attribCount++] = rp.GLMajorVersion;
        attribs[attribCount++] = WGL_CONTEXT_MINOR_VERSION_ARB;
        attribs[attribCount++] = rp.GLMinorVersion;
    }

    // Flags
    if(rp.DebugEnabled)
        flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
    if(rp.GLForwardCompatibleProfile)
        flags |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
    if(flags)
    {
        attribs[attribCount++] = WGL_CONTEXT_FLAGS_ARB;
        attribs[attribCount++] = flags;
    }
    
    // Profile flags
    if(rp.GLCoreProfile)
        profileFlags |= WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
    else if(rp.GLCompatibilityProfile)
        profileFlags |= WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
    if(profileFlags)
    {
        attribs[attribCount++] = WGL_CONTEXT_PROFILE_MASK_ARB;
        attribs[attribCount++] = profileFlags;
    }

    attribs[attribCount] = 0;

	HGLRC context = wglCreateContextAttribsARBFunc(dc, 0, attribs);
	if (!wglMakeCurrent(dc, context))
	{
		wglDeleteContext(context);
		ReleaseDC(hwnd, dc);
		return NULL;
	}

    InitGLExtensions();

    return new RenderDevice(session, rp, hwnd, context);
}

bool RenderDevice::Present(bool useVsync)
{
	BOOL success;
	int swapInterval = (useVsync) ? 1 : 0;
	if (wglGetSwapIntervalEXT() != swapInterval)
		wglSwapIntervalEXT(swapInterval);

	HDC dc = GetDC(Window);
	success = SwapBuffers(dc);
	ReleaseDC(Window, dc);

    OVR_ASSERT(success == TRUE);
    return success == TRUE;
}

void RenderDevice::Shutdown()
{
    //Release any remaining GL resources.
    GL::RenderDevice::Shutdown();

    if (WglContext)
    {
        wglMakeCurrent(NULL,NULL);
        wglDeleteContext(WglContext);
        WglContext = NULL;
        Window = NULL;
    }
}

bool RenderDevice::SetParams(const RendererParams& newParams)
{
    Params = newParams;
    //TODO: Apply changes now.
	return true;
}

}}}}

