/************************************************************************************

Filename    :   OVR_Posix_DeviceManager.h
Content     :   Posix-specific DeviceManager header.
Authors     :   Brad Davis

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#ifndef OVR_Posix_DeviceManager_h
#define OVR_Posix_DeviceManager_h

#include "OVR_DeviceImpl.h"
#include "Kernel/OVR_Timer.h"
#include <boost/thread.hpp>
#include <boost/asio.hpp>

namespace OVR { namespace Posix {

class DeviceManager : public DeviceManagerImpl
{
public:
    typedef boost::asio::io_service	Svc;
    typedef boost::shared_ptr<Svc> SvcPtr;

    DeviceManager();
    ~DeviceManager();

    // Initialize/Shutdowncreate and shutdown manger thread.
    virtual bool Initialize(DeviceBase* parent);
    void Run();
    virtual void Shutdown();
    virtual void OnPushNonEmpty_Locked();
    virtual void OnPopEmpty_Locked();
    void OnCommand();
    Svc & GetAsyncService();

    virtual ThreadCommandQueue* GetThreadQueue();
    virtual ThreadId GetThreadId() const;
    virtual DeviceEnumerator<> EnumerateDevicesEx(const DeviceEnumerationArgs& args);
    virtual bool GetDeviceInfo(DeviceInfo* info) const;

private:
    boost::mutex            	lock;
    SvcPtr						svc;
    Svc::work 					work;
	boost::thread				workerThread;
	bool						queuedCommands;
};

}}

#endif // OVR_Posix_DeviceManager_h
