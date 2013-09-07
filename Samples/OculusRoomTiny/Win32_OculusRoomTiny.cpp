/************************************************************************************

Filename    :   Win32_OculusRoomTiny.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   October 4, 2012
Authors     :   Michael Antonov, Andrew Reisse

Copyright   :   Copyright 2012 Oculus, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#include "OculusRoomTiny.h"
#include <Xinput.h>
//-------------------------------------------------------------------------------------
// ***** Win32-Specific Logic

bool OculusRoomTinyApp::setupWindow()
{

    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpszClassName = L"OVRAppWindow";
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = systemWindowProc;
    wc.cbWndExtra    = sizeof(OculusRoomTinyApp*);
    RegisterClass(&wc);


    RECT winSize = { 0, 0, Width, Height };
    AdjustWindowRect(&winSize, WS_POPUP, false);
    hWnd = CreateWindowA("OVRAppWindow", "OculusRoomTiny", WS_POPUP|WS_VISIBLE,
                         HMDInfo.DesktopX, HMDInfo.DesktopY,
                         winSize.right-winSize.left, winSize.bottom-winSize.top,
                         NULL, NULL, hInstance, (LPVOID)this);


    // Initialize Window center in screen coordinates
    POINT center = { Width / 2, Height / 2 };
    ::ClientToScreen(hWnd, &center);
//    WindowCenter = center;


    return (hWnd != NULL);
}

void OculusRoomTinyApp::destroyWindow()
{
    pRender.Clear();

    if (hWnd)
    {
        // Release window resources.
        ::DestroyWindow(hWnd);
        UnregisterClass(L"OVRAppWindow", hInstance);
        hWnd = 0;
        Width = Height = 0;
    }
}


LRESULT CALLBACK OculusRoomTinyApp::systemWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCCREATE)
        pApp->hWnd = hwnd;
    return pApp->windowProc(msg, wp, lp);
}

/*
void OculusRoomTinyApp::giveUsFocus(bool setFocus)
{
	if (setFocus)
    {
        ::SetCursorPos(WindowCenter.x, WindowCenter.y);

        MouseCaptured = true;
        ::SetCapture(hWnd);
        ::ShowCursor(FALSE);

    }
    else
    {
        MouseCaptured = false;
        ::ReleaseCapture();
        ::ShowCursor(TRUE);
    }
}
*/

LRESULT OculusRoomTinyApp::windowProc(UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_MOUSEMOVE:
        {
            //if (MouseCaptured)
            //{
            //    // Convert mouse motion to be relative (report the offset and re-center).
            //    POINT newPos = { LOWORD(lp), HIWORD(lp) };
            //    ::ClientToScreen(hWnd, &newPos);
            //    if ((newPos.x == WindowCenter.x) && (newPos.y == WindowCenter.y))
            //        break;
            //    ::SetCursorPos(WindowCenter.x, WindowCenter.y);

            //    LONG dx = newPos.x - WindowCenter.x;
            //    LONG dy = newPos.y - WindowCenter.y;
            //    pApp->OnMouseMove(dx, dy, 0);
            //}
        }
        break;

    case WM_MOVE:
        {
            //RECT r;
            //GetClientRect(hWnd, &r);
            //WindowCenter.x = r.right/2;
            //WindowCenter.y = r.bottom/2;
            //::ClientToScreen(hWnd, &WindowCenter);
        }
        break;

    case WM_KEYDOWN:
        OnKey((unsigned)wp, true);
        break;
    case WM_KEYUP:
        OnKey((unsigned)wp, false);
        break;

    //case WM_SETFOCUS:
    //    giveUsFocus(true);
    //    break;

    //case WM_KILLFOCUS:
    //    giveUsFocus(false);
    //    break;

    //case WM_CREATE:
    //    // Hack to position mouse in fullscreen window shortly after startup.
    //    SetTimer(hWnd, 0, 100, NULL);
    //    break;

    //case WM_TIMER:
    //    KillTimer(hWnd, 0);
    //    giveUsFocus(true);
    //    break;

    case WM_QUIT:
    case WM_CLOSE:
        Quit = true;
        return 0;
    }

    return DefWindowProc(hWnd, msg, wp, lp);
}

static inline float GamepadStick(short in)
{
    float v;
    if (abs(in) < 9000)
        return 0;
    else if (in > 9000)
        v = (float) in - 9000;
    else
        v = (float) in + 9000;
    return v / (32767 - 9000);
}

static inline float GamepadTrigger(BYTE in)
{
    return (in < 30) ? 0.0f : (float(in-30) / 225);
}


int OculusRoomTinyApp::Run()
{
    // Loop processing messages until Quit flag is set,
    // rendering game scene inside of OnIdle().

    while (!Quit)
    {
        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            //// Read game-pad.
            //XINPUT_STATE xis;

            //if (pXInputGetState && !pXInputGetState(0, &xis) &&
            //    (xis.dwPacketNumber != LastPadPacketNo))
            //{
            //    OnGamepad(GamepadStick(xis.Gamepad.sThumbLX),
            //              GamepadStick(xis.Gamepad.sThumbLY),
            //              GamepadStick(xis.Gamepad.sThumbRX),
            //              GamepadStick(xis.Gamepad.sThumbRY));
            //    //pad.LT = GamepadTrigger(xis.Gamepad.bLeftTrigger);
            //    LastPadPacketNo = xis.dwPacketNumber;
            //}

            pApp->OnIdle();

            // Keep sleeping when we're minimized.
            if (IsIconic(hWnd))
                Sleep(10);
        }
    }

    return 0;
}


//-------------------------------------------------------------------------------------
// ***** Program Startup

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR inArgs, int)
{
    int exitCode = 0;

    // Initializes LibOVR. This LogMask_All enables maximum logging.
    // Custom allocator can also be specified here.
    OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));

    // Scope to force application destructor before System::Destroy.
    {
        OculusRoomTinyApp app(hinst);
        //app.hInstance = hinst;

        exitCode = app.OnStartup(inArgs);
        if (!exitCode)
        {
            // Processes messages and calls OnIdle() to do rendering.
            exitCode = app.Run();
        }
    }

    // No OVR functions involving memory are allowed after this.
    OVR::System::Destroy();

    OVR_DEBUG_STATEMENT(_CrtDumpMemoryLeaks());
    return exitCode;
}
