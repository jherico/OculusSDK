/************************************************************************************

Filename    :   CAPI_FrameTimeManager3.cpp
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

#include "CAPI_FrameTimeManager3.h"

#include "Kernel/OVR_Log.h"

//#include <dxgi.h>

namespace OVR { namespace CAPI { namespace FTM3 {


// Number of frame delta samples to include in the median calculation.
static const int kFrameDeltaSamples = 12;


void LogTime(const char* msg)
{
    static double lastTime = 0.0;
    double now = ovr_GetTimeInSeconds();

    LogText("t=%.3f, dt=%.3f: %s\n", now, now-lastTime, msg);
    lastTime = now;
}
    

//-------------------------------------------------------------------------------------

FrameTimeManagerCore::FrameTimeManagerCore(bool vsyncEnabled) :
    VsyncEnabled(vsyncEnabled),    
    LastTiming(),
    FrameTimeDeltas(kFrameDeltaSamples)
{            
}

void FrameTimeManagerCore::Initialize(const Timing& defaultTiming)
{
    // Contains default frame delta and indices.
    //   - TBD determine how those are initialized
    //  
    FrameTimeDeltas.Clear();
    FrameIndices.Reset();

    LastTiming        = defaultTiming;
    DefaultFrameDelta = defaultTiming.FrameDelta;
    
    LocklessTiming.SetState(LastTiming);
}


FrameTimeManagerCore::Timing FrameTimeManagerCore::GetAppFrameTiming(unsigned appFrameIndex)
{    
    // Right now this is required for timing.

    Timing timing = LocklessTiming.GetState();

    // TBD: We need AppFrameIndex and DisplayIndex initialized before this is called.

    OVR_ASSERT(appFrameIndex == 0 || appFrameIndex == timing.AppFrameIndex + 1);

    if (appFrameIndex > timing.AppFrameIndex)
    {
        int      appFrameDelta = appFrameIndex - timing.AppFrameIndex;

        // Don't run away too far into the future beyond rendering.
     //   OVR_ASSERT(appFrameDelta < 6);

        // Convert to display frames. If we have one-to-one frame sync, appFrameDelta
        // will be 1.0, so this will give us a correct frame value.
        int      displayFrame          = timing.DisplayFrameIndex +
                                         (int) (appFrameDelta * ( 1.0 / timing.AppToDisplayFrameRatio));
   
        double  prevFrameSubmitSeconds = (timing.FrameSubmitSeconds == 0.0) ?
                                         ovr_GetTimeInSeconds() : timing.FrameSubmitSeconds;

        double  displayFrameSubmitTime = prevFrameSubmitSeconds +
                                          double(displayFrame-timing.DisplayFrameIndex) *
                                          timing.FrameDelta;

        // Initialize to new values.
        timing.AppFrameIndex      = appFrameIndex;
        timing.DisplayFrameIndex  = displayFrame;
        timing.FrameSubmitSeconds = displayFrameSubmitTime;
    }    

    return timing;
}


// Next:  GetDisplayFrameTiming - used for timewarp and late-latching
//
FrameTimeManagerCore::Timing FrameTimeManagerCore::GetDisplayFrameTiming(unsigned displayFrameIndex)
{
    // Thus assumes caller has checked the 'displayFrameIndex' agains the current
    // clock/vsync and this is the actual desired value.
        
    // TBD: Should we use Lockless here? It depends if we plan to access this form different threads.
    Timing timing = LastTiming;

    if (displayFrameIndex > timing.DisplayFrameIndex)
    {
        double  prevFrameSubmitSeconds = (timing.FrameSubmitSeconds == 0.0) ?
                                         ovr_GetTimeInSeconds() : timing.FrameSubmitSeconds;

        // Initialize to new values.
        timing.FrameSubmitSeconds =  prevFrameSubmitSeconds +
                                          double(displayFrameIndex - timing.DisplayFrameIndex) *
                                          timing.FrameDelta;
        timing.DisplayFrameIndex  = displayFrameIndex;

        // Last submitted AppFrameIndex is ok.
    }

    return timing;
}


void FrameTimeManagerCore::SubmitDisplayFrame( unsigned displayFrameIndex,
                                               unsigned appFrameIndex,
                                               double scanoutStartSeconds )
{
    int displayFrameDelta = (displayFrameIndex - LastTiming.DisplayFrameIndex);

    // FIXME: Add queue ahead support here by detecting two frames targeting the same scanout.

    // Update frameDelta tracking.
    if ((LastTiming.FrameSubmitSeconds > 0.0) && (displayFrameDelta < 2))
    {
        if (displayFrameDelta > 0)
        {
            double thisFrameDelta = (scanoutStartSeconds - LastTiming.FrameSubmitSeconds) / displayFrameDelta;
            FrameTimeDeltas.Add(thisFrameDelta);
        }
        LastTiming.FrameDelta = calcFrameDelta();
    }

    // Update indices mapping
    FrameIndices.Add(displayFrameIndex, appFrameIndex);

    LastTiming.AppFrameIndex          = appFrameIndex;
    LastTiming.DisplayFrameIndex      = displayFrameIndex;
    LastTiming.FrameSubmitSeconds     = scanoutStartSeconds;
    LastTiming.AppToDisplayFrameRatio = FrameIndices.GetAppToDisplayFrameRatio();

    // Update Lockless
    LocklessTiming.SetState(LastTiming);
}



double  FrameTimeManagerCore::calcFrameDelta() const
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
        frameDelta = FrameTimeDeltas.GetMedian();
        if (frameDelta > (DefaultFrameDelta + 0.001))
            frameDelta = DefaultFrameDelta;
    }
    else
    {
        frameDelta = DefaultFrameDelta;
    }

    return frameDelta;
}



/*

// *****  Support code

struct FrameStatisticsSampler
{

    FrameStatisticsSampler(IDXGISwapChain* swapChain, double frameDelta);

    DXGI_FRAME_STATISTICS FStats;
    double                LastReportedFrameSeconds;
    unsigned              CurrentDisplayFrameIndex;
    unsigned              CurrentFrameScanoutSeconds;
};

FrameStatisticsSampler::FrameStatisticsSampler(IDXGISwapChain* swapChain, double frameDelta)
{
    swapChain->GetFrameStatistics(&FStats);

    UINT lastReportedFrameIndex = FStats.SyncRefreshCount;
    LastReportedFrameSeconds    = Timer::GetSecondsFromOSTicks(FStats.SyncQPCTime.QuadPart);

    double currentSeconds = ovr_GetTimeInSeconds();
    
    // Determine what frame we are currently scanning out and when it started.
    // This will be a part of input for FrameTimeManager...
    CurrentDisplayFrameIndex   = lastReportedFrameIndex +
                                  (unsigned)((currentSeconds - LastReportedFrameSeconds) / frameDelta);
    CurrentFrameScanoutSeconds = LastReportedFrameSeconds + 
                                  (CurrentDisplayFrameIndex - lastReportedFrameIndex) * frameDelta;
}


// Default, initial frame timing
FrameTimeManagerCore::Timing GetDefaultTiming(unsigned appFrameIndex,
                                              IDXGISwapChain* swapChain, const HmdRenderInfo& hri)
{
    FrameStatisticsSampler       fstats(swapChain, hri.Shutter.VsyncToNextVsync);
    FrameTimeManagerCore::Timing t;

    t.FrameDelta             = hri.Shutter.VsyncToNextVsync;
    t.AppFrameIndex          = appFrameIndex;
    t.DisplayFrameIndex      = fstats.CurrentDisplayFrameIndex;
    t.FrameSubmitSeconds     = fstats.CurrentFrameScanoutSeconds;
    t.AppToDisplayFrameRatio = 1.0;

    return t;
}


void InitializeFrame()
{
    FrameStatisticsSampler  fstats(pswapChain, ftm->GetFrameDelta());

    unsigned DisplayFrameIndex;
    //= FI(clock) + 1;
}



void UpdateFrame()
{
    IDXGISwapChain*       pswapChain = 0; // TBD: Pass argument
    FrameTimeManagerCore* ftm = 0;

    FrameStatisticsSampler  fstats(pswapChain, ftm->GetFrameDelta());    

    // Note that for rendering, there is actually a separate DisplayFrameIndex to think about.
    
    // Target 'Display FrameIndex' for next frame. Fix-up if the clock avanced too much.
    DisplayFrameIndex++;
    if (fstats.CurrentDisplayFrameIndex >= DisplayFrameIndex)
        DisplayFrameIndex = fstats.CurrentDisplayFrameIndex + 1;

    GetDisplayFrameTiming(DisplayFrameIndex);
    
}

FrameTimeManagerCore::ScanoutTimes::ScanoutTimes(
            const TimingInputs& inputs, 
            double frameSubmitSeconds, HmdShutterTypeEnum shutterType)
{
    double  frameDelta = inputs.FrameDelta;

    ScanoutStartSeconds    = frameSubmitSeconds + inputs.ScreenDelay;
    ScanoutMidpointSeconds = ScanoutStartSeconds + frameDelta * 0.5;

    //...   
}
*/


