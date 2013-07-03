/************************************************************************************

Filename    :   OVR_Posix_HMDDevice.cpp
Content     :   Posix Interface to HMD - detects HMD display
Created     :   June 25, 2013
Authors     :   Michael Antonov

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#include "OVR_Posix_HMDDevice.h"

#include "OVR_Posix_DeviceManager.h"

namespace OVR { namespace Posix {

//-------------------------------------------------------------------------------------

HMDDeviceCreateDesc::HMDDeviceCreateDesc(DeviceFactory* factory,
                                         const String& deviceId, const String& displayDeviceName)
        : DeviceCreateDesc(factory, Device_HMD),
          DeviceId(deviceId), DisplayDeviceName(displayDeviceName),
          DesktopX(0), DesktopY(0), Contents(0),
          HResolution(0), VResolution(0), HScreenSize(0), VScreenSize(0)
{
}
HMDDeviceCreateDesc::HMDDeviceCreateDesc(const HMDDeviceCreateDesc& other)
        : DeviceCreateDesc(other.pFactory, Device_HMD),
          DeviceId(other.DeviceId), DisplayDeviceName(other.DisplayDeviceName),
          DesktopX(other.DesktopX), DesktopY(other.DesktopY), Contents(other.Contents),
          HResolution(other.HResolution), VResolution(other.VResolution),
          HScreenSize(other.HScreenSize), VScreenSize(other.VScreenSize)
{
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
             ((HScreenSize == s2.HScreenSize) &&
              (VScreenSize == s2.VScreenSize)) )
        {
            *pcandidate = 0;
            return Match_Found;
        }
    }


    // DisplayInfo takes precedence, although we try to match it first.
    if ((HResolution == s2.HResolution) &&
        (VResolution == s2.VResolution) &&
        (HScreenSize == s2.HScreenSize) &&
        (VScreenSize == s2.VScreenSize))
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
        HScreenSize = s2.HScreenSize;
        VScreenSize = s2.VScreenSize;
        Contents |= Contents_Screen;

        if (s2.Contents & HMDDeviceCreateDesc::Contents_Distortion)
        {
            memcpy(DistortionK, s2.DistortionK, sizeof(float)*4);
            Contents |= Contents_Distortion;
        }
        DeviceId          = s2.DeviceId;
        DisplayDeviceName = s2.DisplayDeviceName;
        if (newDeviceFlag) *newDeviceFlag = true;
    }
    else if (DeviceId.IsEmpty())
    {
        DeviceId          = s2.DeviceId;
        DisplayDeviceName = s2.DisplayDeviceName;
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

HMDDeviceFactory HMDDeviceFactory::Instance;

void HMDDeviceFactory::EnumerateDevices(EnumerateVisitor& visitor)
{
    bool foundHMD = false;

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

DeviceBase* HMDDeviceCreateDesc::NewDeviceInstance()
{
    return new HMDDevice(this);
}

bool HMDDeviceCreateDesc::Is7Inch() const
{
    return (strstr(DeviceId.ToCStr(), "OVR0001") != 0) || (Contents & Contents_7Inch);
}

bool HMDDeviceCreateDesc::GetDeviceInfo(DeviceInfo* info) const
{
    if ((info->InfoClassType != Device_HMD) &&
        (info->InfoClassType != Device_None))
        return false;

    bool is7Inch = Is7Inch();

    OVR_strcpy(info->ProductName,  DeviceInfo::MaxNameLength,
               is7Inch ? "Oculus Rift DK1" : "Oculus Rift DK1-Prototype");
    OVR_strcpy(info->Manufacturer, DeviceInfo::MaxNameLength, "Oculus VR");
    info->Type    = Device_HMD;
    info->Version = 0;

    // Display detection.
    if (info->InfoClassType == Device_HMD)
    {
        HMDInfo* hmdInfo = static_cast<HMDInfo*>(info);

        hmdInfo->DesktopX               = DesktopX;
        hmdInfo->DesktopY               = DesktopY;
        hmdInfo->HResolution            = HResolution;
        hmdInfo->VResolution            = VResolution;
        hmdInfo->HScreenSize            = HScreenSize;
        hmdInfo->VScreenSize            = VScreenSize;
        hmdInfo->VScreenCenter          = VScreenSize * 0.5f;
        hmdInfo->InterpupillaryDistance = 0.064f;  // Default IPD; should be configurable.
        hmdInfo->LensSeparationDistance = 0.0635f;

        if (Contents & Contents_Distortion)
        {
            memcpy(hmdInfo->DistortionK, DistortionK, sizeof(float)*4);
        }
        else
        {
            if (is7Inch)
            {
                // 7" screen.
                hmdInfo->DistortionK[0]      = 1.0f;
                hmdInfo->DistortionK[1]      = 0.22f;
                hmdInfo->DistortionK[2]      = 0.24f;
                hmdInfo->EyeToScreenDistance = 0.041f;

                hmdInfo->ChromaAbCorrection[0] = 0.996f;
                hmdInfo->ChromaAbCorrection[1] = -0.004f;
                hmdInfo->ChromaAbCorrection[2] = 1.014f;
                hmdInfo->ChromaAbCorrection[3] = 0.0f;
            }
            else
            {
                hmdInfo->DistortionK[0]      = 1.0f;
                hmdInfo->DistortionK[1]      = 0.18f;
                hmdInfo->DistortionK[2]      = 0.115f;
                hmdInfo->EyeToScreenDistance = 0.0387f;
            }
        }

        OVR_strcpy(hmdInfo->DisplayDeviceName, sizeof(hmdInfo->DisplayDeviceName),
                   DisplayDeviceName.ToCStr());
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
    pParent.Clear();
}

OVR::SensorDevice* HMDDevice::GetSensor()
{
    // Just return first sensor found since we have no way to match it yet.
    OVR::SensorDevice* sensor = GetManager()->EnumerateDevices<SensorDevice>().CreateDevice();
    if (sensor)
        sensor->SetCoordinateFrame(SensorDevice::Coord_HMD);
    return sensor;
}

}} // namespace OVR::Posix


