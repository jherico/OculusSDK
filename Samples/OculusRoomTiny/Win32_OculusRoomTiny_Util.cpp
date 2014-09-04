/************************************************************************************

Filename    :   Win32_OculusRoomTiny_Util.cpp
Content     :   Win32 system interface & app/graphics initialization ligic  
Created     :   October 4, 2012

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

#include "RenderTiny_D3D11_Device.h"
#include "OVR_CAPI.h"
#include "../CommonSrc/Platform/Win32_Gamepad.h"

// Win32 System Variables
HWND                hWnd = NULL;
HINSTANCE           hInstance;    
POINT               WindowCenter; 

// User inputs
bool                Quit = 0;
uint8_t             MoveForward = 0,
                    MoveBack = 0,
                    MoveLeft = 0,
                    MoveRight = 0;

bool                ShiftDown = false,
                    ControlDown = false;

// Freezes the scene during timewarp rendering.
bool                FreezeEyeRender = false;   

float               AdditionalYawFromMouse = 0;

// Movement speed, in m/s applied during keyboard motion.
const float         MoveSpeed   = 3.0f;

extern ovrHmd       HMD;

// Functions from Win32_OculusRoomTiny.cpp
int     Init();
void    ProcessAndRender();
void    Release();

// Gamepad Variables
OvrPlatform::Win32::GamepadManager GamepadManager;
OvrPlatform::GamepadState LastGamepadState = OvrPlatform::GamepadState();

//-------------------------------------------------------------------------------------

void OnKey(unsigned vk, bool down)
{
    if (down && HMD)
        ovrHmd_DismissHSWDisplay(HMD);

    switch (vk)
    {
    case 'Q':       if (down && ControlDown) Quit = true;                         break;
    case VK_ESCAPE: if (!down)               Quit = true;                         break;

    case 'R':       if (down && HMD) ovrHmd_RecenterPose(HMD);                    break;

    case 'W':       MoveForward = down ? (MoveForward | 1) : (MoveForward & ~1);  break;
    case 'S':       MoveBack    = down ? (MoveBack    | 1) : (MoveBack    & ~1);  break;
    case 'A':       MoveLeft    = down ? (MoveLeft    | 1) : (MoveLeft    & ~1);  break;
    case 'D':       MoveRight   = down ? (MoveRight   | 1) : (MoveRight   & ~1);  break;

    case VK_UP:     MoveForward = down ? (MoveForward | 2) : (MoveForward & ~2);  break;
    case VK_DOWN:   MoveBack    = down ? (MoveBack    | 2) : (MoveBack    & ~2);  break;
    
    case 'F':       FreezeEyeRender = !down ? !FreezeEyeRender : FreezeEyeRender; break;
    
    case VK_SHIFT:  ShiftDown = down;                                             break;
    case VK_CONTROL:ControlDown = down;                                           break;
    }

}

void OnMouseMove(int x)
{
     const float                Sensitivity = 1.0f;
     AdditionalYawFromMouse -= (Sensitivity * x)/ 360.0f;
}

bool Util_RespondToControls(float & EyeYaw, Vector3f & EyePos, Quatf PoseOrientation)
{
    #if 0//Optional debug output
    char debugString[1000];
    sprintf_s(debugString,"Pos = (%0.2f, %0.2f, %0.2f)\n",EyePos.x,EyePos.y,EyePos.z);
    OutputDebugStringA(debugString);
    #endif

    //Mouse rotation
    EyeYaw += AdditionalYawFromMouse;
    AdditionalYawFromMouse = 0;

    //Get HeadYaw
    float tempHeadPitch, tempHeadRoll, HeadYaw, GamepadYaw;
    PoseOrientation.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>(&HeadYaw,&tempHeadPitch, &tempHeadRoll);

    //Move on Eye pos from controls
    Vector3f localMoveVector(0,0,0);

    if (MoveForward) localMoveVector += Vector3f(0,0,-1); 
    if (MoveBack)    localMoveVector += Vector3f(0,0,+1); 
    if (MoveRight)   localMoveVector += Vector3f(1,0,0); 
    if (MoveLeft)    localMoveVector += Vector3f(-1,0,0);

    //respond to gamepad
    OvrPlatform::GamepadState gamepadstate;
    if (GamepadManager.GetGamepadState(1, &gamepadstate))
    {
        if (gamepadstate.Buttons != 0)
            ovrHmd_DismissHSWDisplay(HMD);
        LastGamepadState = gamepadstate;
    }

    //move according to gamepad input
    localMoveVector += Vector3f(LastGamepadState.LX, 0, -LastGamepadState.LY);

    //prevents double speed using gamepad and 
    if (localMoveVector.x > 1) localMoveVector.x = 1;
    if (localMoveVector.y > 1) localMoveVector.y = 1;

    //rotate according to gamepad input
    const float gamepad_sensitivity = .025;
    GamepadYaw = -gamepad_sensitivity * LastGamepadState.RX;

    EyeYaw += GamepadYaw;

    Matrix4f yawRotate = Matrix4f::RotationY(EyeYaw + HeadYaw);

    Vector3f    orientationVector = yawRotate.Transform(localMoveVector);

    const float deltaTime = 1.0f/60.0f;
    orientationVector *= MoveSpeed * deltaTime * (ShiftDown ? 3.0f : 1.0f);
    EyePos += orientationVector;

    //Some rudimentary limitation of movement, so not to go through walls
    const float minDistanceToWall = 0.30f;
    EyePos.x = max(EyePos.x,-10.0f + minDistanceToWall);
    EyePos.x = min(EyePos.x, 10.0f - minDistanceToWall);
    EyePos.z = max(EyePos.z,-20.0f + minDistanceToWall);

    //Return if need to freeze or not
    return(FreezeEyeRender);
}


LRESULT CALLBACK systemWindowProc(HWND arg_hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case(WM_NCCREATE):  hWnd = arg_hwnd; break;

        case WM_MOUSEMOVE:	{
                            // Convert mouse motion to be relative
                            // (report the offset and re-center).
                            POINT newPos = { LOWORD(lp), HIWORD(lp) };
                            ::ClientToScreen(hWnd, &newPos);
                            if ((newPos.x == WindowCenter.x) && (newPos.y == WindowCenter.y))
                                break;
                            ::SetCursorPos(WindowCenter.x, WindowCenter.y);
                            OnMouseMove(newPos.x - WindowCenter.x);
                            break;
                            }

        case WM_MOVE:		RECT r;
                            GetClientRect(hWnd, &r);
                            WindowCenter.x = r.right/2;
                            WindowCenter.y = r.bottom/2;
                            ::ClientToScreen(hWnd, &WindowCenter);
                            break;

        case WM_KEYDOWN:    OnKey((unsigned)wp, true);    break;
        case WM_KEYUP:      OnKey((unsigned)wp, false);   break;
        case WM_CREATE:     SetTimer(hWnd, 0, 100, NULL); break;
        case WM_TIMER:      KillTimer(hWnd, 0);

        case WM_SETFOCUS:   
                            SetCursorPos(WindowCenter.x, WindowCenter.y);
                            SetCapture(hWnd);
                            ShowCursor(FALSE);
                            break;
       case WM_KILLFOCUS:
                            ReleaseCapture();
                            ShowCursor(TRUE);
                            break;

        case WM_QUIT:
        case WM_CLOSE:      Quit = true;
                            return 0;
    }

    return DefWindowProc(hWnd, msg, wp, lp);
}


HWND Util_InitWindowAndGraphics(Recti vp, int fullscreen, int multiSampleCount, bool UseAppWindowFrame, RenderDevice ** returnedDevice)
{
    RendererParams  renderParams;

    // Window
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpszClassName = L"OVRAppWindow";
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = systemWindowProc;
    wc.cbWndExtra    = NULL;
    RegisterClass(&wc);

    DWORD wsStyle = WS_POPUP;
    DWORD sizeDivisor = 1;
   
    if (UseAppWindowFrame)
    {
        // If using our driver, displaya window frame with a smaller window.
        // Original HMD resolution is still passed into the renderer for proper swap chain.
        wsStyle |= WS_OVERLAPPEDWINDOW;
        renderParams.Resolution = vp.GetSize();
        sizeDivisor = 2;
    }

    RECT winSize = { 0, 0, vp.w / sizeDivisor, vp.h / sizeDivisor};
    AdjustWindowRect(&winSize, wsStyle, false);
    hWnd = CreateWindowA("OVRAppWindow", "OculusRoomTiny",
                         wsStyle |WS_VISIBLE,
                         vp.x, vp.y,
                         winSize.right-winSize.left, winSize.bottom-winSize.top,
                         NULL, NULL, hInstance, NULL);

    POINT center = { vp.w / 2 / sizeDivisor, vp.h / 2 / sizeDivisor};
    ::ClientToScreen(hWnd, &center);
    WindowCenter = center;

    if (!hWnd) return(NULL);

    // Graphics
    renderParams.Multisample = multiSampleCount; 
    renderParams.Fullscreen  = fullscreen;

    *returnedDevice = RenderDevice::CreateDevice(renderParams, (void*)hWnd);

    return(hWnd);
}


void Util_ReleaseWindowAndGraphics(RenderDevice * prender)
{    
    if (prender)
        prender->Release();

    if (hWnd)
    {
        // Release window resources.
        ::DestroyWindow(hWnd);
        UnregisterClass(L"OVRAppWindow", hInstance);
    }
}


//-------------------------------------------------------------------------------------
// ***** Program Startup
// 
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR , int)
{
    hInstance = hinst;

    if (!Init())
    {
        // Processes messages and calls OnIdle() to do rendering.
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
                 ProcessAndRender();

                // Keep sleeping when we're minimized.
                if (IsIconic(hWnd)) Sleep(10);
            }
        }
    }
    Release();
    OVR_ASSERT(!_CrtDumpMemoryLeaks());
    return (0);
}

