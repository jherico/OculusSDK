/************************************************************************************

Filename    :   Util_LongPollThread.cpp
Content     :   Allows us to do all long polling tasks from a single thread to minimize deadlock risk
Created     :   June 30, 2013
Authors     :   Chris Taylor

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

*************************************************************************************/

#include "Util_LongPollThread.h"
#include "Util_Watchdog.h"

OVR_DEFINE_SINGLETON(OVR::Util::LongPollThread);

namespace OVR { namespace Util {


void LongPollThread::AddPollFunc(Observer<PollFunc>* func)
{
    func->Observe(PollSubject);
}

LongPollThread::LongPollThread() :
    Terminated(false)
{
    Start();

    PushDestroyCallbacks();
}

LongPollThread::~LongPollThread()
{
    fireTermination();

    Join();
}

void LongPollThread::OnThreadDestroy()
{
    fireTermination();
}

void LongPollThread::Wake()
{
    WakeEvent.SetEvent();
}

void LongPollThread::fireTermination()
{
    Terminated = true;
    Wake();
}

void LongPollThread::OnSystemDestroy()
{
    Release();
}

int LongPollThread::Run()
{
    SetThreadName("LongPoll");
    WatchDog watchdog("LongPoll");

    // While not terminated,
    do
    {
        watchdog.Feed(10000);

        PollSubject->Call();

        WakeEvent.Wait(WakeupInterval);
        WakeEvent.ResetEvent();
    } while (!Terminated);

    return 0;
}


}} // namespace OVR::Util
