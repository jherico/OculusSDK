/************************************************************************************

Filename    :   Util_Watchdog.cpp
Content     :   Deadlock reaction
Created     :   June 27, 2013
Authors     :   Kevin Jenkins, Chris Taylor

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

*************************************************************************************/

#include "Util_Watchdog.h"
#include <Kernel/OVR_DebugHelp.h>
#include <Kernel/OVR_Win32_IncludeWindows.h>

#if defined(OVR_OS_LINUX) || defined(OVR_OS_MAC)
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/ptrace.h>

    #if defined(OVR_OS_LINUX)
        #include <sys/wait.h>
    #endif
#endif

OVR_DEFINE_SINGLETON(OVR::Util::WatchDogObserver);

namespace OVR { namespace Util {

const int DefaultThreshhold = 60000; // milliseconds

//-----------------------------------------------------------------------------
// Tools

static uint32_t GetFastMsTime()
{
#if defined(OVR_OS_MS)
    return ::GetTickCount();
#else
    return Timer::GetTicksMs();
#endif
}


//-----------------------------------------------------------------------------
// WatchDogObserver

static bool ExitingOnDeadlock = false;

bool WatchDogObserver::IsExitingOnDeadlock()
{
    return ExitingOnDeadlock;
}

void WatchDogObserver::SetExitingOnDeadlock(bool enabled)
{
    ExitingOnDeadlock = enabled;
}

WatchDogObserver::WatchDogObserver() :
    IsReporting(false)
{
    Start();

	// Must be at end of function
	PushDestroyCallbacks();
}

WatchDogObserver::~WatchDogObserver()
{
    TerminationEvent.SetEvent();

    Join();
}

void WatchDogObserver::OnThreadDestroy()
{
    TerminationEvent.SetEvent();
}

void WatchDogObserver::OnSystemDestroy()
{
    Release();
}

int WatchDogObserver::Run()
{
    OVR_DEBUG_LOG(("[WatchDogObserver] Starting"));

    SetThreadName("WatchDog");

    while (!TerminationEvent.Wait(WakeupInterval))
    {
        Lock::Locker locker(&ListLock);

        const uint32_t t1 = GetFastMsTime();

        const int count = DogList.GetSizeI();
        for (int i = 0; i < count; ++i)
        {
            WatchDog* dog = DogList[i];

            const int threshold = dog->ThreshholdMilliseconds;
            const uint32_t t0 = dog->WhenLastFedMilliseconds;

            // If threshold exceeded, assume there is thread deadlock of some sort.
            int delta = (int)(t1 - t0);
            if (delta > threshold)
            {
                // Expected behavior:
                // SingleProcessDebug, SingleProcessRelease, Debug: This is only ever done for internal testing, so we don't want it to trigger the deadlock termination.
                // Release: This is our release configuration where we want it to terminate itself.

                // Print a stack trace of all threads if there's no debugger present.
                const bool debuggerPresent = OVRIsDebuggerPresent();

                LogError("{ERR-027} [WatchDogObserver] Deadlock detected: %s", dog->ThreadName.ToCStr());
                if (!debuggerPresent) // We don't print threads if a debugger is present because otherwise every time the developer paused the app to debug, it would spit out a long thread trace upon resuming.
                {
                    if (SymbolLookup::Initialize())
                    {
                        // symbolLookup is static here to avoid putting 32 KB on the stack
                        // and potentially overflowing the stack.  This function is only ever
                        // run by one thread so it should be safe.
                        static SymbolLookup symbolLookup;
                        String threadListOutput, moduleListOutput;
                        symbolLookup.ReportThreadCallstacks(threadListOutput);
                        symbolLookup.ReportModuleInformation(moduleListOutput);
                        LogText("---DEADLOCK STATE---\n\n%s\n\n%s\n---END OF DEADLOCK STATE---\n", threadListOutput.ToCStr(), moduleListOutput.ToCStr());
                    }

                    if (IsReporting)
                    {
                        ExceptionHandler::ReportDeadlock(DogList[i]->ThreadName, OrganizationName , ApplicationName);

                        // Disable reporting after the first deadlock report.
                        IsReporting = false;
                    }
                }

                if (IsExitingOnDeadlock())
                {
                    OVR_ASSERT_M(false, "Watchdog detected a deadlock. Exiting the process."); // This won't have an effect unless asserts are enabled in release builds.
                    OVR::ExitProcess(-1);
                }
            }
        }
    }

    OVR_DEBUG_LOG(("[WatchDogObserver] Good night"));

    return 0;
}

void WatchDogObserver::Add(WatchDog *dog)
{
    Lock::Locker locker(&ListLock);

    if (!dog->Listed)
    {
        DogList.PushBack(dog);
        dog->Listed = true;
    }
}

void WatchDogObserver::Remove(WatchDog *dog)
{
    Lock::Locker locker(&ListLock);

    if (dog->Listed)
    {
        for (int i = 0; i < DogList.GetSizeI(); ++i)
        {
            if (DogList[i] == dog)
            {
                DogList.RemoveAt(i--);
            }
        }

        dog->Listed = false;
    }
}

void WatchDogObserver::EnableReporting(const String organization, const String application)
{
    OrganizationName = organization;
    ApplicationName = application;
    IsReporting = true;
}

void WatchDogObserver::DisableReporting()
{
    IsReporting = false;
}


//-----------------------------------------------------------------------------
// WatchDog

WatchDog::WatchDog(const String& threadName) :
    ThreshholdMilliseconds(DefaultThreshhold),
    ThreadName(threadName),
    Listed(false)
{
    WhenLastFedMilliseconds = GetFastMsTime();
}

WatchDog::~WatchDog()
{
    Disable();
}

void WatchDog::Disable()
{
    WatchDogObserver::GetInstance()->Remove(this);
}

void WatchDog::Enable()
{
    WatchDogObserver::GetInstance()->Add(this);
}

void WatchDog::Feed(int threshold)
{
    WhenLastFedMilliseconds = GetFastMsTime();
    ThreshholdMilliseconds = threshold;

    if (!Listed)
    {
        Enable();
    }
}

}} // namespace OVR::Util
