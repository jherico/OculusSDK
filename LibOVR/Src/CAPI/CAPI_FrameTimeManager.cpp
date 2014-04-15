/************************************************************************************

Filename    :   CAPI_FrameTimeManager.cpp
Content     :   Manage frame timing and pose prediction for rendering
Created     :   November 30, 2013
Authors     :   Volga Aksoy, Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.1 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.1 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "CAPI_FrameTimeManager.h"


namespace OVR { namespace CAPI {


//-------------------------------------------------------------------------------------
// ***** FrameLatencyTracker
    

FrameLatencyTracker::FrameLatencyTracker()
{
   Reset();
}

void FrameLatencyTracker::Reset()
{
    TrackerEnabled         = true;
    WaitMode               = SampleWait_Zeroes;
    FrameIndex             = 0;    
    MatchCount             = 0;
    RenderLatencySeconds   = 0.0;
    TimewarpLatencySeconds = 0.0;
    
    FrameDeltas.Clear();
}


unsigned char FrameLatencyTracker::GetNextDrawColor()
{   
    if (!TrackerEnabled || (WaitMode == SampleWait_Zeroes) ||
        (FrameIndex >= FramesTracked))
    {        
        return (unsigned char)Util::FrameTimeRecord::ReadbackIndexToColor(0);
    }

    OVR_ASSERT(FrameIndex < FramesTracked);    
    return (unsigned char)Util::FrameTimeRecord::ReadbackIndexToColor(FrameIndex+1);
}


void FrameLatencyTracker::SaveDrawColor(unsigned char drawColor, double endFrameTime,
                                        double renderIMUTime, double timewarpIMUTime )
{
    if (!TrackerEnabled || (WaitMode == SampleWait_Zeroes))
        return;

    if (FrameIndex < FramesTracked)
    {
        OVR_ASSERT(Util::FrameTimeRecord::ReadbackIndexToColor(FrameIndex+1) == drawColor);
        OVR_UNUSED(drawColor);

        // saves {color, endFrame time}
        FrameEndTimes[FrameIndex].ReadbackIndex         = FrameIndex + 1;
        FrameEndTimes[FrameIndex].TimeSeconds           = endFrameTime;
        FrameEndTimes[FrameIndex].RenderIMUTimeSeconds  = renderIMUTime;
        FrameEndTimes[FrameIndex].TimewarpIMUTimeSeconds= timewarpIMUTime;
        FrameEndTimes[FrameIndex].MatchedRecord         = false;
        FrameIndex++;
    }
    else
    {
        // If the request was outstanding for too long, switch to zero mode to restart.
        if (endFrameTime > (FrameEndTimes[FrameIndex-1].TimeSeconds + 0.15))
        {
            if (MatchCount == 0)
            {
                // If nothing was matched, we have no latency reading.
                RenderLatencySeconds   = 0.0;
                TimewarpLatencySeconds = 0.0;
            }

            WaitMode   =  SampleWait_Zeroes;
            MatchCount = 0;
            FrameIndex = 0;
        }
    }
}


void FrameLatencyTracker::MatchRecord(const Util::FrameTimeRecordSet &r)
{
    if (!TrackerEnabled)
        return;

    if (WaitMode == SampleWait_Zeroes)
    {
        // Do we have all zeros?
        if (r.IsAllZeroes())
        {
            OVR_ASSERT(FrameIndex == 0);
            WaitMode = SampleWait_Match;
            MatchCount = 0;
        }
        return;
    }

    // We are in Match Mode. Wait until all colors are matched or timeout,
    // at which point we go back to zeros.

    for (int i = 0; i < FrameIndex; i++)
    {
        int recordIndex = 0;
        int consecutiveMatch  = 0;

        OVR_ASSERT(FrameEndTimes[i].ReadbackIndex != 0);

        if (r.FindReadbackIndex(&recordIndex, FrameEndTimes[i].ReadbackIndex))
        {
            // Advance forward to see that we have several more matches.
            int  ri = recordIndex + 1;
            int  j  = i + 1;

            consecutiveMatch++;

            for (; (j < FrameIndex) && (ri < Util::FrameTimeRecordSet::RecordCount); j++, ri++)
            {
                if (r[ri].ReadbackIndex != FrameEndTimes[j].ReadbackIndex)
                    break;
                consecutiveMatch++;
            }

            // Match at least 2 items in the row, to avoid accidentally matching color.
            if (consecutiveMatch > 1)
            {
                // Record latency values for all but last samples. Keep last 2 samples
                // for the future to simplify matching.
                for (int q = 0; q < consecutiveMatch; q++)
                {
                    const Util::FrameTimeRecord &scanoutFrame = r[recordIndex+q];
                    FrameTimeRecordEx           &renderFrame  = FrameEndTimes[i+q];
                    
                    if (!renderFrame.MatchedRecord)
                    {
                        double deltaSeconds = scanoutFrame.TimeSeconds - renderFrame.TimeSeconds;
                        if (deltaSeconds > 0.0)
                        {
                            FrameDeltas.AddTimeDelta(deltaSeconds);
                            LatencyRecordTime      = scanoutFrame.TimeSeconds;
                            RenderLatencySeconds   = scanoutFrame.TimeSeconds - renderFrame.RenderIMUTimeSeconds;
                            TimewarpLatencySeconds = (renderFrame.TimewarpIMUTimeSeconds == 0.0)  ?  0.0 :
                                                     (scanoutFrame.TimeSeconds - renderFrame.TimewarpIMUTimeSeconds);
                        }

                        renderFrame.MatchedRecord = true;
                        MatchCount++;
                    }
                }

                // Exit for.
                break;
            }
        }
    } // for ( i => FrameIndex )


    // If we matched all frames, start over.
    if (MatchCount == FramesTracked)
    {
        WaitMode   =  SampleWait_Zeroes;
        MatchCount = 0;
        FrameIndex = 0;
    }
}


void FrameLatencyTracker::GetLatencyTimings(float latencies[3])
{
    if (ovr_GetTimeInSeconds() > (LatencyRecordTime + 2.0))
    {
        latencies[0] = 0.0f;
        latencies[1] = 0.0f;
        latencies[2] = 0.0f;
    }
    else
    {
        latencies[0] = (float)RenderLatencySeconds;
        latencies[1] = (float)TimewarpLatencySeconds;
        latencies[2] = (float)FrameDeltas.GetMedianTimeDelta();
    }    
}
    
    
//-------------------------------------------------------------------------------------

FrameTimeManager::FrameTimeManager(bool vsyncEnabled)
    : VsyncEnabled(vsyncEnabled), DynamicPrediction(true), SdkRender(false),
      FrameTiming()
{    
    RenderIMUTimeSeconds = 0.0;
    TimewarpIMUTimeSeconds = 0.0;
    
    // HACK: SyncToScanoutDelay observed close to 1 frame in video cards.
    //       Overwritten by dynamic latency measurement on DK2.
    VSyncToScanoutDelay   = 0.013f;
    NoVSyncToScanoutDelay = 0.004f;
}

void FrameTimeManager::Init(HmdRenderInfo& renderInfo)
{
    // Set up prediction distances.
    // With-Vsync timings.
    RenderInfo = renderInfo;

    ScreenSwitchingDelay = RenderInfo.Shutter.PixelSettleTime * 0.5f + 
                           RenderInfo.Shutter.PixelPersistence * 0.5f;
}

void FrameTimeManager::ResetFrameTiming(unsigned frameIndex,
                                        bool vsyncEnabled, bool dynamicPrediction,
                                        bool sdkRender)
{
    VsyncEnabled        = vsyncEnabled;
    DynamicPrediction   = dynamicPrediction;
    SdkRender           = sdkRender;

    FrameTimeDeltas.Clear();
    DistortionRenderTimes.Clear();
    ScreenLatencyTracker.Reset();

    FrameTiming.FrameIndex               = frameIndex;
    FrameTiming.NextFrameTime            = 0.0;
    FrameTiming.ThisFrameTime            = 0.0;
    FrameTiming.Inputs.FrameDelta        = calcFrameDelta();
    FrameTiming.Inputs.ScreenDelay       = calcScreenDelay();
    FrameTiming.Inputs.TimewarpWaitDelta = 0.0f;

    LocklessTiming.SetState(FrameTiming);
}


double  FrameTimeManager::calcFrameDelta() const
{
    // Timing difference between frame is tracked by FrameTimeDeltas, or
    // is a hard-coded value of 1/FrameRate.
    double  frameDelta;    

    if (!VsyncEnabled)
    {
        frameDelta = 0.0;
    }
    else if (FrameTimeDeltas.GetCount() > 3)
    {
        frameDelta = FrameTimeDeltas.GetMedianTimeDelta();
        if (frameDelta > (RenderInfo.Shutter.VsyncToNextVsync + 0.001))
            frameDelta = RenderInfo.Shutter.VsyncToNextVsync;
    }
    else
    {
        frameDelta = RenderInfo.Shutter.VsyncToNextVsync;
    }

    return frameDelta;
}


double  FrameTimeManager::calcScreenDelay() const
{
    double  screenDelay = ScreenSwitchingDelay;
    double  measuredVSyncToScanout;

    // Use real-time DK2 latency tester HW for prediction if its is working.
    // Do sanity check under 60 ms
    if (!VsyncEnabled)
    {
        screenDelay += NoVSyncToScanoutDelay;
    }
    else if ( DynamicPrediction &&
              (ScreenLatencyTracker.FrameDeltas.GetCount() > 3) &&
              (measuredVSyncToScanout = ScreenLatencyTracker.FrameDeltas.GetMedianTimeDelta(),
               (measuredVSyncToScanout > 0.0001) && (measuredVSyncToScanout < 0.06)) ) 
    {
        screenDelay += measuredVSyncToScanout;
    }
    else
    {
        screenDelay += VSyncToScanoutDelay;
    }

    return screenDelay;
}


double  FrameTimeManager::calcTimewarpWaitDelta() const
{
    // If timewarp timing hasn't been calculated, we should wait.
    if (!VsyncEnabled)
        return 0.0;

    if (SdkRender)
    {
        if (NeedDistortionTimeMeasurement())
            return 0.0;
        return -(DistortionRenderTimes.GetMedianTimeDelta() + 0.002);
    }
   
    // Just a hard-coded "high" value for game-drawn code.
    // TBD: Just return 0 and let users calculate this themselves?
    return -0.003;   
}



void FrameTimeManager::Timing::InitTimingFromInputs(const FrameTimeManager::TimingInputs& inputs,
                                                    HmdShutterTypeEnum shutterType,
                                                    double thisFrameTime, unsigned int frameIndex)
{    
    // ThisFrameTime comes from the end of last frame, unless it it changed.  
    double  nextFrameBase;
    double  frameDelta = inputs.FrameDelta;

    FrameIndex        = frameIndex;

    ThisFrameTime     = thisFrameTime;
    NextFrameTime     = ThisFrameTime + frameDelta;
    nextFrameBase     = NextFrameTime + inputs.ScreenDelay;
    MidpointTime      = nextFrameBase + frameDelta * 0.5;
    TimewarpPointTime = (inputs.TimewarpWaitDelta == 0.0) ?
                        0.0 : (NextFrameTime + inputs.TimewarpWaitDelta);

    // Calculate absolute points in time when eye rendering or corresponding time-warp
    // screen edges will become visible.
    // This only matters with VSync.
    switch(shutterType)
    {            
    case HmdShutter_RollingTopToBottom:
        EyeRenderTimes[0]               = MidpointTime;
        EyeRenderTimes[1]               = MidpointTime;
        TimeWarpStartEndTimes[0][0]     = nextFrameBase;
        TimeWarpStartEndTimes[0][1]     = nextFrameBase + frameDelta;
        TimeWarpStartEndTimes[1][0]     = nextFrameBase;
        TimeWarpStartEndTimes[1][1]     = nextFrameBase + frameDelta;
        break;
    case HmdShutter_RollingLeftToRight:
        EyeRenderTimes[0]               = nextFrameBase + frameDelta * 0.25;
        EyeRenderTimes[1]               = nextFrameBase + frameDelta * 0.75;

        /*
        // TBD: MA: It is probably better if mesh sets it up per-eye.
        // Would apply if screen is 0 -> 1 for each eye mesh
        TimeWarpStartEndTimes[0][0]     = nextFrameBase;
        TimeWarpStartEndTimes[0][1]     = MidpointTime;
        TimeWarpStartEndTimes[1][0]     = MidpointTime;
        TimeWarpStartEndTimes[1][1]     = nextFrameBase + frameDelta;
        */

        // Mesh is set up to vary from Edge of scree 0 -> 1 across both eyes
        TimeWarpStartEndTimes[0][0]     = nextFrameBase;
        TimeWarpStartEndTimes[0][1]     = nextFrameBase + frameDelta;
        TimeWarpStartEndTimes[1][0]     = nextFrameBase;
        TimeWarpStartEndTimes[1][1]     = nextFrameBase + frameDelta;

        break;
    case HmdShutter_RollingRightToLeft:

        EyeRenderTimes[0]               = nextFrameBase + frameDelta * 0.75;
        EyeRenderTimes[1]               = nextFrameBase + frameDelta * 0.25;
        
        // This is *Correct* with Tom's distortion mesh organization.
        TimeWarpStartEndTimes[0][0]     = nextFrameBase ;
        TimeWarpStartEndTimes[0][1]     = nextFrameBase + frameDelta;
        TimeWarpStartEndTimes[1][0]     = nextFrameBase ;
        TimeWarpStartEndTimes[1][1]     = nextFrameBase + frameDelta;
        break;
    case HmdShutter_Global:
        // TBD
        EyeRenderTimes[0]               = MidpointTime;
        EyeRenderTimes[1]               = MidpointTime;
        TimeWarpStartEndTimes[0][0]     = MidpointTime;
        TimeWarpStartEndTimes[0][1]     = MidpointTime;
        TimeWarpStartEndTimes[1][0]     = MidpointTime;
        TimeWarpStartEndTimes[1][1]     = MidpointTime;
        break;
    }
}

  
double FrameTimeManager::BeginFrame(unsigned frameIndex)
{    
    RenderIMUTimeSeconds = 0.0;
    TimewarpIMUTimeSeconds = 0.0;

    // ThisFrameTime comes from the end of last frame, unless it it changed.
    double thisFrameTime = (FrameTiming.NextFrameTime != 0.0) ?
                           FrameTiming.NextFrameTime : ovr_GetTimeInSeconds();
    
    // We are starting to process a new frame...
    FrameTiming.InitTimingFromInputs(FrameTiming.Inputs, RenderInfo.Shutter.Type,
                                     thisFrameTime, frameIndex);

    return FrameTiming.ThisFrameTime;
}


