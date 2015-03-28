/************************************************************************************

Filename    :   Util_Stopwatch.h
Content     :   Handy classes for making timing measurements
Created     :   June 1, 2014
Authors     :   Neil Konzen

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

*************************************************************************************/

#include "Kernel/OVR_Timer.h"

// NOTE: This file is designed to be included in multiple .cpp files
// with different settings of OVR_DISABLE_STOPWATCH,
// so there is no include guard or #pragma once.

namespace OVR { namespace Util {

// Declare a StopwatchTimer as a static, global, or member variable.
// Then use Stopwatch() to accumulate timings within a scope.
/*

static StopwatchTimer FooTimer("Foo", 37, .003);	// Compute average of 37 timing samples, print timing if average time is greater than .003 seconds
static StopwatchTimer BarTimer("Bar");			// Print time of each measurement

void SomeFunction()
{
	CodeNotToIncludeInTiming();

	{
		Stopwatch sw(FooTimer);	// sw times everything in its scope
		Foo();
	}	// Every 37 times through the loop, if average time of Foo() is > .003 seconds, the average time is printed with LogText()

	{
		Stopwatch sw(BarTimer);
		Bar();
	}	// Elapsed time of Bar() is printed every time

	MoreCodeNotToIncludeInTiming();
}
*/

#ifndef OVR_DISABLE_STOPWATCH

class StopwatchTimer
{
public:
	StopwatchTimer(const char* label, int printCount = 1, double printThreshold = 0.0) :
		StartTime(0),
		ElapsedTime(0),
                Label(label),
		SampleCount(0),
		PrintCount(printCount),
		PrintThreshold(printThreshold)
	{
	}

	inline void Start()
	{
		StartTime = Timer::GetSeconds();
	}

	inline void Stop()
	{
		ElapsedTime += Timer::GetSeconds() - StartTime;
		if (++SampleCount >= PrintCount)
			PrintAndReset();
	}

private:

	void PrintAndReset()
	{
		double dt = ElapsedTime / SampleCount;
        if (dt > PrintThreshold)
			LogText("%s: %.5f msec\n", Label, dt * 1000);

		// reset for next time
		ElapsedTime = 0;
		SampleCount = 0;
	}

	double StartTime;
	double ElapsedTime;
	const char* Label;
	int SampleCount;
	int PrintCount;
	double PrintThreshold;
};


class Stopwatch
{
public:
	inline Stopwatch(StopwatchTimer& timer) : Timer(&timer)	{ Timer->Start(); }
	inline ~Stopwatch()										{ Timer->Stop(); }
private:
	StopwatchTimer* Timer;
};

#else	// !OVR_DISABLE_STOPWATCH

// Non-invasive, zero-code stopwatch implementation

class StopwatchTimer
{
public:
	inline StopwatchTimer(const char* label, int printCount = 1, double printThreshold = 0.0) {}
	inline void Start()	{}
	inline void Stop()		{}
};

class Stopwatch
{
public:
	inline Stopwatch(StopwatchTimer& timer) {}
	inline ~Stopwatch()						{}
};

#endif	// OVR_DISABLE_STOPWATCH

}} // namespace OVR::Util
