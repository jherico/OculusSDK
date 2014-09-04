/************************************************************************************

Filename    :   Render_GL_Win32 Device.cpp
Content     :   Win32 OpenGL Device implementation
Created     :   September 10, 2012
Authors     :   Andrew Reisse, Michael Antonov, David Borel

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
#include "OVR_CAPI_GL.h"

#include <dwmapi.h>

namespace OVR { namespace Render { namespace GL { namespace Win32 {

typedef HRESULT (WINAPI *PFNDWMENABLECOMPOSITIONPROC) (UINT);

#pragma warning(disable : 4995)
PFNDWMENABLECOMPOSITIONPROC DwmEnableComposition = NULL;


// ***** GL::Win32::RenderDevice
    
RenderDevice::RenderDevice(const Render::RendererParams& p, HWND win, HGLRC gl)
    : GL::RenderDevice(p)
    , Window(win)
    , WglContext(gl)
	, PreFullscreen(0, 0, 0, 0)
    , HMonitor(0)
	, FSDesktop(0, 0, 0, 0)
{
    OVR_UNUSED(p);
}

// Implement static initializer function to create this class.
Render::RenderDevice* RenderDevice::CreateDevice(const RendererParams& rp, void* oswnd)
{
    HWND hwnd = (HWND)oswnd;
	HDC dc = GetDC(hwnd);
    
    if (!DwmEnableComposition)
    {
	    HINSTANCE hInst = LoadLibrary( L"dwmapi.dll" );
	    OVR_ASSERT(hInst);
        DwmEnableComposition = (PFNDWMENABLECOMPOSITIONPROC)GetProcAddress( hInst, "DwmEnableComposition" );
        OVR_ASSERT(DwmEnableComposition);
    }

    DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
    {
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
		
		wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
		wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

		wglDeleteContext(context);
    }


	int iAttributes[] = {
		//WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_COLOR_BITS_ARB, 32,
        WGL_DEPTH_BITS_ARB, 16,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
		WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
        0, 0};

    float fAttributes[] = {0,0};

	int pf = 0;
	UINT numFormats = 0;

	if (!wglChoosePixelFormatARB(dc, iAttributes, fAttributes, 1, &pf, &numFormats))
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

	GLint attribs[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, 2,
		WGL_CONTEXT_MINOR_VERSION_ARB, 1,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0
	};

	HGLRC context = wglCreateContextAttribsARB(dc, 0, attribs);
	if (!wglMakeCurrent(dc, context))
	{
		wglDeleteContext(context);
		ReleaseDC(hwnd, dc);
		return NULL;
	}

    InitGLExtensions();

    return new RenderDevice(rp, hwnd, context);
}

ovrRenderAPIConfig RenderDevice::Get_ovrRenderAPIConfig() const
{
	static ovrGLConfig cfg;
	cfg.OGL.Header.API         = ovrRenderAPI_OpenGL;
	cfg.OGL.Header.RTSize      = Sizei(WindowWidth, WindowHeight);
	cfg.OGL.Header.Multisample = Params.Multisample;
	cfg.OGL.Window             = Window;

	return cfg.Config;
}

void RenderDevice::Present(bool useVsync)
{
	BOOL success;
	int swapInterval = (useVsync) ? 1 : 0;
	if (wglGetSwapIntervalEXT() != swapInterval)
		wglSwapIntervalEXT(swapInterval);

	HDC dc = GetDC(Window);
	success = SwapBuffers(dc);
	ReleaseDC(Window, dc);

    OVR_ASSERT(success);
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

BOOL CALLBACK MonitorEnumFunc(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData)
{
    RenderDevice* renderer = (RenderDevice*)dwData;

    MONITORINFOEXA monitor;
    monitor.cbSize = sizeof(monitor);

    if (::GetMonitorInfoA(hMonitor, &monitor) && monitor.szDevice[0])
    {
        DISPLAY_DEVICEA dispDev;
        memset(&dispDev, 0, sizeof(dispDev));
        dispDev.cb = sizeof(dispDev);

        if (::EnumDisplayDevicesA(monitor.szDevice, 0, &dispDev, 0))
        {
            if (strstr(String(dispDev.DeviceName).ToCStr(), renderer->GetParams().Display.MonitorName.ToCStr()))
            {
                renderer->HMonitor = hMonitor;
                renderer->FSDesktop.x = monitor.rcMonitor.left;
                renderer->FSDesktop.y = monitor.rcMonitor.top;
				renderer->FSDesktop.w = monitor.rcMonitor.right - monitor.rcMonitor.left;
				renderer->FSDesktop.h = monitor.rcMonitor.bottom - monitor.rcMonitor.top;
                return FALSE;
            }
        }
    }

    return TRUE;
}

bool RenderDevice::SetFullscreen(DisplayMode fullscreen)
{
	if (fullscreen == Params.Fullscreen)
    {
        return true;
    }

    if (Params.Fullscreen == Display_FakeFullscreen)
    {
        SetWindowLong(Window, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS);
        SetWindowPos(Window, HWND_NOTOPMOST, PreFullscreen.x, PreFullscreen.y,
                     PreFullscreen.w, PreFullscreen.h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    else if (Params.Fullscreen == Display_Fullscreen)
    {
        {
            // Find out the name of the device this window
            // is on (this is for multi-monitor setups)
            HMONITOR hMonitor = MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFOEX monInfo;
            memset(&monInfo, 0, sizeof(MONITORINFOEX));
            monInfo.cbSize = sizeof(MONITORINFOEX);
            GetMonitorInfo(hMonitor, &monInfo);

            // Restore the display resolution
            ChangeDisplaySettingsEx(monInfo.szDevice, NULL, NULL, 0, NULL);
            //ChangeDisplaySettings(NULL, 0);
        }
        {
            // Restore the window styles
            DWORD style = (DWORD)GetWindowLongPtr(Window, GWL_STYLE);
			DWORD exstyle = (DWORD)GetWindowLongPtr(Window, GWL_EXSTYLE);
            SetWindowLongPtr(Window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
            SetWindowLongPtr(Window, GWL_EXSTYLE, exstyle & (~(WS_EX_APPWINDOW | WS_EX_TOPMOST)));
            
            MONITORINFOEX monInfo;
            memset(&monInfo, 0, sizeof(MONITORINFOEX));
            monInfo.cbSize = sizeof(MONITORINFOEX);
            GetMonitorInfo(HMonitor, &monInfo);

            // Restore the window size/position
            SetWindowPos(Window, NULL, PreFullscreen.x, PreFullscreen.y, PreFullscreen.w, PreFullscreen.h,
                            SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOOWNERZORDER | SWP_NOREPOSITION | SWP_NOZORDER);
        }
    }
	

    if (!Params.Display.MonitorName.IsEmpty())
    {
        EnumDisplayMonitors(0, 0, MonitorEnumFunc, (LPARAM)this);
    }

    if (fullscreen == Display_FakeFullscreen)
    {
        // Get WINDOWPLACEMENT before changing style to get OVERLAPPED coordinates,
        // which we will restore.
        WINDOWPLACEMENT wp;
        wp.length = sizeof(wp);
        GetWindowPlacement(Window, &wp);
        PreFullscreen.w = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
        PreFullscreen.h = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
        PreFullscreen.x = wp.rcNormalPosition.left;
        PreFullscreen.y = wp.rcNormalPosition.top;
        // Warning: SetWindowLong sends message computed based on old size (incorrect).
        // A proper work-around would be to mask that message out during window frame change in Platform.
        SetWindowLong(Window, GWL_STYLE, WS_OVERLAPPED | WS_VISIBLE | WS_CLIPSIBLINGS);
        SetWindowPos(Window, NULL, FSDesktop.x, FSDesktop.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);

        // Relocate cursor into the window to avoid losing focus on first click.
        POINT oldCursor;
        if (GetCursorPos(&oldCursor) &&
            ((oldCursor.x < FSDesktop.x) || (oldCursor.x > (FSDesktop.x + PreFullscreen.w)) ||
            (oldCursor.y < FSDesktop.y) || (oldCursor.x > (FSDesktop.y + PreFullscreen.h))))
        {
            // TBD: FullScreen window logic should really be in platform; it causes world rotation
            // in relative mouse mode.
            ::SetCursorPos(FSDesktop.x, FSDesktop.y);
        }
    }
    else if (fullscreen == Display_Fullscreen)
    {
        // Find out the name of the device this window
        // is on (this is for multi-monitor setups)
        MONITORINFOEX monInfo;
        memset(&monInfo, 0, sizeof(MONITORINFOEX));
        monInfo.cbSize = sizeof(MONITORINFOEX);
        GetMonitorInfo(HMonitor, &monInfo);

        // Save the current window position/size
        RECT rect;
        GetWindowRect(Window, &rect);
        PreFullscreen.x = rect.left;
        PreFullscreen.y = rect.top;
        PreFullscreen.w = rect.right - rect.left;
        PreFullscreen.h = rect.bottom - rect.top;

        // Save the window style and set it for fullscreen mode
        DWORD style = (DWORD)GetWindowLongPtr(Window, GWL_STYLE);
        DWORD exstyle = (DWORD)GetWindowLongPtr(Window, GWL_EXSTYLE);
        SetWindowLongPtr(Window, GWL_STYLE, style & (~WS_OVERLAPPEDWINDOW));
        SetWindowLongPtr(Window, GWL_EXSTYLE, exstyle | WS_EX_APPWINDOW | WS_EX_TOPMOST);

		ChangeDisplaySettingsEx(monInfo.szDevice, NULL, NULL, CDS_FULLSCREEN, NULL);

        // We need to call GetMonitorInfo() again becase
        // details may have changed with the resolution
        GetMonitorInfo(HMonitor, &monInfo);

		int x = monInfo.rcMonitor.left;
		int y = monInfo.rcMonitor.top;
		int w = monInfo.rcMonitor.right - monInfo.rcMonitor.left;
		int h = monInfo.rcMonitor.bottom - monInfo.rcMonitor.top;

        // Set the window's size and position so
        // that it covers the entire screen
        SetWindowPos(Window, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    Params.Fullscreen = fullscreen;
    return true;
}

}}}}

