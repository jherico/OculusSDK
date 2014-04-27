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
#include <GL/wglew.h>
#include <dwmapi.h>

namespace OVR { namespace Render { namespace GL { namespace Win32 {

typedef HRESULT (__stdcall *PFNDWMENABLECOMPOSITIONPROC) (UINT);

#pragma warning(disable : 4995)
PFNDWMENABLECOMPOSITIONPROC DwmEnableComposition;


// ***** GL::Win32::RenderDevice
    
RenderDevice::RenderDevice(const Render::RendererParams& p, HWND win, HDC dc, HGLRC gl)
    : GL::RenderDevice(p)
    , Window(win)
    , WglContext(gl)
    , GdiDc(dc)
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
    
    if (!DwmEnableComposition)
    {
        HINSTANCE hInst = LoadLibrary( L"dwmapi.dll" );
        OVR_ASSERT(hInst);
        DwmEnableComposition = (PFNDWMENABLECOMPOSITIONPROC)GetProcAddress( hInst, "DwmEnableComposition" );
        OVR_ASSERT(DwmEnableComposition);
    }

    DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);

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

    InitGLExtensions();

    return new RenderDevice(rp, hwnd, dc, context);
}

ovrRenderAPIConfig RenderDevice::Get_ovrRenderAPIConfig() const
{
    static ovrGLConfig cfg;
    cfg.OGL.Header.API         = ovrRenderAPI_OpenGL;
    cfg.OGL.Header.RTSize      = Sizei(WindowWidth, WindowHeight);
    cfg.OGL.Header.Multisample = Params.Multisample;

    return cfg.Config;
}

void RenderDevice::Present(bool useVsync)
{
    BOOL success;
    int swapInterval = (useVsync) ? 1 : 0;
    if (wglGetSwapIntervalEXT() != swapInterval)
        wglSwapIntervalEXT(swapInterval);

    success = SwapBuffers(GdiDc);
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
        ReleaseDC(Window, GdiDc);
        WglContext = NULL;
        GdiDc = NULL;
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

    MONITORINFOEX monitor;
    monitor.cbSize = sizeof(monitor);

    if (::GetMonitorInfo(hMonitor, &monitor) && monitor.szDevice[0])
    {
        DISPLAY_DEVICE dispDev;
        memset(&dispDev, 0, sizeof(dispDev));
        dispDev.cb = sizeof(dispDev);

        if (::EnumDisplayDevices(monitor.szDevice, 0, &dispDev, 0))
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

        // Find the requested device mode
        DEVMODE dmode;
        bool foundMode = false;
        memset(&dmode, 0, sizeof(DEVMODE));
        dmode.dmSize = sizeof(DEVMODE);
        Recti vp = VP;
        for(int i=0 ; EnumDisplaySettings(monInfo.szDevice, i, &dmode); ++i)
        {
            foundMode = (dmode.dmPelsWidth==(DWORD)vp.w) &&
                        (dmode.dmPelsHeight==(DWORD)vp.h) &&
                        (dmode.dmBitsPerPel==(DWORD)32);
            if (foundMode)
                break;
        }
        if(!foundMode)
            return false;

        dmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

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

        // Attempt to change the resolution
        LONG ret = ChangeDisplaySettingsEx(monInfo.szDevice, &dmode, NULL, CDS_FULLSCREEN, NULL);
        //LONG ret = ChangeDisplaySettings(&dmode, CDS_FULLSCREEN);

        // If it failed, clean up and return.
        if (ret != DISP_CHANGE_SUCCESSFUL)
        {
            SetWindowLongPtr(Window, GWL_STYLE, style);
            SetWindowLongPtr(Window, GWL_EXSTYLE, exstyle);
            return false;
        }

        // We need to call GetMonitorInfo() again becase
        // details may have changed with the resolution
        GetMonitorInfo(HMonitor, &monInfo);

        // Set the window's size and position so
        // that it covers the entire screen
        SetWindowPos(Window, HWND_TOPMOST, monInfo.rcMonitor.left, monInfo.rcMonitor.top, vp.w, vp.h,
                        SWP_SHOWWINDOW | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    Params.Fullscreen = fullscreen;
    return true;
}

}}}}

