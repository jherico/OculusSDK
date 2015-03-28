/************************************************************************************

Filename    :   CAPI_FrameLatencyTracker.h
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

#ifndef OVR_CAPI_FrameLatencyTracker_h
#define OVR_CAPI_FrameLatencyTracker_h

#include <OVR_CAPI.h>
#include <Kernel/OVR_Timer.h>
#include <Extras/OVR_Math.h>
#include <Util/Util_Render_Stereo.h>
#include "CAPI_FrameTimeManager3.h"

namespace OVR { namespace CAPI {


//-------------------------------------------------------------------------------------
// ***** FrameLatencyData

// This structure contains the timing data for each frame that is tracked by the
// latency tester.

struct FrameLatencyData
{
    uint8_t DrawColor;           // Red channel color value drawn for the latency tester quad for this frame
    double PresentTime;          // (seconds) Time at which Vsync/Present occurred
    double RenderIMUTime;        // (seconds) Time when hardware sensors were sampled for render pose
    double TimewarpIMUTime;      // (seconds) Time when hardware sensors were sampled for timewarp pose

    double TimewarpPredictedScanoutTime; // (seconds) Time at which we expected scanout to start at timewarp time
    double RenderPredictedScanoutTime;   // (seconds) Time at which we expected scanout to start at render time
};


//-------------------------------------------------------------------------------------
// ***** OutputLatencyTimings

// Latency timings returned to the application.

struct OutputLatencyTimings
{
    double LatencyRender;       // (seconds) Last time between render IMU sample and scanout
    double LatencyTimewarp;     // (seconds) Last time between timewarp IMU sample and scanout
    double LatencyPostPresent;  // (seconds) Average time between Vsync and scanout
    double ErrorRender;         // (seconds) Last error in render predicted scanout time
    double ErrorTimewarp;       // (seconds) Last error in timewarp predicted scanout time

    void Clear()
    {
        LatencyRender      = 0.;
        LatencyTimewarp    = 0.;
        LatencyPostPresent = 0.;
        ErrorRender        = 0.;
        ErrorTimewarp      = 0.;
    }
};


//-------------------------------------------------------------------------------------
// ***** FrameLatencyTracker

// FrameLatencyTracker tracks frame Present to display Scan-out timing, as reported by
// the DK2 internal latency tester pixel read-back. The computed value is used in
// FrameTimeManager for prediction. View Render and TimeWarp to scan-out latencies are
// also reported for debugging.
//
// The class operates by generating color values from GetNextDrawColor() that must
// be rendered on the back end and then looking for matching values in FrameTimeRecordSet
// structure as reported by HW.

class FrameLatencyTracker
{
public:
    enum { FramesTracked = Util::LT2_IncrementCount-1 };

    FrameLatencyTracker();

    // DrawColor == 0 is special in that it doesn't need saving of timestamp
    unsigned char GetNextDrawColor();

    void SaveDrawColor(FrameLatencyData const & data);

    void MatchRecord(Util::FrameTimeRecordSet const & r);

    bool IsLatencyTimingAvailable();
    void GetLatencyTimings(OutputLatencyTimings& timings);

    // Returns time between vsync and scanout in seconds as measured by DK2 latency tester.
    // Returns false if measurements are unavailable.
    bool GetVsyncToScanout(double& vsyncToScanoutTime) const;

    void Reset();

protected:
    struct FrameTimeRecordEx : public Util::FrameTimeRecord
    {
        bool             MatchedRecord;
        FrameLatencyData FrameData;
    };

    void onRecordMatch(FrameTimeRecordEx& renderFrame,
                       Util::FrameTimeRecord const& scanoutFrame);

    // True if rendering read-back is enabled.
    bool                  TrackerEnabled;

    enum SampleWaitType
    {
        SampleWait_Zeroes, // We are waiting for a record with all zeros.
        SampleWait_Match   // We are issuing & matching colors.
    };
    
    SampleWaitType        WaitMode;
    int                   MatchCount;
    // Records of frame timings that we are trying to measure.
    FrameTimeRecordEx     History[FramesTracked];
    int                   FrameIndex;
    // Median filter for (ScanoutTimeSeconds - PostPresent frame time)
    mutable OVR::CAPI::FTM3::MedianCalculator FrameDeltas;
    double                LatencyRecordTime;

    // Latency reporting results
    OutputLatencyTimings  OutputTimings;
};


}} // namespace OVR::CAPI

#endif // OVR_CAPI_FrameLatencyTracker_h
