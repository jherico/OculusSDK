/************************************************************************************

Filename    :   OVR_Win32_HIDDevice.cpp
Content     :   Win32 HID device implementation.
Created     :   February 22, 2013
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

#include "OVR_Win32_HIDDevice.h"
#include "OVR_Win32_DeviceManager.h"

#include "Kernel/OVR_System.h"
#include "Kernel/OVR_Log.h"

HANDLE CreateHIDFile(const char* path, bool exclusiveAccess = true) {
  return ::CreateFileA(path, GENERIC_WRITE | GENERIC_READ,
    (!exclusiveAccess) ? (FILE_SHARE_READ | FILE_SHARE_WRITE) : 0x0,
    NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
}

namespace OVR { namespace Platform {

//-------------------------------------------------------------------------------------
// HIDDevicePathWrapper is a simple class used to extract HID device file path
// through SetupDiGetDeviceInterfaceDetail. We use a class since this is a bit messy.
class HIDDevicePathWrapper
{
    SP_INTERFACE_DEVICE_DETAIL_DATA_A* pData;
public:
    HIDDevicePathWrapper() : pData(0) { }
    ~HIDDevicePathWrapper() { if (pData) OVR_FREE(pData); }

    const char* GetPath() const { return pData ? pData->DevicePath : 0; }

    bool InitPathFromInterfaceData(HDEVINFO hdevInfoSet, SP_DEVICE_INTERFACE_DATA* pidata);
};

bool HIDDevicePathWrapper::InitPathFromInterfaceData(HDEVINFO hdevInfoSet, SP_DEVICE_INTERFACE_DATA* pidata)
{
    DWORD detailSize = 0;
    // SetupDiGetDeviceInterfaceDetailA returns "not enough buffer error code"
    // doe size request. Just check valid size.
    SetupDiGetDeviceInterfaceDetailA(hdevInfoSet, pidata, NULL, 0, &detailSize, NULL);
    if (!detailSize ||
        ((pData = (SP_INTERFACE_DEVICE_DETAIL_DATA_A*)OVR_ALLOC(detailSize)) == 0))
        return false;
    pData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA_A);

    if (!SetupDiGetDeviceInterfaceDetailA(hdevInfoSet, pidata, pData, detailSize, NULL, NULL))
        return false;
    return true;
}


Win32HIDDeviceManager * Win32HIDDevice::getHidManager() {
  return (Win32HIDDeviceManager*)HIDManager;
}


Win32DeviceManagerThread * Win32HIDDevice::getThread() {
  return (Win32DeviceManagerThread *)(getHidManager()->Manager->pThread);
}



//-------------------------------------------------------------------------------------
// **** Win32::DeviceManager

Win32HIDDeviceManager::Win32HIDDeviceManager(DeviceManager* manager)
  : HIDDeviceManager(manager)
{
    hHidLib = ::LoadLibraryA("hid.dll");
    OVR_ASSERT_LOG(hHidLib, ("Couldn't load Win32 'hid.dll'."));

    OVR_RESOLVE_HIDFUNC(HidD_GetHidGuid);
    OVR_RESOLVE_HIDFUNC(HidD_SetNumInputBuffers);
    OVR_RESOLVE_HIDFUNC(HidD_GetFeature);
    OVR_RESOLVE_HIDFUNC(HidD_SetFeature);
    OVR_RESOLVE_HIDFUNC(HidD_GetAttributes);
    OVR_RESOLVE_HIDFUNC(HidD_GetManufacturerString);
    OVR_RESOLVE_HIDFUNC(HidD_GetProductString);
    OVR_RESOLVE_HIDFUNC(HidD_GetSerialNumberString);
    OVR_RESOLVE_HIDFUNC(HidD_GetPreparsedData);   
    OVR_RESOLVE_HIDFUNC(HidD_FreePreparsedData);  
    OVR_RESOLVE_HIDFUNC(HidP_GetCaps);    

    if (HidD_GetHidGuid)
        HidD_GetHidGuid(&HidGuid);
}

Win32HIDDeviceManager::~Win32HIDDeviceManager()
{
    ::FreeLibrary(hHidLib);
}

bool Win32HIDDeviceManager::Enumerate(HIDEnumerateVisitor* enumVisitor)
{
    HDEVINFO                 hdevInfoSet;
    SP_DEVICE_INTERFACE_DATA interfaceData;
    interfaceData.cbSize = sizeof(interfaceData);

    // Get handle to info data set describing all available HIDs.
    hdevInfoSet = SetupDiGetClassDevsA(&HidGuid, NULL, NULL, DIGCF_INTERFACEDEVICE | DIGCF_PRESENT);
    if (hdevInfoSet == INVALID_HANDLE_VALUE)
        return false;

    for(int deviceIndex = 0;
        SetupDiEnumDeviceInterfaces(hdevInfoSet, NULL, &HidGuid, deviceIndex, &interfaceData);
        deviceIndex++)
    {
        // For each device, we extract its file path and open it to get attributes,
        // such as vendor and product id. If anything goes wrong, we move onto next device.
        HIDDevicePathWrapper pathWrapper;
        if (!pathWrapper.InitPathFromInterfaceData(hdevInfoSet, &interfaceData))
            continue;

        // Look for the device to check if it is already opened.
        Ptr<DeviceCreateDesc> existingDevice = Manager->FindDevice(pathWrapper.GetPath());
        // if device exists and it is opened then most likely the CreateHIDFile
        // will fail; therefore, we just set Enumerated to 'true' and continue.
        if (existingDevice && existingDevice->pDevice)
        {
            existingDevice->Enumerated = true;
            continue;
        }

        // open device in non-exclusive mode for detection...
        HANDLE hidDev = CreateHIDFile(pathWrapper.GetPath(), false);
        if (hidDev == INVALID_HANDLE_VALUE)
            continue;

        HIDDeviceDesc devDesc;
        devDesc.Path = pathWrapper.GetPath();
        if (initVendorProductVersion(hidDev, &devDesc) &&
            enumVisitor->MatchVendorProduct(devDesc.VendorId, devDesc.ProductId) &&
            initUsage(hidDev, &devDesc))
        {
            initStrings(hidDev, &devDesc);

            // Construct minimal device that the visitor callback can get feature reports from.
            Win32HIDDevice device(this, hidDev);
            enumVisitor->Visit(device, devDesc);
        }

        ::CloseHandle(hidDev);
    }

    SetupDiDestroyDeviceInfoList(hdevInfoSet);
    return true;
}

bool Win32HIDDeviceManager::GetHIDDeviceDesc(const String& path, HIDDeviceDesc* pdevDesc) const
{
    // open device in non-exclusive mode for detection...
    HANDLE hidDev = CreateHIDFile(path, false);
    if (hidDev == INVALID_HANDLE_VALUE)
        return false;

    pdevDesc->Path = path;
    bool succ = getFullDesc(hidDev, pdevDesc);

    ::CloseHandle(hidDev);
    return succ;
}

OVR::HIDDevice* Win32HIDDeviceManager::Open(const String& path)
{

  Ptr<HIDDevice> device = *new Win32HIDDevice(this);

    if (device->HIDInitialize(path))
    {
        device->AddRef();        
        return device;
    }

    return NULL;
}

bool Win32HIDDeviceManager::getFullDesc(HANDLE hidDev, HIDDeviceDesc* desc) const
{

    if (!initVendorProductVersion(hidDev, desc))
    {
        return false;
    }

    if (!initUsage(hidDev, desc))
    {
        return false;
    }

    initStrings(hidDev, desc);

    return true;
}

bool Win32HIDDeviceManager::initVendorProductVersion(HANDLE hidDev, HIDDeviceDesc* desc) const
{
    HIDD_ATTRIBUTES attr;
    attr.Size = sizeof(attr);
    if (!HidD_GetAttributes(hidDev, &attr))
        return false;
    desc->VendorId      = attr.VendorID;
    desc->ProductId     = attr.ProductID;
    desc->VersionNumber = attr.VersionNumber;
    return true;
}

bool Win32HIDDeviceManager::initUsage(HANDLE hidDev, HIDDeviceDesc* desc) const
{
    bool                 result = false;
    HIDP_CAPS            caps;
    HIDP_PREPARSED_DATA* preparsedData = 0;

    if (!HidD_GetPreparsedData(hidDev, &preparsedData))
        return false;

    if (HidP_GetCaps(preparsedData, &caps) == HIDP_STATUS_SUCCESS)
    {
        desc->Usage                  = caps.Usage;
        desc->UsagePage              = caps.UsagePage;
        result = true;
    }
    HidD_FreePreparsedData(preparsedData);
    return result;
}

void Win32HIDDeviceManager::initStrings(HANDLE hidDev, HIDDeviceDesc* desc) const
{
    // Documentation mentions 126 as being the max for USB.
    wchar_t strBuffer[196];

    // HidD_Get*String functions return nothing in buffer on failure,
    // so it's ok to do this without further error checking.
    strBuffer[0] = 0;
    HidD_GetManufacturerString(hidDev, strBuffer, sizeof(strBuffer));
    desc->Manufacturer = strBuffer;

    strBuffer[0] = 0;
    HidD_GetProductString(hidDev, strBuffer, sizeof(strBuffer));
    desc->Product = strBuffer;

    strBuffer[0] = 0;
    HidD_GetSerialNumberString(hidDev, strBuffer, sizeof(strBuffer));
    desc->SerialNumber = strBuffer;
}

//-------------------------------------------------------------------------------------
// **** Win32::HIDDevice

Win32HIDDevice::Win32HIDDevice(HIDDeviceManager* manager)
  : HIDDevice(manager), Device(0), ReadRequested(false)
{
    memset(&ReadOverlapped, 0, sizeof(OVERLAPPED));
}

// This is a minimal constructor used during enumeration for us to pass
// a HIDDevice to the visit function (so that it can query feature reports). 
Win32HIDDevice::Win32HIDDevice(HIDDeviceManager* manager, HANDLE device)
  : HIDDevice(manager, true), Device(device), ReadRequested(true)
{
    memset(&ReadOverlapped, 0, sizeof(OVERLAPPED));
}

Win32HIDDevice::~Win32HIDDevice()
{
    if (!InMinimalMode)
    {
        HIDShutdown();
    }
}

bool Win32HIDDevice::HIDInitialize(const String& path)
{

    DevDesc.Path = path;

    if (!openDevice())
    {
        LogText("OVR::Win32::HIDDevice - Failed to open HIDDevice: ", path);
        return false;
    }


    getThread()->AddTicksNotifier(this);
    getThread()->AddMessageNotifier(this);

    LogText("OVR::Win32::HIDDevice - Opened '%s'\n"
		"                    Manufacturer:'%s'  Product:'%s'  Serial#:'%s'  Version:'%x'\n",
        DevDesc.Path.ToCStr(),
        DevDesc.Manufacturer.ToCStr(), DevDesc.Product.ToCStr(),
        DevDesc.SerialNumber.ToCStr(),
        DevDesc.VersionNumber);

    return true;
}

bool Win32HIDDevice::initInfo()
{
    // Device must have been successfully opened.
    OVR_ASSERT(Device);

    // Get report lengths.
    HIDP_PREPARSED_DATA* preparsedData = 0;
    if (!Win32HIDDeviceManager::HidD_GetPreparsedData(Device, &preparsedData))
    {
        return false;
    }

    HIDP_CAPS caps;
    if (Win32HIDDeviceManager::HidP_GetCaps(preparsedData, &caps) != HIDP_STATUS_SUCCESS)
    {
      Win32HIDDeviceManager::HidD_FreePreparsedData(preparsedData);
        return false;
    }

    InputReportBufferLength  = caps.InputReportByteLength;
    OutputReportBufferLength = caps.OutputReportByteLength;
    FeatureReportBufferLength= caps.FeatureReportByteLength;
    Win32HIDDeviceManager::HidD_FreePreparsedData(preparsedData);

    if (ReadBufferSize < InputReportBufferLength)
    {
        OVR_ASSERT_LOG(false, ("Input report buffer length is bigger than read buffer."));
        return false;
    }

    // Get device desc.
    if (!getHidManager()->getFullDesc(Device, &DevDesc))
    {
        OVR_ASSERT_LOG(false, ("Failed to get device desc while initializing device."));
        return false;
    }

    return true;
}


bool Win32HIDDevice::openDevice()
{
    memset(&ReadOverlapped, 0, sizeof(OVERLAPPED));

    Device = CreateHIDFile(DevDesc.Path.ToCStr());
    if (Device == INVALID_HANDLE_VALUE)
    {
        OVR_DEBUG_LOG(("Failed 'CreateHIDFile' while opening device, error = 0x%X.", 
			::GetLastError()));
        Device = 0;
        return false;
    }

    if (!Win32HIDDeviceManager::HidD_SetNumInputBuffers(Device, 128))
    {
        OVR_ASSERT_LOG(false, ("Failed 'HidD_SetNumInputBuffers' while initializing device."));
        ::CloseHandle(Device);
        Device = 0;
        return false;
    }


    // Create a manual-reset non-signaled event.
    ReadOverlapped.hEvent = ::CreateEvent(0, TRUE, FALSE, 0);

    if (!ReadOverlapped.hEvent)
    {
        OVR_ASSERT_LOG(false, ("Failed to create event."));
        ::CloseHandle(Device);
        Device = 0;
        return false;
    }

    if (!initInfo())
    {
        OVR_ASSERT_LOG(false, ("Failed to get HIDDevice info."));

        ::CloseHandle(ReadOverlapped.hEvent);
        memset(&ReadOverlapped, 0, sizeof(OVERLAPPED));

        ::CloseHandle(Device);
        Device = 0;
        return false;
    }

    if (!initializeRead())
    {
        OVR_ASSERT_LOG(false, ("Failed to get intialize read for HIDDevice."));

        ::CloseHandle(ReadOverlapped.hEvent);
        memset(&ReadOverlapped, 0, sizeof(OVERLAPPED));

        ::CloseHandle(Device);
        Device = 0;
        return false;
    }

    return true;
}

void Win32HIDDevice::HIDShutdown()
{   

    getThread()->RemoveTicksNotifier(this);
    getThread()->RemoveMessageNotifier(this);

    closeDevice();
    LogText("OVR::Win32::HIDDevice - Closed '%s'\n", DevDesc.Path.ToCStr());
}

bool Win32HIDDevice::initializeRead()
{

    if (!ReadRequested)
    {        
        getThread()->AddOverlappedEvent(this, ReadOverlapped.hEvent);
        ReadRequested = true;
    }

    // Read resets the event...
    while(::ReadFile(Device, ReadBuffer, InputReportBufferLength, 0, &ReadOverlapped))
    {
        processReadResult();
    }

    if (GetLastError() != ERROR_IO_PENDING)
    {
        // Some other error (such as unplugged).
        closeDeviceOnIOError();
        return false;
    }

    return true;
}

bool Win32HIDDevice::processReadResult()
{

    OVR_ASSERT(ReadRequested);

    DWORD bytesRead = 0;

    if (GetOverlappedResult(Device, &ReadOverlapped, &bytesRead, FALSE))
    {
        // We've got data.
        if (Handler)
        {
            Handler->OnInputReport(ReadBuffer, bytesRead);
        }

        // TBD: Not needed?
        // Event should be reset by Read call...
        ReadOverlapped.Pointer = 0;
        ReadOverlapped.Internal = 0;
        ReadOverlapped.InternalHigh = 0;
        return true;
    }
    else
    {
        if (GetLastError() != ERROR_IO_PENDING)
        {
            closeDeviceOnIOError();
            return false;
        }
    }

    return false;
}

void Win32HIDDevice::closeDevice()
{
    if (ReadRequested)
    {
        getThread()->RemoveOverlappedEvent(this, ReadOverlapped.hEvent);
        ReadRequested = false;
        // Must call this to avoid Win32 assertion; CloseHandle is not enough.
        ::CancelIo(Device);
    }

    ::CloseHandle(ReadOverlapped.hEvent);
    memset(&ReadOverlapped, 0, sizeof(OVERLAPPED));

    ::CloseHandle(Device);
    Device = 0;
}

void Win32HIDDevice::closeDeviceOnIOError()
{
    LogText("OVR::Win32::HIDDevice - Lost connection to '%s'\n", DevDesc.Path.ToCStr());
    closeDevice();
}

bool Win32HIDDevice::SetFeatureReport(UByte* data, UInt32 length)
{
	if (!ReadRequested)
        return false;

  BOOLEAN res = Win32HIDDeviceManager::HidD_SetFeature(Device, data, (ULONG)length);
	return (res == TRUE);
}

bool Win32HIDDevice::GetFeatureReport(UByte* data, UInt32 length)
{
	if (!ReadRequested)
        return false;

  BOOLEAN res = Win32HIDDeviceManager::HidD_GetFeature(Device, data, (ULONG)length);
	return (res == TRUE);
}

void Win32HIDDevice::OnReadReadyEvent()
{
    if (processReadResult()) 
    {
        // Proceed to read again.
        initializeRead();
    }
}

bool Win32HIDDevice::OnDeviceMessage(DeviceMessageType messageType,
								const String& devicePath,
								bool* error)
{

    // Is this the correct device?
    if (DevDesc.Path.CompareNoCase(devicePath) != 0)
    {
        return false;
    }

    if (messageType == DeviceMessage_DeviceAdded && !Device)
    {
        // A closed device has been re-added. Try to reopen.
        if (!openDevice())
        {
            LogError("OVR::Win32::HIDDevice - Failed to reopen a device '%s' that was re-added.\n", devicePath.ToCStr());
			*error = true;
            return true;
        }

        LogText("OVR::Win32::HIDDevice - Reopened device '%s'\n", devicePath.ToCStr());
    }

    HIDHandler::HIDDeviceMessageType handlerMessageType = HIDHandler::HIDDeviceMessage_DeviceAdded;
    if (messageType == DeviceMessage_DeviceAdded)
    {
    }
    else if (messageType == DeviceMessage_DeviceRemoved)
    {
        handlerMessageType = HIDHandler::HIDDeviceMessage_DeviceRemoved;
    }
    else
    {
        OVR_ASSERT(0);		
    }

    if (Handler)
    {
        Handler->OnDeviceMessage(handlerMessageType);
    }

	*error = false;
    return true;
}

HIDDeviceManager* Win32HIDDeviceManager::CreateInternal(Platform::DeviceManager* devManager)
{

    if (!System::IsInitialized())
    {
        // Use custom message, since Log is not yet installed.
        OVR_DEBUG_STATEMENT(Log::GetDefaultLog()->
            LogMessage(Log_Debug, "HIDDeviceManager::Create failed - OVR::System not initialized"); );
        return 0;
    }

    Ptr<Win32HIDDeviceManager> manager = *new Win32HIDDeviceManager(devManager);

    if (manager)
    {
        if (manager->Initialize())
        {
            manager->AddRef();
        }
        else
        {
            manager.Clear();
        }
    }

    return manager.GetPtr();
}

} // namespace Win32

//-------------------------------------------------------------------------------------
// ***** Creation

// Creates a new HIDDeviceManager and initializes OVR.
HIDDeviceManager* HIDDeviceManager::Create()
{
    OVR_ASSERT_LOG(false, ("Standalone mode not implemented yet."));

    if (!System::IsInitialized())
    {
        // Use custom message, since Log is not yet installed.
        OVR_DEBUG_STATEMENT(Log::GetDefaultLog()->
            LogMessage(Log_Debug, "HIDDeviceManager::Create failed - OVR::System not initialized"); );
        return 0;
    }

    Ptr<Platform::Win32HIDDeviceManager> manager = *new Platform::Win32HIDDeviceManager(NULL);

    if (manager)
    {
        if (manager->Initialize())
        {
            manager->AddRef();
        }
        else
        {
            manager.Clear();
        }
    }

    return manager.GetPtr();
}

} // namespace OVR
