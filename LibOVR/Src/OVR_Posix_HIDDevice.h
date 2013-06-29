/************************************************************************************

 Filename    :   OVR_Posix_HIDDevice.h
 Content     :   Posix HID device implementation.
 Created     :   February 22, 2013
 Authors     :   Lee Cooper

 Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

 Use of this software is subject to the terms of the Oculus license
 agreement provided at the time of installation or download, or which
 otherwise accompanies this software in either electronic or hard copy form.

 *************************************************************************************/

#ifndef OVR_Posix_HIDDevice_h
#define OVR_Posix_HIDDevice_h

#include "OVR_HIDDevice.h"
#include "OVR_Posix_DeviceManager.h"
#include <map>
#include <list>
#include <string>
#include <libudev.h>
#include <boost/shared_ptr.hpp>

namespace OVR {
namespace Posix {

class HIDDeviceManager;
class DeviceManager;


class HIDDevice : public OVR::HIDDevice {
	typedef boost::asio::posix::stream_descriptor 	Fd;
	typedef boost::shared_ptr<Fd> 					FdPtr;
	typedef boost::asio::deadline_timer 			Timer;
	typedef boost::asio::streambuf					Buffer;
	typedef boost::shared_ptr<Timer> 				TimerPtr;

public:
    HIDDevice(HIDDeviceManager& manager, const std::string & path);
    virtual ~HIDDevice();

    // OVR::HIDDevice
    bool SetFeatureReport(UByte* data, UInt32 length);
    bool GetFeatureReport(UByte* data, UInt32 length);

    // DeviceManagerThread::Notifier
    void OnOverlappedEvent();
    UInt64 OnTicks(UInt64 ticksMks);
//    bool OnDeviceMessage(DeviceMessageType messageType, const String& devicePath, bool* error);

private:
    friend class HIDDeviceManager;
    bool openDevice();
    bool initInfo();
    void onTimer(const boost::system::error_code& error);
    void initializeRead();
    void processReadResult(const boost::system::error_code& error, std::size_t length);
    void closeDevice();
    void closeDeviceOnIOError();
    void setKeepAlive(unsigned short milliseconds);

    DeviceManager::Svc & GetAsyncService();

    FdPtr				fd;
    HIDDeviceManager& 	HIDManager;
    HIDDeviceDesc 		DevDesc;
	Buffer 				readBuffer;
	Timer	 			timer;
};

//-------------------------------------------------------------------------------------
// ***** Posix HIDDeviceManager

typedef boost::shared_ptr<HIDDevice> HidDevicePtr;
typedef std::list<HidDevicePtr> HidDeviceList;
typedef boost::shared_ptr<udev> UdevPtr;
typedef HidDeviceList::iterator HidDeviceItr;


class HIDDeviceManager: public OVR::HIDDeviceManager {
    friend class HIDDevice;
public:

    HIDDeviceManager(DeviceManager* manager);
    virtual ~HIDDeviceManager();
    virtual bool Enumerate(HIDEnumerateVisitor* enumVisitor);
    virtual OVR::HIDDevice* Open(const String& path);
private:
    DeviceManager*      Manager;
    UdevPtr             Udev;
    HidDeviceList       Devices;
};

}
} // namespace OVR::Posix

#endif // OVR_Posix_HIDDevice_h
