/************************************************************************************

Filename    :   CAPI_DistortionTiming.cpp
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

#include "CAPI_DistortionTiming.h"
#include <Kernel/OVR_Timer.h>
#include <cmath>

namespace OVR { namespace CAPI {


//-----------------------------------------------------------------------------
// Timing Constants

// If the last known Vsync time is older than this age limit,
// then we should not use it for extrapolating to current time.
static const double kVsyncDataAgeLimit      = 10.;      // 10 seconds

// When Vsync is off and we have no idea when the last frame started,
// assume this amount of time has elapsed since the frame started.
static const double kNoVsyncInfoFrameTime   = 0.002;    // 2 milliseconds


//-----------------------------------------------------------------------------
// Helper Functions

// Based on LastKnownVsyncTime, predict time when the previous frame Vsync occurred.
// If it has no data it will still provide a reasonable estimate of last Vsync time.
double CalculateFrameStartTime(double now,
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
// AppDistortionTimer

AppRenderTimer::AppRenderTimer() :
    TimingUpdater(nullptr)
{
}

AppRenderTimer::~AppRenderTimer()
{
}

OVRError AppRenderTimer::Open(const char* sharedMemoryName)
{
    if (!TimingReader.Open(sharedMemoryName))
    {
        return OVR_MAKE_ERROR_F(ovrError_Initialize, "App render timer cannot open shared memory '%s'", sharedMemoryName);
    }

    // Set the timing updater
    TimingUpdater = TimingReader.Get();

    return OVRError::Success();
}

bool AppRenderTimer::getTimingBase(LocklessAppTimingBase& base) const
{
    if (!TimingUpdater)
    {
        return false;
    }

    base = TimingUpdater->GetState();

    // If no timing data is available,
    if (!base.IsValid)
    {
        return false;
    }

    return true;
}

double AppRenderTimer::GetNextVsyncTime() const
{
    LocklessAppTimingBase base;
    if (!getTimingBase(base))
    {
        return 0.;
    }

    const double now = Timer::GetSeconds();

    // Calculate the current frame's start time
    const double frameStartTime = CalculateFrameStartTime(now, base.LastKnownVsyncTime,
                                                          0.,
                                                          base.FrameInterval);

    // End frame time is just one frame ahead of the frame start
    return frameStartTime + base.FrameInterval;
}

double AppRenderTimer::GetFrameInterval() const
{
    if (!TimingUpdater)
    {
        return 0.;
    }

    LocklessAppTimingBase base = TimingUpdater->GetState();

    // If no timing data is available,
    if (!base.IsValid)
    {
        return 0.;
    }

    return base.FrameInterval;
}

void AppRenderTimer::GetAppTimingForIndex(AppTiming& result, bool vsyncOn, uint32_t frameIndex) const
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

    LocklessAppTimingBase base;
    if (!getTimingBase(base))
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
        double frameStartTime = CalculateFrameStartTime(now, base.LastKnownVsyncTime,
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

    // For now the AppFrameIndex matches the DisplayFrameIndex.
    // TODO: When implementing ATW-friendly timing, these will start to diverge. -cat
    result.DisplayFrameIndex   = frameIndex;
}


//-----------------------------------------------------------------------------
// AppTimingHistoryRecord

AppTimingHistoryRecord::AppTimingHistoryRecord() :
    FrameIndex(0),
    Timing(),
    RenderIMUTime(0.)
{
    Timing.Clear();
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

int AppTimingHistory::findRecordByIndex(uint32_t frameIndex) const
{
    for (int i = 0; i < kFramesMax; ++i)
    {
        if (History[i].FrameIndex == frameIndex)
        {
            return i;
        }
    }

    return -1;
}

int AppTimingHistory::openNextIndex(uint32_t frameIndex)
{
    if (++LastWriteIndex >= kFramesMax)
    {
        LastWriteIndex = 0;
    }

    History[LastWriteIndex] = AppTimingHistoryRecord();
    History[LastWriteIndex].FrameIndex = frameIndex;

    return LastWriteIndex;
}

void AppTimingHistory::Clear()
{
    Lock::Locker locker(&SyncLock);

    LastWriteIndex = 0;
    memset(History, 0, sizeof(History));
}

void AppTimingHistory::SetTiming(uint32_t frameIndex, AppTiming const& timing)
{
    Lock::Locker locker(&SyncLock);

    int index = findRecordByIndex(frameIndex);
    if (index == -1)
    {
        index = openNextIndex(frameIndex);
    }

    History[index].Timing = timing;
}

void AppTimingHistory::SetRenderIMUTime(double predTime, double renderIMUTime)
{
    Lock::Locker locker(&SyncLock);

    // Guess the closest is the first one
    int closestIndex = 0;
    double closestDist = fabs(History[0].Timing.VisibleMidpointTime - predTime);

    // For each other options,
    for (int i = 1; i < kFramesMax; ++i)
    {
        // Calculate the distance of the visible midpoint time from the prediction time
        double dist = fabs(History[i].Timing.VisibleMidpointTime - predTime);

        // If this other entry is closer,
        if (dist < closestDist)
        {
            // Use it instead
            closestIndex = i;
            closestDist = dist;
        }
    }

    // If within 10 milliseconds of the right frame,
    if (closestDist < 0.01)
    {
        // Set the render IMU time for the closest one
        History[closestIndex].RenderIMUTime = renderIMUTime;
    }
}

AppTimingHistoryRecord AppTimingHistory::Lookup(uint32_t frameIndex) const
{
    Lock::Locker locker(&SyncLock);

    int index = findRecordByIndex(frameIndex);
    if (index == -1)
    {
        return AppTimingHistoryRecord();
    }

    return History[index];
}


}} // namespace OVR::CAPI
