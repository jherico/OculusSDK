/************************************************************************************

Filename    :   OVR_Linux_DeviceManager.h
Content     :   Linux implementation of DeviceManager.
Created     :
Authors     :

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Licensed under the Oculus VR SDK License Version 2.0 (the "License");
you may not use the Oculus VR SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#include "OVR_Linux_DeviceManager.h"

// Sensor & HMD Factories
#include "OVR_LatencyTestImpl.h"
#include "OVR_SensorImpl.h"
#include "OVR_Linux_HIDDevice.h"
#include "OVR_Linux_HMDDevice.h"

#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_Std.h"
#include "Kernel/OVR_Log.h"

namespace OVR { namespace Platform {



//-------------------------------------------------------------------------------------
// ***** DeviceManager Thread

DeviceManagerThread::DeviceManagerThread()
    : Thread(ThreadStackSize)
{
    int result = pipe(CommandFd);
    OVR_ASSERT(!result);

    AddSelectFd(NULL, CommandFd[0]);
}

DeviceManagerThread::~DeviceManagerThread()
{
    if (CommandFd[0])
    {
        RemoveSelectFd(NULL, CommandFd[0]);
        close(CommandFd[0]);
        close(CommandFd[1]);
    }
}

bool DeviceManagerThread::AddSelectFd(Notifier* notify, int fd)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN|POLLHUP|POLLERR;
    pfd.revents = 0;

    FdNotifiers.PushBack(notify);
    PollFds.PushBack(pfd);

    OVR_ASSERT(FdNotifiers.GetSize() == PollFds.GetSize());
    return true;
}

bool DeviceManagerThread::RemoveSelectFd(Notifier* notify, int fd)
{
    // [0] is reserved for thread commands with notify of null, but we still
    // can use this function to remove it.
    for (UPInt i = 0; i < FdNotifiers.GetSize(); i++)
    {
        if ((FdNotifiers[i] == notify) && (PollFds[i].fd == fd))
        {
            FdNotifiers.RemoveAt(i);
            PollFds.RemoveAt(i);
            return true;
        }
    }
    return false;
}



int DeviceManagerThread::Run()
{
    ThreadCommand::PopBuffer command;

    SetThreadName("OVR::DeviceManagerThread");
    LogText("OVR::DeviceManagerThread - running (ThreadId=%p).\n", GetThreadId());

    // Signal to the parent thread that initialization has finished.
    StartupEvent.SetEvent();

    while(!IsExiting())
    {
        // PopCommand will reset event on empty queue.
        if (PopCommand(&command))
        {
            command.Execute();
        }
        else
        {
            bool commands = 0;
            do
            {
                int waitMs = -1;

                // If devices have time-dependent logic registered, get the longest wait
                // allowed based on current ticks.
                if (!TicksNotifiers.IsEmpty())
                {
                    double ticksMks = Timer::GetSeconds();
                    int  waitAllowed;

                    for (UPInt j = 0; j < TicksNotifiers.GetSize(); j++)
                    {
                        waitAllowed = (int)(TicksNotifiers[j]->OnTicks(ticksMks) / Timer::MsPerSecond);
                        if (waitAllowed < waitMs)
                            waitMs = waitAllowed;
                    }
                }

                // wait until there is data available on one of the devices or the timeout expires
                int n = poll(&PollFds[0], PollFds.GetSize(), waitMs);

                if (n > 0)
                {
                    // Iterate backwards through the list so the ordering will not be
                    // affected if the called object gets removed during the callback
                    // Also, the HID data streams are located toward the back of the list
                    // and servicing them first will allow a disconnect to be handled
                    // and cleaned directly at the device first instead of the general HID monitor
                    for (int i=PollFds.GetSize()-1; i>=0; i--)
                    {
                        if (PollFds[i].revents & POLLERR)
                        {
                            OVR_DEBUG_LOG(("poll: error on [%d]: %d", i, PollFds[i].fd));
                        }
                        else if (PollFds[i].revents & POLLIN)
                        {
                            if (FdNotifiers[i])
                                FdNotifiers[i]->OnEvent(i, PollFds[i].fd);
                            else if (i == 0) // command
                            {
                                char dummy[128];
                                read(PollFds[i].fd, dummy, 128);
                                commands = 1;
                            }
                        }

                        if (PollFds[i].revents & POLLHUP)
                            PollFds[i].events = 0;

                        if (PollFds[i].revents != 0)
                        {
                            n--;
                            if (n == 0)
                                break;
                        }
                    }
                }
            } while (PollFds.GetSize() > 0 && !commands);
        }
    }

    LogText("OVR::DeviceManagerThread - exiting (ThreadId=%p).\n", GetThreadId());
    return 0;
}

} } // namespace OVR::Platform

