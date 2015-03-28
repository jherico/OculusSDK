/************************************************************************************

Filename    :   CAPI_DistortionTiming.h
Content     :   Implements timing for the distortion renderer
Created     :   Dec 16, 2014
Authors     :   Chris Taylor

Copyright   :   Copyright 2014 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.2 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_CAPI_DistortionTiming_h
#define OVR_CAPI_DistortionTiming_h

#include "CAPI_HMDRenderState.h"
#include "CAPI_FrameLatencyTracker.h"
#include "CAPI_FrameTimeManager3.h"
#include "CAPI_HMDRenderState.h"
#include <Kernel/OVR_Lockless.h>
#include <OVR_CAPI.h> // ovrFrameTiming

/*
    -----------------------------
    Distortion Timing Terminology
    -----------------------------

    To fix on one set of terminology, a frame life-cycle is defined the following:

    (1) Get prediction time for app left/right eye rendering.
    (2) App renders left/right eyes.
    (3) Get prediction time for timewarp.
    (4) SDK renders distortion/timewarp/chroma, perhaps measuring the time it takes.
    (5) SDK presents frame and waits for end of frame to occur.
    (6) End of frame occurs at Vsync.
    (7) App goes back to step 1 and starts rendering the next frame.
    (8) Scanout starts some time later for frame from step 6.
    (9) Display panel emits photons some time later for scanout from step 8.

    "Frame interval" is the time interval between Vsyncs, whether or not Vsync is on.
    "Frame end" time means the time at which the scanout starts at scanline 0.
    "Visible midpoint" is when the middle scanline is half-visible to the user's eye.

    "Start of scanout" is when the hardware begins scanout.  The pixels may not be fully
    illuminated at this point.  A hardware-specific rise time on the order of a millisecond
    or two must be added to get photons time.

    All timing is done in units of seconds.

    We approximate the scanline start-end interval with the frame interval.
*/

namespace OVR { namespace CAPI {


//-----------------------------------------------------------------------------
// AppTiming
//
// This structure provides the measurements for the current app frame.
struct AppTiming
{
    // When half of the frame image data has been visible to the eye.
    double VisibleMidpointTime;

    // When the Rift starts scanning out, not including ScreenSwitchingDelay.
    double ScanoutStartTime;

    // Time between frames.
    double FrameInterval;

    void Clear()
    {
        FrameInterval       = 0.013; // A value that should not break anything.
        ScanoutStartTime    = 0.; // Predict to current time.
        VisibleMidpointTime = 0.; // Predict to current time.
    }
};


//-----------------------------------------------------------------------------
// TimewarpTiming
//
// This structure provides the measurements for the current frame timewarp.
struct TimewarpTiming
{
    // Time at which scanout is predicted to start.
    double ScanoutTime;

    // The time when Just-In-Time timewarp should be started.
    // The app should busy/idle-wait until this time before doing timewarp.
    double JIT_TimewarpTime;

    // Left and right eye start and end render times, respectively.
    double EyeStartEndTimes[2][2];
};


//-----------------------------------------------------------------------------
// LocklessAppTimingBase
//
// Base timing info shared via lockless data structure.
// The AppDistortionTimer can use a copy of this data to derive an AppTiming
// object for a given frame index.
struct LocklessAppTimingBase
{
    // Is the data valid?
    // 0 = Not valid.
    uint32_t IsValid;

    // Frame index of the last EndFrame() call to update timing.
    uint32_t LastEndFrameIndex;

    // Frame start time targetted by the last EndFrame() call to update timing.
    double LastStartFrameTime;

    // Last known Vsync time from distortion timer.
    double LastKnownVsyncTime;

    // Vsync fuzz factor used to measure uncertainty in timing.
    double VsyncFuzzFactor;

    // Most updated measurement of the frame interval.
    double FrameInterval;

    // Scanout delay measured by the builtin latency tester.
    double ScanoutDelay;

