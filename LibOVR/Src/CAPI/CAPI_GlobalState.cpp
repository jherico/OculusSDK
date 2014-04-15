/************************************************************************************

Filename    :   CAPI_GlobalState.cpp
Content     :   Maintains global state of the CAPI
Created     :   January 24, 2014
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

************************************************************************************/

#include "CAPI_GlobalState.h"

namespace OVR { namespace CAPI {


//-------------------------------------------------------------------------------------
// Open Questions / Notes

// 2. Detect HMDs.
// Challenge: If we do everything through polling, it would imply we want all the devices
//            initialized. However, there may be multiple rifts, extra sensors, etc,
//            which shouldn't be allocated.
// 

// How do you reset orientation Quaternion?
// Can you change IPD?



//-------------------------------------------------------------------------------------
// ***** OVRGlobalState

// Global instance
GlobalState* GlobalState::pInstance = 0;


GlobalState::GlobalState()
{
    pManager = *DeviceManager::Create();
    // Handle the DeviceManager's messages
    pManager->AddMessageHandler( this );
    EnumerateDevices();

    // PhoneSensors::Init();
}

GlobalState::~GlobalState()
{
    RemoveHandlerFromDevices();
    OVR_ASSERT(HMDs.IsEmpty());
}

int GlobalState::EnumerateDevices()
{
    // Need to use separate lock for device enumeration, as pManager->GetHandlerLock()
    // would produce deadlocks here.
    Lock::Locker lock(&EnumerationLock);
    
    EnumeratedDevices.Clear();        

    DeviceEnumerator<HMDDevice> e = pManager->EnumerateDevices<HMDDevice>();
    while(e.IsAvailable())
    {
        EnumeratedDevices.PushBack(DeviceHandle(e));       
        e.Next();
    }

    return (int)EnumeratedDevices.GetSize();
}


HMDDevice* GlobalState::CreateDevice(int index)
{
    Lock::Locker lock(&EnumerationLock);

    if (index >= (int)EnumeratedDevices.GetSize())
        return 0;
    return EnumeratedDevices[index].CreateDeviceTyped<HMDDevice>();
}


void GlobalState::AddHMD(HMDState* hmd)
{
    Lock::Locker lock(pManager->GetHandlerLock());
    HMDs.PushBack(hmd);
}
void GlobalState::RemoveHMD(HMDState* hmd)
{
    Lock::Locker lock(pManager->GetHandlerLock());
    hmd->RemoveNode();
}

void GlobalState::NotifyHMDs_AddDevice(DeviceType deviceType)
{
    Lock::Locker lock(pManager->GetHandlerLock());
    for(HMDState* hmd = HMDs.GetFirst(); !HMDs.IsNull(hmd); hmd = hmd->pNext)        
        hmd->NotifyAddDevice(deviceType);
}

void GlobalState::OnMessage(const Message& msg)
{
    if (msg.Type == Message_DeviceAdded || msg.Type == Message_DeviceRemoved)
    {       
        if (msg.pDevice == pManager)
        {   
            const MessageDeviceStatus& statusMsg =
                static_cast<const MessageDeviceStatus&>(msg);

            if (msg.Type == Message_DeviceAdded)
            {
                //LogText("OnMessage DeviceAdded.\n");

                // We may have added a sensor/other device; notify any HMDs that might
                // need it to check for it later.
                NotifyHMDs_AddDevice(statusMsg.Handle.GetType());
            }
            else
            {
                //LogText("OnMessage DeviceRemoved.\n");
            }
        }
    }
}


}} // namespace OVR::CAPI