void FrameTimeManager::EndFrame()
{
    // Record timing since last frame; must be called after Present & sync.
    FrameTiming.NextFrameTime = ovr_GetTimeInSeconds();    
    if (FrameTiming.ThisFrameTime > 0.0)
    {
        FrameTimeDeltas.AddTimeDelta(FrameTiming.NextFrameTime - FrameTiming.ThisFrameTime);
        FrameTiming.Inputs.FrameDelta = calcFrameDelta();
    }

    // Write to Lock-less
    LocklessTiming.SetState(FrameTiming);
}



// Thread-safe function to query timing for a future frame

FrameTimeManager::Timing FrameTimeManager::GetFrameTiming(unsigned frameIndex)
{
    Timing frameTiming = LocklessTiming.GetState();

    if (frameTiming.ThisFrameTime != 0.0)
    {
        // If timing hasn't been initialized, starting based on "now" is the best guess.
        frameTiming.InitTimingFromInputs(frameTiming.Inputs, RenderInfo.Shutter.Type,
                                         ovr_GetTimeInSeconds(), frameIndex);
    }
    
    else if (frameIndex > frameTiming.FrameIndex)
    {
        unsigned frameDelta    = frameIndex - frameTiming.FrameIndex;
        double   thisFrameTime = frameTiming.NextFrameTime +
                                 double(frameDelta-1) * frameTiming.Inputs.FrameDelta;
        // Don't run away too far into the future beyond rendering.
        OVR_ASSERT(frameDelta < 6);

        frameTiming.InitTimingFromInputs(frameTiming.Inputs, RenderInfo.Shutter.Type,
                                         thisFrameTime, frameIndex);
    }    
     
    return frameTiming;
}


