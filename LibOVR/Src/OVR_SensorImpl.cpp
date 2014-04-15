/************************************************************************************

Filename    :   OVR_SensorImpl.cpp
Content     :   Oculus Sensor device implementation.
Created     :   March 7, 2013
Authors     :   Lee Cooper, Dov Katz

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

#include "OVR_SensorImpl.h"
#include "OVR_Sensor2Impl.h"
#include "OVR_SensorImpl_Common.h"
#include "OVR_JSON.h"
#include "OVR_Profile.h"
#include "Kernel/OVR_Alg.h"
#include <time.h>

// HMDDeviceDesc can be created/updated through Sensor carrying DisplayInfo.

#include "Kernel/OVR_Timer.h"

//extern FILE *SF_LOG_fp;

namespace OVR {

using namespace Alg;

//-------------------------------------------------------------------------------------
// ***** Oculus Sensor-specific packet data structures

enum {    
    Sensor_VendorId  = Oculus_VendorId,
    Sensor_Tracker_ProductId = Device_Tracker_ProductId,
    Sensor_Tracker2_ProductId = Device_Tracker2_ProductId,
    Sensor_KTracker_ProductId = Device_KTracker_ProductId,

    Sensor_BootLoader   = 0x1001,

    Sensor_DefaultReportRate = 500, // Hz
    Sensor_MaxReportRate     = 1000 // Hz
};


// Messages we care for
enum TrackerMessageType
{
    TrackerMessage_None              = 0,
    TrackerMessage_Sensors           = 1,
    TrackerMessage_Unknown           = 0x100,
    TrackerMessage_SizeError         = 0x101,
};


struct TrackerSensors
{
    UByte	SampleCount;
    UInt16	Timestamp;
    UInt16	LastCommandID;
    SInt16	Temperature;

    TrackerSample Samples[3];

    SInt16	MagX, MagY, MagZ;

    TrackerMessageType Decode(const UByte* buffer, int size)
    {
        if (size < 62)
            return TrackerMessage_SizeError;

        SampleCount		= buffer[1];
        Timestamp		= DecodeUInt16(buffer + 2);
        LastCommandID	= DecodeUInt16(buffer + 4);
        Temperature		= DecodeSInt16(buffer + 6);
        
        //if (SampleCount > 2)        
        //    OVR_DEBUG_LOG_TEXT(("TackerSensor::Decode SampleCount=%d\n", SampleCount));        

        // Only unpack as many samples as there actually are
        int iterationCount = (SampleCount > 2) ? 3 : SampleCount;

        for (int i = 0; i < iterationCount; i++)
        {
            UnpackSensor(buffer + 8 + 16 * i,  &Samples[i].AccelX, &Samples[i].AccelY, &Samples[i].AccelZ);
            UnpackSensor(buffer + 16 + 16 * i, &Samples[i].GyroX,  &Samples[i].GyroY,  &Samples[i].GyroZ);
        }

        MagX = DecodeSInt16(buffer + 56);
        MagY = DecodeSInt16(buffer + 58);
        MagZ = DecodeSInt16(buffer + 60);

        return TrackerMessage_Sensors;
    }
};

struct TrackerMessage
{
    TrackerMessageType Type;
    TrackerSensors     Sensors;
};


//-------------------------------------------------------------------------------------
// ***** SensorDisplayInfoImpl
SensorDisplayInfoImpl::SensorDisplayInfoImpl()
 :  CommandId(0), DistortionType(Base_None)
{
    memset(Buffer, 0, PacketSize);
    Buffer[0] = 9;
}

void SensorDisplayInfoImpl::Unpack()
{
    CommandId                       = Buffer[1] | (UInt16(Buffer[2]) << 8);
    DistortionType                  = Buffer[3];
    HResolution                     = DecodeUInt16(Buffer+4);
    VResolution                     = DecodeUInt16(Buffer+6);
    HScreenSize                     = DecodeUInt32(Buffer+8) *  (1/1000000.f);
    VScreenSize                     = DecodeUInt32(Buffer+12) * (1/1000000.f);
    VCenter                         = DecodeUInt32(Buffer+16) * (1/1000000.f);
    LensSeparation                  = DecodeUInt32(Buffer+20) * (1/1000000.f);

#if 0
    // These are not well-measured on most devices - probably best to ignore them.
    OutsideLensSurfaceToScreen[0]   = DecodeUInt32(Buffer+24) * (1/1000000.f);
    OutsideLensSurfaceToScreen[1]   = DecodeUInt32(Buffer+28) * (1/1000000.f);
    // TODO: add spline-based distortion.
    // TODO: currently these values are all zeros in the HMD itself.
    DistortionK[0]                  = DecodeFloat(Buffer+32);
    DistortionK[1]                  = DecodeFloat(Buffer+36);
    DistortionK[2]                  = DecodeFloat(Buffer+40);
    DistortionK[3]                  = DecodeFloat(Buffer+44);
    DistortionK[4]                  = DecodeFloat(Buffer+48);
    DistortionK[5]                  = DecodeFloat(Buffer+52);
#else
    // The above are either measured poorly, or don't have values at all.
    // To remove the temptation to use them, set them to junk.
    OutsideLensSurfaceToScreen[0]   = -1.0f;
    OutsideLensSurfaceToScreen[1]   = -1.0f;
    DistortionK[0]                  = -1.0f;
    DistortionK[1]                  = -1.0f;
    DistortionK[2]                  = -1.0f;
    DistortionK[3]                  = -1.0f;
    DistortionK[4]                  = -1.0f;
    DistortionK[5]                  = -1.0f;
#endif
}


//-------------------------------------------------------------------------------------
// ***** SensorDeviceFactory

SensorDeviceFactory SensorDeviceFactory::Instance;

void SensorDeviceFactory::EnumerateDevices(EnumerateVisitor& visitor)
{

    class SensorEnumerator : public HIDEnumerateVisitor
    {
        // Assign not supported; suppress MSVC warning.
        void operator = (const SensorEnumerator&) { }

        DeviceFactory*     pFactory;
        EnumerateVisitor&  ExternalVisitor;   
    public:
        SensorEnumerator(DeviceFactory* factory, EnumerateVisitor& externalVisitor)
            : pFactory(factory), ExternalVisitor(externalVisitor) { }

        virtual bool MatchVendorProduct(UInt16 vendorId, UInt16 productId)
        {
            return pFactory->MatchVendorProduct(vendorId, productId);
        }

        virtual void Visit(HIDDevice& device, const HIDDeviceDesc& desc)
        {
            
            if (desc.ProductId == Sensor_BootLoader)
            {   // If we find a sensor in boot loader mode then notify the app
                // about the existence of the device, but don't allow the app
                // to create or access the device
                BootLoaderDeviceCreateDesc createDesc(pFactory, desc);
                ExternalVisitor.Visit(createDesc);
                return;
            }
            
            SensorDeviceCreateDesc createDesc(pFactory, desc);
            ExternalVisitor.Visit(createDesc);

            // Check if the sensor returns DisplayInfo. If so, try to use it to override potentially
            // mismatching monitor information (in case wrong EDID is reported by splitter),
            // or to create a new "virtualized" HMD Device.
            
            SensorDisplayInfoImpl displayInfo;

            if (device.GetFeatureReport(displayInfo.Buffer, SensorDisplayInfoImpl::PacketSize))
            {
                displayInfo.Unpack();

                // If we got display info, try to match / create HMDDevice as well
                // so that sensor settings give preference.
                if (displayInfo.DistortionType & SensorDisplayInfoImpl::Mask_BaseFmt)
                {
                    SensorDeviceImpl::EnumerateHMDFromSensorDisplayInfo(displayInfo, ExternalVisitor);
                }
            }
        }
    };

    //double start = Timer::GetProfileSeconds();

    SensorEnumerator sensorEnumerator(this, visitor);
    GetManagerImpl()->GetHIDDeviceManager()->Enumerate(&sensorEnumerator);

    //double totalSeconds = Timer::GetProfileSeconds() - start; 
}

bool SensorDeviceFactory::MatchVendorProduct(UInt16 vendorId, UInt16 productId) const
{
    return 	((vendorId == Sensor_VendorId) && (productId == Sensor_Tracker_ProductId)) ||
    		((vendorId == Sensor_VendorId) && (productId == Sensor_Tracker2_ProductId)) ||
    		((vendorId == Sensor_VendorId) && (productId == Sensor_KTracker_ProductId));
}

bool SensorDeviceFactory::DetectHIDDevice(DeviceManager* pdevMgr, const HIDDeviceDesc& desc)
{
    if (MatchVendorProduct(desc.VendorId, desc.ProductId))
    {
        if (desc.ProductId == Sensor_BootLoader)
        {   // If we find a sensor in boot loader mode then notify the app
            // about the existence of the device, but don't allow them
            // to create or access the device
            BootLoaderDeviceCreateDesc createDesc(this, desc);
            pdevMgr->AddDevice_NeedsLock(createDesc);
            return false;  // return false to allow upstream boot loader factories to catch the device
        }
        else
        {
            SensorDeviceCreateDesc createDesc(this, desc);
            return pdevMgr->AddDevice_NeedsLock(createDesc).GetPtr() != NULL;
        }
    }
    return false;
}

//-------------------------------------------------------------------------------------
// ***** SensorDeviceCreateDesc

DeviceBase* SensorDeviceCreateDesc::NewDeviceInstance()
{
    if (HIDDesc.ProductId == Sensor_Tracker2_ProductId)
    {
        return new Sensor2DeviceImpl(this);
    }

    return new SensorDeviceImpl(this);
}

bool SensorDeviceCreateDesc::GetDeviceInfo(DeviceInfo* info) const
{
    if ((info->InfoClassType != Device_Sensor) &&
        (info->InfoClassType != Device_None))
        return false;

    info->Type =            Device_Sensor;
    info->ProductName =     HIDDesc.Product;
    info->Manufacturer =    HIDDesc.Manufacturer;
    info->Version =         HIDDesc.VersionNumber;

    if (info->InfoClassType == Device_Sensor)
    {
        SensorInfo* sinfo = (SensorInfo*)info;
        sinfo->VendorId  = HIDDesc.VendorId;
        sinfo->ProductId = HIDDesc.ProductId;
        sinfo->MaxRanges = SensorRangeImpl::GetMaxSensorRange();
        sinfo->SerialNumber = HIDDesc.SerialNumber;
    }
    return true;
}

//-------------------------------------------------------------------------------------
// ***** SensorDevice

SensorDeviceImpl::SensorDeviceImpl(SensorDeviceCreateDesc* createDesc)
    : OVR::HIDDeviceImpl<OVR::SensorDevice>(createDesc, 0),
      Coordinates(SensorDevice::Coord_Sensor),
      HWCoordinates(SensorDevice::Coord_HMD), // HW reports HMD coordinates by default.
      NextKeepAliveTickSeconds(0),
      FullTimestamp(0),      
      MaxValidRange(SensorRangeImpl::GetMaxSensorRange()),
      magCalibrated(false)
{
    SequenceValid   = false;
    LastSampleCount = 0;
    LastTimestamp   = 0;

    OldCommandId = 0;

    PrevAbsoluteTime = 0.0;

#ifdef OVR_OS_ANDROID
    pPhoneSensors = PhoneSensors::Create();
#endif
}

SensorDeviceImpl::~SensorDeviceImpl()
{
    // Check that Shutdown() was called.
    OVR_ASSERT(!pCreateDesc->pDevice);    
}


// Internal creation APIs.
bool SensorDeviceImpl::Initialize(DeviceBase* parent)
{
    if (HIDDeviceImpl<OVR::SensorDevice>::Initialize(parent))
    {
        openDevice();
        return true;
    }

    return false;
}

void SensorDeviceImpl::openDevice()
{

    // Read the currently configured range from sensor.
    SensorRangeImpl sr(SensorRange(), 0);

    if (GetInternalDevice()->GetFeatureReport(sr.Buffer, SensorRangeImpl::PacketSize))
    {
        sr.Unpack();
        sr.GetSensorRange(&CurrentRange);
        // Increase the magnetometer range, since the default value is not enough in practice
        CurrentRange.MaxMagneticField = 2.5f;
        setRange(CurrentRange);
    }

    // Read the currently configured calibration from sensor.
    SensorFactoryCalibrationImpl sc;
    if (GetInternalDevice()->GetFeatureReport(sc.Buffer, SensorFactoryCalibrationImpl::PacketSize))
    {
        sc.Unpack();
        AccelCalibrationOffset = sc.AccelOffset;
        GyroCalibrationOffset  = sc.GyroOffset;
        AccelCalibrationMatrix = sc.AccelMatrix;
        GyroCalibrationMatrix  = sc.GyroMatrix;
        CalibrationTemperature = sc.Temperature;
    }

    // If the sensor has "DisplayInfo" data, use HMD coordinate frame by default.
    SensorDisplayInfoImpl displayInfo;
    if (GetInternalDevice()->GetFeatureReport(displayInfo.Buffer, SensorDisplayInfoImpl::PacketSize))
    {
        displayInfo.Unpack();
        Coordinates = (displayInfo.DistortionType & SensorDisplayInfoImpl::Mask_BaseFmt) ?
                      Coord_HMD : Coord_Sensor;
    }

    // Read/Apply sensor config.
    setCoordinateFrame(Coordinates);
    setReportRate(Sensor_DefaultReportRate);

    // Set Keep-alive at 10 seconds.
    SensorKeepAliveImpl skeepAlive(10 * 1000);
    GetInternalDevice()->SetFeatureReport(skeepAlive.Buffer, SensorKeepAliveImpl::PacketSize);

    // Load mag calibration
    MagCalibrationReport report;
    bool res = GetMagCalibrationReport(&report);
    if (res && report.Version > 0)
    {
        magCalibration = report.Calibration;
        magCalibrated = true;
    }
}

void SensorDeviceImpl::closeDeviceOnError()
{
    LogText("OVR::SensorDevice - Lost connection to '%s'\n", getHIDDesc()->Path.ToCStr());
    NextKeepAliveTickSeconds = 0;
}

void SensorDeviceImpl::Shutdown()
{   
    HIDDeviceImpl<OVR::SensorDevice>::Shutdown();

    LogText("OVR::SensorDevice - Closed '%s'\n", getHIDDesc()->Path.ToCStr());
}

void SensorDeviceImpl::OnInputReport(UByte* pData, UInt32 length)
{

	bool processed = false;
    if (!processed)
    {
        TrackerMessage message;
        if (decodeTrackerMessage(&message, pData, length))
        {
            processed = true;
            onTrackerMessage(&message);
        }
    }
}

double SensorDeviceImpl::OnTicks(double tickSeconds)
{
    if (tickSeconds >= NextKeepAliveTickSeconds)
    {
        // Use 3-seconds keep alive by default.
        double keepAliveDelta = 3.0;

        // Set Keep-alive at 10 seconds.
        SensorKeepAliveImpl skeepAlive(10 * 1000);
        // OnTicks is called from background thread so we don't need to add this to the command queue.
        GetInternalDevice()->SetFeatureReport(skeepAlive.Buffer, SensorKeepAliveImpl::PacketSize);

		// Emit keep-alive every few seconds.
        NextKeepAliveTickSeconds = tickSeconds + keepAliveDelta;
    }
    return NextKeepAliveTickSeconds - tickSeconds;
}

bool SensorDeviceImpl::SetRange(const SensorRange& range, bool waitFlag)
{
    bool                 result = 0;
    ThreadCommandQueue * threadQueue = GetManagerImpl()->GetThreadQueue();

    if (!waitFlag)
    {
        return threadQueue->PushCall(this, &SensorDeviceImpl::setRange, range);
    }
    
    if (!threadQueue->PushCallAndWaitResult(this, 
                                            &SensorDeviceImpl::setRange,
                                            &result, 
                                            range))
    {
        return false;
    }

    return result;
}

void SensorDeviceImpl::GetRange(SensorRange* range) const
{
    Lock::Locker lockScope(GetLock());
    *range = CurrentRange;
}

bool SensorDeviceImpl::setRange(const SensorRange& range)
{
    SensorRangeImpl sr(range);
    
    if (GetInternalDevice()->SetFeatureReport(sr.Buffer, SensorRangeImpl::PacketSize))
    {
        Lock::Locker lockScope(GetLock());
        sr.GetSensorRange(&CurrentRange);
        return true;
    }
    
    return false;
}

void SensorDeviceImpl::SetCoordinateFrame(CoordinateFrame coordframe)
{ 
    // Push call with wait.
    GetManagerImpl()->GetThreadQueue()->
        PushCall(this, &SensorDeviceImpl::setCoordinateFrame, coordframe, true);
}

SensorDevice::CoordinateFrame SensorDeviceImpl::GetCoordinateFrame() const
{
    return Coordinates;
}

Void SensorDeviceImpl::setCoordinateFrame(CoordinateFrame coordframe)
{

    Coordinates = coordframe;

    // Read the original coordinate frame, then try to change it.
    SensorConfigImpl scfg;
    if (GetInternalDevice()->GetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize))
    {
        scfg.Unpack();
    }

    scfg.SetSensorCoordinates(coordframe == Coord_Sensor);
    scfg.Pack();

    GetInternalDevice()->SetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize);
    
    // Re-read the state, in case of older firmware that doesn't support Sensor coordinates.
    if (GetInternalDevice()->GetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize))
    {
        scfg.Unpack();
        HWCoordinates = scfg.IsUsingSensorCoordinates() ? Coord_Sensor : Coord_HMD;
    }
    else
    {
        HWCoordinates = Coord_HMD;
    }
    return 0;
}

void SensorDeviceImpl::SetReportRate(unsigned rateHz)
{ 
    // Push call with wait.
    GetManagerImpl()->GetThreadQueue()->
        PushCall(this, &SensorDeviceImpl::setReportRate, rateHz, true);
}

unsigned SensorDeviceImpl::GetReportRate() const
{
    // Read the original configuration
    SensorConfigImpl scfg;
    if (GetInternalDevice()->GetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize))
    {
        scfg.Unpack();
        return Sensor_MaxReportRate / (scfg.PacketInterval + 1);
    }
    return 0; // error
}

Void SensorDeviceImpl::setReportRate(unsigned rateHz)
{
    // Read the original configuration
    SensorConfigImpl scfg;
    if (GetInternalDevice()->GetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize))
    {
        scfg.Unpack();
    }

    if (rateHz > Sensor_MaxReportRate)
        rateHz = Sensor_MaxReportRate;
    else if (rateHz == 0)
        rateHz = Sensor_DefaultReportRate;

    scfg.PacketInterval = UInt16((Sensor_MaxReportRate / rateHz) - 1);

    scfg.Pack();

    GetInternalDevice()->SetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize);
    return 0;
}

void SensorDeviceImpl::GetFactoryCalibration(Vector3f* AccelOffset, Vector3f* GyroOffset,
                                             Matrix4f* AccelMatrix, Matrix4f* GyroMatrix, 
                                             float* Temperature)
{
    *AccelOffset = AccelCalibrationOffset;
    *GyroOffset  = GyroCalibrationOffset;
    *AccelMatrix = AccelCalibrationMatrix;
    *GyroMatrix  = GyroCalibrationMatrix;
    *Temperature = CalibrationTemperature;
}

void SensorDeviceImpl::SetOnboardCalibrationEnabled(bool enabled)
{
    // Push call with wait.
    GetManagerImpl()->GetThreadQueue()->
        PushCall(this, &SensorDeviceImpl::setOnboardCalibrationEnabled, enabled, true);
}

Void SensorDeviceImpl::setOnboardCalibrationEnabled(bool enabled)
{
    // Read the original configuration
    SensorConfigImpl scfg;
    if (GetInternalDevice()->GetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize))
    {
        scfg.Unpack();
    }

    if (enabled)
        scfg.Flags |= (SensorConfigImpl::Flag_AutoCalibration | SensorConfigImpl::Flag_UseCalibration);
    else
        scfg.Flags &= ~(SensorConfigImpl::Flag_AutoCalibration | SensorConfigImpl::Flag_UseCalibration);

    scfg.Pack();

    GetInternalDevice()->SetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize);
    return 0;
}

void SensorDeviceImpl::AddMessageHandler(MessageHandler* handler)
{
    if (handler)
        SequenceValid = false;
    DeviceBase::AddMessageHandler(handler);
}

// Sensor reports data in the following coordinate system:
// Accelerometer: 10^-4 m/s^2; X forward, Y right, Z Down.
// Gyro:          10^-4 rad/s; X positive roll right, Y positive pitch up; Z positive yaw right.


// We need to convert it to the following RHS coordinate system:
// X right, Y Up, Z Back (out of screen)
//
Vector3f AccelFromBodyFrameUpdate(const TrackerSensors& update, UByte sampleNumber,
                                  bool convertHMDToSensor = false)
{
    const TrackerSample& sample = update.Samples[sampleNumber];
    float                ax = (float)sample.AccelX;
    float                ay = (float)sample.AccelY;
    float                az = (float)sample.AccelZ;

    Vector3f val = convertHMDToSensor ? Vector3f(ax, az, -ay) :  Vector3f(ax, ay, az);
    return val * 0.0001f;
}


Vector3f MagFromBodyFrameUpdate(const TrackerSensors& update,
                                Matrix4f magCalibration,
                                bool convertHMDToSensor = false)
{   
    float mx = (float)update.MagX;
    float my = (float)update.MagY;
    float mz = (float)update.MagZ;
    // Note: Y and Z are swapped in comparison to the Accel.  
    // This accounts for DK1 sensor firmware axis swap, which should be undone in future releases.
    Vector3f mag = convertHMDToSensor ? Vector3f(mx, my, -mz) : Vector3f(mx, mz, my);
    mag *= 0.0001f;
    // Apply calibration
    return magCalibration.Transform(mag);
}

Vector3f EulerFromBodyFrameUpdate(const TrackerSensors& update, UByte sampleNumber,
                                  bool convertHMDToSensor = false)
{
    const TrackerSample& sample = update.Samples[sampleNumber];
    float                gx = (float)sample.GyroX;
    float                gy = (float)sample.GyroY;
    float                gz = (float)sample.GyroZ;

    Vector3f val = convertHMDToSensor ? Vector3f(gx, gz, -gy) :  Vector3f(gx, gy, gz);
    return val * 0.0001f;
}

bool  SensorDeviceImpl::decodeTrackerMessage(TrackerMessage* message, UByte* buffer, int size)
{
    memset(message, 0, sizeof(TrackerMessage));

    if (size < 4)
    {
        message->Type = TrackerMessage_SizeError;
        return false;
    }

    switch (buffer[0])
    {
    case TrackerMessage_Sensors:
        message->Type = message->Sensors.Decode(buffer, size);
        break;

    default:
        message->Type = TrackerMessage_Unknown;
        break;
    }

    return (message->Type < TrackerMessage_Unknown) && (message->Type != TrackerMessage_None);
}

void SensorDeviceImpl::onTrackerMessage(TrackerMessage* message)
{
    if (message->Type != TrackerMessage_Sensors)
        return;
    
    const double    timeUnit        = (1.0 / 1000.0);
    double          scaledTimeUnit  = timeUnit;
    TrackerSensors& s               = message->Sensors;
    // DK1 timestamps the first sample, so the actual device time will be later
    // by the time we get the message if there are multiple samples.
    int             timestampAdjust = (s.SampleCount > 0) ? s.SampleCount-1 : 0;

    const double now                 = Timer::GetSeconds();
    double       absoluteTimeSeconds = 0.0;
    

    if (SequenceValid)
    {
        unsigned timestampDelta;

        if (s.Timestamp < LastTimestamp)
        {
        	// The timestamp rolled around the 16 bit counter, so FullTimeStamp
        	// needs a high word increment.
        	FullTimestamp += 0x10000;
            timestampDelta = ((((int)s.Timestamp) + 0x10000) - (int)LastTimestamp);
        }
        else
        {
            timestampDelta = (s.Timestamp - LastTimestamp);
        }
        // Update the low word of FullTimeStamp
        FullTimestamp = ( FullTimestamp & ~0xffff ) | s.Timestamp;       

        double deviceTime   = (FullTimestamp + timestampAdjust) * timeUnit;
        absoluteTimeSeconds = TimeFilter.SampleToSystemTime(deviceTime, now, PrevAbsoluteTime);
        scaledTimeUnit      = TimeFilter.ScaleTimeUnit(timeUnit);
        PrevAbsoluteTime    = absoluteTimeSeconds;
        
        // If we missed a small number of samples, generate the sample that would have immediately
        // proceeded the current one. Re-use the IMU values from the last processed sample.
        if ((timestampDelta > LastSampleCount) && (timestampDelta <= 254))
        {
            if (HandlerRef.HasHandlers())
            {
                MessageBodyFrame sensors(this);

                sensors.AbsoluteTimeSeconds = absoluteTimeSeconds - s.SampleCount * scaledTimeUnit;
                sensors.TimeDelta           = (float)((timestampDelta - LastSampleCount) * scaledTimeUnit);
                sensors.Acceleration        = LastAcceleration;
                sensors.RotationRate        = LastRotationRate;
                sensors.MagneticField       = LastMagneticField;
                sensors.Temperature         = LastTemperature;
                sensors.MagCalibrated       = magCalibrated;

                HandlerRef.Call(sensors);
            }
        }
    }
    else
    {
        LastAcceleration = Vector3f(0);
        LastRotationRate = Vector3f(0);
        LastMagneticField= Vector3f(0);
        LastTemperature  = 0;
        SequenceValid    = true;

        // This is our baseline sensor to host time delta,
        // it will be adjusted with each new message.
        FullTimestamp = s.Timestamp;

        double deviceTime   = (FullTimestamp + timestampAdjust) * timeUnit;
        absoluteTimeSeconds = TimeFilter.SampleToSystemTime(deviceTime, now, PrevAbsoluteTime);
        scaledTimeUnit      = TimeFilter.ScaleTimeUnit(timeUnit);
        PrevAbsoluteTime    = absoluteTimeSeconds;
    }

    LastSampleCount = s.SampleCount;
    LastTimestamp   = s.Timestamp;

    bool convertHMDToSensor = (Coordinates == Coord_Sensor) && (HWCoordinates == Coord_HMD);
	
#ifdef OVR_OS_ANDROID
    // LDC - Normally we get the coordinate system from the tracker.
    // Since KTracker doesn't store it we'll always assume HMD coordinate system.
    convertHMDToSensor = false;
#endif

    if (HandlerRef.HasHandlers())
    {
        MessageBodyFrame sensors(this);
        sensors.MagCalibrated = magCalibrated;
        UByte            iterations = s.SampleCount;

        if (s.SampleCount > 3)
        {
            iterations        = 3;
            sensors.TimeDelta = (float)((s.SampleCount - 2) * scaledTimeUnit);
        }
        else
        {
            sensors.TimeDelta = (float)scaledTimeUnit;
        }

        for (UByte i = 0; i < iterations; i++)
        {     
            sensors.AbsoluteTimeSeconds = absoluteTimeSeconds - ( iterations - 1 - i ) * scaledTimeUnit;
            sensors.Acceleration        = AccelFromBodyFrameUpdate(s, i, convertHMDToSensor);
            sensors.RotationRate        = EulerFromBodyFrameUpdate(s, i, convertHMDToSensor);
            sensors.MagneticField       = MagFromBodyFrameUpdate(s, magCalibration, convertHMDToSensor);

#ifdef OVR_OS_ANDROID
            replaceWithPhoneMag(&(sensors.MagneticField));
#endif
            sensors.Temperature   = s.Temperature * 0.01f;
            HandlerRef.Call(sensors);
            // TimeDelta for the last two sample is always fixed.
            sensors.TimeDelta = (float)scaledTimeUnit;
        }

        LastAcceleration = sensors.Acceleration;
        LastRotationRate = sensors.RotationRate;
        LastMagneticField= sensors.MagneticField;
        LastTemperature  = sensors.Temperature;
    }
    else
    {
        UByte i = (s.SampleCount > 3) ? 2 : (s.SampleCount - 1);
        LastAcceleration  = AccelFromBodyFrameUpdate(s, i, convertHMDToSensor);
        LastRotationRate  = EulerFromBodyFrameUpdate(s, i, convertHMDToSensor);
        LastMagneticField = MagFromBodyFrameUpdate(s, magCalibration, convertHMDToSensor);

#ifdef OVR_OS_ANDROID
        replaceWithPhoneMag(&LastMagneticField);
#endif
        LastTemperature   = s.Temperature * 0.01f;
    }
}


#ifdef OVR_OS_ANDROID

void SensorDeviceImpl::replaceWithPhoneMag(Vector3f* val)
{

	// Native calibrated.
	pPhoneSensors->SetMagSource(PhoneSensors::MagnetometerSource_Native);

	Vector3f magPhone;
	pPhoneSensors->GetLatestMagValue(&magPhone);

	// Phone value is in micro-Tesla. Convert it to Gauss and flip axes.
	magPhone *= 10000.0f/1000000.0f;

	Vector3f res;
	res.x = -magPhone.y;
	res.y = magPhone.x;
	res.z = magPhone.z;

	*val = res;
}
#endif 

const int MAX_DEVICE_PROFILE_MAJOR_VERSION = 1;

// Writes the current calibration for a particular device to a device profile file
bool SensorDeviceImpl::SetMagCalibrationReport(const MagCalibrationReport &data)
{
    // Get device info
    SensorInfo sinfo;
    GetDeviceInfo(&sinfo);
    
    // A named calibration may be specified for calibration in different
    // environments, otherwise the default calibration is used
    const char* calibrationName = "default";

    // Generate a mag calibration event
    JSON* calibration = JSON::CreateObject();
    // (hardcoded for now) the measurement and representation method 
    calibration->AddStringItem("Version", "2.0");   
    calibration->AddStringItem("Name", "default");

    // time stamp the calibration
    char time_str[64];
   
#ifdef OVR_OS_WIN32
    struct tm caltime;
    time_t now = time(0);
    localtime_s(&caltime, &now);
    strftime(time_str, 64, "%Y-%m-%d %H:%M:%S", &caltime);
#else
    struct tm* caltime;
    time_t now = time(0);
    caltime = localtime(&now);
    strftime(time_str, 64, "%Y-%m-%d %H:%M:%S", caltime);
#endif
   
    calibration->AddStringItem("Time", time_str);

    // write the full calibration matrix
    char matrix[256];
    data.Calibration.ToString(matrix, 256);
    calibration->AddStringItem("CalibrationMatrix", matrix);
    // save just the offset, for backwards compatibility
    // this can be removed when we don't want to support 0.2.4 anymore
    Vector3f center(data.Calibration.M[0][3], data.Calibration.M[1][3], data.Calibration.M[2][3]);
    Matrix4f tmp = data.Calibration; tmp.M[0][3] = tmp.M[1][3] = tmp.M[2][3] = 0; tmp.M[3][3] = 1;
    center = tmp.Inverted().Transform(center);
    Matrix4f oldcalmat; oldcalmat.M[0][3] = center.x; oldcalmat.M[1][3] = center.y; oldcalmat.M[2][3] = center.z; 
    oldcalmat.ToString(matrix, 256);
    calibration->AddStringItem("Calibration", matrix);
    
    String path = GetBaseOVRPath(true);
    path += "/Devices.json";

    // Look for a preexisting device file to edit
    Ptr<JSON> root = *JSON::Load(path);
    if (root)
    {   // Quick sanity check of the file type and format before we parse it
        JSON* version = root->GetFirstItem();
        if (version && version->Name == "Oculus Device Profile Version")
        {   
            int major = atoi(version->Value.ToCStr());
            if (major > MAX_DEVICE_PROFILE_MAJOR_VERSION)
            {
                // don't use the file on unsupported major version number
                root->Release();
                root = NULL;
            }
        }
        else
        {
            root->Release();
            root = NULL;
        }
    }

    JSON* device = NULL;
    if (root)
    {
        device = root->GetFirstItem();   // skip the header
        device = root->GetNextItem(device);
        while (device)
        {   // Search for a previous calibration with the same name for this device
            // and remove it before adding the new one
            if (device->Name == "Device")
            {   
                JSON* item = device->GetItemByName("Serial");
                if (item && item->Value == sinfo.SerialNumber)
                {   // found an entry for this device
                    item = device->GetNextItem(item);
                    while (item)
                    {
                        if (item->Name == "MagCalibration")
                        {   
                            JSON* name = item->GetItemByName("Name");
                            if (name && name->Value == calibrationName)
                            {   // found a calibration of the same name
                                item->RemoveNode();
                                item->Release();
                                break;
                            } 
                        }
                        item = device->GetNextItem(item);
                    }


                    /*
                    this is removed temporarily, since this is a sensor fusion setting, not sensor itself
                    should be moved to the correct place when Brant has finished the user profile implementation
                    // update the auto-mag flag
                    item = device->GetItemByName("EnableYawCorrection");
                    if (item)
                        item->dValue = (double)EnableYawCorrection;
                    else
                        device->AddBoolItem("EnableYawCorrection", EnableYawCorrection);*/

                    break;
                }
            }

            device = root->GetNextItem(device);
        }
    }
    else
    {   // Create a new device root
        root = *JSON::CreateObject();
        root->AddStringItem("Oculus Device Profile Version", "1.0");
    }

    if (device == NULL)
    {
        device = JSON::CreateObject();
        device->AddStringItem("Product", sinfo.ProductName);
        device->AddNumberItem("ProductID", sinfo.ProductId);
        device->AddStringItem("Serial", sinfo.SerialNumber);
        // removed temporarily, see above
        //device->AddBoolItem("EnableYawCorrection", EnableYawCorrection);

        root->AddItem("Device", device);
    }

    // Create and the add the new calibration event to the device
    device->AddItem("MagCalibration", calibration);
    return root->Save(path);
}

