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


namespace OVR { namespace Posix {

class HIDDeviceManager;
class DeviceManager;

//-------------------------------------------------------------------------------------
// ***** Posix HIDDevice

class HIDDevice : public OVR::HIDDevice, public DeviceManagerThread::Notifier
{
public:

    HIDDevice(HIDDeviceManager* manager);

    ~HIDDevice();

    bool HIDInitialize(const String& path);
    void HIDShutdown();

    // OVR::HIDDevice
	bool SetFeatureReport(UByte* data, UInt32 length);
	bool GetFeatureReport(UByte* data, UInt32 length);


    // DeviceManagerThread::Notifier
    void OnOverlappedEvent(HANDLE hevent);
    UInt64 OnTicks(UInt64 ticksMks);
    bool OnDeviceMessage(DeviceMessageType messageType, const String& devicePath, bool* error);

private:
    bool openDevice();
    bool initInfo();
    bool initializeRead();
    bool processReadResult();
    void closeDevice();
    void closeDeviceOnIOError();

    bool                inMinimalMode;
    HIDDeviceManager*   HIDManager;
	HANDLE              Device;
    HIDDeviceDesc       DevDesc;

    //OVERLAPPED          ReadOverlapped;
    bool                ReadRequested;

    enum { ReadBufferSize = 96 };
    UByte               ReadBuffer[ReadBufferSize];

    UInt16              InputReportBufferLength;
    UInt16              OutputReportBufferLength;
    UInt16              FeatureReportBufferLength;
};

//-------------------------------------------------------------------------------------
// ***** Posix HIDDeviceManager

class HIDDeviceManager : public OVR::HIDDeviceManager
{
	friend class HIDDevice;
public:

	HIDDeviceManager(DeviceManager* manager);
    virtual ~HIDDeviceManager();

    virtual bool Initialize();
    virtual void Shutdown();

    virtual bool Enumerate(HIDEnumerateVisitor* enumVisitor);
    virtual OVR::HIDDevice* Open(const String& path);

    // Fills HIDDeviceDesc by using the path.
    // Returns 'true' if successful, 'false' otherwise.
    bool GetHIDDeviceDesc(const String& path, HIDDeviceDesc* pdevDesc) const;

    //GUID GetHIDGuid() { return HidGuid; }

    static HIDDeviceManager* CreateInternal(DeviceManager* manager);

private:

    DeviceManager* Manager;     // Back pointer can just be a raw pointer.
};

}} // namespace OVR::Posix

#endif // OVR_Posix_HIDDevice_h