double FrameTimeManager::GetEyePredictionTime(ovrEyeType eye)
{
    if (VsyncEnabled)
    {
        return FrameTiming.EyeRenderTimes[eye];
    }

    // No VSync: Best guess for the near future
    return ovr_GetTimeInSeconds() + ScreenSwitchingDelay + NoVSyncToScanoutDelay;
}

Posef FrameTimeManager::GetEyePredictionPose(ovrHmd hmd, ovrEyeType eye)
{
    double         eyeRenderTime = GetEyePredictionTime(eye);
    ovrSensorState eyeState      = ovrHmd_GetSensorState(hmd, eyeRenderTime);

//    EyeRenderPoses[eye] = eyeState.Predicted.Pose;

    // Record view pose sampling time for Latency reporting.
    if (RenderIMUTimeSeconds == 0.0)
        RenderIMUTimeSeconds = eyeState.Recorded.TimeInSeconds;

    return eyeState.Predicted.Pose;
}


void FrameTimeManager::GetTimewarpPredictions(ovrEyeType eye, double timewarpStartEnd[2])
{
    if (VsyncEnabled)
    {
        timewarpStartEnd[0] = FrameTiming.TimeWarpStartEndTimes[eye][0];
        timewarpStartEnd[1] = FrameTiming.TimeWarpStartEndTimes[eye][1];
        return;
    }    

    // Free-running, so this will be displayed immediately.
    // Unfortunately we have no idea which bit of the screen is actually going to be displayed.
    // TODO: guess which bit of the screen is being displayed!
    // (e.g. use DONOTWAIT on present and see when the return isn't WASSTILLWAITING?)

    // We have no idea where scan-out is currently, so we can't usefully warp the screen spatially.
    timewarpStartEnd[0] = ovr_GetTimeInSeconds() + ScreenSwitchingDelay + NoVSyncToScanoutDelay;
    timewarpStartEnd[1] = timewarpStartEnd[0];
}


