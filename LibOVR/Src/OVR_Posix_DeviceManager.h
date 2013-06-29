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

namespace OVR {
namespace Posix {

class DeviceManager: public DeviceManagerImpl
{
    friend class HIDDeviceManager;
public:
    typedef boost::asio::io_service Svc;
    typedef boost::shared_ptr<Svc> SvcPtr;
    typedef boost::asio::deadline_timer Timer;

    DeviceManager();
    virtual ~DeviceManager();

    virtual bool Initialize(DeviceBase* parent);
    virtual void Run();
    virtual void Shutdown();
    // Allow commands pushed on to the thread command queue to trigger work
    // for the io_service
    virtual void OnPushNonEmpty_Locked();
    // Allow commands pushed on to the thread command queue to trigger work
    // for the io_service
    virtual void OnPopEmpty_Locked();
    // Callback that executes the command and also checks if the command queue
    // is empty, and requeues the command handler for execution if it's not
    virtual void OnCommand();

    virtual ThreadCommandQueue* GetThreadQueue();
    virtual ThreadId GetThreadId() const;
    virtual DeviceEnumerator<> EnumerateDevicesEx(const DeviceEnumerationArgs& args);
    virtual bool GetDeviceInfo(DeviceInfo* info) const;

private:
    Svc & GetAsyncService();
    Timer & GetTimer();

    boost::mutex lock;
    SvcPtr svc;
    Svc::work work;
    Timer timer;
    boost::thread workerThread;
    volatile bool queuedCommands;
};

} }

#endif // OVR_Posix_DeviceManager_h
