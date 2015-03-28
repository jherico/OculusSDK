/************************************************************************************

Filename    :   OVR_System.cpp
Content     :   General kernel initialization/cleanup, including that
                of the memory allocator.
Created     :   September 19, 2012
Notes       : 

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

#include "OVR_System.h"
#include "OVR_Threads.h"
#include "OVR_Timer.h"
#include "OVR_DebugHelp.h"

#ifdef OVR_OS_MS
    #pragma warning(push, 0)
    #include "OVR_Win32_IncludeWindows.h" // GetModuleHandleEx
    #pragma warning(pop)
#endif

namespace OVR {


//-----------------------------------------------------------------------------
// Initialization/Shutdown Callbacks

static SystemSingletonInternal *SystemShutdownListenerList = nullptr;  // Points to the most recent SystemSingletonInternal added to the list.

static Lock& GetSSILock()
{                           // Put listLock in a function so that it can be constructed on-demand.
    static Lock listLock;   // Will construct on the first usage. However, the guarding of this construction is not thread-safe 
    return listLock;        // under all compilers. However, since we are initially calling this on startup before other threads
}                           // could possibly exist, the first usage of this will be before other threads exist.

void SystemSingletonInternal::RegisterDestroyCallback()
{
    Lock::Locker locker(&GetSSILock());

    // Insert the listener at the front of the list (top of the stack). This is an analogue of a C++ forward_list::push_front or stack::push.
    NextShutdownSingleton = SystemShutdownListenerList;
    SystemShutdownListenerList = this;
}


//-----------------------------------------------------------------------------
// System

// Initializes System core, installing allocator.
void System::Init(Log* log, Allocator *palloc)
{    
    if (!Allocator::GetInstance())
    {
        if (Allocator::IsTrackingLeaks())
        {
            SymbolLookup::Initialize();
        }

        Log::SetGlobalLog(log);
        Timer::initializeTimerSystem();
        Allocator::setInstance(palloc);
    }
    else
    {
        OVR_DEBUG_LOG(("[System] Init failed - duplicate call."));
    }
}

void System::Destroy()
{    
    if (Allocator::GetInstance())
    {
        // Invoke all of the post-finish callbacks (normal case)
        for (SystemSingletonInternal *listener = SystemShutdownListenerList; listener; listener = listener->NextShutdownSingleton)
        {
            listener->OnThreadDestroy();
        }

        #ifdef OVR_ENABLE_THREADS
            // Wait for all threads to finish; this must be done so that memory
            // allocator and all destructors finalize correctly.
            Thread::FinishAllThreads();
        #endif

        // Invoke all of the post-finish callbacks (normal case)
        for (SystemSingletonInternal* next, *listener = SystemShutdownListenerList; listener; listener = next)
        {
            next = listener->NextShutdownSingleton;

            listener->OnSystemDestroy();
        }

        SystemShutdownListenerList = nullptr;

        // Shutdown heap and destroy SysAlloc singleton, if any.
        Allocator::GetInstance()->onSystemShutdown();
        Allocator::setInstance(nullptr);

        if (Allocator::IsTrackingLeaks())
        {
            SymbolLookup::Shutdown();
        }

        Timer::shutdownTimerSystem();
        Log::SetGlobalLog(Log::GetDefaultLog());

        if (Allocator::IsTrackingLeaks())
        {
            int ovrLeakCount = DumpMemory(); 

            OVR_ASSERT(ovrLeakCount == 0);
            if (ovrLeakCount == 0)
            {
                OVR_DEBUG_LOG(("[System] No OVR object leaks detected."));
            }
        }
    }
    else
    {
        OVR_DEBUG_LOG(("[System] Destroy failed - System not initialized."));
    }
}

// Returns 'true' if system was properly initialized.
bool System::IsInitialized()
{
    return Allocator::GetInstance() != nullptr;
}


} // namespace OVR
