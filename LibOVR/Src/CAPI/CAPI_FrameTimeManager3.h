/************************************************************************************

Filename    :   CAPI_FrameTimeManager3.h
Content     :   Manage frame timing and pose prediction for rendering
Created     :   November 30, 2013
Authors     :   Volga Aksoy, Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

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

#ifndef OVR_CAPI_FrameTimeManager3_h
#define OVR_CAPI_FrameTimeManager3_h

#include <OVR_CAPI.h>
#include <Kernel/OVR_Timer.h>
#include <Extras/OVR_Math.h>
#include <Util/Util_Render_Stereo.h>

namespace OVR { namespace CAPI { namespace FTM3 {

void LogTime(const char* msg);


//-------------------------------------------------------------------------------------
// MedianCalculator
//
// Helper class to calculate statistics
class MedianCalculator
{
public:
    MedianCalculator(int capacity);

    void    Clear();

    // This function throws away data larger than 100 milliseconds,
    // and it sanitizes negative time deltas replacing them with zero milliseconds.
    void    Add(double timeDelta);

    int     GetCount() const
    {
        return DataCount;
    }
    int     GetCapacity() const
    {
        return Capacity;
    }
    bool    AtCapacity() const
    {
        return DataCount >= Capacity;
    }

    double  GetMedian();
    bool    GetStats(double& minValue, double& maxValue,
        double& meanValue, double& medianValue);

private:
    Array<double> Data;
    Array<double> SortBuffer; // Buffer for quick select algorithm

    double MinValue, MaxValue, MeanValue, MedianValue;

    int    Index;
    int    DataCount, Capacity;
    bool   Recalculate;

    // Precondition: Count > 0
    void doRecalculate();
};


// Helper used to compute AddFrameIndex to DisplayFrameIndex ratio, by tracking
// how much each has advanced over the recent frames.
struct FrameIndexMapper
{
    enum { Capacity = 12 };

    // Circular buffer starting with StartIndex
    unsigned    DisplayFrameIndices[Capacity];
    unsigned    AppFrameIndices[Capacity];
    unsigned    StartIndex;
    unsigned    Count;

    FrameIndexMapper() : StartIndex(0), Count(0) { }

    void Reset()
    { StartIndex = Count = 0; }

    void    Add(unsigned displayFrameIndex, unsigned appFrameIndex);
   
    double  GetAppToDisplayFrameRatio() const;
};


//-------------------------------------------------------------------------------------
// ***** FrameTimeManagerCore

// FrameTimeManager keeps track of rendered frame timing needed for predictions for
// orientations and time-warp.
//
//  The following items are not in Core for now
//
//      - TimewarpWaitDelta (how many seconds before EndFrame we start timewarp)
//         this should be handled externally.
//
//      - ScreenDelay (Screen delay from present to scan-out, as potentially reported by ScreenLatencyTracker.)
//           this si rendering setup and HW specific
//
//      - TimeWarpStartEndTimes  (move matrices logic outside for now)
//
//      - For now, always assume VSync on


class FrameTimeManagerCore
{
public:

    // Describes last presented frame data.
    struct Timing
    {
        // Hard-coded value or dynamic as reported by FrameTimeDeltas.GetMedianTimeDelta().
        double          FrameDelta;

        // Application frame index for which we requested timing.
        unsigned        AppFrameIndex;
        // HW frame index that we expect this will hit, the specified
        // frame will start scan-out at ScanoutStartSeconds. Monotonically increasing.
        unsigned        DisplayFrameIndex;

        // PostPresent & Flush (old approach) or reported scan-out time (new HW-reported approach).
        double          FrameSubmitSeconds;

        // Ratio of (AppFrames/DisplayFrames) in
        double          AppToDisplayFrameRatio;

        Timing()
        {
            memset(this, 0, sizeof(Timing));
        }

    };

public:

    FrameTimeManagerCore(bool vsyncEnabled);

    // Called on startup to provided data on HMD timing.  DefayltTiming should include FrameDelta
    // and other default values. This can also be called to reset timing.
    void    Initialize(const Timing& defaultTiming);

    // Returns frame timing data for any thread to access, including
    //   - Simulating thread that may be running ahead
    //   - Rendering thread, which would treat as BeginFrame data
    Timing GetAppFrameTiming(unsigned appFrameIndex);

    // Returns frame timing values for a particular DisplayFrameIndex.
    //  Maintaining DisplayFrameIndex is the job of the caller, as it may be fied to OS
    //  VSync frame reporting functionality ad/or system clock.
    Timing GetDisplayFrameTiming(unsigned displayFrameIndex);

    // To be called from TW Thread when displayFrame has been submitted for present.
    // This call is used to update lock-less timing frame basis. The provided
    // values do the following:
    //  - Establish relationship between App and Display frame index.
    //  - Track the real frameDelta (difference between VSyncs)
    void    SubmitDisplayFrame(unsigned displayFrameIndex, 
                               unsigned appFrameIndex, 
                               double scanoutStartSeconds);

    unsigned GetLastDisplayFrameIndex() const
    {
        return LastTiming.DisplayFrameIndex;
    }
    double GetLastDisplayFrameTime() const
    {
        return LastTiming.FrameSubmitSeconds;
    }

    double  GetFrameDelta() const
    {
        return calcFrameDelta();
    }

    void    SetVsync(bool enabled) { VsyncEnabled = enabled; }

    // ovrFrameTiming ovrFrameTimingFromTiming(const Timing& timing) const;
   
private:
    
    double      calcFrameDelta() const;
    

    // Timing changes if we have no Vsync (all prediction is reduced to fixed interval).
    bool        VsyncEnabled;
    // Default VsyncToVSync value, received in Initialize.
    double      DefaultFrameDelta;    
    // Last timing 
    Timing      LastTiming;

    // Current (or last) frame timing info. Used as a source for LocklessTiming.
    LocklessUpdater<Timing, Timing> LocklessTiming;

    // Timings are collected through a median filter, to avoid outliers.
    mutable MedianCalculator        FrameTimeDeltas;
    // Associates AppFrameIndex <-> 
    FrameIndexMapper                FrameIndices;
    
};


}}} // namespace OVR::CAPI::FTM3

#endif // OVR_CAPI_FrameTimeManager_h
