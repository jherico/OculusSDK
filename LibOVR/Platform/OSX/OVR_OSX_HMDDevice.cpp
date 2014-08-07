/************************************************************************************

Filename    :   OVR_OSX_HMDDevice.cpp
Content     :   OSX Interface to HMD - detects HMD display
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

#include "OVR_OSX_HMDDevice.h"

#include "OVR_OSX_DeviceManager.h"
#include "Util/Util_Render_Stereo.h"

#include "OVR_OSX_HMDDevice.h"
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFString.h>
#include <IOKit/graphics/IOGraphicsLib.h>

namespace OVR { namespace OSX {

using namespace OVR::Util::Render;
    
//-------------------------------------------------------------------------------------

HMDDeviceCreateDesc::HMDDeviceCreateDesc(DeviceFactory* factory, 
                                         UInt32 vend, UInt32 prod, const String& displayDeviceName, int dispId)
        : DeviceCreateDesc(factory, Device_HMD),
          DisplayDeviceName(displayDeviceName),
          Contents(0),
          DisplayId(dispId)
{
    OVR_UNUSED(vend);
    OVR_UNUSED(prod);
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
    
    // SensorDisplayInfo may override resolution settings, so store as candidiate.
    if (s2.DeviceId.IsEmpty() && s2.DisplayId == 0)
    {        
        *pcandidate = const_cast<DeviceCreateDesc*>((const DeviceCreateDesc*)this);        
        return Match_Candidate;
    }
    // OTHER HMD Monitor desc may initialize DeviceName/Id
    else if (DeviceId.IsEmpty() && DisplayId == 0)
    {
        *pcandidate = const_cast<DeviceCreateDesc*>((const DeviceCreateDesc*)this);        
        return Match_Candidate;
    }
    
    return Match_None;
}


bool HMDDeviceCreateDesc::UpdateMatchedCandidate(const DeviceCreateDesc& other, bool* newDeviceFlag)
{
    // This candidate was the the "best fit" to apply sensor DisplayInfo to.
    OVR_ASSERT(other.Type == Device_HMD);
    
    const HMDDeviceCreateDesc& s2 = (const HMDDeviceCreateDesc&) other;

    // Force screen size on resolution from SensorDisplayInfo.
    // We do this because USB detection is more reliable as compared to HDMI EDID,
    // which may be corrupted by splitter reporting wrong monitor 
    if (s2.DeviceId.IsEmpty() && s2.DisplayId == 0)
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

    
//-------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------
// ***** HMDDeviceFactory

HMDDeviceFactory &HMDDeviceFactory::GetInstance()
{
	static HMDDeviceFactory instance;
	return instance;
}

void HMDDeviceFactory::EnumerateDevices(EnumerateVisitor& visitor)
{
    CGDirectDisplayID Displays[32];
    uint32_t NDisplays = 0;
    CGGetOnlineDisplayList(32, Displays, &NDisplays);

    for (unsigned int i = 0; i < NDisplays; i++)
    {
        io_service_t port = CGDisplayIOServicePort(Displays[i]);
        CFDictionaryRef DispInfo = IODisplayCreateInfoDictionary(port, kIODisplayMatchingInfo);

        uint32_t vendor = CGDisplayVendorNumber(Displays[i]);
        uint32_t product = CGDisplayModelNumber(Displays[i]);

        CGRect desktop = CGDisplayBounds(Displays[i]);
        
        if (vendor == 16082 && ( (product == 1)||(product == 2)||(product == 3) ) ) // 7" or HD
        {
            char idstring[9];
            idstring[0] = 'A'-1+((vendor>>10) & 31);
            idstring[1] = 'A'-1+((vendor>>5) & 31);
            idstring[2] = 'A'-1+((vendor>>0) & 31);
            snprintf(idstring+3, 5, "%04d", product);

            HMDDeviceCreateDesc hmdCreateDesc(this, vendor, product, idstring, Displays[i]);
            
			// Hard-coded defaults in case the device doesn't have the data itself.
            if (product == 3)
            {   // DK2 prototypes and variants (default to HmdType_DK2)
                hmdCreateDesc.SetScreenParameters(desktop.origin.x, desktop.origin.y,
                                                  1920, 1080, 0.12576f, 0.07074f, 0.07074f*0.5f, 0.0635f );
            }
			else if (product == 2)
			{   // HD Prototypes (default to HmdType_DKHDProto) 
				hmdCreateDesc.SetScreenParameters(desktop.origin.x, desktop.origin.y,
                                                  1920, 1080, 0.12096f, 0.06804f, 0.06804f*0.5f, 0.0635f);
			}
			else if (product == 1)
			{   // DK1
                hmdCreateDesc.SetScreenParameters(desktop.origin.x, desktop.origin.y,
                                                  1280, 800, 0.14976f, 0.0936f, 0.0936f*0.5f, 0.0635f);
            }
            else 
            {   // Future Oculus HMD devices (default to DK1 dimensions)
				hmdCreateDesc.SetScreenParameters(desktop.origin.x, desktop.origin.y,
                                                      1280, 800, 0.14976f, 0.0936f, 0.0936f*0.5f, 0.0635f);
			}
            
            OVR_DEBUG_LOG_TEXT(("DeviceManager - HMD Found %x:%x\n", vendor, product));
            
            // Notify caller about detected device. This will call EnumerateAddDevice
            // if the this is the first time device was detected.
            visitor.Visit(hmdCreateDesc);
        }
        CFRelease(DispInfo);
    }
}

#include "OVR_Common_HMDDevice.cpp"

}} // namespace OVR::OSX


