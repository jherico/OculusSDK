/************************************************************************************

Filename    :   OVR_Win32_DeviceManager.h
Content     :   Win32-specific DeviceManager header.
Created     :   September 21, 2012
Authors     :   Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.1 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.1

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#ifndef OVR_Win32_DeviceManager_h
#define OVR_Win32_DeviceManager_h

#include "../OVR_Platform.h"

namespace OVR { namespace Platform {

class Win32DeviceManagerThread : public DeviceManagerThread
{
    friend class DeviceManager;
    enum { ThreadStackSize = 32 * 1024 };
public:
    Win32DeviceManagerThread(DeviceManager* pdevMgr);
    ~Win32DeviceManagerThread();

    virtual int Run();

    // ThreadCommandQueue notifications for CommandEvent handling.
    virtual void OnPushNonEmpty_Locked() { ::SetEvent(hCommandEvent); }
    virtual void OnPopEmpty_Locked()     { ::ResetEvent(hCommandEvent); }

    // Adds device's OVERLAPPED structure for I/O.
    // After it's added, Overlapped object will be signaled if a message arrives.
    bool AddOverlappedEvent(Notifier* notify, HANDLE hevent);
    bool RemoveOverlappedEvent(Notifier* notify, HANDLE hevent);

    bool AddMessageNotifier(Notifier* notify);
    bool RemoveMessageNotifier(Notifier* notify);

    // DeviceStatus::Notifier interface.
    bool OnMessage(MessageType type, const String& devicePath);


private:
    void DetachDeviceManager();

    bool threadInitialized() { return hCommandEvent != 0; }

    // Event used to wake us up thread commands are enqueued.
    HANDLE                  hCommandEvent;

    // Event notifications for devices whose OVERLAPPED I/O we service.
    // This list is modified through AddDeviceOverlappedEvent.
    // WaitHandles[0] always == hCommandEvent, with null device.
    ArrayPOD<HANDLE>        WaitHandles;
    ArrayPOD<Notifier*>     WaitNotifiers;

    // Ticks notifiers - used for time-dependent events such as keep-alive.
    ArrayPOD<Notifier*>     TicksNotifiers;

    // Message notifiers.
    ArrayPOD<Notifier*>     MessageNotifiers;

    // Object that manages notifications originating from Windows messages.
    Ptr<DeviceStatus>       pStatusObject;

    Lock                    DevMgrLock;
};

}} // namespace Win32::OVR

#endif // OVR_Win32_DeviceManager_h