//-----------------------------------------------------------------------------------
// MedianCalculator

MedianCalculator::MedianCalculator(int capacity) :
    Capacity(capacity)
{
    Data.Resize(Capacity);
    SortBuffer.Resize(Capacity);

    Clear();
}

void MedianCalculator::Clear()
{
    MinValue = 0.;
    MaxValue = 0.;
    MeanValue = 0.;
    MedianValue = 0.;
    Index = 0;
    DataCount = 0;
    Recalculate = false;
}

void MedianCalculator::Add(double datum)
{
    // Write next datum
    Data[Index] = datum;

    // Loop circular buffer index around
    if (++Index >= Capacity)
    {
        Index = 0;
    }

    // Update data count
    if (!AtCapacity())
    {
        // Limited to capacity
        ++DataCount;
    }

    // Mark as dirty
    Recalculate = true;
}

double MedianCalculator::GetMedian()
{
    if (Recalculate)
    {
        doRecalculate();
    }

    return MedianValue;
}

bool MedianCalculator::GetStats(double& minValue, double& maxValue,
                                double& meanValue, double& medianValue)
{
    if (GetCount() <= 0)
    {
        // Cannot get statistics without one data point.
        return false;
    }

    if (Recalculate)
    {
        doRecalculate();
    }

    minValue = MinValue;
    maxValue = MaxValue;
    meanValue = MeanValue;
    medianValue = MedianValue;
    return true;
}

