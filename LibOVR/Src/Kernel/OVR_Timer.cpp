/************************************************************************************

Filename    :   OVR_Timer.cpp
Content     :   Provides static functions for precise timing
Created     :   September 19, 2012
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

#include "OVR_Timer.h"
#include "OVR_Log.h"

#if defined (OVR_OS_WIN32)
#include <windows.h>
#elif defined(OVR_OS_ANDROID)
#include <time.h>
#include <android/log.h>

#else
#include <sys/time.h>
#endif

namespace OVR {

// For recorded data playback
bool   Timer::useFakeSeconds  = false; 
double Timer::FakeSeconds     = 0;


//------------------------------------------------------------------------
// *** Timer - Platform Independent functions

// Returns global high-resolution application timer in seconds.
double Timer::GetSeconds()
{
	if(useFakeSeconds)
		return FakeSeconds;

    return double(Timer::GetTicksNanos()) * 0.000000001;
}


#ifndef OVR_OS_WIN32

// Unused on OSs other then Win32.
void Timer::initializeTimerSystem()
{
}
void Timer::shutdownTimerSystem()
{
}

#endif



//------------------------------------------------------------------------
// *** Android Specific Timer

#if defined(OVR_OS_ANDROID)

UInt64 Timer::GetTicksNanos()
{
    if (useFakeSeconds)
        return (UInt64) (FakeSeconds * NanosPerSecond);

    // Choreographer vsync timestamp is based on.
    struct timespec tp;
    const int       status = clock_gettime(CLOCK_MONOTONIC, &tp);

    if (status != 0)
    {
        OVR_DEBUG_LOG(("clock_gettime status=%i", status ));
    }
    const UInt64 result = (UInt64)tp.tv_sec * (UInt64)(1000 * 1000 * 1000) + UInt64(tp.tv_nsec);
    return result;
}


//------------------------------------------------------------------------
// *** Win32 Specific Timer

#elif defined (OVR_OS_WIN32)


// This helper class implements high-resolution wrapper that combines timeGetTime() output
// with QueryPerformanceCounter.  timeGetTime() is lower precision but drives the high bits,
// as it's tied to the system clock.
struct PerformanceTimer
{
    PerformanceTimer()
        : OldMMTimeMs(0), MMTimeWrapCounter(0), PrefFrequency(0),
          LastResultNanos(0), PerfMinusTicksDeltaNanos(0)
    { }
    
    enum {
        MMTimerResolutionNanos = 1000000
    };
   
    void    Initialize();
    void    Shutdown();

    UInt64  GetTimeNanos();


    UINT64 getFrequency()
    {
        if (PrefFrequency == 0)
        {
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            PrefFrequency = freq.QuadPart;
        }        
        return PrefFrequency;
    }
    

    CRITICAL_SECTION TimeCS;
    // timeGetTime() support with wrap.
    UInt32          OldMMTimeMs;
    UInt32          MMTimeWrapCounter;
    // Cached performance frequency result.
    UInt64          PrefFrequency;
    
    // Computed as (perfCounterNanos - ticksCounterNanos) initially,
    // and used to adjust timing.
    UInt64          PerfMinusTicksDeltaNanos;
    // Last returned value in nanoseconds, to ensure we don't back-step in time.
    UInt64          LastResultNanos;
};

PerformanceTimer Win32_PerfTimer;


void PerformanceTimer::Initialize()
{
    timeBeginPeriod(1);
    InitializeCriticalSection(&TimeCS);
    MMTimeWrapCounter = 0;
    getFrequency();
}

void PerformanceTimer::Shutdown()
{
    DeleteCriticalSection(&TimeCS);
    timeEndPeriod(1);
}

UInt64 PerformanceTimer::GetTimeNanos()
{
    UInt64          resultNanos;
    LARGE_INTEGER   li;
    DWORD           mmTimeMs;

    // On Win32 QueryPerformanceFrequency is unreliable due to SMP and
    // performance levels, so use this logic to detect wrapping and track
    // high bits.
    ::EnterCriticalSection(&TimeCS);

    // Get raw value and perf counter "At the same time".
    mmTimeMs = timeGetTime();
    QueryPerformanceCounter(&li);

    if (OldMMTimeMs > mmTimeMs)
        MMTimeWrapCounter++;
    OldMMTimeMs = mmTimeMs;

    // Normalize to nanoseconds.
    UInt64  mmCounterNanos     = ((UInt64(MMTimeWrapCounter) << 32) | mmTimeMs) * 1000000;
    UInt64  frequency          = getFrequency();
    UInt64  perfCounterSeconds = UInt64(li.QuadPart) / frequency;
    UInt64  perfRemainderNanos = ( (UInt64(li.QuadPart) - perfCounterSeconds * frequency) *
                                   Timer::NanosPerSecond ) / frequency;
    UInt64  perfCounterNanos   = perfCounterSeconds * Timer::NanosPerSecond + perfRemainderNanos;

    if (PerfMinusTicksDeltaNanos == 0)
        PerfMinusTicksDeltaNanos = perfCounterNanos - mmCounterNanos;
 

    // Compute result before snapping. 
    //
    // On first call, this evaluates to:
    //          resultNanos = mmCounterNanos.    
    // Next call, assuming no wrap:
    //          resultNanos = prev_mmCounterNanos + (perfCounterNanos - prev_perfCounterNanos).        
    // After wrap, this would be:
    //          resultNanos = snapped(prev_mmCounterNanos +/- 1ms) + (perfCounterNanos - prev_perfCounterNanos).
    //
    resultNanos = perfCounterNanos - PerfMinusTicksDeltaNanos;    

    // Snap the range so that resultNanos never moves further apart then its target resolution.
    // It's better to allow more slack on the high side as timeGetTime() may be updated at sporadically 
    // larger then 1 ms intervals even when 1 ms resolution is requested.
    if (resultNanos > (mmCounterNanos + MMTimerResolutionNanos*2))
    {
        resultNanos = mmCounterNanos + MMTimerResolutionNanos*2;
        if (resultNanos < LastResultNanos)
            resultNanos = LastResultNanos;
        PerfMinusTicksDeltaNanos = perfCounterNanos - resultNanos;
    }
    else if (resultNanos < (mmCounterNanos - MMTimerResolutionNanos))
    {
        resultNanos = mmCounterNanos - MMTimerResolutionNanos;
        if (resultNanos < LastResultNanos)
            resultNanos = LastResultNanos;
        PerfMinusTicksDeltaNanos = perfCounterNanos - resultNanos;
    }

    LastResultNanos = resultNanos;
    ::LeaveCriticalSection(&TimeCS);

	//Tom's addition, to keep precision
	static UInt64      initial_time = 0;
	if (!initial_time) initial_time = resultNanos;
	resultNanos -= initial_time;


    return resultNanos;
}


// Delegate to PerformanceTimer.
UInt64 Timer::GetTicksNanos()
{
    if (useFakeSeconds)
        return (UInt64) (FakeSeconds * NanosPerSecond);

    return Win32_PerfTimer.GetTimeNanos();
}
void Timer::initializeTimerSystem()
{
    Win32_PerfTimer.Initialize();

}
void Timer::shutdownTimerSystem()
{
    Win32_PerfTimer.Shutdown();
}

#else   // !OVR_OS_WIN32 && !OVR_OS_ANDROID


//------------------------------------------------------------------------
// *** Standard OS Timer     

UInt64 Timer::GetTicksNanos()
{
    if (useFakeSeconds)
        return (UInt64) (FakeSeconds * NanosPerSecond);

    // TODO: prefer rdtsc when available?
	UInt64 result;

    // Return microseconds.
    struct timeval tv;

    gettimeofday(&tv, 0);

    result = (UInt64)tv.tv_sec * 1000000;
    result += tv.tv_usec;

    return result * 1000;
}

#endif  // OS-specific



} // OVR