void FrameTimeManager::GetTimewarpMatrices(ovrHmd hmd, ovrEyeType eyeId,
                                           ovrPosef renderPose, ovrMatrix4f twmOut[2])
{
    if (!hmd)
    {
        return;
    }

    double timewarpStartEnd[2] = { 0.0, 0.0 };    
    GetTimewarpPredictions(eyeId, timewarpStartEnd);
      
    ovrSensorState startState = ovrHmd_GetSensorState(hmd, timewarpStartEnd[0]);
    ovrSensorState endState   = ovrHmd_GetSensorState(hmd, timewarpStartEnd[1]);

    if (TimewarpIMUTimeSeconds == 0.0)
        TimewarpIMUTimeSeconds = startState.Recorded.TimeInSeconds;

    Quatf quatFromStart = startState.Predicted.Pose.Orientation;
    Quatf quatFromEnd   = endState.Predicted.Pose.Orientation;
    Quatf quatFromEye   = renderPose.Orientation; //EyeRenderPoses[eyeId].Orientation;
    quatFromEye.Invert();
    
    Quatf timewarpStartQuat = quatFromEye * quatFromStart;
    Quatf timewarpEndQuat   = quatFromEye * quatFromEnd;

    Matrix4f timewarpStart(timewarpStartQuat);
    Matrix4f timewarpEnd(timewarpEndQuat);
    

    // The real-world orientations have:                                  X=right, Y=up,   Z=backwards.
    // The vectors inside the mesh are in NDC to keep the shader simple: X=right, Y=down, Z=forwards.
    // So we need to perform a similarity transform on this delta matrix.
    // The verbose code would look like this:
    /*
    Matrix4f matBasisChange;
    matBasisChange.SetIdentity();
    matBasisChange.M[0][0] =  1.0f;
    matBasisChange.M[1][1] = -1.0f;
    matBasisChange.M[2][2] = -1.0f;
    Matrix4f matBasisChangeInv = matBasisChange.Inverted();
    matRenderFromNow = matBasisChangeInv * matRenderFromNow * matBasisChange;
    */
    // ...but of course all the above is a constant transform and much more easily done.
    // We flip the signs of the Y&Z row, then flip the signs of the Y&Z column,
    // and of course most of the flips cancel:
    // +++                        +--                     +--
    // +++ -> flip Y&Z columns -> +-- -> flip Y&Z rows -> -++
    // +++                        +--                     -++
    timewarpStart.M[0][1] = -timewarpStart.M[0][1];
    timewarpStart.M[0][2] = -timewarpStart.M[0][2];
    timewarpStart.M[1][0] = -timewarpStart.M[1][0];
    timewarpStart.M[2][0] = -timewarpStart.M[2][0];

    timewarpEnd  .M[0][1] = -timewarpEnd  .M[0][1];
    timewarpEnd  .M[0][2] = -timewarpEnd  .M[0][2];
    timewarpEnd  .M[1][0] = -timewarpEnd  .M[1][0];
    timewarpEnd  .M[2][0] = -timewarpEnd  .M[2][0];

    twmOut[0] = timewarpStart;
    twmOut[1] = timewarpEnd;
}


