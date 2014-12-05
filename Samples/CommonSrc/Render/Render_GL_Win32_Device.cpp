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
    
RenderDevice::RenderDevice(const Render::RendererParams& p, HWND win, HGLRC gl)
    : GL::RenderDevice(p)
    , Window(win)
    , WglContext(gl)
	, PreFullscreen(0, 0, 0, 0)
	, FSDesktop(0, 0, 0, 0)
    , HMonitor(0)
{
    OVR_UNUSED(p);
}


// Implement static initializer function to create this class.
Render::RenderDevice* RenderDevice::CreateDevice(const RendererParams& rp, void* oswnd)
{
    HWND hwnd = (HWND)oswnd;
	HDC dc = GetDC(hwnd);
    
    // Under OpenGL we call DwmEnableComposition(DWM_EC_DISABLECOMPOSITION).
    // Why do we need to disable composition for OpenGL rendering?
    // Response 1: "Enabling DWM in extended mode causes 60Hz judder on NVIDIA cards unless the Rift is the main display. "
    // Response 2: "Maybe the confusion is that GL goes through the same compositional pipeline as DX. To the kernel and DWM there is no difference between the two."
    // Response 3: "The judder does not occur with DX. My understanding is that for DWM, GL actually requires an additional blt whose timing is dependent on the main monitor."
    bool dwmCompositionEnabled = false;

    #if defined(OVR_BUILD_DEBUG) && !defined(OVR_OS_CONSOLE) // Console platforms have no getenv function.
        OVR_DISABLE_MSVC_WARNING(4996) // "This function or variable may be unsafe..."
        const char* value = getenv("Oculus_LibOVR_DwmCompositionEnabled"); // Composition is temporarily configurable via an environment variable for the purposes of testing.
        if(value && (strcmp(value, "1") == 0))
            dwmCompositionEnabled = true;
        OVR_RESTORE_MSVC_WARNING()
    #endif

    if (!dwmCompositionEnabled)
    {
        // To consider: Make a generic helper macro/class for managing the loading of libraries and functions.
        typedef HRESULT (WINAPI *PFNDWMENABLECOMPOSITIONPROC) (UINT);
        PFNDWMENABLECOMPOSITIONPROC DwmEnableComposition;

	    HINSTANCE HInstDwmapi = LoadLibraryW( L"dwmapi.dll" );
	    OVR_ASSERT(HInstDwmapi);

        if (HInstDwmapi)
        {
            DwmEnableComposition = (PFNDWMENABLECOMPOSITIONPROC)GetProcAddress( HInstDwmapi, "DwmEnableComposition" );
            OVR_ASSERT(DwmEnableComposition);

            if (DwmEnableComposition)
            {
                DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
            }

            FreeLibrary(HInstDwmapi);
	        HInstDwmapi = NULL;
        }
    }

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

    return new RenderDevice(rp, hwnd, context);
}

ovrRenderAPIConfig RenderDevice::Get_ovrRenderAPIConfig() const
{
	static ovrGLConfig cfg;
	cfg.OGL.Header.API              = ovrRenderAPI_OpenGL;
	cfg.OGL.Header.BackBufferSize   = Sizei(WindowWidth, WindowHeight);
	cfg.OGL.Header.Multisample      = Params.Multisample;
	cfg.OGL.Window                  = Window;

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

