/************************************************************************************

Filename    :   OVR_Win32_DeviceManager.cpp
Content     :   Win32 implementation of DeviceManager.
Created     :   September 21, 2012
Authors     :   Michael Antonov

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

#include "../OVR_Platform.h"
#include "OVR_Win32_DeviceManager.h"

// Sensor & HMD Factories
#include "OVR_SensorImpl.h"
#include "OVR_LatencyTestImpl.h"
#include "OVR_Win32_HMDDevice.h"
#include "OVR_Win32_DeviceStatus.h"
#include "OVR_Win32_HIDDevice.h"

#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_Std.h"
#include "Kernel/OVR_Log.h"

DWORD Debug_WaitedObjectCount = 0;

namespace OVR { namespace Platform {


//-------------------------------------------------------------------------------------
// ***** DeviceManager Thread 

Win32DeviceManagerThread::Win32DeviceManagerThread(DeviceManager* pdevMgr)
  : DeviceManagerThread(pdevMgr), hCommandEvent(0)
{    
    // Create a non-signaled manual-reset event.
    hCommandEvent = ::CreateEvent(0, TRUE, FALSE, 0);
    if (!hCommandEvent)
        return;

    // Must add event before starting.
    WaitNotifiers.PushBack(0);
    WaitHandles.PushBack(hCommandEvent);
    OVR_ASSERT(WaitNotifiers.GetSize() <= MAXIMUM_WAIT_OBJECTS);

    // Create device messages object.
    pStatusObject = *new DeviceStatus(this);
}

Win32DeviceManagerThread::~Win32DeviceManagerThread()
{
    // Remove overlapped event [0], after thread service exit.
    if (hCommandEvent)
    {
        // [0] is reserved for thread commands with notify of null, but we still
        // can use this function to remove it.
        for (UPInt i = 0; i < WaitNotifiers.GetSize(); i++)
        {
            if ((WaitNotifiers[i] == 0) && (WaitHandles[i] == hCommandEvent))
            {
                WaitNotifiers.RemoveAt(i);
                WaitHandles.RemoveAt(i);
            }
        }

        ::CloseHandle(hCommandEvent);
        hCommandEvent = 0;
    }
}

int Win32DeviceManagerThread::Run()
{
    ThreadCommand::PopBuffer command;

    SetThreadName("OVR::DeviceManagerThread");
    LogText("OVR::DeviceManagerThread - running (ThreadId=0x%X).\n", GetThreadId());
  
    if (!pStatusObject->Initialize())
    {
        LogText("OVR::DeviceManagerThread - failed to initialize MessageObject.\n");
    }

    while(!IsExiting())
    {
        // PopCommand will reset event on empty queue.
        if (PopCommand(&command))
        {
            command.Execute();
        }
        else
        {
            DWORD eventIndex = 0;
            do {
                UPInt numberOfWaitHandles = WaitHandles.GetSize();
                Debug_WaitedObjectCount = (DWORD)numberOfWaitHandles;

                DWORD waitMs = INFINITE;

                // If devices have time-dependent logic registered, get the longest wait
                // allowed based on current ticks.
                if (!TicksNotifiers.IsEmpty())
                {
                    double  timeSeconds = Timer::GetSeconds();
                    DWORD   waitAllowed;

                    for (UPInt j = 0; j < TicksNotifiers.GetSize(); j++)
                    {
                        waitAllowed = (DWORD)(TicksNotifiers[j]->OnTicks(timeSeconds) * Timer::MsPerSecond);
                        if (waitAllowed < waitMs)
                            waitMs = waitAllowed;
                    }
                }
          
                // Wait for event signals or window messages.
                eventIndex = MsgWaitForMultipleObjects((DWORD)numberOfWaitHandles, &WaitHandles[0], FALSE, waitMs, QS_ALLINPUT);

                if (eventIndex != WAIT_FAILED)
                {
                    if (eventIndex == WAIT_TIMEOUT)
                        continue;

                    // TBD: Does this ever apply?
                    OVR_ASSERT(eventIndex < WAIT_ABANDONED_0);

                    if (eventIndex == WAIT_OBJECT_0)
                    {
                        // Handle [0] services commands.
                        break;
                    }
                    else if (eventIndex == WAIT_OBJECT_0 + numberOfWaitHandles)
                    {
                        // Handle Windows messages.
                        pStatusObject->ProcessMessages();
                    }
                    else 
                    {
                        // Notify waiting device that its event is signaled.
                        unsigned i = eventIndex - WAIT_OBJECT_0; 
                        OVR_ASSERT(i < numberOfWaitHandles);
                        if (WaitNotifiers[i])                        
                            WaitNotifiers[i]->OnOverlappedEvent(WaitHandles[i]);
                    }
                }

            } while(eventIndex != WAIT_FAILED);
                    
        }
    }

    pStatusObject->ShutDown();

    LogText("OVR::DeviceManagerThread - exiting (ThreadId=0x%X).\n", GetThreadId());
    return 0;
}


bool Win32DeviceManagerThread::AddMessageNotifier(Notifier* notify)
{
    MessageNotifiers.PushBack(notify);
    return true;
}

bool Win32DeviceManagerThread::RemoveMessageNotifier(Notifier* notify)
{
    for (UPInt i = 0; i < MessageNotifiers.GetSize(); i++)
    {
        if (MessageNotifiers[i] == notify)
        {
            MessageNotifiers.RemoveAt(i);
            return true;
        }
    }
    return false;
}

bool Win32DeviceManagerThread::OnMessage(MessageType type, const String& devicePath)
{
    Notifier::DeviceMessageType notifierMessageType = Notifier::DeviceMessage_DeviceAdded;
    if (type == DeviceAdded)
    {
    }
    else if (type == DeviceRemoved)
    {
        notifierMessageType = Notifier::DeviceMessage_DeviceRemoved;
    }
    else
    {
        OVR_ASSERT(false);
    }

    bool error = false;
    bool deviceFound = false;
    for (UPInt i = 0; i < MessageNotifiers.GetSize(); i++)
    {
        if (MessageNotifiers[i] &&
            MessageNotifiers[i]->OnDeviceMessage(notifierMessageType, devicePath, &error))
        {
            // The notifier belonged to a device with the specified device name so we're done.
            deviceFound = true;
            break;
        }
    }
    if (type == DeviceAdded && !deviceFound)
    {
        Lock::Locker devMgrLock(&DevMgrLock);
        // a new device was connected. Go through all device factories and
        // try to detect the device using HIDDeviceDesc.
        HIDDeviceDesc devDesc;
        if (pDeviceMgr->GetHIDDeviceDesc(devicePath, &devDesc))
        {
            Lock::Locker deviceLock(pDeviceMgr->GetLock());
            DeviceFactory* factory = pDeviceMgr->Factories.GetFirst();
            while(!pDeviceMgr->Factories.IsNull(factory))
            {
                if (factory->DetectHIDDevice(pDeviceMgr, devDesc))
                {
                    deviceFound = true;
                    break;
                }
                factory = factory->pNext;
            }
        }
    }

    if (!deviceFound && strstr(devicePath.ToCStr(), "#OVR00"))
    {
        Ptr<DeviceManager> pmgr;
        {
            Lock::Locker devMgrLock(&DevMgrLock);
            pmgr = pDeviceMgr;
        }
        // HMD plugged/unplugged
        // This is not a final solution to enumerate HMD devices and get
        // a first available handle. This won't work with multiple rifts.
        // @TODO (!AB)
        pmgr->EnumerateDevices<HMDDevice>();
    }

    return !error;
}

void Win32DeviceManagerThread::DetachDeviceManager()
{
    Lock::Locker devMgrLock(&DevMgrLock);
    pDeviceMgr = NULL;
}

} } // namespace OVR::Platform

