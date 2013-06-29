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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <boost/lexical_cast.hpp>
#include <fcntl.h>
#include <errno.h>

#include <boost/system/linux_error.hpp>


namespace OVR {
namespace Posix {

using namespace std;
using namespace boost;


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
    for( HidDeviceItr it = Devices.begin(); it != Devices.end(); ++it) {
        HidDevicePtr device = *it;
        if (device->DevDesc.Path == path && device->openDevice()) {
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
	HIDManager(manager), timer(manager.Manager->GetAsyncService()), readBuffer(21)
{
    udev_device * hid_dev = udev_device_new_from_syspath(manager.Udev.get(), path.c_str());
    DevDesc.Path = udev_device_get_devnode(hid_dev);

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
    DeviceManager::Svc & svc = GetAsyncService();
    fd = FdPtr(new boost::asio::posix::stream_descriptor(svc, open(DevDesc.Path, O_RDWR)));
}

HIDDevice::~HIDDevice() {
	fd->close();
}

bool HIDDevice::initInfo() {
    // Device must have been successfully opened.
    OVR_ASSERT(Device);

    return true;
}

DeviceManager::Svc & HIDDevice::GetAsyncService() {
	return HIDManager.Manager->GetAsyncService();
}


void HIDDevice::setKeepAlive(unsigned short milliseconds) {
	static unsigned char*IO_BUF = new unsigned char[16];
	/* Set Feature */
    memset(IO_BUF, 0, 5);
    IO_BUF[0] = 0x8; /* Report Number */
    memcpy(IO_BUF + 3, &milliseconds, 2);
    SetFeatureReport(IO_BUF, 5);
}

bool HIDDevice::openDevice() {
	onTimer(boost::system::error_code());
    initializeRead();
    return true;
}


void HIDDevice::onTimer(const boost::system::error_code& error) {
	GetAsyncService().post( boost::bind( &HIDDevice::setKeepAlive, this, 10000 ) );
    timer.expires_from_now( boost::posix_time::seconds( 3 ) );
	timer.async_wait( boost::bind( &HIDDevice::onTimer, this, _1));
}

void HIDDevice::initializeRead() {
	boost::asio::async_read(*fd, readBuffer,
		boost::bind(
			&HIDDevice::processReadResult, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
}

void HIDDevice::processReadResult(const boost::system::error_code& error, std::size_t length) {
	if (error) {
		closeDeviceOnIOError();
		return;
    }
	cout << "Got tracker data" << endl;

	if (length) {
		const unsigned char* p1 = boost::asio::buffer_cast<const unsigned char*>(readBuffer.data());
		readBuffer.consume(length);
		// We've got data.
		if (Handler) {
			Handler->OnInputReport(p1, length);
		}
	}
	initializeRead();
}

void HIDDevice::closeDevice() {
    fd->close();
}

void HIDDevice::closeDeviceOnIOError() {
    LogText("OVR::Posix::HIDDevice - Lost connection to '%s'\n", DevDesc.Path.ToCStr());
    closeDevice();
}

bool HIDDevice::SetFeatureReport(UByte* data, UInt32 length) {
    int res = ioctl(fd->native(), HIDIOCSFEATURE(length), data);
    return res >= 0;
}

bool HIDDevice::GetFeatureReport(UByte* data, UInt32 length) {
    int res = ioctl(fd->native(), HIDIOCGFEATURE(length), data);
    return res >= 0;
}

void HIDDevice::OnOverlappedEvent()
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

    return 0; // DeviceManagerThread::Notifier::OnTicks(ticksMks);
}

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