// Loads a saved calibration for the specified device from the device profile file
bool SensorDeviceImpl::GetMagCalibrationReport(MagCalibrationReport* data)
{
    data->Version = 0;
    data->Calibration.SetIdentity();

    // Get device info
    SensorInfo sinfo;
    GetDeviceInfo(&sinfo);

    // A named calibration may be specified for calibration in different
    // environments, otherwise the default calibration is used
    const char* calibrationName = "default";

    String path = GetBaseOVRPath(true);
    path += "/Devices.json";

    // Load the device profiles
    Ptr<JSON> root = *JSON::Load(path);
    if (root == NULL)
        return false;

    // Quick sanity check of the file type and format before we parse it
    JSON* version = root->GetFirstItem();
    if (version && version->Name == "Oculus Device Profile Version")
    {   
        int major = atoi(version->Value.ToCStr());
        if (major > MAX_DEVICE_PROFILE_MAJOR_VERSION)
            return false;   // don't parse the file on unsupported major version number
    }
    else
    {
        return false;
    }

    JSON* device = root->GetNextItem(version);
    while (device)
    {   // Search for a previous calibration with the same name for this device
        // and remove it before adding the new one
        if (device->Name == "Device")
        {   
            JSON* item = device->GetItemByName("Serial");
            if (item && item->Value == sinfo.SerialNumber)
            {   // found an entry for this device

                JSON* autoyaw = device->GetItemByName("EnableYawCorrection");
                // as a temporary HACK, return no calibration if EnableYawCorrection is off
                // this will force disable yaw correction in SensorFusion
                // proper solution would load the value in the Profile, which SensorFusion can access
                if (autoyaw && autoyaw->dValue == 0)
                    return true;

                item = device->GetNextItem(item);
                while (item)
                {
                    if (item->Name == "MagCalibration")
                    {   
                        JSON* calibration = item;
                        JSON* name = calibration->GetItemByName("Name");
                        if (name && name->Value == calibrationName)
                        {   // found a calibration with this name
                            
                            int major = 0;
                            JSON* version = calibration->GetItemByName("Version");
                            if (version)
                                major = atoi(version->Value.ToCStr());

                            if (major > data->Version && major <= 2)
                            {
                                time_t now;
                                time(&now);

                                // parse the calibration time
                                time_t calibration_time = now;
                                JSON* caltime = calibration->GetItemByName("Time");
                                if (caltime)
                                {
                                    const char* caltime_str = caltime->Value.ToCStr();

                                    tm ct;
                                    memset(&ct, 0, sizeof(tm));
                            
#ifdef OVR_OS_WIN32
                                    struct tm nowtime;
                                    localtime_s(&nowtime, &now);
                                    ct.tm_isdst = nowtime.tm_isdst;
                                    sscanf_s(caltime_str, "%d-%d-%d %d:%d:%d", 
                                        &ct.tm_year, &ct.tm_mon, &ct.tm_mday,
                                        &ct.tm_hour, &ct.tm_min, &ct.tm_sec);
#else
                                    struct tm* nowtime = localtime(&now);
                                    ct.tm_isdst = nowtime->tm_isdst;
                                    sscanf(caltime_str, "%d-%d-%d %d:%d:%d", 
                                        &ct.tm_year, &ct.tm_mon, &ct.tm_mday,
                                        &ct.tm_hour, &ct.tm_min, &ct.tm_sec);
#endif
                                    ct.tm_year -= 1900;
                                    ct.tm_mon--;
                                    calibration_time = mktime(&ct);
                                }
                                                        
                                // parse the calibration matrix
                                JSON* cal = calibration->GetItemByName("CalibrationMatrix");
                                if (!cal)
                                    cal = calibration->GetItemByName("Calibration");
                                if (cal)
                                {
                                    data->Calibration = Matrix4f::FromString(cal->Value.ToCStr());
                                    data->Version = (UByte)major;
                                }
                            }
                        } 
                    }
                    item = device->GetNextItem(item);
                }

                return true;
            }
        }

        device = root->GetNextItem(device);
    }
    
    return true;
}

} // namespace OVR


