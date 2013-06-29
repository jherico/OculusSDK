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

#include <linux/hidraw.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <boost/lexical_cast.hpp>


namespace OVR {
namespace Posix {

using namespace std;
using namespace boost;

typedef HidDeviceList::iterator HidDeviceItr;

HIDDeviceManager::HIDDeviceManager(DeviceManager* manager) :
        Manager(manager), Udev(udev_new(), ptr_fun(udev_unref)) {
    // List all the HID devices
    shared_ptr<udev_enumerate> enumerate(udev_enumerate_new(Udev.get()), ptr_fun(udev_enumerate_unref));
    udev_enumerate_add_match_subsystem(enumerate.get(), "hidraw");
    udev_enumerate_scan_devices(enumerate.get());

    udev_list_entry * entry;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate.get())) {
        string hid_path(udev_list_entry_get_name(entry));
        Devices.push_back(HidDevicePtr(new HIDDevice(*this, hid_path)));
    }
}

HIDDeviceManager::~HIDDeviceManager() {
    Devices.clear();
}

bool HIDDeviceManager::Enumerate(HIDEnumerateVisitor* enumVisitor) {
    for( HidDeviceItr it = Devices.begin(); it != Devices.end(); ++it) {
        HidDevicePtr device = *it;
        if (enumVisitor->MatchVendorProduct(device->DevDesc.VendorId, device->DevDesc.ProductId)) {
            enumVisitor->Visit(*(device.get()), device->DevDesc);
        }
    }
    return true;
}

OVR::HIDDevice* HIDDeviceManager::Open(const String& path) {
    printf(path);
    printf("\n");
    for( HidDeviceItr it = Devices.begin(); it != Devices.end(); ++it) {
        HidDevicePtr device = *it;
        if (String(device->Path.c_str()) == path && device->openDevice()) {
            return device.get();
        }
    }

    return NULL;
}

//-------------------------------------------------------------------------------------
// **** Posix::HIDDevice

template<typename T2, typename T1>
inline T2 lexical_cast2(const T1 &in) {
    T2 out;
    std::stringstream ss;
    ss << std::hex << in;
    ss >> out;
    return out;
}

HIDDevice::HIDDevice(HIDDeviceManager& manager, const std::string & path) :
        HIDManager(manager), Path(path), ReadRequested(false), FileDescriptor(-1) {
    udev_device * hid_dev = udev_device_new_from_syspath(manager.Udev.get(), path.c_str());
    shared_ptr<udev_device> usb_dev(udev_device_get_parent_with_subsystem_devtype(hid_dev, "usb", "usb_device"),
            ptr_fun(udev_device_unref));

    if (!usb_dev.get()) {
        throw "bad usb device";
    }

    DevDesc.Manufacturer = udev_device_get_sysattr_value(usb_dev.get(), "manufacturer");
    DevDesc.Product = udev_device_get_sysattr_value(usb_dev.get(), "product");
    DevDesc.SerialNumber = udev_device_get_sysattr_value(usb_dev.get(), "serial");
    string vendorId = string("0x") + string(udev_device_get_sysattr_value(usb_dev.get(), "idVendor"));
    string productId = string("0x") + string(udev_device_get_sysattr_value(usb_dev.get(), "idProduct"));
    DevDesc.VendorId = lexical_cast2<short>(vendorId);
    DevDesc.ProductId = lexical_cast2<short>(productId);
}

HIDDevice::~HIDDevice() {
}

//bool HIDDevice::open()
//{
//    //    HIDManager->Manager->pThread->AddTicksNotifier(this);
//    //    HIDManager->Manager->pThread->AddMessageNotifier(this);
//
//    DevDesc.Path = path;
//
//    if (!openDevice())
//    {
//        return false;
//    }
//
//    LogText("OVR::Posix::HIDDevice - Opened '%s'\n"
//        "                    Manufacturer:'%s'  Product:'%s'  Serial#:'%s'\n",
//        DevDesc.Path.ToCStr(),
//        DevDesc.Manufacturer.ToCStr(), DevDesc.Product.ToCStr(),
//        DevDesc.SerialNumber.ToCStr());
//
//    return true;
//}

