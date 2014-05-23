/************************************************************************************

Filename    :   OVR_HIDDeviceImpl.h
Content     :   Implementation of HIDDevice.
Created     :   March 7, 2013
Authors     :   Lee Cooper

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

#ifndef OVR_HIDDeviceImpl_h
#define OVR_HIDDeviceImpl_h

//#include "OVR_Device.h"
#include "OVR_DeviceImpl.h"

namespace OVR {

//-------------------------------------------------------------------------------------
class HIDDeviceCreateDesc : public DeviceCreateDesc
{
public:
    HIDDeviceCreateDesc(DeviceFactory* factory, DeviceType type, const HIDDeviceDesc& hidDesc)
        : DeviceCreateDesc(factory, type), HIDDesc(hidDesc) { }
    HIDDeviceCreateDesc(const HIDDeviceCreateDesc& other)
        : DeviceCreateDesc(other.pFactory, other.Type), HIDDesc(other.HIDDesc) { }

    virtual bool MatchDevice(const String& path)
    {
        // should it be case insensitive?
        return HIDDesc.Path.CompareNoCase(path) == 0;
    }

    HIDDeviceDesc HIDDesc;
};

//-------------------------------------------------------------------------------------
template<class B>
class HIDDeviceImpl : public DeviceImpl<B>, public HIDDevice::HIDHandler
{
public:
    HIDDeviceImpl(HIDDeviceCreateDesc* createDesc, DeviceBase* parent)
     :  DeviceImpl<B>(createDesc, parent)        
    {
    }

    // HIDDevice::Handler interface.
    virtual void OnDeviceMessage(HIDDeviceMessageType messageType)
    {
        MessageType handlerMessageType;
        switch (messageType) {
            case HIDDeviceMessage_DeviceAdded:
                handlerMessageType           = Message_DeviceAdded;
                DeviceImpl<B>::ConnectedFlag = true;
                break;

            case HIDDeviceMessage_DeviceRemoved:
                handlerMessageType           = Message_DeviceRemoved;
                DeviceImpl<B>::ConnectedFlag = false;
                break;

            default: OVR_ASSERT(0); return;
        }

        // Do device notification.
        MessageDeviceStatus status(handlerMessageType, this, OVR::DeviceHandle(this->pCreateDesc));
        this->HandlerRef.Call(status);

        // Do device manager notification.
        DeviceManagerImpl*   manager = this->GetManagerImpl();
        switch (handlerMessageType) {
            case Message_DeviceAdded:
                manager->CallOnDeviceAdded(this->pCreateDesc);
                break;
                
            case Message_DeviceRemoved:
                manager->CallOnDeviceRemoved(this->pCreateDesc);
                break;
                
            default:;
        }
    }

    virtual bool Initialize(DeviceBase* parent)
    {
        // Open HID device.
        HIDDeviceDesc&		hidDesc = *getHIDDesc();
        HIDDeviceManager*   pManager = GetHIDDeviceManager();


        HIDDevice* device = pManager->Open(hidDesc.Path);
        if (!device)
        {
            return false;
        }

        InternalDevice = *device;
        InternalDevice->SetHandler(this);

        // AddRef() to parent, forcing chain to stay alive.
        DeviceImpl<B>::pParent = parent;

        return true;
    }

    virtual void Shutdown()
    {   
        InternalDevice->SetHandler(NULL);

        DeviceImpl<B>::pParent.Clear();
    }

    DeviceManager* GetDeviceManager()
    {
        return DeviceImpl<B>::pCreateDesc->GetManagerImpl();
    }

    HIDDeviceManager* GetHIDDeviceManager()
    {
        return DeviceImpl<B>::pCreateDesc->GetManagerImpl()->GetHIDDeviceManager();
    }

    bool SetFeatureReport(UByte* data, UInt32 length)
    { 
        // Push call with wait.
        bool result = false;

		ThreadCommandQueue* pQueue = this->GetManagerImpl()->GetThreadQueue();
        if (!pQueue->PushCallAndWaitResult(this, &HIDDeviceImpl::setFeatureReport, &result, data, length))
            return false;

        return result;
    }

    bool setFeatureReport(UByte* data, UInt32 length)
    {
        return InternalDevice->SetFeatureReport(data, length);
    }

    bool GetFeatureReport(UByte* data, UInt32 length)
    { 
        bool result = false;

		ThreadCommandQueue* pQueue = this->GetManagerImpl()->GetThreadQueue();
        if (!pQueue->PushCallAndWaitResult(this, &HIDDeviceImpl::getFeatureReport, &result, data, length))
            return false;

        return result;
    }

    bool getFeatureReport(UByte* data, UInt32 length)
    {
        return InternalDevice->GetFeatureReport(data, length);
    }

	UByte GetDeviceInterfaceVersion()
	{
		UInt16 versionNumber = getHIDDesc()->VersionNumber;

		// Our interface and hardware versions are represented as two BCD digits each.
		// Interface version is in the last two digits.
		UByte interfaceVersion = (UByte)	((versionNumber & 0x000F) >> 0) * 1 +
											((versionNumber & 0x00F0) >> 4) * 10;
		return interfaceVersion;
	}

protected:
    HIDDevice* GetInternalDevice() const
    {
        return InternalDevice;
    }

    HIDDeviceDesc* getHIDDesc() const
    { return &getCreateDesc()->HIDDesc; }

    HIDDeviceCreateDesc* getCreateDesc() const
    { return (HIDDeviceCreateDesc*) &(*DeviceImpl<B>::pCreateDesc); }

private:
    Ptr<HIDDevice> InternalDevice;
};

} // namespace OVR

#endif
