/************************************************************************************

PublicHeader:   None
Filename    :   OVR_SensorTimeFilter.cpp
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

#include "OVR_SensorTimeFilter.h"
#include "Kernel/OVR_Log.h"


#include <stdio.h>
#include <math.h>

namespace OVR {  

// Comment out for debug logging to file
//#define OVR_TIMEFILTER_LOG_CODE( code )  code
#define OVR_TIMEFILTER_LOG_CODE( code )

#if defined(OVR_OS_ANDROID)
    #define OVR_TIMEFILTER_LOG_FILENAME "/sdcard/TimeFilterLog.txt"
#elif defined(OVR_OS_WIN32)
    #define OVR_TIMEFILTER_LOG_FILENAME "C:\\TimeFilterLog.txt"
#else
    #define OVR_TIMEFILTER_LOG_FILENAME "TimeFilterLog.txt"
#endif

OVR_TIMEFILTER_LOG_CODE( FILE* pTFLogFile = 0; )


// Ideally, the following would always be true:
//  - NewSampleTime > PrevSample
//  - NewSampleTime < now systemTime
//  - (NewSampleTime - PrevSampleTime) == integration delta, matching
//    HW sample time difference + drift
//
// In practice, these issues affect us:
//  - System thread can be suspended for a while
//  - System de-buffering of recorded samples cause deviceTime to advance up
//    much faster then system time for ~100+ samples
//  - Device (DK1) and system clock granularities are high; this can
//    lead to potentially having estimations in the future
//


// ***** TimerFilter

SensorTimeFilter::SensorTimeFilter(const Settings& settings)
{
    FilterSettings = settings;

    ClockInitialized             = false;
    ClockDelta                   = 0;       
    ClockDeltaDriftPerSecond     = 0;
    ClockDeltaCorrectPerSecond   = 0;
    ClockDeltaCorrectSecondsLeft = 0;
    OldClockDeltaDriftExpire     = 0;

    LastLargestDeviceTime = 0;
    PrevSystemTime        = 0;    
    PastSampleResetTime   = 0;

    MinWindowsCollected  = 0;
    MinWindowDuration    = 0; // assigned later
    MinWindowLastTime    = 0;
    MinWindowSamples     = settings.MinSamples; // Force initialization

    OVR_TIMEFILTER_LOG_CODE( pTFLogFile = fopen(OVR_TIMEFILTER_LOG_FILENAME, "w+"); )
}


double SensorTimeFilter::SampleToSystemTime(double sampleDeviceTime, double systemTime,
                                            double prevResult, const char* debugTag)
{
    double clockDelta      = systemTime - sampleDeviceTime + FilterSettings.ClockDeltaAdjust;
    double deviceTimeDelta = sampleDeviceTime - LastLargestDeviceTime;       
    double result;

    // Collect a sample ClockDelta for a "MinimumWindow" or process
    // the window by adjusting drift rates if it's full of samples.
    //   - (deviceTimeDelta < 1.0f) is a corner cases, as it would imply timestamp skip/wrap.

    if (ClockInitialized)
    {
        // Samples in the past commonly occur if they come from separately incrementing
        // data channels. Just adjust them with ClockDelta.

        if (deviceTimeDelta < 0.0)
        {
            result = sampleDeviceTime + ClockDelta;

            if (result > (prevResult - 0.00001))
                goto clamp_and_log_result;

            // Consistent samples less then prevResult for indicate a back-jump or bad input.
            // In this case we return prevResult for a while, then reset filter if it keeps going.
            if (PastSampleResetTime < 0.0001)
            {
                PastSampleResetTime = systemTime + FilterSettings.PastSampleResetSeconds;
                goto clamp_and_log_result;
            }
            else if (systemTime > PastSampleResetTime) 
            {
                OVR_DEBUG_LOG(("SensorTimeFilter - Filtering reset due to samples in the past!\n"));
                initClockSampling(sampleDeviceTime, clockDelta);
                // Fall through to below, to ' PastSampleResetTime = 0.0; '
            }
            else
            {
                goto clamp_and_log_result;
            }
        }

        // Most common case: Record window sample.
        else if ( (deviceTimeDelta < 1.0f) &&
                  ( (sampleDeviceTime < MinWindowLastTime) ||
                  (MinWindowSamples < FilterSettings.MinSamples) ) )
        {
            // Pick minimum ClockDelta sample.
            if (clockDelta < MinWindowClockDelta)
                MinWindowClockDelta = clockDelta;
            MinWindowSamples++;        
        }
        else
        {
            processFinishedMinWindow(sampleDeviceTime, clockDelta);
        }

        PastSampleResetTime = 0.0;
    }
    else
    {
        initClockSampling(sampleDeviceTime, clockDelta);
    }
        

    // Clock adjustment for drift.
    ClockDelta += ClockDeltaDriftPerSecond  * deviceTimeDelta;

    // ClockDelta "nudging" towards last known MinWindowClockDelta.
    if (ClockDeltaCorrectSecondsLeft > 0.000001)
    {
        double correctTimeDelta = deviceTimeDelta;
        if (deviceTimeDelta > ClockDeltaCorrectSecondsLeft)        
            correctTimeDelta = ClockDeltaCorrectSecondsLeft;
        ClockDeltaCorrectSecondsLeft -= correctTimeDelta;
        
        ClockDelta += ClockDeltaCorrectPerSecond  * correctTimeDelta;
    }

    // Record largest device time, so we know what samples to use in accumulation
    // of min-window in the future.
    LastLargestDeviceTime = sampleDeviceTime;

    // Compute our resulting sample time after ClockDelta adjustment.
    result = sampleDeviceTime + ClockDelta;

    
clamp_and_log_result:

    OVR_TIMEFILTER_LOG_CODE( double savedResult = result; )

    // Clamp to ensure that result >= PrevResult, or not to far in the future.
    // Future clamp primarily happens in the very beginning if we are de-queuing
    // system buffer full of samples.
    if (result < prevResult)
    {
        result = prevResult;
    }    
    if (result > (systemTime + FilterSettings.FutureClamp))
    {
        result = (systemTime + FilterSettings.FutureClamp);
    }

    OVR_TIMEFILTER_LOG_CODE(

        // Tag lines that were outside desired range, with '<' or '>'.
        char rangeClamp         = ' '; 
        char resultDeltaFar     = ' ';

        if (savedResult > (systemTime + 0.0000001))
            rangeClamp = '>';
        if (savedResult < prevResult)
            rangeClamp = '<';

        // Tag any result delta outside desired threshold with a '*'.
        if (fabs(deviceTimeDelta - (result - prevResult)) >= 0.00002)
            resultDeltaFar = '*';
    
        fprintf(pTFLogFile, "Res%s = %13.7f, dt = % 8.7f,  ClkD = %13.6f  "
                            "sysT = %13.6f, sysDt = %f,  "
                            "sysDiff = % f, devT = %11.6f,  ddevT = %9.6f %c%c\n",
                            debugTag, result, result - prevResult, ClockDelta,
                            systemTime, systemTime - PrevSystemTime,
                            -(systemTime - result),  // Negatives in the past, positive > now.
                            sampleDeviceTime,  deviceTimeDelta, rangeClamp, resultDeltaFar);

        ) // OVR_TIMEFILTER_LOG_CODE()
    OVR_UNUSED(debugTag);

    // Record prior values. Useful or logging and clamping.
    PrevSystemTime = systemTime;    

    return result;    
}


void SensorTimeFilter::initClockSampling(double sampleDeviceTime, double clockDelta)
{
    ClockInitialized             = true;
    ClockDelta                   = clockDelta;
    ClockDeltaDriftPerSecond     = 0;
    OldClockDeltaDriftExpire     = 0;
    ClockDeltaCorrectSecondsLeft = 0;
    ClockDeltaCorrectPerSecond   = 0;

    MinWindowsCollected          = 0;
    MinWindowDuration            = 0.25;
    MinWindowClockDelta          = clockDelta;
    MinWindowLastTime            = sampleDeviceTime + MinWindowDuration;
    MinWindowSamples             = 0;
}


void SensorTimeFilter::processFinishedMinWindow(double sampleDeviceTime, double clockDelta)
{
    MinRecord newRec = { MinWindowClockDelta, sampleDeviceTime };
    
    double    clockDeltaDiff    = MinWindowClockDelta - ClockDelta;
    double    absClockDeltaDiff = fabs(clockDeltaDiff);


    // Abrupt change causes Reset of minClockDelta collection.
    //  > 8 ms would a Large jump in a minimum sample, as those are usually stable.
    //  > 1 second intantaneous jump would land us here as well, as that would imply
    //    device being suspended, clock wrap or some other unexpected issue.
    if ((absClockDeltaDiff > 0.008) ||
        ((sampleDeviceTime - LastLargestDeviceTime) >= 1.0))
    {            
        OVR_TIMEFILTER_LOG_CODE(
            fprintf(pTFLogFile,
                    "\nMinWindow Finished:  %d Samples, MinWindowClockDelta=%f, MW-CD=%f,"
                    "  ** ClockDelta Reset **\n\n",
                    MinWindowSamples, MinWindowClockDelta, MinWindowClockDelta-ClockDelta);
            )

        // Use old collected ClockDeltaDriftPerSecond drift value 
        // up to 1 minute until we collect better samples.
        if (!MinRecords.IsEmpty())
        {
            OldClockDeltaDriftExpire = MinRecords.GetNewest().LastSampleDeviceTime -
                                       MinRecords.GetOldest().LastSampleDeviceTime;
            if (OldClockDeltaDriftExpire > 60.0)
                OldClockDeltaDriftExpire = 60.0;
            OldClockDeltaDriftExpire += sampleDeviceTime;           
        }

        // Jump to new ClockDelta value.
        if ((sampleDeviceTime - LastLargestDeviceTime) > 1.0)
            ClockDelta = clockDelta;
        else
            ClockDelta = MinWindowClockDelta;

        ClockDeltaCorrectSecondsLeft = 0;
        ClockDeltaCorrectPerSecond   = 0;

        // Reset buffers, we'll be collecting a new MinWindow.
        MinRecords.Reset();
        MinWindowsCollected = 0;
        MinWindowDuration   = 0.25;
        MinWindowSamples    = 0;
    }
    else
    {        
        OVR_ASSERT(MinWindowSamples >= FilterSettings.MinSamples);

        double timeElapsed = 0;

        // If we have older values, use them to update clock drift in 
        // ClockDeltaDriftPerSecond
        if (!MinRecords.IsEmpty() && (sampleDeviceTime > OldClockDeltaDriftExpire))
        {
            MinRecord rec = MinRecords.GetOldest();

            // Compute clock rate of drift.            
            timeElapsed = sampleDeviceTime - rec.LastSampleDeviceTime;

            // Check for divide by zero shouldn't be necessary here, but just be be safe...
            if (timeElapsed > 0.000001)
            {
                ClockDeltaDriftPerSecond = (MinWindowClockDelta - rec.MinClockDelta) / timeElapsed;
                ClockDeltaDriftPerSecond = clampRate(ClockDeltaDriftPerSecond,
                                                      FilterSettings.MaxChangeRate);
            }
            else
            {
                ClockDeltaDriftPerSecond = 0.0;
            }
        }

        MinRecords.AddRecord(newRec);


        // Catchup correction nudges ClockDelta towards MinWindowClockDelta.
        // These are needed because clock drift correction alone is not enough
        // for past accumulated error/high-granularity clock delta changes.
        // The further away we are, the stronger correction we apply.
        // Correction has timeout, as we don't want it to overshoot in case
        // of a large delay between samples.
       
        if (absClockDeltaDiff >= 0.00125)
        {
            // Correct large discrepancy immediately.
            if (absClockDeltaDiff > 0.00175)
            {
                if (clockDeltaDiff > 0)
                    ClockDelta += (clockDeltaDiff - 0.00175);
                else
                    ClockDelta += (clockDeltaDiff + 0.00175);

                clockDeltaDiff = MinWindowClockDelta - ClockDelta;
            }

            ClockDeltaCorrectPerSecond   = clockDeltaDiff;
            ClockDeltaCorrectSecondsLeft = 1.0;
        }            
        else if (absClockDeltaDiff > 0.0005)
        {                
            ClockDeltaCorrectPerSecond   = clockDeltaDiff / 8.0;
            ClockDeltaCorrectSecondsLeft = 8.0;
        }
        else
        {                
            ClockDeltaCorrectPerSecond   = clockDeltaDiff / 15.0;
            ClockDeltaCorrectSecondsLeft = 15.0;
        }

        ClockDeltaCorrectPerSecond = clampRate(ClockDeltaCorrectPerSecond,
                                               FilterSettings.MaxCorrectRate);

        OVR_TIMEFILTER_LOG_CODE(
            fprintf(pTFLogFile,
                    "\nMinWindow Finished:  %d Samples, MinWindowClockDelta=%f, MW-CD=%f,"
                    " tileElapsed=%f, ClockChange=%f, ClockCorrect=%f\n\n",
                    MinWindowSamples, MinWindowClockDelta, MinWindowClockDelta-ClockDelta,
                    timeElapsed, ClockDeltaDriftPerSecond, ClockDeltaCorrectPerSecond);
            )                           
    }

    // New MinClockDelta collection window.
    // Switch to longer duration after first few windows.
    MinWindowsCollected ++;
    if (MinWindowsCollected > 5)
        MinWindowDuration = 0.5; 

    MinWindowClockDelta = clockDelta;
    MinWindowLastTime   = sampleDeviceTime + MinWindowDuration;
    MinWindowSamples    = 0;
}


} // namespace OVR

