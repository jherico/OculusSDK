/************************************************************************************

Filename    :   Util_LongPollThread.h
Content     :   Allows us to do all long polling tasks from a single thread to minimize deadlock risk
Created     :   June 30, 2013
Authors     :   Chris Taylor

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

#ifndef OVR_Util_LongPollThread_h
#define OVR_Util_LongPollThread_h

#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_Atomic.h"
#include "Kernel/OVR_Allocator.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_System.h"
#include "Kernel/OVR_Threads.h"
#include "Kernel/OVR_Callbacks.h"

namespace OVR { namespace Util {


//-----------------------------------------------------------------------------
// LongPollThread

// This thread runs long-polling subsystems that wake up every second or so
// The motivation is to reduce the number of threads that are running to minimize the risk of deadlock
class LongPollThread : public Thread, public SystemSingletonBase<LongPollThread>
{
    OVR_DECLARE_SINGLETON(LongPollThread);
    virtual void OnThreadDestroy();

public:
    typedef Delegate0<void> PollFunc;
    static const int WakeupInterval = 1000; // milliseconds

    void AddPollFunc(CallbackListener<PollFunc>* func);

    void Wake();

protected:
    CallbackEmitter<PollFunc> PollSubject;

    bool Terminated;
    Event WakeEvent;
    void fireTermination();

    virtual int Run();
};


}} // namespace OVR::Util

#endif // OVR_Util_LongPollThread_h
