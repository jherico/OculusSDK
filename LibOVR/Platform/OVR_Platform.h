#pragma once

#include "OVR_DeviceImpl.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_RefCount.h"
#include "Kernel/OVR_Array.h"

namespace OVR { namespace Platform {

//-------------------------------------------------------------------------------------
// ***** DeviceStatus
//
// DeviceStatus abstracts the handling of windows messages of interest for
// example the WM_DEVICECHANGED message which occurs when a device is plugged/unplugged.
// The device manager thread creates an instance of this class and passes its pointer
// in the constructor. That thread is also responsible for periodically calling 'ProcessMessages'
// to process queued windows messages. The client is notified via the 'OnMessage' method
// declared in the 'DeviceMessages::Notifier' interface.
class DeviceStatus : public RefCountBase<DeviceStatus>
{
public:

    // Notifier used for device messages.
    class Notifier
    {
    public:
        enum MessageType
        {
            DeviceAdded     = 0,
            DeviceRemoved   = 1,
        };

        virtual bool OnMessage(MessageType type, const String& devicePath)
        { OVR_UNUSED2(type, devicePath); return true; }
    };

    DeviceStatus(Notifier* const pClient);
    virtual ~DeviceStatus();

    void operator = (const DeviceStatus&);  // No assignment implementation.

    bool Initialize();
    void ShutDown();

    void ProcessMessages();
protected:
    Notifier* const     pNotificationClient;    // Don't reference count a back-pointer.
};

class DeviceManagerThread;

//-------------------------------------------------------------------------------------
// ***** Win32 DeviceManager

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
protected:
    DeviceManagerThread(DeviceManager* pdevMgr);
public:
    static DeviceManagerThread * Create(DeviceManager* pdevMgr);
    ~DeviceManagerThread();


    virtual int Run() = 0;

    // ThreadCommandQueue notifications for CommandEvent handling.
    virtual void OnPushNonEmpty_Locked() = 0;
    virtual void OnPopEmpty_Locked() = 0;

    // Notifier used for different updates (EVENT or regular timing or messages).
    class Notifier
    {
    public:

        // Called when timing ticks are updated.
        // Returns the largest number of seconds this function can
        // wait till next call.
        virtual double  OnTicks(double tickSeconds)
        { OVR_UNUSED1(tickSeconds);  return 1000.0; }

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

    // Add notifier that will be called at regular intervals.
    bool AddTicksNotifier(Notifier* notify);
    bool RemoveTicksNotifier(Notifier* notify);

    void DetachDeviceManager();

protected:
    DeviceManager* pDeviceMgr;
};


class HIDDeviceManager : public OVR::HIDDeviceManager
{
  friend class HIDDevice;
public:

    HIDDeviceManager(DeviceManager* manager);
    virtual ~HIDDeviceManager();

    virtual bool Initialize()
    {
        return true;
    }

    virtual void Shutdown()
    {
        LogText("OVR::Win32::HIDDeviceManager - shutting down.\n");
    }

    virtual bool Enumerate(HIDEnumerateVisitor* enumVisitor) = 0;
    virtual OVR::HIDDevice* Open(const String& path) = 0;

    // Fills HIDDeviceDesc by using the path.
    // Returns 'true' if successful, 'false' otherwise.
    bool GetHIDDeviceDesc(const String& path, HIDDeviceDesc* pdevDesc) const;

    static HIDDeviceManager* CreateInternal(DeviceManager* manager);

protected:

    DeviceManager* Manager;     // Back pointer can just be a raw pointer.
};


//-------------------------------------------------------------------------------------
// ***** Win32 HIDDevice

class HIDDevice : public OVR::HIDDevice, public DeviceManagerThread::Notifier
{
public:

  HIDDevice(HIDDeviceManager* manager, bool inMinimalMode = false)
    : HIDManager(manager), InMinimalMode(inMinimalMode) {

  }
  virtual ~HIDDevice();

  bool HIDInitialize(const String& path);
  void HIDShutdown();

  // OVR::HIDDevice
  virtual bool SetFeatureReport(UByte* data, UInt32 length) = 0;
  virtual bool GetFeatureReport(UByte* data, UInt32 length) = 0;

  // DeviceManagerThread::Notifier
  virtual void OnReadReadyEvent() = 0;
  double OnTicks(double tickSeconds) {
    if (Handler)
    {
      return Handler->OnTicks(tickSeconds);
    }

    return DeviceManagerThread::Notifier::OnTicks(tickSeconds);
  }
  bool OnDeviceMessage(DeviceMessageType messageType, const String& devicePath, bool* error);

protected:
  bool                InMinimalMode;
  HIDDeviceManager*   HIDManager;
  enum { ReadBufferSize = 96 };
  UByte               ReadBuffer[ReadBufferSize];

  UInt16              InputReportBufferLength;
  UInt16              OutputReportBufferLength;
  UInt16              FeatureReportBufferLength;

};

}} // namespace OVR::Win32
