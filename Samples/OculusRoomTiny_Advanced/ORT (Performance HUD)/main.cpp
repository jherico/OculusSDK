/************************************************************************************
Filename    :   Win32_RoomTiny_Main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   18th Dec 2014
Authors     :   Tom Heath
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
/// This sample shows the built in performance data 
/// available from the SDK.
/// Press '1' to see performance summary.
/// Press '2' to see latency timing.
/// Press '3' to see application render timing.
/// Press '4' to see compositor render timing.
/// Press '5' to see version info.

#include "OVR_Version.h"

/// Press '0' to dismiss again.

/// Oculus Performance (aka Performance Summary) :
///     
///     App Motion-to-Photon Latency
///         Latency from when the last predictied tracking info is queried by the application using ovr_GetTrackingState()
///         to the point when the middle scanline of the target frame is illuminated on the HMD display.
///         This is the same info presented in "Application Latency Timing" section, presented here as part of the performance summary.
///
///     Performance Headroom
///         The percentage of available PC performance that has not been utilized by the client application and compositor.
///         This is essentially the application CPU & GPU time tracked in the "Application Render Timing" pane section divided by the
///         native frame time (inverse of refresh rate) of the HMD. It is meant to be a simple guide for the user to verify that their
///         PC has enough CPU & GPU performance buffer to avoid dropping frames and leading to unwanted judder. It is important to
///         note that as GPU utilization is pushed closer to 100%, the asynchoronous nature of the compositor will cause
///         the "Application GPU" times to start accounting for that GPU time as the application's GPU work is preempted
///         by the compositor's GPU work.
///
///     Application Frames Dropped
///         This is the same value provided in the "Application Render Timing" pane called "App Missed Submit Count".
///
///     Compositor Frames Dropped
///         This is the same value provided in the "Compositor Render Timing" pane called "Compositor Missed V-Sync Count".
///
///     Left-side graph:    Plots frame rate of the application
///     Right-side graph:   Plots the "Performance headroom %" provided in the same section
///
/// Latency Timing Pane :
///
///     App Tracking to Mid-Photon
///         Latency from when the app called ovr_GetTrackingState() to the point in time when
///         the middle scanline of the target frame is illuminated on the HMD display
///        
///     Timewarp to Mid-Photon
///         Latency from when the last predictied tracking info is queried on the CPU for timewarp execution
///         to the point in time when the middle scanline of the target frame is illuminated on the HMD display
///        
///     Flip to Photon-Start
///         Time difference from the point the back buffer is presented to the HMD to the point the target frame's
///         first scanline is illuminated on the HMD display
///        
///     Left-side graph:    Plots "App to Mid-Photon"
///     Right-side graph:   Plots "Timewarp to Mid-Photon" time
///     
/// Application Render Timing Pane :
///
///     App Missed Submit Count
///         Increments each time the application fails to submit a new set of layers using ovr_SubmitFrame() before the
///         compositor is executed before each V-Sync (Vertical Synchronization).
///        
///     App Frame-rate
///         The rate at which application rendering is able to call ovr_SubmitFrame().
///         It will never go above the native refresh rate of the HMD as the call to ovr_SubmitFrame() will throttle
///         the application's CPU execution as necessary.
///        
///     App Render GPU Time
///         The total GPU time spent on rendering by the client application. This includes the work done by the application
///         after returning from ovr_SubmitFrame() using the mirror texture if applicable. It also includes GPU command-buffer
///         "bubbles" that might be prevalent due to the application's CPU thread not pushing data to the GPU fast enough
///         to keep it occupied. Similarly, if the app pushes the GPU close to full-utilization, then there is a
///         good chance that the app's GPU work in-flight for frame (N+1) will be preempted by the compositor's render work
///         for frame (N). Since the app-GPU timing query operates like a "wall clock timer", this will lead to artificially
///         inflated app-GPU times being reported as they will start to include the compositor-GPU-usage times.
///        
///     App Render CPU Time
///         The time difference from when the application continued execution on CPU after ovr_SubmitFrame() returned
///         subsequent call to ovr_SubmitFrame(). This will show "N/A" if the latency tester is not functioning
///         as expected (e.g. HMD display is sleeping due to prolonged inactivity). This includes IPC call overhead
///         to compositor after ovr_SubmitFrame() is called by client application.
///
///     App Queue Ahead Time
///         The amount of adaptive-queue-ahead the application is granted. Queue Ahead is the amount of CPU time the
///         application is given which will change over time based on the application's work load. The value indicates
///         the point in time when the application's CPU thread is yielded in relation to the previous frame's V-Sync.
///
///     Left-side graph:    Plots "App Frame-rate"
///     Right-side graph:   Plots "App Render GPU Time"
///     
/// Compositor Render Timing Pane :
///
///     Compositor Missed V-Sync Count
///         Increments each time the compositor fails to present a new rendered frame at V-Sync (Vertical Synchronization).
///        
///     Compositor Frame-rate
///         The rate at which final composition is happening. This is independent of the client application rendering rate.
///         Since compositor is always locked to V-Sync, this value will never go above the native HMD refresh rate,
///         but if the compositor fails to finish new frames on time, it can go below HMD the native refresh rate.
///        
///     Compositor GPU Time
///         The amount of time the GPU spends executing the compositor renderer.
///         This includes timewarp and distortion of all the layers submitted by the application. The amount of
///         active layers, their resolution and the requested sampling quality can all affect the GPU times.
///
///     Comp Gpu-End to Present
///         The amount of time between when the GPU completes the compositor rendering to the point in time when
///         that buffer is latched in the swap chain to be scanned out on the HMD.
///
///     Left-side graph:    Plots "Compositor GPU Time"
///     Right-side graph:   Plots "Comp Gpu-End to Present"
///
///Version Info :
///     
///     OVR SDK Runtime Ver
///         Version of the runtime currently installed on the PC. Every VR application that uses the OVR SDK
///         since 0.5.0 will be using this installed runtime.
///
///     OVR SDK Client DLL Ver
///         The SDK version the client app was compiled against.


#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct PerformanceHUD : BasicVR
{
    PerformanceHUD(HINSTANCE hinst) : BasicVR(hinst, L"Performance HUD") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
            // Handle Perf HUD Toggle
            if (DIRECTX.Key['0']) ovr_SetInt(Session, OVR_PERF_HUD_MODE, (int)ovrPerfHud_Off);
            if (DIRECTX.Key['1']) ovr_SetInt(Session, OVR_PERF_HUD_MODE, (int)ovrPerfHud_PerfSummary);
            if (DIRECTX.Key['2']) ovr_SetInt(Session, OVR_PERF_HUD_MODE, (int)ovrPerfHud_LatencyTiming);
            if (DIRECTX.Key['3']) ovr_SetInt(Session, OVR_PERF_HUD_MODE, (int)ovrPerfHud_AppRenderTiming);
            if (DIRECTX.Key['4']) ovr_SetInt(Session, OVR_PERF_HUD_MODE, (int)ovrPerfHud_CompRenderTiming);
            if (DIRECTX.Key['5']) ovr_SetInt(Session, OVR_PERF_HUD_MODE, (int)ovrPerfHud_VersionInfo);

		    ActionFromInput();
		    Layer[0]->GetEyePoses();

		    for (int eye = 0; eye < 2; ++eye)
		    {
			    Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
		    }

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	PerformanceHUD app(hinst);
    return app.Run();
}
