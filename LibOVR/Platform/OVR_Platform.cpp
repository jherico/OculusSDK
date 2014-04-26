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

#include "OVR_Platform.h"

#if defined(OVR_OS_WIN32)
#include "Win32/OVR_Win32_HIDDevice.h"
#include "Win32/OVR_Win32_HMDDevice.h"
#elif defined(OVR_OS_LINUX)
#include "OVR_Linux_DeviceManager.h"
#include "OVR_Linux_HIDDevice.h"
#include "OVR_Linux_HMDDevice.h"
#elif defined(OVR_OS_OSX)
#include "OVR_OSX_DeviceManager.h"
#include "OVR_OSX_HIDDevice.h"
#include "OVR_OSX_HMDDevice.h"
#endif


// Sensor & HMD Factories
#include "OVR_LatencyTestImpl.h"
#include "OVR_SensorImpl.h"

#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_Std.h"
#include "Kernel/OVR_Log.h"

namespace OVR { namespace Platform {


//-------------------------------------------------------------------------------------
// **** Linux::DeviceManager

DeviceManager::DeviceManager()
{
  // Do this now that we know the thread's run loop.
  HidDeviceManager = *HIDDeviceManager::CreateInternal(this);
}

DeviceManager::~DeviceManager()
{
  // make sure Shutdown was called.
  OVR_ASSERT(!pThread);
}

bool DeviceManager::GetHIDDeviceDesc(const String& path, HIDDeviceDesc* pdevDesc) const
{
    if (GetHIDDeviceManager())
        return static_cast<HIDDeviceManager*>(GetHIDDeviceManager())->GetHIDDeviceDesc(path, pdevDesc);
    return false;
}

bool DeviceManager::Initialize(DeviceBase*)
{
    if (!DeviceManagerImpl::Initialize(0))
        return false;

    pThread = DeviceManagerThread::Create(this);
    if (!pThread || !pThread->Start())
        return false;

//    // Wait for the thread to be fully up and running.
//    pThread->StartupEvent.Wait();

    pCreateDesc->pDevice = this;
    LogText("OVR::DeviceManager - initialized.\n");
    return true;
}

void DeviceManager::Shutdown()
{
    LogText("OVR::DeviceManager - shutting down.\n");

    // Set Manager shutdown marker variable; this prevents
    // any existing DeviceHandle objects from accessing device.
    pCreateDesc->pLock->pManager = 0;

    // Push for thread shutdown *WITH NO WAIT*.
    // This will have the following effect:
    //  - Exit command will get enqueued, which will be executed later on the thread itself.
    //  - Beyond this point, this DeviceManager object may be deleted by our caller.
    //  - Other commands, such as CreateDevice, may execute before ExitCommand, but they will
    //    fail gracefully due to pLock->pManager == 0. Future commands can't be enqued
    //    after pManager is null.
    //  - Once ExitCommand executes, ThreadCommand::Run loop will exit and release the last
    //    reference to the thread object.
    pThread->PushExitCommand(false);
    pThread->DetachDeviceManager();
    pThread.Clear();

    DeviceManagerImpl::Shutdown();
}

ThreadCommandQueue* DeviceManager::GetThreadQueue()
{
    return pThread;
}

ThreadId DeviceManager::GetThreadId() const
{
    return pThread->GetThreadId();
}

bool DeviceManager::GetDeviceInfo(DeviceInfo* info) const
{
    if ((info->InfoClassType != Device_Manager) &&
        (info->InfoClassType != Device_None))
        return false;

    info->Type    = Device_Manager;
    info->Version = 0;
    info->ProductName = "DeviceManager";
    info->Manufacturer = "Oculus VR, Inc.";
    return true;
}

DeviceEnumerator<> DeviceManager::EnumerateDevicesEx(const DeviceEnumerationArgs& args)
{
    // TBD: Can this be avoided in the future, once proper device notification is in place?
    if (GetThreadId() != OVR::GetCurrentThreadId())
    {
        pThread->PushCall((DeviceManagerImpl*)this,
            &DeviceManager::EnumerateAllFactoryDevices, true);
    }
    else
        DeviceManager::EnumerateAllFactoryDevices();

    return DeviceManagerImpl::EnumerateDevicesEx(args);
}

bool DeviceManagerThread::AddTicksNotifier(Notifier* notify)
{
     TicksNotifiers.PushBack(notify);
     return true;
}

bool DeviceManagerThread::RemoveTicksNotifier(Notifier* notify)
{
    for (UPInt i = 0; i < TicksNotifiers.GetSize(); i++)
    {
        if (TicksNotifiers[i] == notify)
        {
            TicksNotifiers.RemoveAt(i);
            return true;
        }
    }
    return false;
}

} // namespace Platform


//-------------------------------------------------------------------------------------
// ***** Creation


// Creates a new DeviceManager and initializes OVR.
DeviceManager* DeviceManager::Create()
{
    if (!System::IsInitialized())
    {
        // Use custom message, since Log is not yet installed.
        OVR_DEBUG_STATEMENT(Log::GetDefaultLog()->
            LogMessage(Log_Debug, "DeviceManager::Create failed - OVR::System not initialized"); );
        return 0;
    }

    Ptr<Platform::DeviceManager> manager = *new Platform::DeviceManager;

    if (manager)
    {
        if (manager->Initialize(0))
        {
            manager->AddFactory(&LatencyTestDeviceFactory::Instance);
            manager->AddFactory(&SensorDeviceFactory::Instance);
            manager->AddFactory(&Platform::HMDDeviceFactory::Instance);

            manager->AddRef();
        }
        else
        {
            manager.Clear();
        }

    }

    return manager.GetPtr();
}


//-------------------------------------------------------------------------------------
void SensorDeviceImpl::EnumerateHMDFromSensorDisplayInfo
    (const SensorDisplayInfoImpl& displayInfo,
     DeviceFactory::EnumerateVisitor& visitor)
{

    Platform::HMDDeviceCreateDesc hmdCreateDesc(&Platform::HMDDeviceFactory::Instance, String(), String());
    hmdCreateDesc.SetScreenParameters(  0, 0,
                                        displayInfo.HResolution, displayInfo.VResolution,
                                        displayInfo.HScreenSize, displayInfo.VScreenSize,
                                        displayInfo.VCenter, displayInfo.LensSeparation);

    if ((displayInfo.DistortionType & SensorDisplayInfoImpl::Mask_BaseFmt) == SensorDisplayInfoImpl::Base_Distortion)
    {
        // TODO: update to spline system.
        hmdCreateDesc.SetDistortion(displayInfo.DistortionK);
    }

    visitor.Visit(hmdCreateDesc);
}

} // namespace OVR

