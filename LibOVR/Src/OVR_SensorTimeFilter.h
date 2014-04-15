/************************************************************************************

PublicHeader:   None
Filename    :   OVR_SensorTimeFilter.h
Content     :   Class to filter HMD time and convert it to system time
Created     :   December 20, 2013
Author      :   Michael Antonov
Notes       :

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

#ifndef OVR_SensorTimeFilter_h
#define OVR_SensorTimeFilter_h

#include "Kernel/OVR_Types.h"

namespace OVR {  
    

//-----------------------------------------------------------------------------------
// ***** SensorTimeFilter
    
// SensorTimeFilter converts sample device time, in seconds, to absolute system
// time. It filter maintains internal state to estimate the following:
//
//  - Difference between system and device time values (ClockDelta).
//      ~= (systemTime - deviceTime)
//  - Drift rate between system and device clocks (ClockDeltaDriftPerSecond).
//
//  Additionally, the following criteria are enforced:
//   - Resulting samples must be increasing, compared to prevSample.
//   - Returned sample time should not exceed 'now' system time by more then a fixed
//     value.
//       * Ideally this should be 0, however, enforcing this is hard when clocks
//         have high discrete values.
//   - Returned sample AbsoluteTime values deltas are very close to HW samples,
//     adjusted by drift rate. Note that this is not always possible due to clamping,
//     in which case it is better to use ScaleTimeUnit(deviceTimeDelta) 
//     for integration.
//
// Algorithm: We collect minimum ClockDelta  on windows of 
// consecutive samples (500 ms each set). Long term difference between sample
// set minimums is drift. ClockDelta is also continually nudged towards most recent
// minimum. 

class SensorTimeFilter
{
public:

    // It may be desirable to configure these per device/platform.
    // For example, rates can be tighter for DK2 because of microsecond clock.    
    struct Settings
    {
        Settings(int minSamples = 50,
                 double clockDeltaAdjust = -0.0002, // 200 mks in the past.
                 double futureClamp      = 0.0008)
            : MinSamples(minSamples),
              ClockDeltaAdjust(clockDeltaAdjust), 
          //    PastClamp(-0.032),
              FutureClamp(futureClamp),
              PastSampleResetSeconds(0.2),
              MaxChangeRate(0.004),
              MaxCorrectRate(0.004)
        { }

        // Minimum number of samples in a window. Different number may be desirable
        // based on how often samples come in.
        int    MinSamples;

        // Factor always added to ClockDelta, used to skew all values into the past by fixed
        // value and reduce the chances we report a sample "in the future".        
        double ClockDeltaAdjust;  
        // How much away in a past can a sample be before being shifted closer to system time.
        //double PastClamp;
        // How much larger then systemTime can a value be? Set to 0 to clamp to null,
        // put small positive value is better.
        double FutureClamp;

        // How long (in system time) do we take to reset the system if a device sample.
        // comes in the past. Generally, this should never happened, but exists as a way to
        // address bad timing coming form firmware (temp CCove issue, presumably fixed)
        // or buggy input.
        double PastSampleResetSeconds;

        // Maximum drift change and near-term correction rates, in seconds.
        double MaxChangeRate;
        double MaxCorrectRate;
    };


    SensorTimeFilter(const Settings& settings = Settings());


    // Convert device sample time to system time, driving clock drift estimation.    
    // Input:  SampleTime, System Time
    // Return: Absolute system time for sample
    double SampleToSystemTime(double sampleDeviceTime, double systemTime,
                              double prevResult, const char* debugTag = "");


    // Scales device time to account for drift.
    double ScaleTimeUnit(double deviceClockDelta)
    {
        return deviceClockDelta * (1.0 + ClockDeltaDriftPerSecond);
    }

    // Return currently estimated difference between the clocks.
    double GetClockDelta() const { return ClockDelta; }
    

private:

    void   initClockSampling(double sampleDeviceTime, double clockDelta);
    void   processFinishedMinWindow(double sampleDeviceTime, double systemTime);

    static double clampRate(double rate, double limit)
    {
        if (rate > limit)
            rate = limit;
        else if (rate < -limit)
            rate = -limit;
        return rate;
    }


    // Describes minimum observed ClockDelta for sample set seen in the past.
    struct MinRecord
    {
        double  MinClockDelta;
        double  LastSampleDeviceTime;
    };

    // Circular buffer storing MinRecord(s) several minutes into the past.
    // Oldest value here is used to help estimate drift.
    class MinRecordBuffer
    {
        enum { BufferSize = 60*6 }; // 3 min
    public:

        MinRecordBuffer() : Head(0), Tail(0) { }

        void      Reset()         { Head = Tail = 0; }
        bool      IsEmpty() const { return Head == Tail; }

        const MinRecord& GetOldest() const
        {
            OVR_ASSERT(!IsEmpty());
            return Records[Tail];
        }
        const MinRecord& GetNewest() const
        {
            OVR_ASSERT(!IsEmpty());
            return Records[(BufferSize + Head - 1) % BufferSize];
        }

        void     AddRecord(const MinRecord& rec)
        {
            Records[Head] = rec;
            Head = advanceIndex(Head);
            if (Head == Tail)
                Tail = advanceIndex(Tail);
        }

    private:

        static int advanceIndex(int index)
        {
            index++;
            if (index >= BufferSize)
                index = 0;
            return index;
        }

        MinRecord Records[BufferSize];
        int       Head;  // Location we will most recent entry, unused.
        int       Tail;  // Oldest entry.
    };


    Settings    FilterSettings;

    // Clock correction state.
    bool        ClockInitialized;
    double      ClockDelta;    
    double      ClockDeltaDriftPerSecond;
    double      ClockDeltaCorrectPerSecond;
    double      ClockDeltaCorrectSecondsLeft;
    double      OldClockDeltaDriftExpire;
    
    double      LastLargestDeviceTime;
    double      PrevSystemTime;    
    // Used to reset timing if we get multiple "samples in the past"
    double      PastSampleResetTime;

    // "MinWindow" is a block of time during which minimum ClockDelta values
    // are collected into MinWindowClockDelta.
    int         MinWindowsCollected;
    double      MinWindowDuration; // Device sample seconds
    double      MinWindowLastTime;
    double      MinWindowClockDelta;
    int         MinWindowSamples;

    // Historic buffer used to determine rate of clock change over time.
    MinRecordBuffer MinRecords;
};

} // namespace OVR

#endif // OVR_SensorTimeFilter_h