    // Screen switching delay calculated in distortion timer.
    double ScreenSwitchingDelay;
};


//-----------------------------------------------------------------------------
// DistortionTimer
//
// This is a calculator for the app and timewarp/distortion timing.
class DistortionTimer
{
    typedef OVR::CAPI::FTM3::FrameTimeManagerCore TimeMan;

public:
    DistortionTimer();
    ~DistortionTimer();

    // Returns false if distortion timing could not be initialized.
    bool Initialize(HMDRenderState const* renderState,
                    FrameLatencyTracker const* lagTester);
    void Reset();

    //-------------------------------------------------------------------------
    // Timewarp Timing

    // Calculate timing for current frame timewarp.
    // Result can be retrieved via GetTimewarpTiming().
    void CalculateTimewarpTiming(uint32_t frameIndex, double previousKnownVsyncTime = 0.);

    // Called after CalculateTimewarpTiming() will retrieve the timewarp
    // timing for this frame.
    TimewarpTiming const* GetTimewarpTiming()
    {
        return &CurrentFrameTimewarpTiming;
    }

    // Add a distortion draw call timing measurement.
    void AddDistortionTimeMeasurement(double distortionTimeSeconds);

    // Returns true if more distortion timing measurements are needed.
    bool NeedDistortionTimeMeasurement() const
    {
        // NOTE: Even when Vsync is off this measurement is still valid and useful.
        return !DistortionRenderTimes.AtCapacity();
    }

    // Insert right after spin-wait for Present query to finish for the renderer.
    void SetLastPresentTime()
    {
        // Update vsync time.  This is the post-present time, which is expected to be
        // after the Vsync has completed and our query event put in after the Present
        // has indicated that it is signaled.  However this is not reliable.
        LastPresentTime = Timer::GetSeconds();
    }

    // Returns the time to use for the current frame for latency tester present time,
    // which is not the same as the LastPresentTime.
    double GetLatencyTesterPresentTime() const
    {
        return LatencyTesterPresentTime;
    }

    // Set/get the Timewarp IMU time, which is the time at which the IMU was sampled.
    void SetTimewarpIMUTime(double t)
    {
        LastTimewarpIMUTime = t;
    }

    double GetTimewarpIMUTime() const
    {
        return LastTimewarpIMUTime;
    }

protected:
    // Vsync/no-vsync versions split out for readability.
    AppTiming getAppTimingWithVsync();
    AppTiming getAppTimingNoVsync();

protected:
    // Last time that Present() was called for post-present latency measurement.
    // Provided by SetLastPresentTime() cooperatively with the distortion renderer.
    double LastPresentTime;

    // The time to use for the latency tester for present time, which is the reference
    // time used for calculating present-scanout delay.
    double LatencyTesterPresentTime;

    // Last known Vsync time, provided cooperatively by the distortion renderer
    // via the GetTimewarpTiming() call or internal estimation.
    double LastKnownVsyncTime;

    // Time in seconds that the Vsync measurement may be in error.
    // It is assumed to be pretty tight for D3D11 and Display Driver data but for
    // end frame -based timing we need to add some buffer to avoid misprediction.
    double LastKnownVsyncFuzzBuffer;

    // The current app frame index, initially zero.
    uint32_t AppFrameIndex;
    // Updated in getAppTiming()
    // Read in CalculateTimewarpTiming()

protected:
    // Calculator for the time it takes to render distortion.
    mutable OVR::CAPI::FTM3::MedianCalculator DistortionRenderTimes;

    // Current estimate for timewarp render time.
    double EstimatedTimewarpRenderTime;

protected:
#ifdef OVR_OS_WIN32
    ScopedFileHANDLE DeviceHandle;

    // Attempt to use the display driver for getting a previous vsync
    bool getDriverVsyncTime(double* previousKnownVsyncTime);
#endif