// Used by renderer to determine if it should time distortion rendering.
bool  FrameTimeManager::NeedDistortionTimeMeasurement() const
{
    if (!VsyncEnabled)
        return false;
    return DistortionRenderTimes.GetCount() < 10;
}


void  FrameTimeManager::AddDistortionTimeMeasurement(double distortionTimeSeconds)
{
    DistortionRenderTimes.AddTimeDelta(distortionTimeSeconds);

    // If timewarp timing changes based on this sample, update it.
    double newTimewarpWaitDelta = calcTimewarpWaitDelta();
    if (newTimewarpWaitDelta != FrameTiming.Inputs.TimewarpWaitDelta)
    {
        FrameTiming.Inputs.TimewarpWaitDelta = newTimewarpWaitDelta;
        LocklessTiming.SetState(FrameTiming);
    }
}


void FrameTimeManager::UpdateFrameLatencyTrackingAfterEndFrame(
                                    unsigned char frameLatencyTestColor,
                                    const Util::FrameTimeRecordSet& rs)
{    
    // FrameTiming.NextFrameTime in this context (after EndFrame) is the end frame time.
    ScreenLatencyTracker.SaveDrawColor(frameLatencyTestColor,
                                       FrameTiming.NextFrameTime,
                                       RenderIMUTimeSeconds,
                                       TimewarpIMUTimeSeconds);

    ScreenLatencyTracker.MatchRecord(rs);

    // If screen delay changed, update timing.
    double newScreenDelay = calcScreenDelay();
    if (newScreenDelay != FrameTiming.Inputs.ScreenDelay)
    {
        FrameTiming.Inputs.ScreenDelay = newScreenDelay;
        LocklessTiming.SetState(FrameTiming);
    }
}


