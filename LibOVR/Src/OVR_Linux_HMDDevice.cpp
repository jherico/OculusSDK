/************************************************************************************

Filename    :   OVR_Linux_HMDDevice.h
Content     :   Linux HMDDevice implementation
Created     :   June 17, 2013
Authors     :   Brant Lewis

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

#include "OVR_Linux_HMDDevice.h"

#include "OVR_Linux_DeviceManager.h"

#include "OVR_Profile.h"

#include "../../3rdParty/EDID/edid.h"

namespace OVR { namespace Linux {

//-------------------------------------------------------------------------------------

HMDDeviceCreateDesc::HMDDeviceCreateDesc(DeviceFactory* factory, const String& displayDeviceName, long dispId)
        : DeviceCreateDesc(factory, Device_HMD),
          DisplayDeviceName(displayDeviceName),
          Contents(0),
          DisplayId(dispId)
{
    DeviceId = DisplayDeviceName;

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
          Contents(other.Contents),
          DisplayId(other.DisplayId)
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
        (DisplayId == s2.DisplayId))
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
        ScreenSizeInMeters = s2.ScreenSizeInMeters;        
        Contents |= Contents_Screen;

        if (s2.Contents & HMDDeviceCreateDesc::Contents_Distortion)
        {
            memcpy(DistortionK, s2.DistortionK, sizeof(float)*4);
            // TODO: DistortionEqn
            Contents |= Contents_Distortion;
        }
        DeviceId          = s2.DeviceId;
        DisplayId         = s2.DisplayId;
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
        DisplayId         = s2.DisplayId;
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
// ***** HMDDeviceFactory

HMDDeviceFactory &HMDDeviceFactory::GetInstance()
{
	static HMDDeviceFactory instance;
	return instance;
}

void HMDDeviceFactory::EnumerateDevices(EnumerateVisitor& visitor)
{
    // For now we'll assume the Rift DK1 is attached in extended monitor mode. Ultimately we need to
    // use XFree86 to enumerate X11 screens in case the Rift is attached as a separate screen.

    bool foundHMD = false;
    Display* display = XOpenDisplay(NULL);
    XRRScreenResources *screen = XRRGetScreenResources(display, DefaultRootWindow(display));
    for (int iscres = screen->noutput - 1; iscres >= 0; --iscres) {
        RROutput output = screen->outputs[iscres];
        MonitorInfo * mi = read_edid_data(display, output);
        if (mi == NULL) {
            continue;
        }

        XRROutputInfo * info = XRRGetOutputInfo (display, screen, output);
        if (info && (0 == memcmp(mi->manufacturer_code, "OVR", 3))) {

            // Generate a device ID string similar to the way Windows does it
            char device_id[32];
            OVR_sprintf(device_id, 32, "%s%04d", mi->manufacturer_code, mi->product_code);

            // The default monitor coordinates
            int mx      = 0;
            int my      = 0;
            int mwidth  = 1280;
            int mheight = 800;

            if (info->connection == RR_Connected && info->crtc) {
                XRRCrtcInfo * crtc_info = XRRGetCrtcInfo (display, screen, info->crtc);
                if (crtc_info)
                {
                    mx = crtc_info->x;
                    my = crtc_info->y;
                    //mwidth = crtc_info->width;
                    //mheight = crtc_info->height;
                    XRRFreeCrtcInfo(crtc_info);
                }
            }

            String deviceID = device_id;
            HMDDeviceCreateDesc hmdCreateDesc(this, deviceID, iscres);

            // Hard-coded defaults in case the device doesn't have the data itself.
            if (strstr(device_id, "OVR0003"))
            {   // DK2 prototypes and variants (default to HmdType_DK2)
                hmdCreateDesc.SetScreenParameters(mx, my, 1920, 1080, 0.12576f, 0.07074f, 0.12576f*0.5f, 0.0635f );
            }
            else if (strstr(device_id, "OVR0002"))
            {   // HD Prototypes (default to HmdType_DKHDProto)
                hmdCreateDesc.SetScreenParameters(mx, my, 1920, 1080, 0.12096f, 0.06804f, 0.06804f*0.5f, 0.0635f );
            }
            else if (strstr(device_id, "OVR0001"))
            {   // DK1
                hmdCreateDesc.SetScreenParameters(mx, my, mwidth, mheight, 0.14976f, 0.0936f, 0.0936f*0.5f, 0.0635f);
            }
            else if (strstr(device_id, "OVR00"))
            {   // Future Oculus HMD devices (default to DK1 dimensions)
                hmdCreateDesc.SetScreenParameters(mx, my, mwidth, mheight, 0.14976f, 0.0936f, 0.0936f*0.5f, 0.0635f);
            }
            else
            {   // Duct-tape prototype
                hmdCreateDesc.SetScreenParameters(mx, my, mwidth, mheight, 0.12096f, 0.0756f, 0.0756f*0.5f, 0.0635f);
            }

            OVR_DEBUG_LOG_TEXT(("DeviceManager - HMD Found %s - %s\n", device_id, mi->dsc_product_name));

            // Notify caller about detected device. This will call EnumerateAddDevice
            // if the this is the first time device was detected.
            visitor.Visit(hmdCreateDesc);
            foundHMD = true;
            break;
        } // if

        XRRFreeOutputInfo(info);
        delete mi;
    } // for
    XRRFreeScreenResources(screen);


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

}} // namespace OVR::Linux


