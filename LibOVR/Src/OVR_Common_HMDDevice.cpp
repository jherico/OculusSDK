/************************************************************************************

Filename    :   OVR_Common_HMDDevice.cpp
Content     :   
Created     :
Authors     :

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

// Should be #included from the relevant OVR_YourPlatformHere_HMDDevice.cpp

#include "Kernel/OVR_Alg.h"

//-------------------------------------------------------------------------------------
// ***** HMDDeviceCreateDesc

DeviceBase* HMDDeviceCreateDesc::NewDeviceInstance()
{
    return new HMDDevice(this);
}

void  HMDDeviceCreateDesc::SetScreenParameters(int x, int y,
                              int hres, int vres,
                              float hsize, float vsize,
                              float vCenterFromTopInMeters, float lensSeparationInMeters)
{
    Desktop.X = x;
    Desktop.Y = y;
    ResolutionInPixels = Sizei(hres, vres);
    ScreenSizeInMeters = Sizef(hsize, vsize);
    VCenterFromTopInMeters = vCenterFromTopInMeters;
    LensSeparationInMeters = lensSeparationInMeters;

    Contents |= Contents_Screen;
}


void HMDDeviceCreateDesc::SetDistortion(const float* dks)
{
    for (int i = 0; i < 4; i++)
        DistortionK[i] = dks[i];
    // TODO: add DistortionEqn
    Contents |= Contents_Distortion;
}

HmdTypeEnum HMDDeviceCreateDesc::GetHmdType() const
{
    // Determine the HMD model
    // The closest thing we have to a dependable model indicator are the 
    // the screen characteristics.  Additionally we can check the sensor
    // (on attached devices) to further refine our guess
    HmdTypeEnum hmdType = HmdType_Unknown;

    if ( ResolutionInPixels.w == 1280 )
    {
        if ( ScreenSizeInMeters.w > 0.1497f && ScreenSizeInMeters.w < 0.1498f )
            hmdType = HmdType_DK1;
        else
            hmdType = HmdType_DKProto;
    }
    else if ( ResolutionInPixels.w == 1920 )
    {
        // DKHD protoypes, all 1920x1080
        if ( ScreenSizeInMeters.w > 0.1209f && ScreenSizeInMeters.w < 0.1210f )
        {
            // Screen size 0.12096 x 0.06804
            hmdType = HmdType_DKHDProto;
        }
        else if ( ScreenSizeInMeters.w > 0.1257f && ScreenSizeInMeters.w < 0.1258f )
        {
            // Screen size 0.125 x 0.071
            // Could be a HmdType_DKHDProto566Mi, HmdType_CrystalCoveProto, or DK2
            // - most likely the latter.
            hmdType = HmdType_DK2;

            // If available, check the sensor to determine exactly which variant this is
            if (pDevice)
            {
                Ptr<SensorDevice> sensor = *((HMDDevice*)pDevice)->GetSensor();
                
                SensorInfo sinfo;
                if (sensor && sensor->GetDeviceInfo(&sinfo))
                {
                    if (sinfo.ProductId == 1)
                    {
                        hmdType = HmdType_DKHDProto566Mi;
                    }
                    else
                    {   // Crystal Cove uses 0.# firmware, DK2 uses 1.#
                        int firm_major = Alg::DecodeBCD((sinfo.Version >> 8) & 0x00ff);
                        int firm_minor = Alg::DecodeBCD(sinfo.Version & 0xff);
                        OVR_UNUSED(firm_minor);
                        if (firm_major == 0)
                            hmdType = HmdType_CrystalCoveProto;
                        else
                            hmdType = HmdType_DK2;
                    }
                }
            }
        }
        else if (ScreenSizeInMeters.w > 0.1295f && ScreenSizeInMeters.w < 0.1297f)
        {
            // Screen size 0.1296 x 0.0729
            hmdType = HmdType_DKHD2Proto;
        }
    }
    
    OVR_ASSERT( hmdType != HmdType_Unknown );
    return hmdType;
}

bool HMDDeviceCreateDesc::GetDeviceInfo(DeviceInfo* info) const
{
    if ((info->InfoClassType != Device_HMD) &&
        (info->InfoClassType != Device_None))
        return false;

    HmdTypeEnum hmdType = GetHmdType();
    char const* deviceName = "Oculus HMD";
    switch (hmdType)
    {
        case HmdType_DKProto:          deviceName = "Oculus Rift Prototype";    break;
        case HmdType_DK1:              deviceName = "Oculus Rift DK1";          break;
        case HmdType_DKHDProto:        deviceName = "Oculus Rift DKHD";         break;
        case HmdType_DKHD2Proto:       deviceName = "Oculus Rift DKHD2";        break;
        case HmdType_DKHDProto566Mi:   deviceName = "Oculus Rift DKHD 566 Mi";  break;
        case HmdType_CrystalCoveProto: deviceName = "Oculus Rift Crystal Cove"; break;
        case HmdType_DK2:              deviceName = "Oculus Rift DK2";          break;
    }
   
    info->ProductName = deviceName;
    info->Manufacturer = "Oculus VR";
    info->Type    = Device_HMD;
    info->Version = 0;

    // Display detection.
    if (info->InfoClassType == Device_HMD)
    {
        HMDInfo* hmdInfo = static_cast<HMDInfo*>(info);

        hmdInfo->HmdType                = hmdType;
        hmdInfo->DesktopX               = Desktop.X;
        hmdInfo->DesktopY               = Desktop.Y;
        hmdInfo->ResolutionInPixels     = ResolutionInPixels;                
        hmdInfo->ScreenSizeInMeters     = ScreenSizeInMeters;        // Includes ScreenGapSizeInMeters
        hmdInfo->ScreenGapSizeInMeters  = 0.0f;
        hmdInfo->CenterFromTopInMeters  = VCenterFromTopInMeters;
        hmdInfo->LensSeparationInMeters = LensSeparationInMeters;
        // TODO: any other information we get from the hardware itself should be added to this list

        switch ( hmdInfo->HmdType )
        {
        case HmdType_DKProto:
            // WARNING - estimated.
            hmdInfo->Shutter.Type                             = HmdShutter_RollingTopToBottom;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 60.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.000052f;
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.016580f;
            hmdInfo->Shutter.PixelSettleTime                  = 0.015f; // estimated.
            hmdInfo->Shutter.PixelPersistence                 = hmdInfo->Shutter.VsyncToNextVsync; // Full persistence
            break;
        case HmdType_DK1:
            // Data from specs.
            hmdInfo->Shutter.Type                             = HmdShutter_RollingTopToBottom;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 60.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.00018226f;
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.01620089f;
            hmdInfo->Shutter.PixelSettleTime                  = 0.017f; // estimated.
            hmdInfo->Shutter.PixelPersistence                 = hmdInfo->Shutter.VsyncToNextVsync; // Full persistence
            break;
        case HmdType_DKHDProto:
            // Data from specs.
            hmdInfo->Shutter.Type                             = HmdShutter_RollingRightToLeft;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 60.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.0000859f;
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.0164948f;
            hmdInfo->Shutter.PixelSettleTime                  = 0.012f; // estimated.
            hmdInfo->Shutter.PixelPersistence                 = hmdInfo->Shutter.VsyncToNextVsync; // Full persistence
            break;
        case HmdType_DKHD2Proto:
            // Data from specs.
            hmdInfo->Shutter.Type                             = HmdShutter_RollingRightToLeft;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 60.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.000052f;
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.016580f;
            hmdInfo->Shutter.PixelSettleTime                  = 0.015f; // estimated.
            hmdInfo->Shutter.PixelPersistence                 = hmdInfo->Shutter.VsyncToNextVsync; // Full persistence
            break;
        case HmdType_DKHDProto566Mi:
#if 0
            // Low-persistence global shutter
            hmdInfo->Shutter.Type                             = HmdShutter_Global;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 76.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.0000273f + 0.0131033f;    // Global shutter - first visible scan line is actually the last!
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.000f;                     // Global shutter - all visible at once.
            hmdInfo->Shutter.PixelSettleTime                  = 0.0f;                       // <100us
            hmdInfo->Shutter.PixelPersistence                 = 0.18f * hmdInfo->Shutter.VsyncToNextVsync;     // Confgurable - currently set to 18% of total frame.
#else
            // Low-persistence rolling shutter
            hmdInfo->Shutter.Type                             = HmdShutter_RollingRightToLeft;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 76.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.0000273f;
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.0131033f;
            hmdInfo->Shutter.PixelSettleTime                  = 0.0f;                       // <100us
            hmdInfo->Shutter.PixelPersistence                 = 0.18f * hmdInfo->Shutter.VsyncToNextVsync;     // Confgurable - currently set to 18% of total frame.
#endif
            break;
        case HmdType_CrystalCoveProto:
            // Low-persistence rolling shutter
            hmdInfo->Shutter.Type                             = HmdShutter_RollingRightToLeft;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 76.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.0000273f;
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.0131033f;
            hmdInfo->Shutter.PixelSettleTime                  = 0.0f;                       // <100us
            hmdInfo->Shutter.PixelPersistence                 = 0.18f * hmdInfo->Shutter.VsyncToNextVsync;     // Confgurable - currently set to 18% of total frame.
            break;
        case HmdType_DK2:
            // Low-persistence rolling shutter
            hmdInfo->Shutter.Type                             = HmdShutter_RollingRightToLeft;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 76.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.0000273f;
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.0131033f;
            hmdInfo->Shutter.PixelSettleTime                  = 0.0f;                       // <100us
            hmdInfo->Shutter.PixelPersistence                 = 0.18f * hmdInfo->Shutter.VsyncToNextVsync;     // Confgurable - currently set to 18% of total frame.
            break;
        default: OVR_ASSERT ( false ); break;
        }


        OVR_strcpy(hmdInfo->DisplayDeviceName, sizeof(hmdInfo->DisplayDeviceName),
                   DisplayDeviceName.ToCStr());
#if   defined(OVR_OS_WIN32)
        // Nothing special for Win32.
#elif defined(OVR_OS_MAC)
        hmdInfo->DisplayId = DisplayId;
#elif defined(OVR_OS_LINUX)
        hmdInfo->DisplayId = DisplayId;
#elif defined(OVR_OS_ANDROID)
        hmdInfo->DisplayId = DisplayId;
#else
#error Unknown platform
#endif

    }

    return true;
}





//-------------------------------------------------------------------------------------
// ***** HMDDevice

HMDDevice::HMDDevice(HMDDeviceCreateDesc* createDesc)
    : OVR::DeviceImpl<OVR::HMDDevice>(createDesc, 0)
{
}
HMDDevice::~HMDDevice()
{
}

bool HMDDevice::Initialize(DeviceBase* parent)
{
    pParent = parent;
    return true;
}
void HMDDevice::Shutdown()
{
    ProfileName.Clear();
    pCachedProfile.Clear();
    pParent.Clear();
}

Profile* HMDDevice::GetProfile()
{    
    // Loads and returns a cached profile based on this device and current user
    if (pCachedProfile == NULL)
    {
        ProfileManager* mgr = GetManager()->GetProfileManager();
        const char* profile_name = GetProfileName();
        if (profile_name && profile_name[0])
            pCachedProfile = *mgr->GetProfile(this, profile_name);

        if (pCachedProfile == NULL)
            pCachedProfile = *mgr->GetDefaultProfile(this);
        
    }
    return pCachedProfile.GetPtr();
}

const char* HMDDevice::GetProfileName()
{
    if (ProfileName.IsEmpty())
    {   // If the profile name has not been initialized then
        // retrieve the stored default user for this specific device
        ProfileManager* mgr = GetManager()->GetProfileManager();
        const char* name = mgr->GetDefaultUser(this);
        ProfileName = name;
    }
    
    return ProfileName.ToCStr();
}

bool HMDDevice::SetProfileName(const char* name)
{
    if (ProfileName == name)
        return true;   // already set
    
    // Flush the old profile
    pCachedProfile.Clear();
    if (!name)
    {
        ProfileName.Clear();
        return false;
    }

    // Set the name and attempt to cache the profile
    ProfileName = name;
    if (GetProfile())
    {
        return true;
    }
    else
    {
        ProfileName.Clear();
        return false;
    }
}

OVR::SensorDevice* HMDDevice::GetSensor()
{
    // Just return first sensor found since we have no way to match it yet.    

	// Create DK2 sensor if it exists otherwise create first DK1 sensor.
    SensorDevice* sensor = NULL;

    DeviceEnumerator<SensorDevice> enumerator = GetManager()->EnumerateDevices<SensorDevice>();

    while(enumerator.GetType() != Device_None)
    {
        SensorInfo info;
        enumerator.GetDeviceInfo(&info);
      
        if (info.ProductId == Device_Tracker2_ProductId)
        {
            sensor = enumerator.CreateDevice();
            break;
        }

        enumerator.Next();
    }

    if (sensor == NULL)
    {
        sensor = GetManager()->EnumerateDevices<SensorDevice>().CreateDevice();
    }    

    if (sensor)
	{
        sensor->SetCoordinateFrame(SensorDevice::Coord_HMD);
	}

    return sensor;
}
