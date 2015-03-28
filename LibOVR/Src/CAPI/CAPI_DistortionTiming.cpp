/************************************************************************************

Filename    :   CAPI_DistortionTiming.cpp
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

#include "CAPI_DistortionRenderer.h"

#ifdef OVR_OS_WIN32
#include "../Displays/OVR_Win32_Dxgi_Display.h" // Display driver timing info
#endif

namespace OVR { namespace CAPI {


//-----------------------------------------------------------------------------
// Timing Constants

// Number of milliseconds to pad on top of the timewarp draw call measured time
// in order to account for random variations in execution time due to preemption.
// If this is set too low the rendering will occasionally judder.
static const double kJITPreemptBufferTime   = 0.004;    // 4 milliseconds

// When validating measured frame intervals, the following constants
// bound the acceptable measurements.
static const double kMinFrameInterval       = 0.001;    // 1 millisecond
static const double kMaxFrameInterval       = 0.020;    // 20 milliseconds

// If the last known Vsync time is older than this age limit,
// then we should not use it for extrapolating to current time.
static const double kVsyncDataAgeLimit      = 10.;      // 10 seconds

// When Vsync is off and we have no idea when the last frame started,
// assume this amount of time has elapsed since the frame started.
static const double kNoVsyncInfoFrameTime   = 0.002;    // 2 milliseconds

#ifdef OVR_OS_WIN32
// The latest driver provides a post-present vsync-to-scanout delay
// that is roughly zero.  The actual measured latency should be
// about the same as this.
static const double kExpectedDriverLatency  = 0.0002f;  // 200 microseconds
#endif

// Number from a hat for post-present latency when Vsync is off.
static const double kExpectedNoVSyncLatency = 0.003;    // 3 milliseconds

// Number of timewarp render time samples to collect
static const int kTimewarpRenderTimeSamples = 12;       // 12 samples

// Adding a fuzz time because the last known Vsync time is sometimes fuzzy and
// we don't want to predict behind a whole frame.  This is most often used in
// app rendered and D3D9 renderers and on Win/Mac/Linux with OpenGL.
static const double kFuzzyVsyncBufferTime   = kJITPreemptBufferTime;
// Currently set to the same fuzz factor used for JIT preemption because the
// same amount of error is accounted for by both constants.

// Even when the Vsync timing data source is precise we should add some kind of
// buffer in to avoid floating point rounding or unexpected sync problems.
static const double kExactVsyncBufferTime   = 0.001;    // 1 millisecond


//-----------------------------------------------------------------------------
// Helper Functions

// Based on LastKnownVsyncTime, predict time when the previous frame Vsync occurred.
// If it has no data it will still provide a reasonable estimate of last Vsync time.
static double calculateFrameStartTime(double now,
                                      double lastKnownVsyncTime,
                                      double lastKnownVsyncFuzzBuffer,
                                      double frameInterval)
{
    // Calculate time since last known vsync
    // Adding a fuzz time because the last known Vsync time is sometimes fuzzy and
    // we don't want to predict behind a frame.
    const double delta = now - lastKnownVsyncTime + lastKnownVsyncFuzzBuffer;

    // If last known vsync time was too long ago,
    if (delta < 0. ||
        delta > kVsyncDataAgeLimit)
    {
        // We have no idea when Vsync will happen!

        // Assume we are some time into the frame when this is called.
        return now - kNoVsyncInfoFrameTime;
    }

    // Calculate number of Vsyncs since the last known Vsync time.
    int numVsyncs = (int)(delta / frameInterval);

    // Calculate the last Vsync time.
    double lastFrameVsyncTime = lastKnownVsyncTime + numVsyncs * frameInterval;

    // Sanity checking...
    OVR_ASSERT(lastFrameVsyncTime - now > -0.16 && lastFrameVsyncTime - now < 0.30);

    return lastFrameVsyncTime;
}


//-----------------------------------------------------------------------------
// DistortionTiming : Initialization

DistortionTimer::DistortionTimer() :
    LastPresentTime(0),
    LastKnownVsyncTime(0),
    LastKnownVsyncFuzzBuffer(0),
    AppFrameIndex(0),
    DistortionRenderTimes(kTimewarpRenderTimeSamples),
    EstimatedTimewarpRenderTime(0),
  #ifdef OVR_OS_WIN32
    DeviceHandle(nullptr),
  #endif
    LatencyTester(nullptr),
    RenderState(nullptr),
    ScreenSwitchingDelay(0),
    TimeManager(true),
    LastTimewarpFrameEndTime(0),
    AlreadyInitialized(false),
    CurrentFrameTimewarpTiming(),
    LastTimewarpIMUTime(0)
{
    Reset();
}

void DistortionTimer::Reset()
{
    // Clear state
    LastKnownVsyncTime       = 0.;
    LastKnownVsyncFuzzBuffer = 0.;
    LastPresentTime          = 0.;
    LastTimewarpFrameEndTime = 0.;
    AppFrameIndex            = 0;

    ClearAppTimingUpdater();

    // Does not clear the distortion render times because this data is still good
    //DistortionRenderTimes.Clear();
    //EstimatedTimewarpRenderTime = 0.;
    //LatencyTester = nullptr;
    //RenderState = nullptr;
}

DistortionTimer::~DistortionTimer()
{
    RenderState   = nullptr;
    LatencyTester = nullptr;
}

bool DistortionTimer::Initialize(HMDRenderState const * renderState,
                                 FrameLatencyTracker const * lagTester)
{
    if (AlreadyInitialized)
    {
        OVR_ASSERT(renderState == RenderState && lagTester == LatencyTester);
        return true;
    }

    if (!renderState || !lagTester)
    {
        OVR_ASSERT(false);
        return false;
    }

    // Store members
    RenderState   = renderState;
    LatencyTester = lagTester;

#ifdef OVR_OS_WIN32
    // If in direct mode,
    if (!RenderState->OurHMDInfo.InCompatibilityMode)
    {
        // Attempt to open the driver
        DeviceHandle = CreateFile(L"\\\\.\\ovr_video",
            GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    }
#endif

    HmdRenderInfo::ShutterInfo const& shutter = RenderState->RenderInfo.Shutter;

    // Calculate the screen switching delay from shutter info.
    ScreenSwitchingDelay = shutter.PixelSettleTime * 0.5 + shutter.PixelPersistence * 0.5;

    // Set default frame delta for the TimeManager.
    TimeMan::Timing defaultTiming;
    defaultTiming.FrameDelta = shutter.VsyncToNextVsync;
    TimeManager.Initialize(defaultTiming);

    AlreadyInitialized = true;
    return true;
}


//-----------------------------------------------------------------------------
// DistortionTiming : Helper Member Functions

double DistortionTimer::getFrameInterval() const
{
    // Get the latest frame interval from the time manager.
    double frameInterval = TimeManager.GetFrameDelta();

    // If bad data is coming from the frame delta calculator,
    if (frameInterval < kMinFrameInterval ||
        frameInterval > kMaxFrameInterval)
    {
        // Use the shutter value by default.
        HmdRenderInfo::ShutterInfo const& shutter = RenderState->RenderInfo.Shutter;
        frameInterval = shutter.VsyncToNextVsync;
    }

    return frameInterval;
}

double DistortionTimer::getScanoutDelay()
{
    // If Vsync is off,
    if ((RenderState->EnabledHmdCaps & ovrHmdCap_NoVSync) != 0)
        return kExpectedNoVSyncLatency;

    double vsyncToScanoutDelay = 0.;

    // If latency tester results are not available,
    if (!LatencyTester || !LatencyTester->GetVsyncToScanout(vsyncToScanoutDelay))
    {
        // Use a reasonable default post-present latency estimate.
#ifdef OVR_OS_WIN32
        vsyncToScanoutDelay = RenderState->OurHMDInfo.InCompatibilityMode ?
            RenderState->RenderInfo.Shutter.VsyncToNextVsync : kExpectedDriverLatency;
#else
        // FIXME: This is a heuristic value that may need to be better tuned later
        // as the Mac/Linux render architecture solidifies.
        vsyncToScanoutDelay = 0.0007; // Observed as 0.7 ms on Linux
#endif
    }

    // Clamp the result be zero or positive.
    if (vsyncToScanoutDelay < 0.)
        vsyncToScanoutDelay = 0;

    return vsyncToScanoutDelay;
}

#ifdef OVR_OS_WIN32

bool DistortionTimer::getDriverVsyncTime(double* previousKnownVsyncTime)
{
    // If using the driver,
    if (!RenderState->OurHMDInfo.InCompatibilityMode)
    {
        ULONG riftId = (ULONG)RenderState->OurHMDInfo.ShimInfo.DeviceNumber;
        UINT64 results[2];
        ULONG bytesReturned = 0;

        BOOL success = DeviceIoControl(DeviceHandle.Get(), IOCTL_RIFTMGR_GETCURRENTFRAMEINFO, &riftId,
            sizeof(riftId), results, sizeof(results), &bytesReturned, nullptr);

        if (success)
        {
            // Calculate Vsync time in seconds based on QPC from display driver.
            *previousKnownVsyncTime = results[1] * Timer::GetPerfFrequencyInverse();
            return true;
        }
    }

    return false;
}

#endif // OVR_OS_WIN32


//-----------------------------------------------------------------------------
// DistortionTiming : Timewarp Timing

void DistortionTimer::AddDistortionTimeMeasurement(double distortionTimeSeconds)
{
    // Accumulate the new measurement.
    DistortionRenderTimes.Add(distortionTimeSeconds);

    // If enough measurements are collected now,
    if (!NeedDistortionTimeMeasurement())
    {
        EstimatedTimewarpRenderTime = DistortionRenderTimes.GetMedian();
    }
}

void DistortionTimer::submitDisplayFrame(double frameEndTime, double frameInterval)
{
    // Get the last display frame index
    uint32_t frameIndex = TimeManager.GetLastDisplayFrameIndex();
    double lastTime     = TimeManager.GetLastDisplayFrameTime();

    // If a previous submit time was recorded,
    if (lastTime > 0.)
    {
        // Calculate number of elapsed frames since last submit
        int elapsed = (int)((frameEndTime - lastTime + frameInterval * 0.5) / frameInterval);

        frameIndex += elapsed;
    }

    // Submit this display frame to the TimeManager
    TimeManager.SubmitDisplayFrame(frameIndex, AppFrameIndex, frameEndTime);
}

void DistortionTimer::updateLastKnownVsyncTime(double previousKnownVsyncTime)
{
    // Assume the data is exact.
    LastKnownVsyncFuzzBuffer = kExactVsyncBufferTime;

    // If previous vsync time was not provided,
    if (previousKnownVsyncTime <= 0.)
    {
#ifdef OVR_OS_WIN32
        // If the display driver was not helpful,
        if (!getDriverVsyncTime(&previousKnownVsyncTime))
#endif
        {
            // Use the last fuzzy vsync time and frame index
            // Add in a fuzz factor to prevent from predicting behind a whole frame!
            previousKnownVsyncTime   = LastPresentTime;

            // The data is pretty fuzzy so increase the buffer time.
            LastKnownVsyncFuzzBuffer = kFuzzyVsyncBufferTime;
        }
    }

    // Update last known vsync time
    LastKnownVsyncTime = previousKnownVsyncTime;
}

double DistortionTimer::getJITTimewarpTime(double frameEndTime)
{
    // If there is no timing information available for the timewarp draw call,
    if (EstimatedTimewarpRenderTime <= 0.)
    {
        // Disable JIT until we have some idea how long timewarp draw call takes.
        return 0.;
    }

    // Calculate Just-in-Time timewarp time
    return frameEndTime - EstimatedTimewarpRenderTime - kJITPreemptBufferTime;
}

// Rolls the previous known vsync time forward and then checks queue-ahead conditions
void DistortionTimer::CalculateTimewarpTiming(uint32_t frameIndex, double previousKnownVsyncTime)
{
    // Update LastKnownVsyncTime from previous known vsync time.
    updateLastKnownVsyncTime(previousKnownVsyncTime);

    // Calculate the frame start time from available information.
    const double frameInterval  = getFrameInterval();
    const double frameStartTime = calculateFrameStartTime(
        Timer::GetSeconds(),
        LastKnownVsyncTime,
        LastKnownVsyncFuzzBuffer,
        frameInterval);
    const double scanoutDelay   = getScanoutDelay();

    // If Vsync is off,
    if ((RenderState->EnabledHmdCaps & ovrHmdCap_NoVSync) != 0)
    {
        // Always render for current frame start-end times.
        CurrentFrameTimewarpTiming.ScanoutTime      = frameStartTime + scanoutDelay;
        CurrentFrameTimewarpTiming.JIT_TimewarpTime = 0.; // JIT disabled when Vsync is off

        // Reset the last timewarp frame end time when Vsync is turned off.
        LastTimewarpFrameEndTime = 0.;

        // Set the reference point for the scanout delay to the frame start time when Vsync
        // is off.
        LatencyTesterPresentTime = frameStartTime;
    }
    else // Vsync is on:
    {
        // Calculate frame end time with Vsync on.
        double frameEndTime = frameStartTime + frameInterval;

        // If JIT is turned off,
        if (!(RenderState->DistortionCaps & ovrDistortionCap_TimewarpJitDelay))
        {
#ifdef OVR_SUPPORT_QUEUE_AHEAD
            // Without JIT it can render ahead a frame.
            // If Vsync is on and it targets the same end of frame time twice
            // then the second timewarp render is queued ahead a frame, as two
            // consecutive distortion renders cannot target the same frame twice.

            // If the last frame end time is about the same as this one,
            if (fabs(LastTimewarpFrameEndTime - frameEndTime) < frameInterval * 0.25)
            {
                // Skip ahead to the next frame time.
                frameEndTime += frameInterval;
            }
#endif

            // Set JIT time to zero so that if JIT is turned off after this,
            // that the JIT wait code will be skipped and timing will be right
            // for this frame.
            CurrentFrameTimewarpTiming.JIT_TimewarpTime = 0.;
        }
        else
        {
            // JIT timewarp is enabled, so provide a time estimate.
            CurrentFrameTimewarpTiming.JIT_TimewarpTime = getJITTimewarpTime(frameEndTime);
        }

        // Record the new frame end time.
        LastTimewarpFrameEndTime = frameEndTime;

        // Scanout is based on frame end time when Vsync is on due to potential queue-ahead.
        CurrentFrameTimewarpTiming.ScanoutTime = frameEndTime + scanoutDelay;

        // Update the TimeManager.
        submitDisplayFrame(frameEndTime, frameInterval);

        // Set the reference point for the scanout delay to the frame end time when Vsync
        // is on.  This way our calculations will work out where we add scanout delay to
        // get the actual scanout time from this reference point in the future.
        LatencyTesterPresentTime = frameEndTime;
    }

    // Update lockless app timing base values
    LocklessAppTimingBase appTimingBase;
    appTimingBase.FrameInterval        = frameInterval;
    appTimingBase.LastEndFrameIndex    = frameIndex;
    appTimingBase.LastStartFrameTime   = frameStartTime;
    appTimingBase.LastKnownVsyncTime   = LastKnownVsyncTime;
    appTimingBase.ScanoutDelay         = scanoutDelay;
    appTimingBase.ScreenSwitchingDelay = ScreenSwitchingDelay;
    appTimingBase.VsyncFuzzFactor      = LastKnownVsyncFuzzBuffer;
    appTimingBase.IsValid              = 1;
    LocklessAppTimingBaseUpdater.SetState(appTimingBase);

    // Get eye timewarp times
    // NOTE: Approximating scanline start-end interval with Vsync-Vsync interval here.
    CalculateEyeTimewarpTimes(
        CurrentFrameTimewarpTiming.ScanoutTime + ScreenSwitchingDelay,
        frameInterval,
        RenderState->RenderInfo.Shutter.Type,
        CurrentFrameTimewarpTiming.EyeStartEndTimes[0],
        CurrentFrameTimewarpTiming.EyeStartEndTimes[1]);
}


//-----------------------------------------------------------------------------
// AppDistortionTimer

AppRenderTimer::AppRenderTimer() :
    AppTimingBaseUpdater(nullptr)
{
}

AppRenderTimer::~AppRenderTimer()
{
}

void AppRenderTimer::GetAppTimingForIndex(AppTiming& result, bool vsyncOn, uint32_t frameIndex)
{
    /*
        This code has to handle two big cases:

            Queue-Ahead:

        In this case the application is requesting poses for an upcoming frame, which is
        very common.  We need to predict ahead potentially beyond the next frame scanout
        time to a following scanout time.

            Missed Frames:

        In this case the rendering
            (1) game physics/other code ate too much CPU time and delayed the frame, or
            (2) the render command queuing took too long, or
            (3) took too long to complete on the GPU.

        Regarding (1):
            Game code is pretty much out of the way in the case of Unity which has
            two threads: A game code thread and a render thread.  So in a real game
            engine it's mainly due to too much render complexity not CPU game logic.

        Regarding (2):
            Distortion is done after the game queues render commands, and so
            the timewarp timing calculation can get pushed off into the next frame
            and actually get timed correctly.

        So as a result judder is mainly due to GPU performance, as other other sources
        of frame drops are mitigated.
    */

    if (!IsValid())
    {
        OVR_ASSERT(false);
        result.Clear();
        return;
    }

    LocklessAppTimingBase base = AppTimingBaseUpdater->GetState();

    // If no timing data is available,
    if (!base.IsValid)
    {
        result.Clear();
        return;
    }

    int32_t deltaIndex = (int32_t)(frameIndex - base.LastEndFrameIndex);

    // Calculate the end frame time.
    // Vsync on: This is the targeted Vsync for the provided frame index.
    // Vsync off: This is the middle of the frame requested by index.
    double endFrameTime;
    if (vsyncOn)
    {
        endFrameTime = base.LastStartFrameTime + base.FrameInterval * (deltaIndex + 1);
    }
    else
    {
        endFrameTime = base.LastStartFrameTime + base.FrameInterval * 0.5;
        endFrameTime += base.FrameInterval * deltaIndex;
    }

    // If targeted Vsync is now in the past,
    const double now = Timer::GetSeconds();
    if (now + base.VsyncFuzzFactor > endFrameTime)
    {
        // Assume there is no queue-ahead, so we should target the very
        // next upcoming Vsync
        double frameStartTime = calculateFrameStartTime(now, base.LastKnownVsyncTime,
                                                        base.VsyncFuzzFactor,
                                                        base.FrameInterval);
        if (vsyncOn)
        {
            // End frame time is just one frame ahead of the frame start
            endFrameTime = frameStartTime + base.FrameInterval;
        }
        else
        {
            // End frame time is half way through the current frame
            endFrameTime = frameStartTime + base.FrameInterval * 0.5;
        }
    }

    // Add Vsync-Scanout delay to get scanout time
    double scanoutTime = endFrameTime + base.ScanoutDelay;

    // Construct app frame information object
    result.FrameInterval       = base.FrameInterval;
    result.ScanoutStartTime    = scanoutTime;
    // NOTE: Approximating scanline start-end interval with Vsync-Vsync interval here.
    result.VisibleMidpointTime = scanoutTime + base.ScreenSwitchingDelay + base.FrameInterval * 0.5;
}


//-----------------------------------------------------------------------------
// AppTimingHistory

AppTimingHistory::AppTimingHistory()
{
    Clear();
}

AppTimingHistory::~AppTimingHistory()
{
}

void AppTimingHistory::Clear()
{
    LastWriteIndex = 0;
    memset(History, 0, sizeof(History));
}

void AppTimingHistory::SetScanoutTimeForFrame(uint32_t frameIndex, double scanoutTime)
{
    if (++LastWriteIndex >= kFramesMax)
    {
        LastWriteIndex = 0;
    }

    History[LastWriteIndex].FrameIndex = frameIndex;
    History[LastWriteIndex].ScanoutTime = scanoutTime;
}

double AppTimingHistory::LookupScanoutTime(uint32_t frameIndex)
{
    // Check last written entry first
    if (History[LastWriteIndex].FrameIndex == frameIndex)
    {
        return History[LastWriteIndex].ScanoutTime;
    }

    for (int i = 0; i < kFramesMax; ++i)
    {
        if (History[i].FrameIndex == frameIndex)
        {
            return History[i].ScanoutTime;
        }
    }

    return 0.;
}


}} // namespace OVR::CAPI
