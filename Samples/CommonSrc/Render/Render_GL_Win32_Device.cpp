/************************************************************************************

Filename    :   Render_GL_Win32 Device.cpp
Content     :   Win32 OpenGL Device implementation
Created     :   September 10, 2012
Authors     :   Andrew Reisse, Michael Antonov

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

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

namespace OVR { namespace Render { namespace GL { namespace Win32 {


// ***** GL::Win32::RenderDevice

// Implement static initializer function to create this class.
Render::RenderDevice* RenderDevice::CreateDevice(const RendererParams&, void* oswnd)
{
    HWND hwnd = (HWND)oswnd;

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));

    pfd.nSize       = sizeof(pfd);
    pfd.nVersion    = 1;
    pfd.iPixelType  = PFD_TYPE_RGBA;
    pfd.dwFlags     = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    pfd.cColorBits  = 32;
    pfd.cDepthBits  = 16;

    HDC dc = GetDC(hwnd);
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

   // return new RenderDevice(rp, hwnd, dc, context);
    return 0;
}


void RenderDevice::Present()
{
    SwapBuffers(GdiDc);
}

void RenderDevice::Shutdown()
{
    if (WglContext)
    {
        wglMakeCurrent(NULL,NULL);
        wglDeleteContext(WglContext);
        ReleaseDC(Window, GdiDc);
        WglContext = NULL;
        GdiDc = NULL;
        Window = NULL;
    }
}

}}}}