//-----------------------------------------------------------------------------------
// ***** TimeDeltaCollector

void TimeDeltaCollector::AddTimeDelta(double timeSeconds)
{
    // avoid adding invalid timing values
    if(timeSeconds < 0.0f)
        return;

    if (Count == Capacity)
    {
        for(int i=0; i< Count-1; i++)
            TimeBufferSeconds[i] = TimeBufferSeconds[i+1];
        Count--;
    }
    TimeBufferSeconds[Count++] = timeSeconds;
}

double TimeDeltaCollector::GetMedianTimeDelta() const
{
    double  SortedList[Capacity];
    bool    used[Capacity];

    memset(used, 0, sizeof(used));
    SortedList[0] = 0.0; // In case Count was 0...

    // Probably the slowest way to find median...
    for (int i=0; i<Count; i++)
    {
        double smallestDelta = 1000000.0;
        int    index = 0;

        for (int j = 0; j < Count; j++)
        {
            if (!used[j])
            {                
                if (TimeBufferSeconds[j] < smallestDelta)
                {
                    smallestDelta = TimeBufferSeconds[j];
                    index = j;
                }
            }
        }

        // Mark as used
        used[index]   = true;
        SortedList[i] = smallestDelta;
    }

    return SortedList[Count/2];
}
      

}} // namespace OVR::CAPI

