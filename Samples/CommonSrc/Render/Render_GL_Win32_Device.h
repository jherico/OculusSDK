/************************************************************************************

Filename    :   Render_GL_Win32 Device.h
Content     :   Win32 OpenGL Device implementation header
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

#ifndef OVR_Render_GL_Win32_Device_h
#define OVR_Render_GL_Win32_Device_h

#include "Render_GL_Device.h"

#ifdef OVR_OS_WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif


namespace OVR { namespace Render { namespace GL { namespace Win32 {

// ***** GL::Win32::RenderDevice

// Win32-Specific GL Render Device, used to create OpenGL under Windows.
class RenderDevice : public GL::RenderDevice
{
    friend BOOL CALLBACK MonitorEnumFunc(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData);

    HWND Window;
    HGLRC WglContext;
    Recti PreFullscreen;
    Recti FSDesktop;
    HMONITOR HMonitor;

public:
    RenderDevice(const Render::RendererParams& p, HWND win, HGLRC gl);
    virtual ~RenderDevice() { Shutdown(); }

    // Implement static initializer function to create this class.
    static Render::RenderDevice* CreateDevice(const RendererParams& rp, void* oswnd);
	
	virtual ovrRenderAPIConfig Get_ovrRenderAPIConfig() const;

    virtual void Shutdown();
    virtual void Present(bool withVsync);
	bool SetParams(const RendererParams& newParams);

    virtual bool SetFullscreen(DisplayMode fullscreen);
};


}}}} // OVR::Render::GL::Win32

#endif
