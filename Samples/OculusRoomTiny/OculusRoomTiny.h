/************************************************************************************

 Filename    :   OSX_OculusRoomTiny.h
 Content     :   Simplest possible first-person view test application for Oculus Rift
 Created     :   May 7, 2013
 Authors     :   Michael Antonov, Andrew Reisse, Artem Bolgar

 Copyright   :   Copyright 2013 Oculus, Inc. All Rights reserved.

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
#pragma once

#include "OVR.h"

#if defined(OVR_OS_WIN32)
typedef HINSTANCE OVR_TINY_STARTUP;
typedef HWND OVR_TINY_WINDOW;
#elif defined(OVR_OS_MAC)
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreGraphics/CGDirectDisplay.h>
typedef OVRApp* OVR_TINY_STARTUP
#elif defined(OVR_OS_LINUX)
//#include <EGL/egl.h>
#include <X11/Xlib.h>
typedef void * OVR_TINY_STARTUP;
typedef Window OVR_TINY_WINDOW;
#else 
#error "Unknown platform"
#endif


//#include "Util/Util_Render_Stereo.h"
#include <Kernel/OVR_Timer.h>
#include "RenderTiny_GL_Device.h"

using namespace OVR;
using namespace OVR::Util::Render;
using namespace OVR::RenderTiny;

//class OculusRoomTinyApp;
//
//@interface OVRApp : NSApplication
//
//@property (assign) IBOutlet NSWindow* win;
//@property (assign) OculusRoomTinyApp* App;
//
//-(void) run;
//
//@end
//
//@interface OVRView : NSOpenGLView <NSWindowDelegate>
//
////@property (assign) OVR::Platform::OSX::PlatformCore* Platform;
//@property (assign) OculusRoomTinyApp* App;
//@property unsigned long Modifiers;
//
//-(void)ProcessMouse:(NSEvent*)event;
//-(void)warpMouseToCenter;
//
//+(CGDirectDisplayID) displayFromScreen:(NSScreen*)s;
//
//@end
//
////-------------------------------------------------------------------------------------
//// ***** OculusRoomTiny Description
//
//// This app renders a simple flat-shaded room allowing the user to move along the
//// floor and look around with an HMD, mouse, keyboard and gamepad.
//// By default, the application will start full-screen on Oculus Rift.
////
//// The following keys work:
////
////  'W', 'S', 'A', 'D' - Move forward, back; strafe left/right.
////  F1 - No stereo, no distortion.
////  F2 - Stereo, no distortion.
////  F3 - Stereo and distortion.
////
//
// The world RHS coordinate system is defines as follows (as seen in perspective view):
//  Y - Up
//  Z - Back
//  X - Right
const Vector3f UpVector(0.0f, 1.0f, 0.0f);
const Vector3f ForwardVector(0.0f, 0.0f, -1.0f);
const Vector3f RightVector(1.0f, 0.0f, 0.0f);

// We start out looking in the positive Z (180 degree rotation).
const float    YawInitial  = 3.141592f;
const float    Sensitivity = 1.0f;
const float    MoveSpeed   = 3.0f; // m/s
//
//namespace OSX
//{
//    class RenderDevice : public GL::RenderDevice
//    {
//    public:
//        void* Context; // NSOpenGLContext
//
//        // osview = NSView*
//        RenderDevice(const RendererParams& p, void* osview, void* context);
//
//        virtual void Shutdown();
//        virtual void Present();
//
//        virtual bool SetFullscreen(DisplayMode fullscreen);
//
//        // osview = NSView*
//        static RenderDevice* CreateDevice(const RendererParams& rp, void* osview);
//    };
//}
//
////-------------------------------------------------------------------------------------
//// ***** OculusRoomTiny Application class
//
//// An instance of this class is created on application startup (main/WinMain).
////
//// It then works as follows:
////
////  OnStartup   - Window, graphics and HMD setup is done here.
////                This function will initialize OVR::DeviceManager and HMD,
////                creating SensorDevice and attaching it to SensorFusion.
////                This needs to be done before obtaining sensor data.
////
////  OnIdle      - Does per-frame processing, processing SensorFusion and
////                movement input and rendering the frame.

class OculusRoomTinyApp : public MessageHandler
{
public:
    OculusRoomTinyApp(OVR_TINY_STARTUP startup);
    ~OculusRoomTinyApp();

    // Initializes graphics, Rift input and creates world model.
    virtual int  OnStartup(const char* args);
    // Called per frame to sample SensorFucion and render the world.
    virtual void OnIdle();

    // Installed for Oculus device messages. Optional.
    virtual void OnMessage(const Message& msg);

    // Handle input events for movement.
    virtual void OnGamepad(float padLx, float padLY, float padRx, float padRy);
    virtual void OnMouseMove(int x, int y, int modifiers);
    virtual void OnKey(unsigned vk, bool down);

    // Render the view for one eye.
    void         Render(const StereoEyeParams& stereo);

    // Main application loop.
    int          Run();
    void         Exit();

    // Return amount of time passed since application started in seconds.
    double       GetAppTime() const
    {
        return (OVR::Timer::GetTicks() - StartupTicks) * (1.0 / (double)OVR::Timer::MksPerSecond);
    }

    bool        IsQuiting() const { return Quit; }
    int         GetWidth() const { return Width; }
    int         GetHeight() const { return Height; }
    bool        SetFullscreen(const RendererParams& rp, int fullscreen);

protected:
    bool        setupWindow();
    void        destroyWindow();

#ifdef OVR_OS_WIN32
    LRESULT     windowProc(UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK systemWindowProc(HWND window, UINT msg, WPARAM wp, LPARAM lp);
#elif defined(OVR_OS_MAC)
#elif defined(OVR_OS_LINUX)
    bool        processXEvents();
#endif
//    // Win32 window setup interface.
//    // Win32 static function that delegates to WindowProc member function.
//    void        giveUsFocus(bool setFocus);
//    // *** Win32 System Variables
//    HWND                hWnd;
//    HINSTANCE           hInstance;
//    POINT               WindowCenter; // In desktop coordinates
//
//    // Dynamically ink to XInput to simplify projects.
//    typedef DWORD (WINAPI *PFn_XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
//
//    bool                MouseCaptured;
//    PFn_XInputGetState  pXInputGetState;
//    HMODULE             hXInputModule;
//    UInt32              LastPadPacketNo;


//    NSView*         View;
//    NSWindow*       Win;
//    OVRApp*         NsApp;

    static OculusRoomTinyApp*   pApp;

	OVR_TINY_STARTUP hInstance;
    OVR_TINY_WINDOW hWnd;

    // *** Rendering Variables
    Ptr<RenderTiny::RenderDevice>   pRender;
    RendererParams      RenderParams;
    int                 Width, Height;

    bool                Quit;

    // *** Oculus HMD Variables
    Ptr<DeviceManager>  pManager;
    Ptr<SensorDevice>   pSensor;
    Ptr<HMDDevice>      pHMD;
    SensorFusion        SFusion;
    OVR::HMDInfo        HMDInfo;

    // Last update seconds, used for move speed timing.
    double              LastUpdate;
    OVR::UInt64         StartupTicks;

    // Position and look. The following apply:
    Vector3f            EyePos;
    // Rotation around Y, CCW positive when looking at RHS (X,Z) plane.
    float               EyeYaw;
    // Pitch. If sensor is plugged in, only read from sensor.
    float               EyePitch;
    // Roll, only accessible from Sensor.
    float               EyeRoll;
    // Stores previous Yaw value from to support computing delta.
    float               LastSensorYaw;

    // Movement state; different bits may be set based on the state of keys.
    UByte               MoveForward;
    UByte               MoveBack;
    UByte               MoveLeft;
    UByte               MoveRight;
    Vector3f            GamepadMove, GamepadRotate;

    Matrix4f            View;
    RenderTiny::Scene   Scene;

    // Stereo view parameters.
    StereoConfig        SConfig;
    PostProcessType     PostProcess;

    // Shift accelerates movement/adjustment velocity.
    bool                ShiftDown;
    bool                ControlDown;
};

// Adds sample models and lights to the argument scene.
void PopulateRoomScene(Scene* scene, RenderDevice* render);

