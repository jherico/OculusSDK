/************************************************************************************

Filename    :   OVR_Win32_HMDDevice.cpp
Content     :   Win32 Interface to HMD - detects HMD display
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

#include "OVR_Win32_HMDDevice.h"

#include "OVR_Win32_DeviceManager.h"
#include "util/Util_Render_Stereo.h"

#include <tchar.h>

namespace OVR { namespace Win32 {

using namespace OVR::Util::Render;

//-------------------------------------------------------------------------------------

HMDDeviceCreateDesc::HMDDeviceCreateDesc(DeviceFactory* factory, 
                                         const String& deviceId, const String& displayDeviceName)
        : DeviceCreateDesc(factory, Device_HMD),
          DeviceId(deviceId), DisplayDeviceName(displayDeviceName),
          Contents(0)
{
    Desktop.X = 0;
    Desktop.Y = 0;
    ResolutionInPixels = Sizei(0);
    ScreenSizeInMeters = Sizef(0.0f);
    VCenterFromTopInMeters = 0.0f;
    LensSeparationInMeters = 0.0f;
}
HMDDeviceCreateDesc::HMDDeviceCreateDesc(const HMDDeviceCreateDesc& other)
        : DeviceCreateDesc(other.pFactory, Device_HMD),
          DeviceId(other.DeviceId), DisplayDeviceName(other.DisplayDeviceName),
          Contents(other.Contents)
{
    Desktop.X              = other.Desktop.X;
    Desktop.Y              = other.Desktop.Y;
    ResolutionInPixels     = other.ResolutionInPixels;
    ScreenSizeInMeters     = other.ScreenSizeInMeters;
    VCenterFromTopInMeters = other.VCenterFromTopInMeters;
    LensSeparationInMeters = other.LensSeparationInMeters;
}

HMDDeviceCreateDesc::MatchResult HMDDeviceCreateDesc::MatchDevice(const DeviceCreateDesc& other,
                                                                  DeviceCreateDesc** pcandidate) const
{
    if ((other.Type != Device_HMD) || (other.pFactory != pFactory))
        return Match_None;

    // There are several reasons we can come in here:
    //   a) Matching this HMD Monitor created desc to OTHER HMD Monitor desc
    //          - Require exact device DeviceId/DeviceName match
    //   b) Matching SensorDisplayInfo created desc to OTHER HMD Monitor desc
    //          - This DeviceId is empty; becomes candidate
    //   c) Matching this HMD Monitor created desc to SensorDisplayInfo desc
    //          - This other.DeviceId is empty; becomes candidate

    const HMDDeviceCreateDesc& s2 = (const HMDDeviceCreateDesc&) other;

    if ((DeviceId == s2.DeviceId) &&
        (DisplayDeviceName == s2.DisplayDeviceName))
    {
        // Non-null DeviceId may match while size is different if screen size was overwritten
        // by SensorDisplayInfo in prior iteration.
        if (!DeviceId.IsEmpty() ||
            (ScreenSizeInMeters == s2.ScreenSizeInMeters) )
        {            
            *pcandidate = 0;
            return Match_Found;
        }
    }


    // DisplayInfo takes precedence, although we try to match it first.
    if ((ResolutionInPixels == s2.ResolutionInPixels) &&        
        (ScreenSizeInMeters == s2.ScreenSizeInMeters))
    {
        if (DeviceId.IsEmpty() && !s2.DeviceId.IsEmpty())
        {
            *pcandidate = const_cast<DeviceCreateDesc*>((const DeviceCreateDesc*)this);
            return Match_Candidate;
        }

        *pcandidate = 0;
        return Match_Found;
    }
    
    // SensorDisplayInfo may override resolution settings, so store as candidate.
    if (s2.DeviceId.IsEmpty())
    {        
        *pcandidate = const_cast<DeviceCreateDesc*>((const DeviceCreateDesc*)this);
        return Match_Candidate;
    }
    // OTHER HMD Monitor desc may initialize DeviceName/Id
    else if (DeviceId.IsEmpty())
    {
        *pcandidate = const_cast<DeviceCreateDesc*>((const DeviceCreateDesc*)this);
        return Match_Candidate;
    }
    
    return Match_None;
}


bool HMDDeviceCreateDesc::UpdateMatchedCandidate(const DeviceCreateDesc& other, 
                                                 bool* newDeviceFlag)
{
    // This candidate was the the "best fit" to apply sensor DisplayInfo to.
    OVR_ASSERT(other.Type == Device_HMD);
    
    const HMDDeviceCreateDesc& s2 = (const HMDDeviceCreateDesc&) other;

    // Force screen size on resolution from SensorDisplayInfo.
    // We do this because USB detection is more reliable as compared to HDMI EDID,
    // which may be corrupted by splitter reporting wrong monitor 
    if (s2.DeviceId.IsEmpty())
    {
        // disconnected HMD: replace old descriptor by the 'fake' one.
        ScreenSizeInMeters = s2.ScreenSizeInMeters;        
        Contents |= Contents_Screen;

        if (s2.Contents & HMDDeviceCreateDesc::Contents_Distortion)
        {
            memcpy(DistortionK, s2.DistortionK, sizeof(float)*4);
            // TODO: DistortionEqn
            Contents |= Contents_Distortion;
        }
        DeviceId          = s2.DeviceId;
        DisplayDeviceName = s2.DisplayDeviceName;
        Desktop.X         = s2.Desktop.X;
        Desktop.Y         = s2.Desktop.Y;
        if (newDeviceFlag) *newDeviceFlag = true;
    }
    else if (DeviceId.IsEmpty())
    {
        // This branch is executed when 'fake' HMD descriptor is being replaced by
        // the real one.
        DeviceId          = s2.DeviceId;
        DisplayDeviceName = s2.DisplayDeviceName;
        Desktop.X         = s2.Desktop.X;
        Desktop.Y         = s2.Desktop.Y;

		// ScreenSize and Resolution are NOT assigned here, since they may have
		// come from a sensor DisplayInfo (which has precedence over HDMI).

        if (newDeviceFlag) *newDeviceFlag = true;
    }
    else
    {
        if (newDeviceFlag) *newDeviceFlag = false;
    }

    return true;
}

bool HMDDeviceCreateDesc::MatchDevice(const String& path)
{
    return DeviceId.CompareNoCase(path) == 0;
}
    
//-------------------------------------------------------------------------------------


const wchar_t* FormatDisplayStateFlags(wchar_t* buff, int length, DWORD flags)
{
    buff[0] = 0;
    if (flags & DISPLAY_DEVICE_ACTIVE)
        wcscat_s(buff, length, L"Active ");
    if (flags & DISPLAY_DEVICE_MIRRORING_DRIVER)
        wcscat_s(buff, length, L"Mirroring_Driver ");
    if (flags & DISPLAY_DEVICE_MODESPRUNED)
        wcscat_s(buff, length, L"ModesPruned ");
    if (flags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        wcscat_s(buff, length, L"Primary ");
    if (flags & DISPLAY_DEVICE_REMOVABLE)
        wcscat_s(buff, length, L"Removable ");
    if (flags & DISPLAY_DEVICE_VGA_COMPATIBLE)
        wcscat_s(buff, length, L"VGA_Compatible ");
    return buff;
}


//-------------------------------------------------------------------------------------
// Callback for monitor enumeration to store all the monitor handles

// Used to capture all the active monitor handles
struct MonitorSet
{
    enum { MaxMonitors = 8 };
    HMONITOR Monitors[MaxMonitors];
    int      MonitorCount;
};

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData)
{
    MonitorSet* monitorSet = (MonitorSet*)dwData;
    if (monitorSet->MonitorCount > MonitorSet::MaxMonitors)
        return FALSE;

    monitorSet->Monitors[monitorSet->MonitorCount] = hMonitor;
    monitorSet->MonitorCount++;
    return TRUE;
};

//-------------------------------------------------------------------------------------
// ***** HMDDeviceFactory

HMDDeviceFactory HMDDeviceFactory::Instance;

void HMDDeviceFactory::EnumerateDevices(EnumerateVisitor& visitor)
{
    MonitorSet monitors;
    monitors.MonitorCount = 0;
    // Get all the monitor handles 
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&monitors);

    bool foundHMD = false;
    
   // DeviceManager* manager = getManager();
    DISPLAY_DEVICE dd, ddm;
    UINT           i, j;    

    for (i = 0; 
        (ZeroMemory(&dd, sizeof(dd)), dd.cb = sizeof(dd),
        EnumDisplayDevices(0, i, &dd, 0)) != 0;  i++)
    {
        
        /*
        wchar_t buff[500], flagsBuff[200];
        
        swprintf_s(buff, 500, L"\nDEV: \"%s\" \"%s\" 0x%08x=%s\n     \"%s\" \"%s\"\n",
            dd.DeviceName, dd.DeviceString,
            dd.StateFlags, FormatDisplayStateFlags(flagsBuff, 200, dd.StateFlags),
            dd.DeviceID, dd.DeviceKey);
        ::OutputDebugString(buff);
        */

        for (j = 0; 
            (ZeroMemory(&ddm, sizeof(ddm)), ddm.cb = sizeof(ddm),
            EnumDisplayDevices(dd.DeviceName, j, &ddm, 0)) != 0;  j++)
        {
            /*
            wchar_t mbuff[500];
            swprintf_s(mbuff, 500, L"MON: \"%s\" \"%s\" 0x%08x=%s\n     \"%s\" \"%s\"\n",
                ddm.DeviceName, ddm.DeviceString,
                ddm.StateFlags, FormatDisplayStateFlags(flagsBuff, 200, ddm.StateFlags),
                ddm.DeviceID, ddm.DeviceKey);
            ::OutputDebugString(mbuff);
            */

            // Our monitor hardware has string "RTD2205" in it
            // Nate's device "CVT0003"
            if (wcsstr(ddm.DeviceID, L"RTD2205") || 
                wcsstr(ddm.DeviceID, L"CVT0003") || 
                wcsstr(ddm.DeviceID, L"MST0030") ||
                wcsstr(ddm.DeviceID, L"OVR00") ) // Part of Oculus EDID.
            {
                String deviceId(ddm.DeviceID);
                String displayDeviceName(ddm.DeviceName);

                // The default monitor coordinates
                int mx      = 0;
                int my      = 0;
                int mwidth  = 1280;
                int mheight = 800;

                // Find the matching MONITORINFOEX for this device so we can get the 
                // screen coordinates
                MONITORINFOEX info;
                for (int m=0; m < monitors.MonitorCount; m++)
                {
                    info.cbSize = sizeof(MONITORINFOEX);
                    GetMonitorInfo(monitors.Monitors[m], &info);
                    if (_tcsstr(ddm.DeviceName, info.szDevice) == ddm.DeviceName)
                    {   // If the device name starts with the monitor name
                        // then we found the matching DISPLAY_DEVICE and MONITORINFO
                        // so we can gather the monitor coordinates
                        mx = info.rcMonitor.left;
                        my = info.rcMonitor.top;
                        //mwidth = info.rcMonitor.right - info.rcMonitor.left;
                        //mheight = info.rcMonitor.bottom - info.rcMonitor.top;
                        break;
                    }
                }

                HMDDeviceCreateDesc hmdCreateDesc(this, deviceId, displayDeviceName);
		
                // Hard-coded defaults in case the device doesn't have the data itself.
                if (wcsstr(ddm.DeviceID, L"OVR0003"))
                {   // DK2 prototypes and variants (default to HmdType_DK2)
                    hmdCreateDesc.SetScreenParameters(mx, my, 1920, 1080, 0.12576f, 0.07074f, 0.12576f*0.5f, 0.0635f );
                }
				else if (wcsstr(ddm.DeviceID, L"OVR0002"))
				{   // HD Prototypes (default to HmdType_DKHDProto) 
					hmdCreateDesc.SetScreenParameters(mx, my, 1920, 1080, 0.12096f, 0.06804f, 0.06804f*0.5f, 0.0635f );
				}
				else if (wcsstr(ddm.DeviceID, L"OVR0001"))
				{   // DK1
                    hmdCreateDesc.SetScreenParameters(mx, my, mwidth, mheight, 0.14976f, 0.0936f, 0.0936f*0.5f, 0.0635f);
                }
                else if (wcsstr(ddm.DeviceID, L"OVR00"))
                {   // Future Oculus HMD devices (default to DK1 dimensions)
					hmdCreateDesc.SetScreenParameters(mx, my, mwidth, mheight, 0.14976f, 0.0936f, 0.0936f*0.5f, 0.0635f);
				}
                else
                {   // Duct-tape prototype
                    hmdCreateDesc.SetScreenParameters(mx, my, mwidth, mheight, 0.12096f, 0.0756f, 0.0756f*0.5f, 0.0635f);
                }

                OVR_DEBUG_LOG_TEXT(("DeviceManager - HMD Found %s - %s\n",
                                    deviceId.ToCStr(), displayDeviceName.ToCStr()));

                // Notify caller about detected device. This will call EnumerateAddDevice
                // if the this is the first time device was detected.
                visitor.Visit(hmdCreateDesc);
                foundHMD = true;
                break;
            }
        }
    }

    // Real HMD device is not found; however, we still may have a 'fake' HMD
    // device created via SensorDeviceImpl::EnumerateHMDFromSensorDisplayInfo.
    // Need to find it and set 'Enumerated' to true to avoid Removal notification.
    if (!foundHMD)
    {
        Ptr<DeviceCreateDesc> hmdDevDesc = getManager()->FindDevice("", Device_HMD);
        if (hmdDevDesc)
            hmdDevDesc->Enumerated = true;
    }
}

#include "OVR_Common_HMDDevice.cpp"

}} // namespace OVR::Win32


