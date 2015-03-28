/************************************************************************************

Filename    :   OVR_Win32_Display.cpp
Content     :   Win32 Display implementation
Created     :   May 6, 2014
Authors     :   Dean Beeler

Copyright   :   Copyright 2014 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.2 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#include "Kernel/OVR_Win32_IncludeWindows.h"

#include "OVR_Win32_Display.h"
#include "OVR_Win32_Dxgi_Display.h"
#include "OVR_Win32_ShimFunctions.h"
#include "Util/Util_Direct3D.h"

#include <stdio.h>
#include <tchar.h>
#include <string.h>
#include <stdlib.h>
#include <winioctl.h>
#include <SetupAPI.h>
#include <Mmsystem.h>
#include <conio.h>

#include <setupapi.h>
#include <initguid.h>
#pragma comment(lib, "setupapi.lib") // SetupDiGetDeviceRegistryProperty et al
#pragma comment(lib, "dxgi.lib") // D3D11 adapter/output enumeration

// WIN32_LEAN_AND_MEAN included in OVR_Atomic.h may break 'byte' declaration.
#ifdef WIN32_LEAN_AND_MEAN
 typedef unsigned char byte;
#endif

#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Log.h"

typedef struct
{
    HANDLE  hDevice;
    UINT    ExpectedWidth;
    UINT    ExpectedHeight;
    HWND    hWindow;
    bool    CompatibilityMode;
    bool    HideDK1Mode;
} ContextStruct;

static ContextStruct GlobalDisplayContext = {};

 
//-------------------------------------------------------------------------------------
// ***** Display enumeration Helpers

// THere are two ways to enumerate display: through our driver (DeviceIoControl)
// and through Win32 EnumDisplayMonitors (compatibility mode).


namespace OVR { 


//-----------------------------------------------------------------------------
// Direct Mode

ULONG getRiftCount( HANDLE hDevice )
{
    ULONG riftCount = 0;
    DWORD bytesReturned = 0;

    BOOL result = DeviceIoControl( hDevice, IOCTL_RIFTMGR_GET_RIFT_COUNT, nullptr, 0,
                                   &riftCount, sizeof( ULONG ), &bytesReturned, nullptr );

    if( result )
        return riftCount;
    else
        return 0;
}

ULONG getRift( HANDLE hDevice, int index )
{
    ULONG riftCount = getRiftCount( hDevice );
    DWORD bytesReturned = 0;
    BOOL result;

    if( riftCount >= (ULONG)index )
    {
        RIFT_STATUS riftStatus[16] = {0};

        result = DeviceIoControl( hDevice, IOCTL_RIFTMGR_GET_RIFT_ARRAY, riftStatus,
                                  riftCount * sizeof( RIFT_STATUS ), &riftCount,
                                  sizeof( ULONG ), &bytesReturned, nullptr );
        if( result )
        {
            PRIFT_STATUS tmpRift;
            unsigned int i;
            for( i = 0, tmpRift = riftStatus; i < riftCount; ++i, ++tmpRift )
            {
                if( i == (unsigned int)index )
                    return tmpRift->childUid;
            }
        }
    }

    return 0;
}

static bool getEdid(HANDLE hDevice, ULONG uid, DisplayEDID& edid)
{
    ULONG       riftCount = 0;
    DWORD       bytesReturned = 0;
    RIFT_STATUS riftStatus[16] = {0};

    BOOL result = DeviceIoControl( hDevice, IOCTL_RIFTMGR_GET_RIFT_COUNT, nullptr, 0,
                                   &riftCount, sizeof( ULONG ), &bytesReturned, nullptr );

    if (!result)
    {
        return false;
    }

    result = DeviceIoControl( hDevice, IOCTL_RIFTMGR_GET_RIFT_ARRAY, &riftStatus,
                              riftCount * sizeof( RIFT_STATUS ), &riftCount, sizeof(ULONG),
                              &bytesReturned, nullptr );
    if (!result)
    {
        return false;
    }

    for (ULONG i = 0; i < riftCount; ++i)
    {
        ULONG riftUid = riftStatus[i].childUid;
        if (riftUid == uid)
        {
            char edidBuffer[512];

            result = DeviceIoControl(hDevice, IOCTL_RIFTMGR_GETEDID, &riftUid, sizeof(ULONG),
                                     edidBuffer, 512, &bytesReturned, nullptr);

            if (result)
            {
                if (edid.Parse((unsigned char*)edidBuffer))
                {
                    return true;
                }
                else
                {
                    LogError(("[Win32Display] WARNING: The driver was not able to return EDID for a display"));
                }
            }

            break;
        }
    }

    return false;
}


//-----------------------------------------------------------------------------
// Rift Monitors

static bool GetMonitorEDID(const wchar_t* deviceID, DisplayEDID& edid)
{
    // Find second slash and split there
    const wchar_t* pSlash1 = wcschr(deviceID, L'\\');
    if (!pSlash1)
        return false;

    // And the second one
    const wchar_t* pSlash2 = wcschr(pSlash1 + 1, L'\\');
    if (!pSlash2)
        return false;

    wchar_t hardwareID[128] = {};
    wcsncpy_s(hardwareID, pSlash1 + 1, (pSlash2 - pSlash1 - 1));

    wchar_t driverID[128] = {};
    wcscpy_s(driverID, pSlash2 + 1);

    ScopedHKEY displayKey;
    if (RegOpenKey(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Enum\\DISPLAY",
                   &displayKey.GetRawRef()) == 0)
    {
        wchar_t keyName[MAX_PATH] = {};
        DWORD iKey = 0;
        while (RegEnumKey(displayKey.Get(), iKey++, keyName, _countof(keyName)) == 0)
        {
            if (_wcsicmp(keyName, hardwareID) == 0)
            {
                ScopedHKEY devKey;
                if (RegOpenKey(displayKey.Get(), keyName, &devKey.GetRawRef()) == 0)
                {
                    wchar_t nodeName[MAX_PATH] = {};
                    DWORD iNode = 0;
                    while (RegEnumKey(devKey.Get(), iNode++, nodeName, _countof(nodeName)) == 0)
                    {
                        wchar_t devDriverID[MAX_PATH] = {};
                        DWORD cbDriverID = sizeof(devDriverID);
                        if (RegGetValue(devKey.Get(), nodeName, L"Driver", RRF_RT_REG_SZ, nullptr,
                                        devDriverID, &cbDriverID) == 0)
                        {
                            // Check if DriverID matches
                            if (_wcsicmp(devDriverID, driverID) == 0)
                            {
                                ScopedHKEY nodeKey;
                                if (RegOpenKey(devKey.Get(), nodeName, &nodeKey.GetRawRef()) == 0)
                                {
                                    BYTE edidBytes[512] = {};
                                    DWORD cbEDID = sizeof(edidBytes);
                                    if (RegGetValue(nodeKey.Get(), L"Device Parameters", L"EDID", RRF_RT_REG_BINARY,
                                        nullptr, edidBytes, &cbEDID) == 0)
                                    {
                                        return edid.Parse(edidBytes);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return true;
}

static bool FillDisplayDesc(DXGI_OUTPUT_DESC const& outputDesc, DISPLAY_DEVICEW& dispDev, DisplayEDID const& edid, DisplayDesc& result)
{
    // EdidSerialNumber
    static_assert(sizeof(result.EdidSerialNumber) == sizeof(edid.SerialNumber), "sizes need to match");
    memcpy(result.EdidSerialNumber, edid.SerialNumber, sizeof(result.EdidSerialNumber));

    // DisplayID
    size_t converted = 0;
    if (0 != wcstombs_s(&converted, result.DisplayID, dispDev.DeviceName, _TRUNCATE))
        return false;

    // DesktopDisplayOffset
    result.DesktopDisplayOffset = Vector2i(outputDesc.DesktopCoordinates.left,
                                           outputDesc.DesktopCoordinates.top);

    // ModelName
    static_assert(sizeof(result.ModelName) == sizeof(edid.MonitorName), "sizes need to match");
    memcpy(result.ModelName, edid.MonitorName, sizeof(result.ModelName));

    // Resolution
    result.ResolutionInPixels.w = edid.Width;
    result.ResolutionInPixels.h = edid.Height;

    // DK2 Landscape = DXGI_MODE_ROTATION_IDENTITY -> DXGI_MODE_ROTATION_ROTATE90
    // DK2 Portrait = DXGI_MODE_ROTATION_ROTATE90 -> DXGI_MODE_ROTATION_IDENTITY
    // DK2 Landscape(Flipped) = DXGI_MODE_ROTATION_ROTATE180 -> DXGI_MODE_ROTATION_ROTATE270
    // DK2 Portrait(Flipped) = DXGI_MODE_ROTATION_ROTATE270 -> DXGI_MODE_ROTATION_ROTATE180
    bool tallScreen = result.ResolutionInPixels.w < result.ResolutionInPixels.h;

    // Rotation
    // This is the rotation from portrait mode, and the DK2 (for example) is naturally landscape,
    // so we need to adjust the rotation value based on the HMD type.
    switch (outputDesc.Rotation)
    {
    case DXGI_MODE_ROTATION_IDENTITY:
    default:
        result.Rotation = tallScreen ? 270 : 0;
        break;
    case DXGI_MODE_ROTATION_ROTATE90:
        result.Rotation = tallScreen ? 0 : 90;
        break;
    case DXGI_MODE_ROTATION_ROTATE180:
        result.Rotation = tallScreen ? 90 : 180;
        break;
    case DXGI_MODE_ROTATION_ROTATE270:
        result.Rotation = tallScreen ? 180 : 270;
        break;
    }

    // DeviceTypeGuess
    result.DeviceTypeGuess = HmdTypeFromModelNumber(edid.ModelNumber);

    return true;
}

// Pass in 0 if only an existence check is requested.
static int DiscoverRiftMonitors(DisplayDesc* descriptorArray = nullptr, int inputArraySize = 0)
{
    int outputArraySize = 0;

    Ptr<IDXGIFactory1> factory;
    HRESULT hr = ::CreateDXGIFactory1(IID_PPV_ARGS(&factory.GetRawRef()));
    OVR_D3D_CHECK_RET_FALSE(hr);

    Ptr<IDXGIAdapter> adapter;
    uint32_t iAdapter = 0;
    while (adapter = nullptr,
           SUCCEEDED(factory->EnumAdapters(iAdapter++, &adapter.GetRawRef())))
    {
        Ptr<IDXGIAdapter1> adapter1;
        hr = adapter->QueryInterface(IID_PPV_ARGS(&adapter1.GetRawRef()));
        if (!OVR_D3D_CHECK(hr))
            continue;

        DXGI_ADAPTER_DESC1 adapterDesc = {};
        adapter1->GetDesc1(&adapterDesc);

        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't bother with software adapters (WARP, BasicRender, etc...)
            continue;
        }

        Ptr<IDXGIOutput> output;
        uint32_t iOutput = 0;
        while (output = nullptr,
               SUCCEEDED(adapter->EnumOutputs(iOutput++, &output.GetRawRef())))
        {
            // Get DeviceName for each output
            DXGI_OUTPUT_DESC outputDesc;
            hr = output->GetDesc(&outputDesc);
            if (!OVR_D3D_CHECK(hr))
                continue;

            // Get DeviceID from DeviceName
            DISPLAY_DEVICEW dispDev = {};
            dispDev.cb = sizeof(dispDev);
            if (!::EnumDisplayDevicesW(outputDesc.DeviceName, 0, &dispDev, 0))
                continue;

            // If it is a Rift monitor,
            if (wcsstr(dispDev.DeviceID, L"RTD2205") ||
                wcsstr(dispDev.DeviceID, L"CVT0003") ||
                wcsstr(dispDev.DeviceID, L"MST0030") ||
                wcsstr(dispDev.DeviceID, L"OVR00")) // Part of Oculus EDID.
            {
                DisplayEDID edid = {};

                // If monitor existence is the goal,
                if (!descriptorArray || inputArraySize <= 0)
                    return 1;

                if (outputArraySize < inputArraySize)
                {
                    if (GetMonitorEDID(dispDev.DeviceID, edid) &&
                        FillDisplayDesc(outputDesc, dispDev, edid, descriptorArray[outputArraySize]))
                    {
                        outputArraySize++;
                    }
                }
            }
        }
    }

    return outputArraySize;
}

bool Display::ExtendedModeDevicesExist()
{
    return DiscoverRiftMonitors() > 0;
}


//-------------------------------------------------------------------------------------
// ***** Display 

bool Display::InCompatibilityMode(bool displaySearch)
{
    return (displaySearch && ExtendedModeDevicesExist()) ||
            GlobalDisplayContext.CompatibilityMode;
}

#define OVR_FLAG_COMPATIBILITY_MODE 1
#define OVR_FLAG_HIDE_DK1 2


void Display::Shutdown()
{
    Win32::DisplayShim::GetInstance().Shutdown();
    OVR::Display::SetDirectDisplayInitialized(false);
}

bool Display::Initialize()
{
    // This function is re-entrant because it may be called again to
    // patch-up comatibility mode by the Config Util (Hack in HMDView::SetupGraphics).
    // For this reason, the GetDirectDisplayInitialized() check is only done for
    // shim initialization below.

    HANDLE hDevice = INVALID_HANDLE_VALUE;

    if (GlobalDisplayContext.hDevice == 0)
    {
        hDevice = CreateFile(L"\\\\.\\ovr_video" ,
                             GENERIC_READ | GENERIC_WRITE, 0,
                             nullptr, OPEN_EXISTING, 0, nullptr);
    }
    else
    {   // The device has already been created
        hDevice = GlobalDisplayContext.hDevice;
    }

    if (hDevice != nullptr && hDevice != INVALID_HANDLE_VALUE)
    {
        GlobalDisplayContext.hDevice             = hDevice;
        GlobalDisplayContext.CompatibilityMode = false;

        DWORD bytesReturned = 0;
        LONG compatiblityResult = OVR_STATUS_SUCCESS;

        BOOL result = DeviceIoControl( hDevice, IOCTL_RIFTMGR_GETCOMPATIBILITYMODE, nullptr, 0,
                                       &compatiblityResult, sizeof( LONG ), &bytesReturned, nullptr );
        if (result)
        {
            GlobalDisplayContext.CompatibilityMode = (compatiblityResult & OVR_FLAG_COMPATIBILITY_MODE) != 0;
            GlobalDisplayContext.HideDK1Mode = (compatiblityResult & OVR_FLAG_HIDE_DK1) != 0;
        }
        else
        {
            // If calling our driver fails in any way, assume compatibility mode as well
            GlobalDisplayContext.CompatibilityMode = true;
        }

        if (!GlobalDisplayContext.CompatibilityMode)
        {
            // If a display is actually connected, bring up the shim layers so we can actually use it
            // TBD: Do we care about extended mode displays? -cat
            if (OVR::Display::ExtendedModeDevicesExist() ||
                getRiftCount(GlobalDisplayContext.hDevice) > 0)
            {
                // FIXME: Initializing DX9 with landscape numbers rather than portrait
                GlobalDisplayContext.ExpectedWidth = 1080;
                GlobalDisplayContext.ExpectedHeight = 1920;
            }
            else
            {
                GlobalDisplayContext.CompatibilityMode = true;
            }
        }
    }
    else
    {
        GlobalDisplayContext.CompatibilityMode = true;
    }



    // Set up display code for Windows
    Win32::DisplayShim::GetInstance();

    // This code will look for the first display. If it's a display
    // that's extending the desktop, the code will assume we're in
    // compatibility mode. Compatibility mode prevents shim loading
    // and renders only to extended Rifts.
    // If we find a display and it's application exclusive,
    // we load the shim so we can render to it.
    // If no display is available, we revert to whatever the
    // driver tells us we're in

    bool anyExtendedRifts = OVR::Display::ExtendedModeDevicesExist() ||
                            GlobalDisplayContext.CompatibilityMode;

    if (!OVR::Display::GetDirectDisplayInitialized())
    {
        OVR::Display::SetDirectDisplayInitialized(
            Win32::DisplayShim::GetInstance().Initialize(anyExtendedRifts));
    }
    
    return true;
}

bool Display::GetDriverMode(bool& driverInstalled, bool& compatMode, bool& hideDK1Mode)
{
    if (GlobalDisplayContext.hDevice == nullptr)
    {
        driverInstalled = false;
        compatMode      = true;
        hideDK1Mode     = false;
    }
    else
    {
        driverInstalled = true;
        compatMode      = GlobalDisplayContext.CompatibilityMode;
        hideDK1Mode     = GlobalDisplayContext.HideDK1Mode;
    }

    return true;
}

bool Display::SetDriverMode(bool compatMode, bool hideDK1Mode)
{
    // If device is not initialized,
    if (GlobalDisplayContext.hDevice == nullptr)
    {
        OVR_ASSERT(false);
        return false;
    }

    // If no change,
    if ((compatMode == GlobalDisplayContext.CompatibilityMode) &&
        (hideDK1Mode == GlobalDisplayContext.HideDK1Mode))
    {
        return true;
    }

    LONG mode_flags = 0;
    if (compatMode)
    {
        mode_flags |= OVR_FLAG_COMPATIBILITY_MODE;
    }
    if (hideDK1Mode)
    {
        mode_flags |= OVR_FLAG_HIDE_DK1;
    }

    DWORD bytesReturned = 0;
    LONG err = 1;

    if (!DeviceIoControl(GlobalDisplayContext.hDevice,
                         IOCTL_RIFTMGR_SETCOMPATIBILITYMODE,
                         &mode_flags,
                         sizeof(LONG),
                         &err,
                         sizeof(LONG),
                         &bytesReturned,
                         nullptr) ||
        (err != 0 && err != -3))
    {
        LogError("{ERR-001w} [Win32Display] Unable to set device mode to (compat=%d dk1hide=%d): err=%d",
            (int)compatMode, (int)hideDK1Mode, (int)err);
        return false;
    }

    OVR_DEBUG_LOG(("[Win32Display] Set device mode to (compat=%d dk1hide=%d)",
        (int)compatMode, (int)hideDK1Mode));

    GlobalDisplayContext.HideDK1Mode = hideDK1Mode;
    GlobalDisplayContext.CompatibilityMode = compatMode;
    return true;
}

DisplaySearchHandle* Display::GetDisplaySearchHandle()
{
    return new Win32::Win32DisplaySearchHandle();
}

// FIXME: The handle parameter will be used to unify GetDisplayCount and GetDisplay calls
// The handle will be written to the 64-bit value pointed and will store the enumerated
// display list. This will allow the indexes to be meaningful between obtaining
// the count. With a single handle the count should be stable
int Display::GetDisplayCount(DisplaySearchHandle* handle, bool extended, bool applicationOnly, bool extendedEDIDSerials)
{
    OVR_UNUSED(extendedEDIDSerials);
    static int extendedCount = -1;
    static int applicationCount = -1;

    Win32::Win32DisplaySearchHandle* localHandle = (Win32::Win32DisplaySearchHandle*)handle;
    
    if( localHandle == nullptr )
        return 0;

    if( extendedCount == -1 || extended )
    {
        extendedCount = DiscoverRiftMonitors(localHandle->cachedDescriptorArray, 16);
    }

    localHandle->extended = true;
    localHandle->extendedDisplayCount = extendedCount;
    int totalCount = extendedCount;

    if( applicationCount == -1 || applicationOnly )
    {
        applicationCount = getRiftCount(GlobalDisplayContext.hDevice);
        localHandle->application = true;
    }

    totalCount += applicationCount;
    localHandle->applicationDisplayCount = applicationCount;
    localHandle->displayCount = totalCount;

    return totalCount;
}

Ptr<Display> Display::GetDisplay(int index, DisplaySearchHandle* handle)
{
    Ptr<Display> result;

    if( index < 0 )
        return result;

    Win32::Win32DisplaySearchHandle* localHandle = (Win32::Win32DisplaySearchHandle*)handle;

    if( localHandle == nullptr )
        return nullptr;

    if (localHandle->extended)
    {
        if (index >= 0 && index < (int)localHandle->extendedDisplayCount)
        {
            return *new Win32::Win32DisplayGeneric(localHandle->cachedDescriptorArray[index]);
        }

        index -= localHandle->extendedDisplayCount;
    }

    if(localHandle->application)
    {
        if (index >= 0 && index < (int)getRiftCount(GlobalDisplayContext.hDevice))
        {
            ULONG riftChildId = getRift(GlobalDisplayContext.hDevice, index);
            DisplayEDID dEdid;

            if (!getEdid(GlobalDisplayContext.hDevice, riftChildId, dEdid))
            {
                return nullptr;
            }

            uint32_t nativeWidth = dEdid.Width, nativeHeight = dEdid.Height;
            uint32_t rotation    = (dEdid.ModelNumber == 2 ||
                                    dEdid.ModelNumber == 3) ? 90 : 0;

            uint32_t logicalWidth, logicalHeight;
            if (rotation == 0)
            {
                logicalWidth  = nativeWidth;
                logicalHeight = nativeHeight;
            }
            else
            {
                logicalWidth  = nativeHeight;
                logicalHeight = nativeWidth;
            }

            result = *new Win32::Win32DisplayDriver( 
                        HmdTypeFromModelNumber(dEdid.ModelNumber),
                        "",
                        dEdid.MonitorName,
                        dEdid.SerialNumber,
                        Sizei(logicalWidth, logicalHeight),
                        Sizei(nativeWidth, nativeHeight),
                        Vector2i(0),
                        dEdid,
                        GlobalDisplayContext.hDevice,
                        riftChildId,
                        rotation);
        }
    }
    return result;
}

Display::MirrorMode Win32::Win32DisplayDriver::SetMirrorMode( Display::MirrorMode newMode )
{
    return newMode;
}

static bool SetDisplayPower(HANDLE hDevice, ULONG childId, int mode)
{
#ifdef OVR_OS_WIN64
    BOOL is64BitOS = TRUE;
#else
    BOOL is64BitOS = FALSE;
    BOOL res = IsWow64Process(GetCurrentProcess(), &is64BitOS);
    OVR_ASSERT_AND_UNUSED(res == TRUE,res);
#endif
    ULONG localResult = 0;
    DWORD bytesReturned = 0;
    BOOL result;
    if (is64BitOS)
    {
        ULONG64 longArray[2];
        longArray[0] = childId;
        longArray[1] = mode;
        result = DeviceIoControl(hDevice,
            IOCTL_RIFTMGR_DISPLAYPOWER,
            longArray,
            2 * sizeof(ULONG64),
            &localResult,
            sizeof(ULONG),
            &bytesReturned,
            nullptr);
    }
    else
    {
        ULONG32 longArray[2];
        longArray[0] = childId;
        longArray[1]  = mode;
        result = DeviceIoControl(hDevice,
            IOCTL_RIFTMGR_DISPLAYPOWER,
            longArray,
            2 * sizeof(ULONG32),
            &localResult,
            sizeof(ULONG),
            &bytesReturned,
            nullptr);
    }

    // Note: bytesReturned does not seem to be set
    return result != FALSE /* && bytesReturned == sizeof(ULONG) */ && mode == (int)localResult;
}

bool Win32::Win32DisplayDriver::SetDisplaySleep(bool sleep)
{
    return SetDisplayPower(hDevice, ChildId, sleep ? 2 : 1);
}


} // namespace OVR
