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

#ifndef OVR_Linux_HMDDevice_h
#define OVR_Linux_HMDDevice_h

#include "OVR_Linux_DeviceManager.h"
#include "OVR_Profile.h"

namespace OVR { namespace Linux {

class HMDDevice;

//-------------------------------------------------------------------------------------

// HMDDeviceFactory enumerates attached Oculus HMD devices.
//
// This is currently done by matching monitor device strings.

class HMDDeviceFactory : public DeviceFactory
{
public:
    static HMDDeviceFactory &GetInstance();

    // Enumerates devices, creating and destroying relevant objects in manager.
    virtual void EnumerateDevices(EnumerateVisitor& visitor);

protected:
    DeviceManager* getManager() const { return (DeviceManager*) pManager; }
};


class HMDDeviceCreateDesc : public DeviceCreateDesc
{
    friend class HMDDevice;

protected:
    enum
    {
        Contents_Screen     = 1,
        Contents_Distortion = 2,
    };
    String              DeviceId;
    String              DisplayDeviceName;
    struct
    {
        int             X, Y;
    }                   Desktop;
    unsigned int        Contents;

    Sizei               ResolutionInPixels;
    Sizef               ScreenSizeInMeters;
    float               VCenterFromTopInMeters;
    float               LensSeparationInMeters;

    // TODO: update these to splines.
    DistortionEqnType   DistortionEqn;
    float               DistortionK[4];

    long                DisplayId;

public:
    HMDDeviceCreateDesc(DeviceFactory* factory,
                        const String& displayDeviceName, long dispId);
    HMDDeviceCreateDesc(const HMDDeviceCreateDesc& other);

    virtual DeviceCreateDesc* Clone() const
    {
        return new HMDDeviceCreateDesc(*this);
    }

    virtual DeviceBase* NewDeviceInstance();

    virtual MatchResult MatchDevice(const DeviceCreateDesc& other,
                                    DeviceCreateDesc**) const;

    // Matches device by path.
    virtual bool        MatchDevice(const String& path);

    virtual bool        UpdateMatchedCandidate(const DeviceCreateDesc&, bool* newDeviceFlag = NULL);

    virtual bool GetDeviceInfo(DeviceInfo* info) const;

    void  SetScreenParameters(int x, int y,
                              int hres, int vres,
                              float hsize, float vsize,
                              float vCenterFromTopInMeters, float lensSeparationInMeters);
    void SetDistortion(const float* dks);
   
    HmdTypeEnum GetHmdType() const;
};


//-------------------------------------------------------------------------------------

// HMDDevice represents an Oculus HMD device unit. An instance of this class
// is typically created from the DeviceManager.
//  After HMD device is created, we its sensor data can be obtained by 
//  first creating a Sensor object and then wrappig it in SensorFusion.

class HMDDevice : public DeviceImpl<OVR::HMDDevice>
{
public:
    HMDDevice(HMDDeviceCreateDesc* createDesc);
    ~HMDDevice();    

    virtual bool Initialize(DeviceBase* parent);
    virtual void Shutdown();

    // Requests the currently used default profile. This profile affects the
    // settings reported by HMDInfo. 
    virtual Profile*    GetProfile();
    virtual const char* GetProfileName();
    virtual bool        SetProfileName(const char* name);

    // Query associated sensor.
    virtual OVR::SensorDevice* GetSensor();  

protected:
    HMDDeviceCreateDesc* getDesc() const { return (HMDDeviceCreateDesc*)pCreateDesc.GetPtr(); }

    // User name for the profile used with this device.
    String               ProfileName;
    mutable Ptr<Profile> pCachedProfile;
};


}} // namespace OVR::Linux

#endif // OVR_Linux_HMDDevice_h