/*
    quick_select()

    Calculates the median.

    This Quickselect routine is based on the algorithm described in
	"Numerical recipes in C", Second Edition,
	Cambridge University Press, 1992, Section 8.5, ISBN 0-521-43108-5
	This code by Nicolas Devillard - 1998. Public domain.
*/

#define ELEM_SWAP(a,b) {double t=(a);(a)=(b);(b)=t; }

static double quick_select(double arr[], int n)
{
	int low, high ;
	int median;
	int middle, ll, hh;
	low = 0 ; high = n-1 ; median = (low + high) / 2;
	for (;;) {
		if (high <= low) /* One element only */
			return arr[median] ;
		if (high == low + 1) { /* Two elements only */
			if (arr[low] > arr[high])
				ELEM_SWAP(arr[low], arr[high]) ;
			return arr[median] ;
		}
		/* Find median of low, middle and high items; swap into position low */
		middle = (low + high) / 2;
		if (arr[middle] > arr[high]) ELEM_SWAP(arr[middle], arr[high]) ;
		if (arr[low] > arr[high]) ELEM_SWAP(arr[low], arr[high]) ;
		if (arr[middle] > arr[low]) ELEM_SWAP(arr[middle], arr[low]) ;
		/* Swap low item (now in position middle) into position (low+1) */
		ELEM_SWAP(arr[middle], arr[low+1]) ;
		/* Nibble from each end towards middle, swapping items when stuck */
		ll = low + 1;
		hh = high;
		for (;;) {
			do ll++; while (arr[low] > arr[ll]) ;
			do hh--; while (arr[hh] > arr[low]) ;
			if (hh < ll)
				break;
			ELEM_SWAP(arr[ll], arr[hh]) ;
		}
		/* Swap middle item (in position low) back into correct position */
		ELEM_SWAP(arr[low], arr[hh]) ;
		/* Re-set active partition */
		if (hh <= median)
			low = ll;
		if (hh >= median)
			high = hh - 1;
	}
}
#undef ELEM_SWAP
    
void MedianCalculator::doRecalculate()
{
    Recalculate = false;

    // Set default result
    MinValue = 0.;
    MaxValue = 0.;
    MeanValue = 0.;
    MedianValue = 0.;

    // Calculate median
    const int count = GetCount();
    for (int i = 0; i < count; ++i)
        SortBuffer[i] = Data[i];
    MedianValue = quick_select(&SortBuffer[0], count);

    double minValue = Data[0], maxValue = Data[0], sumValue = Data[0];

    for (int i = 1; i < count; ++i)
    {
        double value = Data[i];

        if (value < minValue)
        {
            minValue = value;
        }

        if (value > maxValue)
        {
            maxValue = value;
        }

        sumValue += value;
    }

    MinValue = minValue;
    MaxValue = maxValue;
    MeanValue = sumValue / count;

    OVR_ASSERT(MinValue <= MeanValue && MeanValue <= MaxValue);
    OVR_ASSERT(MinValue <= MedianValue && MedianValue <= MaxValue);
}


//-----------------------------------------------------------------------------------
// ***** FrameIndexMapper

void FrameIndexMapper::Add(unsigned displayFrameIndex, unsigned appFrameIndex)
{
    // Circular buffer; fill up then overwrite tail
    if (Count == Capacity)
    {
        DisplayFrameIndices[StartIndex] = displayFrameIndex;
        AppFrameIndices[StartIndex]     = appFrameIndex;
        StartIndex++;
        if (StartIndex == Count)
            StartIndex = 0;
    }
    else
    {
        OVR_ASSERT(StartIndex == 0);
        DisplayFrameIndices[Count] = displayFrameIndex;
        AppFrameIndices[Count]     = appFrameIndex;
        Count++;
    }
}

double FrameIndexMapper::GetAppToDisplayFrameRatio() const
{
    // Default ratio is one-to-one. Do aggressive clamping since we
    // don't want ratio to go too low since we can't predict that far anyway.
    if (Count < 3)
        return 1.0;

    unsigned newestIndex = StartIndex + Count - 1;
    if (newestIndex >= Capacity)
        newestIndex -= Capacity;

    unsigned displayDelta = DisplayFrameIndices[newestIndex] - DisplayFrameIndices[StartIndex];
    unsigned appDelta     = AppFrameIndices[newestIndex] - AppFrameIndices[StartIndex];

    if (displayDelta < 2)
        return 1.0f;

    double  frameRatio = double(appDelta) / double(displayDelta);
    if (frameRatio < 0.33f)
        return 0.33f;

    return frameRatio;
}



}}} // namespace OVR::CAPI::FTM2

