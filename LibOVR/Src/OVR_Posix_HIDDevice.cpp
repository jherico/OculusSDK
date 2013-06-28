/************************************************************************************

Filename    :   OVR_Posix_HIDDevice.cpp
Content     :   Posix HID device implementation.
Created     :   February 22, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#include "OVR_Posix_HIDDevice.h"
#include "OVR_Posix_DeviceManager.h"

#include "Kernel/OVR_System.h"
#include "Kernel/OVR_Log.h"

namespace OVR { namespace Posix {

HIDDeviceManager::HIDDeviceManager(DeviceManager* manager)
 :  Manager(manager)
{
}

HIDDeviceManager::~HIDDeviceManager()
{
}

bool HIDDeviceManager::Initialize()
{
    return true;
}

void HIDDeviceManager::Shutdown()
{
    LogText("OVR::Posix::HIDDeviceManager - shutting down.\n");
}

bool HIDDeviceManager::Enumerate(HIDEnumerateVisitor* enumVisitor)
{
    return true;
}

bool HIDDeviceManager::GetHIDDeviceDesc(const String& path, HIDDeviceDesc* pdevDesc) const
{
    return true;
}

OVR::HIDDevice* HIDDeviceManager::Open(const String& path)
{
    Ptr<Posix::HIDDevice> device = *new Posix::HIDDevice(this);

    if (device->HIDInitialize(path))
    {
        device->AddRef();
        return device;
    }

    return NULL;
}

//-------------------------------------------------------------------------------------
// **** Posix::HIDDevice

HIDDevice::HIDDevice(HIDDeviceManager* manager)
 : HIDManager(manager), inMinimalMode(false), Device(0), ReadRequested(false)
{
}

HIDDevice::~HIDDevice()
{
    if (!inMinimalMode)
    {
        HIDShutdown();
    }
}

bool HIDDevice::HIDInitialize(const String& path)
{

    DevDesc.Path = path;

    if (!openDevice())
    {
        return false;
    }


//    HIDManager->Manager->pThread->AddTicksNotifier(this);
//    HIDManager->Manager->pThread->AddMessageNotifier(this);

    LogText("OVR::Posix::HIDDevice - Opened '%s'\n"
        "                    Manufacturer:'%s'  Product:'%s'  Serial#:'%s'\n",
        DevDesc.Path.ToCStr(),
        DevDesc.Manufacturer.ToCStr(), DevDesc.Product.ToCStr(),
        DevDesc.SerialNumber.ToCStr());

    return true;
}

bool HIDDevice::initInfo()
{
    // Device must have been successfully opened.
    OVR_ASSERT(Device);

    return true;
}

bool HIDDevice::openDevice()
{

    return true;
}

void HIDDevice::HIDShutdown()
{
    closeDevice();
    LogText("OVR::Posix::HIDDevice - Closed '%s'\n", DevDesc.Path.ToCStr());
}

bool HIDDevice::initializeRead()
{
    return true;
}

bool HIDDevice::processReadResult()
{
    return false;
}

void HIDDevice::closeDevice()
{
    Device = 0;
}

void HIDDevice::closeDeviceOnIOError()
{
    LogText("OVR::Posix::HIDDevice - Lost connection to '%s'\n", DevDesc.Path.ToCStr());
    closeDevice();
}

bool HIDDevice::SetFeatureReport(UByte* data, UInt32 length)
{
        return false;
}

bool HIDDevice::GetFeatureReport(UByte* data, UInt32 length)
{
        return false;
}

void HIDDevice::OnOverlappedEvent(HANDLE hevent)
{
    OVR_UNUSED(hevent);
    OVR_ASSERT(hevent == ReadOverlapped.hEvent);

    if (processReadResult())
    {
        // Proceed to read again.
        initializeRead();
    }
}

UInt64 HIDDevice::OnTicks(UInt64 ticksMks)
{
    if (Handler)
    {
        return Handler->OnTicks(ticksMks);
    }

    return DeviceManagerThread::Notifier::OnTicks(ticksMks);
}

bool HIDDevice::OnDeviceMessage(DeviceMessageType messageType,
								const String& devicePath,
								bool* error)
{

    // Is this the correct device?
    if (DevDesc.Path.CompareNoCase(devicePath) != 0)
    {
        return false;
    }

    if (messageType == DeviceMessage_DeviceAdded && !Device)
    {
        // A closed device has been re-added. Try to reopen.
        if (!openDevice())
        {
            LogError("OVR::Posix::HIDDevice - Failed to reopen a device '%s' that was re-added.\n", devicePath.ToCStr());
			*error = true;
            return true;
        }

        LogText("OVR::Posix::HIDDevice - Reopened device '%s'\n", devicePath.ToCStr());
    }

    HIDHandler::HIDDeviceMessageType handlerMessageType = HIDHandler::HIDDeviceMessage_DeviceAdded;
    if (messageType == DeviceMessage_DeviceAdded)
    {
    }
    else if (messageType == DeviceMessage_DeviceRemoved)
    {
        handlerMessageType = HIDHandler::HIDDeviceMessage_DeviceRemoved;
    }
    else
    {
        OVR_ASSERT(0);
    }

    if (Handler)
    {
        Handler->OnDeviceMessage(handlerMessageType);
    }

	*error = false;
    return true;
}

HIDDeviceManager* HIDDeviceManager::CreateInternal(Posix::DeviceManager* devManager)
{

    if (!System::IsInitialized())
    {
        // Use custom message, since Log is not yet installed.
        OVR_DEBUG_STATEMENT(Log::GetDefaultLog()->
            LogMessage(Log_Debug, "HIDDeviceManager::Create failed - OVR::System not initialized"); );
        return 0;
    }

    Ptr<Posix::HIDDeviceManager> manager = *new Posix::HIDDeviceManager(devManager);

    if (manager)
    {
        if (manager->Initialize())
        {
            manager->AddRef();
        }
        else
        {
            manager.Clear();
        }
    }

    return manager.GetPtr();
}

} // namespace Posix

//-------------------------------------------------------------------------------------
// ***** Creation

// Creates a new HIDDeviceManager and initializes OVR.
HIDDeviceManager* HIDDeviceManager::Create()
{
    OVR_ASSERT_LOG(false, ("Standalone mode not implemented yet."));

    if (!System::IsInitialized())
    {
        // Use custom message, since Log is not yet installed.
        OVR_DEBUG_STATEMENT(Log::GetDefaultLog()->
            LogMessage(Log_Debug, "HIDDeviceManager::Create failed - OVR::System not initialized"); );
        return 0;
    }

    Ptr<Posix::HIDDeviceManager> manager = *new Posix::HIDDeviceManager(NULL);

    if (manager)
    {
        if (manager->Initialize())
        {
            manager->AddRef();
        }
        else
        {
            manager.Clear();
        }
    }

    return manager.GetPtr();
}

} // namespace OVR