bool HIDDevice::initInfo() {
    // Device must have been successfully opened.
    OVR_ASSERT(Device);

    return true;
}

bool HIDDevice::openDevice() {
    return true;
}

bool HIDDevice::initializeRead() {
    return true;
}

bool HIDDevice::processReadResult() {
    return false;
}

void HIDDevice::closeDevice() {
    close(FileDescriptor);
}

void HIDDevice::closeDeviceOnIOError() {
    LogText("OVR::Posix::HIDDevice - Lost connection to '%s'\n", DevDesc.Path.ToCStr());
    closeDevice();
}

bool HIDDevice::SetFeatureReport(UByte* data, UInt32 length) {
    int res = ioctl(FileDescriptor, HIDIOCSFEATURE(length), data);
    return res >= 0;
}

bool HIDDevice::GetFeatureReport(UByte* data, UInt32 length) {
    int res = ioctl(FileDescriptor, HIDIOCGFEATURE(length), data);
    return res >= 0;
}

void HIDDevice::OnOverlappedEvent(HANDLE hevent)
{
//    OVR_UNUSED(hevent);
//    OVR_ASSERT(hevent == ReadOverlapped.hEvent);
//
//    if (processReadResult())
//    {
//        // Proceed to read again.
//        initializeRead();
//    }
}

UInt64 HIDDevice::OnTicks(UInt64 ticksMks) {
    if (Handler) {
        return Handler->OnTicks(ticksMks);
    }

    return DeviceManagerThread::Notifier::OnTicks(ticksMks);
}

bool HIDDevice::OnDeviceMessage(DeviceMessageType messageType, const String& devicePath, bool* error) {

    // Is this the correct device?
    if (DevDesc.Path.CompareNoCase(devicePath) != 0) {
        return false;
    }

    if (messageType == DeviceMessage_DeviceAdded && FileDescriptor < 0) {
        // A closed device has been re-added. Try to reopen.
        if (!openDevice()) {
            LogError("OVR::Posix::HIDDevice - Failed to reopen a device '%s' that was re-added.\n",
                    devicePath.ToCStr());
            *error = true;
            return true;
        }

        LogText("OVR::Posix::HIDDevice - Reopened device '%s'\n", devicePath.ToCStr());
    }

    HIDHandler::HIDDeviceMessageType handlerMessageType = HIDHandler::HIDDeviceMessage_DeviceAdded;
    if (messageType == DeviceMessage_DeviceAdded) {
    } else if (messageType == DeviceMessage_DeviceRemoved) {
        handlerMessageType = HIDHandler::HIDDeviceMessage_DeviceRemoved;
    } else {
        OVR_ASSERT(0);
    }

    if (Handler) {
        Handler->OnDeviceMessage(handlerMessageType);
    }

    *error = false;
    return true;
}

//HIDDeviceManager* HIDDeviceManager::CreateInternal(Posix::DeviceManager* devManager) {
//    if (!System::IsInitialized()) {
//        // Use custom message, since Log is not yet installed.
//        OVR_DEBUG_STATEMENT(Log::GetDefaultLog()->
//                LogMessage(Log_Debug, "HIDDeviceManager::Create failed - OVR::System not initialized"); );
//        return 0;
//    }
//
//    Ptr<Posix::HIDDeviceManager> manager = *new Posix::HIDDeviceManager(devManager);
//
//    if (manager) {
//        if (manager->Initialize()) {
//            manager->AddRef();
//        } else {
//            manager.Clear();
//        }
//    }
//
//    return manager.GetPtr();
//}

} // namespace Posix

//-------------------------------------------------------------------------------------
// ***** Creation

// Creates a new HIDDeviceManager and initializes OVR.
HIDDeviceManager* HIDDeviceManager::Create() {
    OVR_ASSERT_LOG(false, ("Standalone mode not implemented yet."));

    if (!System::IsInitialized()) {
        // Use custom message, since Log is not yet installed.
        OVR_DEBUG_STATEMENT(Log::GetDefaultLog()->
                LogMessage(Log_Debug, "HIDDeviceManager::Create failed - OVR::System not initialized"); );
        return 0;
    }

    return new Posix::HIDDeviceManager(NULL);
}

} // namespace OVR

