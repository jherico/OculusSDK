/************************************************************************************

Filename    :   OVR_SensorImpl.h
Content     :   Sensor device specific implementation.
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

#ifndef OVR_SensorImpl_h
#define OVR_SensorImpl_h

#include "OVR_HIDDeviceImpl.h"
#include "OVR_SensorTimeFilter.h"
#include "OVR_Device.h"

#ifdef OVR_OS_ANDROID
#include "OVR_PhoneSensors.h"
#endif

namespace OVR {

struct TrackerMessage;
class ExternalVisitor;

//-------------------------------------------------------------------------------------
// SensorDeviceFactory enumerates Oculus Sensor devices.
class SensorDeviceFactory : public DeviceFactory
{
public:
	static SensorDeviceFactory &GetInstance();

    // Enumerates devices, creating and destroying relevant objects in manager.
    virtual void EnumerateDevices(EnumerateVisitor& visitor);

    virtual bool MatchVendorProduct(UInt16 vendorId, UInt16 productId) const;
    virtual bool DetectHIDDevice(DeviceManager* pdevMgr, const HIDDeviceDesc& desc);
protected:
    DeviceManager* getManager() const { return (DeviceManager*) pManager; }   
};


// Describes a single a Oculus Sensor device and supports creating its instance.
class SensorDeviceCreateDesc : public HIDDeviceCreateDesc
{
public:
    SensorDeviceCreateDesc(DeviceFactory* factory, const HIDDeviceDesc& hidDesc)
        : HIDDeviceCreateDesc(factory, Device_Sensor, hidDesc) { }
    
    virtual DeviceCreateDesc* Clone() const
    {
        return new SensorDeviceCreateDesc(*this);
    }

    virtual DeviceBase* NewDeviceInstance();

    virtual MatchResult MatchDevice(const DeviceCreateDesc& other,
                                    DeviceCreateDesc**) const
    {
        if ((other.Type == Device_Sensor) && (pFactory == other.pFactory))
        {
            const SensorDeviceCreateDesc& s2 = (const SensorDeviceCreateDesc&) other;
            if (MatchHIDDevice(s2.HIDDesc))
                return Match_Found;
        }
        return Match_None;
    }

    virtual bool MatchHIDDevice(const HIDDeviceDesc& hidDesc) const
    {
        // should paths comparison be case insensitive?
        return ((HIDDesc.Path.CompareNoCase(hidDesc.Path) == 0) &&
                (HIDDesc.SerialNumber == hidDesc.SerialNumber) &&
                (HIDDesc.VersionNumber == hidDesc.VersionNumber));
    }

    virtual bool        GetDeviceInfo(DeviceInfo* info) const;
};

// A simple stub for notification of a sensor in Boot Loader mode
// This descriptor does not support the creation of a device, only the detection
// of its existence to warn apps that the sensor device needs firmware.
// The Boot Loader descriptor reuses and is created by the Sensor device factory
// but in the future may use a dedicated factory
class BootLoaderDeviceCreateDesc : public HIDDeviceCreateDesc
{
public:
    BootLoaderDeviceCreateDesc(DeviceFactory* factory, const HIDDeviceDesc& hidDesc)
        : HIDDeviceCreateDesc(factory, Device_BootLoader, hidDesc) { }
    
    virtual DeviceCreateDesc* Clone() const
    {
        return new BootLoaderDeviceCreateDesc(*this);
    }

    // Boot Loader device creation is not allowed
    virtual DeviceBase* NewDeviceInstance() { return NULL; };

    virtual MatchResult MatchDevice(const DeviceCreateDesc& other,
                                    DeviceCreateDesc**) const
    {
        if ((other.Type == Device_BootLoader) && (pFactory == other.pFactory))
        {
            const BootLoaderDeviceCreateDesc& s2 = (const BootLoaderDeviceCreateDesc&) other;
            if (MatchHIDDevice(s2.HIDDesc))
                return Match_Found;
        }
        return Match_None;
    }

    virtual bool MatchHIDDevice(const HIDDeviceDesc& hidDesc) const
    {
        // should paths comparison be case insensitive?
        return ((HIDDesc.Path.CompareNoCase(hidDesc.Path) == 0) &&
                (HIDDesc.SerialNumber == hidDesc.SerialNumber));
    }

    virtual bool        GetDeviceInfo(DeviceInfo* info) const 
    {
        OVR_UNUSED(info);
        return false; 
    }
};


//-------------------------------------------------------------------------------------
// ***** OVR::SensorDisplayInfoImpl

// DisplayInfo obtained from sensor; these values are used to report distortion
// settings and other coefficients.
// Older SensorDisplayInfo will have all zeros, causing the library to apply hard-coded defaults.
// Currently, only resolutions and sizes are used.
struct SensorDisplayInfoImpl
{
    enum  { PacketSize = 56 };
    UByte   Buffer[PacketSize];

    enum
    {
        Mask_BaseFmt    = 0x0f,
        Mask_OptionFmts = 0xf0,
        Base_None       = 0,
        Base_ScreenOnly = 1,
        Base_Distortion = 2,
    };

    UInt16  CommandId;

    UByte   DistortionType;    
    UInt16  HResolution, VResolution;
    float   HScreenSize, VScreenSize;
    float   VCenter;
    float   LensSeparation;
    // Currently these values are not well-measured.
    float   OutsideLensSurfaceToScreen[2];
    // TODO: add DistortionEqn
    // TODO: currently these values are all zeros and the
    //       distortion is hard-coded in HMDDeviceCreateDesc::GetDeviceInfo()
    float   DistortionK[6];

    SensorDisplayInfoImpl();

    void Unpack();
};

//-------------------------------------------------------------------------------------
// ***** OVR::SensorDeviceImpl

// Oculus Sensor interface.

class SensorDeviceImpl : public HIDDeviceImpl<OVR::SensorDevice>
{
public:
     SensorDeviceImpl(SensorDeviceCreateDesc* createDesc);
    ~SensorDeviceImpl();


    // DeviceCommaon interface
    virtual bool Initialize(DeviceBase* parent);
    virtual void Shutdown();
    
    virtual void AddMessageHandler(MessageHandler* handler);

    // HIDDevice::Notifier interface.
    virtual void OnInputReport(UByte* pData, UInt32 length);
    virtual double OnTicks(double tickSeconds);

    // HMD-Mounted sensor has a different coordinate frame.
    virtual void SetCoordinateFrame(CoordinateFrame coordframe);    
    virtual CoordinateFrame GetCoordinateFrame() const;    

    // SensorDevice interface
    virtual bool SetRange(const SensorRange& range, bool waitFlag);
    virtual void GetRange(SensorRange* range) const;

    virtual void GetFactoryCalibration(Vector3f* AccelOffset, Vector3f* GyroOffset,
                                       Matrix4f* AccelMatrix, Matrix4f* GyroMatrix, 
                                       float* Temperature);
    virtual void SetOnboardCalibrationEnabled(bool enabled);
    virtual bool IsMagCalibrated();

    // Sets report rate (in Hz) of MessageBodyFrame messages (delivered through MessageHandler::OnMessage call). 
    // Currently supported maximum rate is 1000Hz. If the rate is set to 500 or 333 Hz then OnMessage will be 
    // called twice or thrice at the same 'tick'. 
    // If the rate is  < 333 then the OnMessage / MessageBodyFrame will be called three
    // times for each 'tick': the first call will contain averaged values, the second
    // and third calls will provide with most recent two recorded samples.
    virtual void        SetReportRate(unsigned rateHz);
    // Returns currently set report rate, in Hz. If 0 - error occurred.
    // Note, this value may be different from the one provided for SetReportRate. The return
    // value will contain the actual rate.
    virtual unsigned    GetReportRate() const;

	bool				SetSerialReport(const SerialReport& data);
    bool				GetSerialReport(SerialReport* data);

    // Hack to create HMD device from sensor display info.
    static void         EnumerateHMDFromSensorDisplayInfo(const SensorDisplayInfoImpl& displayInfo, 
                                                                DeviceFactory::EnumerateVisitor& visitor);

    // These methods actually store data in a JSON file
    virtual bool		SetMagCalibrationReport(const MagCalibrationReport& data);
	virtual bool		GetMagCalibrationReport(MagCalibrationReport* data);

protected:

    virtual void    openDevice();
    void            closeDeviceOnError();

    Void            setCoordinateFrame(CoordinateFrame coordframe);
    bool            setRange(const SensorRange& range);

    Void            setReportRate(unsigned rateHz);

    Void            setOnboardCalibrationEnabled(bool enabled);

	bool	        setSerialReport(const SerialReport& data);
    bool            getSerialReport(SerialReport* data);

    // Called for decoded messages
    void			onTrackerMessage(TrackerMessage* message);
	bool			decodeTrackerMessage(TrackerMessage* message, UByte* buffer, int size);

    // Helpers to reduce casting.
/*
    SensorDeviceCreateDesc* getCreateDesc() const
    { return (SensorDeviceCreateDesc*)pCreateDesc.GetPtr(); }

    HIDDeviceDesc* getHIDDesc() const
    { return &getCreateDesc()->HIDDesc; }    
*/

    // Set if the sensor is located on the HMD.
    // Older prototype firmware doesn't support changing HW coordinates,
    // so we track its state.
    CoordinateFrame Coordinates;
    CoordinateFrame HWCoordinates;
    double      NextKeepAliveTickSeconds;

    bool        SequenceValid;
    UInt16      LastTimestamp;
    UByte       LastSampleCount;
    float       LastTemperature;
    Vector3f    LastAcceleration;
    Vector3f    LastRotationRate;
    Vector3f    LastMagneticField;

    // This tracks wrap around, and should be monotonically increasing.
    UInt32		FullTimestamp;

    // Current sensor range obtained from device. 
    SensorRange MaxValidRange;
    SensorRange CurrentRange;

    // IMU calibration obtained from device.
    Vector3f    AccelCalibrationOffset;
    Vector3f    GyroCalibrationOffset;
    Matrix4f    AccelCalibrationMatrix;
    Matrix4f    GyroCalibrationMatrix;
    float       CalibrationTemperature;
    
    UInt16      OldCommandId;

    SensorTimeFilter TimeFilter;
    double           PrevAbsoluteTime;

#ifdef OVR_OS_ANDROID
    void 	        replaceWithPhoneMag(Vector3f* val);
    PhoneSensors* 	pPhoneSensors;
#endif

private:
    Matrix4f    magCalibration;
    bool        magCalibrated;
};

} // namespace OVR

#endif // OVR_SensorImpl_h
