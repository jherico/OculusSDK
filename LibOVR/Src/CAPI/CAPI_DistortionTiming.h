/************************************************************************************

Filename    :   CAPI_DistortionTiming.h
Content     :   Implements timing for the distortion renderer
Created     :   Dec 16, 2014
Authors     :   Chris Taylor

Copyright   :   Copyright 2015 Oculus VR, LLC All Rights reserved.

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

#include <Kernel/OVR_Lockless.h>
#include <Kernel/OVR_SharedMemory.h>
#include <OVR_CAPI.h> // ovrFrameTiming
#include "OVR_Error.h"

/*
    -----------------------------
    Distortion Timing Terminology
    -----------------------------

    To fix on one set of terminology, a frame life-cycle is defined as the following:

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

    // Display frame index.
    uint32_t DisplayFrameIndex;

    void Clear()
    {
        FrameInterval       = 0.013; // A value that should not break anything.
        ScanoutStartTime    = 0.; // Predict to current time.
        VisibleMidpointTime = 0.; // Predict to current time.
        DisplayFrameIndex   = 0;
    }
};


//-----------------------------------------------------------------------------
// LocklessAppTimingBase
//
// Base timing info shared via lockless data structure.
// The AppDistortionTimer can use a copy of this data to derive an AppTiming
// object for a given frame index.
//
// This structure needs to be the same size and layout on 32-bit and 64-bit arch.
// Update OVR_PadCheck.cpp when updating this object.
struct OVR_ALIGNAS(8) LocklessAppTimingBase
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

static_assert(sizeof(LocklessAppTimingBase) == 4 + 4 + 8*6, "size mismatch");


typedef LocklessUpdater<LocklessAppTimingBase, LocklessPadding<LocklessAppTimingBase, 512> > TimingStateUpdater;


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

    OVRError Open(const char* sharedMemoryName);

    void SetUpdater(TimingStateUpdater const* updater)
    {
        TimingUpdater = updater;
    }

    // Pass in 0 for frameIndex to use the next scanout time,
    // or a non-zero incrementing number for each frame to support queue ahead.
    void GetAppTimingForIndex(AppTiming& result, bool vsyncOn, uint32_t frameIndex = 0) const;

    // Get the time at which Vsync will happen next.
    // This time does not include any perceptual delay and is mainly
    // useful for scheduling GPU work.
    double GetNextVsyncTime() const;

    // Get the frame interval between two consecutive vsyncs.
    double GetFrameInterval() const;

protected:
    SharedObjectReader<TimingStateUpdater> TimingReader;
    TimingStateUpdater const* TimingUpdater;

    // Refactored out common state checking at the top of GetAppTimingForIndex() and GetNextVsyncTime().
    bool getTimingBase(LocklessAppTimingBase& base) const;
};

// Based on LastKnownVsyncTime, predict time when the previous frame Vsync occurred.
// If it has no data it will still provide a reasonable estimate of last Vsync time.
double CalculateFrameStartTime(double now,
                               double lastKnownVsyncTime,
                               double lastKnownVsyncFuzzBuffer,
                               double frameInterval);


//-----------------------------------------------------------------------------
// AppTimingHistoryRecord
//
// One record in the AppTimingHistory.
struct AppTimingHistoryRecord
{
    uint32_t  FrameIndex;
    AppTiming Timing;
    double    RenderIMUTime;

    // Initializes members to zero
    AppTimingHistoryRecord();
};


//-----------------------------------------------------------------------------
// AppTimingHistory
//
// Keep a history of recent application render timing data, to keep a record of
// when frame indices are expected to scanout.  This is used later to compare
// with when those frames scan out to self-test the timing code.
class AppTimingHistory
{
public:
    AppTimingHistory();
    ~AppTimingHistory();

    // Clear history
    void Clear();

    // Setters
    void SetTiming(uint32_t frameIndex, AppTiming const& timing);
    void SetRenderIMUTime(double predTime, double renderIMUTime);

    // Looks up the frame index
    AppTimingHistoryRecord Lookup(uint32_t frameIndex) const;

protected:
    static const int kFramesMax = 8;

    // Lock to allow multiple app threads to access the history without races
    mutable Lock SyncLock;

    // Last index written
    int LastWriteIndex;

    // History circular buffer
    AppTimingHistoryRecord History[kFramesMax];

    // Find record by frame index
    // Returns -1 if not found, otherwise the array index containing it
    int findRecordByIndex(uint32_t frameIndex) const;

    int openNextIndex(uint32_t frameIndex);
};


}} // namespace OVR::CAPI

#endif // OVR_CAPI_DistortionTiming_h
