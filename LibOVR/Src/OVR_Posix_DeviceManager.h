/************************************************************************************

Filename    :   OVR_Posix_DeviceManager.h
Content     :   Posix-specific DeviceManager header.
Created     :   September 21, 2012
Authors     :   Michael Antonov

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#ifndef OVR_Posix_DeviceManager_h
#define OVR_Posix_DeviceManager_h

#include "OVR_DeviceImpl.h"
#include "OVR_Posix_DeviceStatus.h"
#define HANDLE long
#include "Kernel/OVR_Timer.h"
#include <boost/thread.hpp>

namespace OVR { namespace Posix {

class DeviceManagerThread;

//-------------------------------------------------------------------------------------
// ***** Posix DeviceManager

class DeviceManager : public DeviceManagerImpl
{
public:
    DeviceManager();
    ~DeviceManager();

    // Initialize/Shutdowncreate and shutdown manger thread.
    virtual bool Initialize(DeviceBase* parent);
    virtual void Shutdown();

    virtual ThreadCommandQueue* GetThreadQueue();
    virtual ThreadId GetThreadId() const;

    virtual DeviceEnumerator<> EnumerateDevicesEx(const DeviceEnumerationArgs& args);

    virtual bool  GetDeviceInfo(DeviceInfo* info) const;

    // Fills HIDDeviceDesc by using the path.
    // Returns 'true' if successful, 'false' otherwise.
    bool GetHIDDeviceDesc(const String& path, HIDDeviceDesc* pdevDesc) const;

    Ptr<DeviceManagerThread> pThread;
};

//-------------------------------------------------------------------------------------
// ***** Device Manager Background Thread

class DeviceManagerThread : public Thread, public ThreadCommandQueue, public DeviceStatus::Notifier
{
    friend class DeviceManager;
    enum { ThreadStackSize = 32 * 1024 };
public:
    DeviceManagerThread(DeviceManager* pdevMgr);
    virtual ~DeviceManagerThread();

    virtual int Run();

    // ThreadCommandQueue notifications for CommandEvent handling.
    virtual void OnPushNonEmpty_Locked() {
//        ::SetEvent(hCommandEvent);
    }
    virtual void OnPopEmpty_Locked()     {
//        ::ResetEvent(hCommandEvent);
    }


    // Notifier used for different updates (EVENT or regular timing or messages).
    class Notifier
    {
    public:
		// Called when overlapped I/O handle is signaled.
        virtual void    OnOverlappedEvent(HANDLE hevent) { OVR_UNUSED1(hevent); }

        // Called when timing ticks are updated.
        // Returns the largest number of microseconds this function can
        // wait till next call.
        virtual UInt64  OnTicks(UInt64 ticksMks)
        { OVR_UNUSED1(ticksMks);  return Timer::MksPerSecond * 1000; }

		enum DeviceMessageType
		{
			DeviceMessage_DeviceAdded     = 0,
			DeviceMessage_DeviceRemoved   = 1,
		};

		// Called to notify device object.
		virtual bool    OnDeviceMessage(DeviceMessageType messageType,
										const String& devicePath,
										bool* error)
        { OVR_UNUSED3(messageType, devicePath, error); return false; }
    };

    // DeviceStatus::Notifier interface.
	bool OnMessage(MessageType type, const String& devicePath);

    void DetachDeviceManager();

private:
    //boost::condition        signal;
    boost::mutex            lock;
    DeviceManager*          pDeviceMgr;

    // Event notifications for devices whose OVERLAPPED I/O we service.
    // This list is modified through AddDeviceOverlappedEvent.
    // WaitHandles[0] always == hCommandEvent, with null device.
    Array<HANDLE>           WaitHandles;
    Array<Notifier*>        WaitNotifiers;

    // Ticks notifiers - used for time-dependent events such as keep-alive.
    Array<Notifier*>        TicksNotifiers;

	// Message notifiers.
    Array<Notifier*>        MessageNotifiers;

	// Object that manages notifications originating from Windows messages.
	Ptr<DeviceStatus>		pStatusObject;
};

}} // namespace Posix::OVR

#endif // OVR_Posix_DeviceManager_h
