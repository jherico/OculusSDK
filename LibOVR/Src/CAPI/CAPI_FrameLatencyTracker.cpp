/************************************************************************************

Filename    :   CAPI_FrameLatencyTracker.cpp
Content     :   DK2 Latency Tester implementation
Created     :   Dec 12, 2013
Authors     :   Volga Aksoy, Michael Antonov

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

#include "CAPI_FrameLatencyTracker.h"
#include "Kernel/OVR_Log.h"

namespace OVR { namespace CAPI {


// Number of frame delta samples to include in the median calculation.
static const int kFrameDeltaSamples = 12;


//-------------------------------------------------------------------------------------
// ***** FrameLatencyTracker

FrameLatencyTracker::FrameLatencyTracker() :
    FrameDeltas(kFrameDeltaSamples)
{
    Reset();
}

void FrameLatencyTracker::Reset()
{
    TrackerEnabled         = true;
    WaitMode               = SampleWait_Zeroes;
    MatchCount             = 0;
    memset(History, 0, sizeof(History));
    FrameIndex             = 0;
    LatencyRecordTime      = 0.0;

    OutputTimings.Clear();

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

void FrameLatencyTracker::SaveDrawColor(FrameLatencyData const & data)
{
    if (!TrackerEnabled || (WaitMode == SampleWait_Zeroes))
        return;

    if (FrameIndex < FramesTracked)
    {
        OVR_ASSERT(Util::FrameTimeRecord::ReadbackIndexToColor(FrameIndex + 1) == data.DrawColor);

        // FrameTimeRecord data
        History[FrameIndex].ReadbackIndex = FrameIndex + 1;
        History[FrameIndex].TimeSeconds   = data.PresentTime;

        // FrameTimeRecordEx data
        History[FrameIndex].MatchedRecord = false;
        History[FrameIndex].FrameData     = data;

        FrameIndex++;
    }
    else
    {
        // If the request was outstanding for too long, switch to zero mode to restart.
        if (data.PresentTime > (History[FrameIndex-1].TimeSeconds + 0.15))
        {
            if (MatchCount == 0)
            {
                OutputTimings.Clear();
            }

            WaitMode   = SampleWait_Zeroes;
            MatchCount = 0;
            FrameIndex = 0;
        }
    }
}

void FrameLatencyTracker::onRecordMatch(FrameTimeRecordEx& renderFrame,
                                        Util::FrameTimeRecord const& scanoutFrame)
{
    MatchCount++;

    double deltaSeconds = scanoutFrame.TimeSeconds - renderFrame.TimeSeconds;

    // Reject latencies longer than 100 ms
    // This can happen in transient situations like dragging the render window around,
    // and since some critical systems depend on this latency data to provide steady-state
    // statistics for prediction purposes these outliers should not dirty the data.
    if (deltaSeconds < 0.1)
    {
        if (deltaSeconds < 0.)
        {
            deltaSeconds = 0.;
        }

        FrameDeltas.Add(deltaSeconds);
    }

    LatencyRecordTime = scanoutFrame.TimeSeconds;
    OutputTimings.LatencyRender = scanoutFrame.TimeSeconds - renderFrame.FrameData.RenderIMUTime;
    OutputTimings.LatencyTimewarp = (renderFrame.FrameData.TimewarpIMUTime == 0.0) ? 0.0 :
        (scanoutFrame.TimeSeconds - renderFrame.FrameData.TimewarpIMUTime);
    OutputTimings.ErrorRender = scanoutFrame.TimeSeconds - renderFrame.FrameData.RenderPredictedScanoutTime;
    OutputTimings.ErrorTimewarp = scanoutFrame.TimeSeconds - renderFrame.FrameData.TimewarpPredictedScanoutTime;
}

void FrameLatencyTracker::MatchRecord(Util::FrameTimeRecordSet const & r)
{
    if (!TrackerEnabled)
        return;

    if (WaitMode == SampleWait_Zeroes)
    {
        // Do we have all zeros?
        if (r.IsAllZeroes())
        {
            OVR_ASSERT(FrameIndex == 0);
            WaitMode   = SampleWait_Match;
            MatchCount = 0;
        }
        return;
    }

    // We are in Match Mode. Wait until all colors are matched or timeout,
    // at which point we go back to zeros.

    for (int i = 0; i < FrameIndex; i++)
    {
        int recordIndex      = 0;
        int consecutiveMatch = 0;

        OVR_ASSERT(History[i].ReadbackIndex != 0);

        if (r.FindReadbackIndex(&recordIndex, History[i].ReadbackIndex))
        {
            // Advance forward to see that we have several more matches.
            int  ri = recordIndex + 1;
            int  j  = i + 1;

            consecutiveMatch++;

            for (; (j < FrameIndex) && (ri < Util::FrameTimeRecordSet::RecordCount); j++, ri++)
            {
                if (r[ri].ReadbackIndex != History[j].ReadbackIndex)
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
                    FrameTimeRecordEx           &renderFrame  = History[i+q];
                    
                    if (!renderFrame.MatchedRecord)
                    {
                        renderFrame.MatchedRecord = true;

                        onRecordMatch(renderFrame, scanoutFrame);
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
        WaitMode   = SampleWait_Zeroes;
        MatchCount = 0;
        FrameIndex = 0;
    }
}

bool FrameLatencyTracker::IsLatencyTimingAvailable()
{
    return ovr_GetTimeInSeconds() < (LatencyRecordTime + 2.0);
}

void FrameLatencyTracker::GetLatencyTimings(OutputLatencyTimings& timings)
{
    if (!IsLatencyTimingAvailable())
    {
        timings.Clear();
        return;
    }

    timings = OutputTimings;
    timings.LatencyPostPresent = FrameDeltas.GetMedian();
}

bool FrameLatencyTracker::GetVsyncToScanout(double& vsyncToScanoutTime) const
{
    if (FrameDeltas.GetCount() <= 3)
    {
        return false;
    }

    double medianDelta = FrameDeltas.GetMedian();

    // Sanity check the result
    static const double SmallestAcceptedDelta = -0.0020; // -20 ms
    static const double LargestAcceptedDelta  =  0.060;  // 60 ms

    if ((medianDelta < SmallestAcceptedDelta) ||
        (medianDelta > LargestAcceptedDelta))
    {
        return false;
    }

    vsyncToScanoutTime = medianDelta;
    return true;
}


}} // namespace OVR::CAPI