    // Get Vsync to next Vsync interval.
    // NOTE: Technically the Vsync-Vsync frame interval is not the same as the scanout
    // start to end interval because there is a back porch that implies some blanking time
    double getFrameInterval() const;

    // Get Frame End to Scanout delay.  Measured by DK2 Latency Tester if available.
    // This works for Vsync on or off.
    double getScanoutDelay();

protected:
    // Update the TimeManager during timewarp calculation
    void submitDisplayFrame(double frameEndTime, double frameInterval);

    // Update LastKnownVsyncTime.
    // Pass zero if no Vsync timing information is available.
    void updateLastKnownVsyncTime(double previousKnownVsyncTime = 0.);

    double getJITTimewarpTime(double frameEndTime);

protected:
    // DK2 Latency Tester object
    FrameLatencyTracker const* LatencyTester;

    // Render state parameters from HMD
    HMDRenderState const* RenderState;

    // Constant screen switching delay calculated from the shutter info.
    // This is the time it takes between pixels starting to scan out and
    // for the visible light to rise to half the expected brightness value.
    // For OLEDs on the DK2 this is about 1 millisecond.
    double ScreenSwitchingDelay;

    // Time Manager
    TimeMan TimeManager;

    // The last predicted vsync time from the previous frame.
    double LastTimewarpFrameEndTime;

    // Has the timing object already been initialized?
    bool AlreadyInitialized;

protected:
    // Updated by CalculateTimewarpTiming(). 
    TimewarpTiming CurrentFrameTimewarpTiming;

    // Time when sensor was sampled for timewarp pose.
    double LastTimewarpIMUTime;

protected:
    // Lockless data used by application for eye pose timing via the
    // provided AppDistortionTimer class.
    LocklessUpdater<LocklessAppTimingBase> LocklessAppTimingBaseUpdater;

    void ClearAppTimingUpdater()
    {
        LocklessAppTimingBase cleared = LocklessAppTimingBase();
        LocklessAppTimingBaseUpdater.SetState(cleared);
    }

public:
    LocklessUpdater<LocklessAppTimingBase>* GetUpdater()
    {
        return &LocklessAppTimingBaseUpdater;
    }
};


// TODO: This header needs to be split up.


//-----------------------------------------------------------------------------
// AppRenderTimer
//
// This is an app-side calculator for predicted render times based on frame
// indices provided by the app.
class AppRenderTimer
{
public:
    AppRenderTimer();
    ~AppRenderTimer();

    void SetUpdater(LocklessUpdater<LocklessAppTimingBase>* updater)
    {
        AppTimingBaseUpdater = updater;
    }

    bool IsValid() const
    {
        return AppTimingBaseUpdater != nullptr;
    }

    // Returns true on success.
    // Pass in 0 for frameIndex to use the next scanout time,
    // or a non-zero incrementing number for each frame to support queue ahead.
    void GetAppTimingForIndex(AppTiming& result, bool vsyncOn, uint32_t frameIndex = 0);

protected:
    LocklessUpdater<LocklessAppTimingBase>* AppTimingBaseUpdater;
};


//-----------------------------------------------------------------------------
// AppTimingHistory
//
// Keep a history of recent application render timing data, to keep a record of
// when frame indices are expected to scanout.  This is used later to compare
// with when those frames scan out to self-test the timing code.
//
// This class is not thread-safe.
class AppTimingHistory
{
public:
    AppTimingHistory();
    ~AppTimingHistory();

    void Clear();
    void SetScanoutTimeForFrame(uint32_t frameIndex, double scanoutTime);

    // Returns 0.0 if not found.
    double LookupScanoutTime(uint32_t frameIndex);

protected:
    static const int kFramesMax = 8;

    struct Record
    {
        uint32_t FrameIndex;
        double   ScanoutTime;
    };

    int    LastWriteIndex;
    Record History[kFramesMax];
};


}} // namespace OVR::CAPI

#endif // OVR_CAPI_DistortionTiming_h
