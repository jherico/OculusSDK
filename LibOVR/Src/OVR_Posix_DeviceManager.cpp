/************************************************************************************

 Filename    :   OVR_Posix_DeviceManager.cpp
 Content     :   Posix implementation of DeviceManager.
 Created     :   September 21, 2012
 Authors     :   Brad Davis

 Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

 Use of this software is subject to the terms of the Oculus license
 agreement provided at the time of installation or download, or which
 otherwise accompanies this software in either electronic or hard copy form.

 *************************************************************************************/

#include "OVR_Posix_DeviceManager.h"
#include "OVR_SensorImpl.h"
#include "OVR_LatencyTestImpl.h"
#include "OVR_Posix_HMDDevice.h"
#include "OVR_Posix_HIDDevice.h"

#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_Std.h"
#include "Kernel/OVR_Log.h"
#include <unistd.h>

namespace OVR {
namespace Posix {

DeviceManager::DeviceManager() :
        svc(new Svc()), work(*svc), timer(*svc),
                workerThread(boost::bind(&DeviceManager::Run, this)),
                queuedCommands(false) {
    HidDeviceManager = new HIDDeviceManager(*this);
}

DeviceManager::~DeviceManager() {
    svc->stop();
    workerThread.join();
    // make sure Shutdown was called.
    OVR_ASSERT(svc->stopped());
}

void DeviceManager::Run() {
    while (!IsExiting() && !svc->stopped()) {
        svc->run();
    }
}

void DeviceManager::OnPushNonEmpty_Locked() {
    queuedCommands = true;
    svc->post(boost::bind(&DeviceManager::OnCommand, this));
}

void DeviceManager::OnPopEmpty_Locked() {
    queuedCommands = false;
}

void DeviceManager::OnCommand() {
    ThreadCommand::PopBuffer command;
    if (PopCommand(&command)) {
        command.Execute();
    }

    if (queuedCommands) {
        svc->post(boost::bind(&DeviceManager::OnCommand, this));
    }
}

bool DeviceManager::Initialize(DeviceBase*) {
    if (!DeviceManagerImpl::Initialize(0))
        return false;
    pCreateDesc->pDevice = this;
    LogText("OVR::DeviceManager - initialized.\n");
    return true;
}

void DeviceManager::Shutdown() {
    LogText("OVR::DeviceManager - shutting down.\n");
    // Set Manager shutdown marker variable; this prevents
    // any existing DeviceHandle objects from accessing device.
    pCreateDesc->pLock->pManager = 0;
    PushExitCommand(false);
    DeviceManagerImpl::Shutdown();
}

ThreadCommandQueue* DeviceManager::GetThreadQueue() {
    return this;
}

DeviceManager::Svc& DeviceManager::GetAsyncService() {
    return *svc;
}

DeviceManager::Timer& DeviceManager::GetTimer() {
    return timer;
}

ThreadId DeviceManager::GetThreadId() const {
    return (void*) -1;
}

bool DeviceManager::GetDeviceInfo(DeviceInfo* info) const {
    if ((info->InfoClassType != Device_Manager) && (info->InfoClassType != Device_None))
        return false;

    info->Type = Device_Manager;
    info->Version = 0;
    OVR_strcpy(info->ProductName, DeviceInfo::MaxNameLength, "DeviceManager");
    OVR_strcpy(info->Manufacturer, DeviceInfo::MaxNameLength, "Oculus VR, Inc.");
    return true;
}

DeviceEnumerator<> DeviceManager::EnumerateDevicesEx(const DeviceEnumerationArgs& args) {
    DeviceManager::EnumerateAllFactoryDevices();
    return DeviceManagerImpl::EnumerateDevicesEx(args);
}

} // namespace Posix

// Creates a new DeviceManager and initializes OVR.
DeviceManager* DeviceManager::Create() {
    if (!System::IsInitialized()) {
        // Use custom message, since Log is not yet installed.
        OVR_DEBUG_STATEMENT(Log::GetDefaultLog()->
                LogMessage(Log_Debug, "DeviceManager::Create failed - OVR::System not initialized"); );
        return 0;
    }

    Ptr<Posix::DeviceManager> manager = *new Posix::DeviceManager;
    if (manager) {
        if (manager->Initialize(0)) {
            manager->AddFactory(&SensorDeviceFactory::Instance);
            manager->AddFactory(&LatencyTestDeviceFactory::Instance);
            manager->AddFactory(&Posix::HMDDeviceFactory::Instance);
            manager->AddRef();
        } else {
            manager.Clear();
        }
    }
    return manager.GetPtr();
}

} // namespace OVR

