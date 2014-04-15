/************************************************************************************

Filename    :   OVR_Sensor2Impl.cpp
Content     :   DK2 sensor device specific implementation.
Created     :   January 21, 2013
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

#include "OVR_Sensor2Impl.h"
#include "OVR_SensorImpl_Common.h"
#include "OVR_Sensor2ImplUtil.h"
#include "Kernel/OVR_Alg.h"

//extern FILE *SF_LOG_fp;

namespace OVR {

//-------------------------------------------------------------------------------------
// ***** Oculus Sensor2-specific packet data structures

enum {    
    Sensor2_VendorId            = Oculus_VendorId,
    Sensor2_ProductId           = 0x0021,

    Sensor2_BootLoader          = 0x1001,

    Sensor2_DefaultReportRate   = 1000, // Hz
};


// Messages we care for
enum Tracker2MessageType
{
    Tracker2Message_None              = 0,
    Tracker2Message_Sensors           = 11,
    Tracker2Message_Unknown           = 0x100,
    Tracker2Message_SizeError         = 0x101,
};


struct Tracker2Sensors
{
    UInt16	LastCommandID;
    UByte	NumSamples;
    UInt16	RunningSampleCount;				// Named 'SampleCount' in the firmware docs.
    SInt16	Temperature;
	UInt32	SampleTimestamp;
    TrackerSample Samples[2];
    SInt16	MagX, MagY, MagZ;
    UInt16	FrameCount;
	UInt32	FrameTimestamp;
    UByte	FrameID;
    UByte	CameraPattern;
    UInt16	CameraFrameCount;				// Named 'CameraCount' in the firmware docs.
	UInt32	CameraTimestamp;

    Tracker2MessageType Decode(const UByte* buffer, int size)
    {
        if (size < 64)
            return Tracker2Message_SizeError;

		LastCommandID		= DecodeUInt16(buffer + 1);
        NumSamples			= buffer[3];
        RunningSampleCount	= DecodeUInt16(buffer + 4);
        Temperature			= DecodeSInt16(buffer + 6);
        SampleTimestamp		= DecodeUInt32(buffer + 8);
        
		// Only unpack as many samples as there actually are.
        UByte iterationCount = (NumSamples > 1) ? 2 : NumSamples;

        for (UByte i = 0; i < iterationCount; i++)
        {
			UnpackSensor(buffer + 12 + 16 * i, &Samples[i].AccelX, &Samples[i].AccelY, &Samples[i].AccelZ);
            UnpackSensor(buffer + 20 + 16 * i, &Samples[i].GyroX,  &Samples[i].GyroY,  &Samples[i].GyroZ);
		}

        MagX = DecodeSInt16(buffer + 44);
        MagY = DecodeSInt16(buffer + 46);
        MagZ = DecodeSInt16(buffer + 48);

		FrameCount = DecodeUInt16(buffer + 50);

		FrameTimestamp		= DecodeUInt32(buffer + 52);
		FrameID				= buffer[56];
		CameraPattern		= buffer[57];
		CameraFrameCount	= DecodeUInt16(buffer + 58);
		CameraTimestamp		= DecodeUInt32(buffer + 60);
        
        return Tracker2Message_Sensors;
    }
};

struct Tracker2Message
{
    Tracker2MessageType Type;
    Tracker2Sensors     Sensors;
};

// Sensor reports data in the following coordinate system:
// Accelerometer: 10^-4 m/s^2; X forward, Y right, Z Down.
// Gyro:          10^-4 rad/s; X positive roll right, Y positive pitch up; Z positive yaw right.


// We need to convert it to the following RHS coordinate system:
// X right, Y Up, Z Back (out of screen)
//
Vector3f AccelFromBodyFrameUpdate(const Tracker2Sensors& update, UByte sampleNumber)
{
    const TrackerSample& sample = update.Samples[sampleNumber];
    float                ax = (float)sample.AccelX;
    float                ay = (float)sample.AccelY;
    float                az = (float)sample.AccelZ;

    return Vector3f(ax, ay, az) * 0.0001f;
}


Vector3f MagFromBodyFrameUpdate(const Tracker2Sensors& update)
{   
    return Vector3f( (float)update.MagX, (float)update.MagY, (float)update.MagZ) * 0.0001f;
}

Vector3f EulerFromBodyFrameUpdate(const Tracker2Sensors& update, UByte sampleNumber)
{
    const TrackerSample& sample = update.Samples[sampleNumber];
    float                gx = (float)sample.GyroX;
    float                gy = (float)sample.GyroY;
    float                gz = (float)sample.GyroZ;

    return Vector3f(gx, gy, gz) * 0.0001f;
}

bool  Sensor2DeviceImpl::decodeTracker2Message(Tracker2Message* message, UByte* buffer, int size)
{
    memset(message, 0, sizeof(Tracker2Message));

    if (size < 4)
    {
        message->Type = Tracker2Message_SizeError;
        return false;
    }

    switch (buffer[0])
    {
    case Tracker2Message_Sensors:
        message->Type = message->Sensors.Decode(buffer, size);
        break;

    default:
        message->Type = Tracker2Message_Unknown;
        break;
    }

    return (message->Type < Tracker2Message_Unknown) && (message->Type != Tracker2Message_None);
}

//-------------------------------------------------------------------------------------
// ***** Sensor2Device

Sensor2DeviceImpl::Sensor2DeviceImpl(SensorDeviceCreateDesc* createDesc)
    :   SensorDeviceImpl(createDesc),
        LastNumSamples(0),
        LastRunningSampleCount(0),
        FullCameraFrameCount(0),
        LastCameraTime("C"),
        LastFrameTime("F"),
        LastSensorTime("S"),
        LastFrameTimestamp(0)
{
    // 15 samples ok in min-window for DK2 since it uses microsecond clock.
    TimeFilter = SensorTimeFilter(SensorTimeFilter::Settings(15));

    pCalibration = new SensorCalibration(this);
}

Sensor2DeviceImpl::~Sensor2DeviceImpl()
{
    delete pCalibration;
}

void Sensor2DeviceImpl::openDevice()
{

    // Read the currently configured range from sensor.
    SensorRangeImpl sr(SensorRange(), 0);

    if (GetInternalDevice()->GetFeatureReport(sr.Buffer, SensorRangeImpl::PacketSize))
    {
        sr.Unpack();
        sr.GetSensorRange(&CurrentRange);
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
	Coordinates = Coord_HMD; // TODO temporary to force it behave

    // Read/Apply sensor config.
    setCoordinateFrame(Coordinates);
    setReportRate(Sensor2_DefaultReportRate);
    setOnboardCalibrationEnabled(false);

    // Must send DK2 keep-alive. Set Keep-alive at 10 seconds.
    KeepAliveMuxReport keepAlive;
    keepAlive.CommandId = 0;
    keepAlive.INReport = 11;
    keepAlive.Interval = 10 * 1000;

    // Device creation is done from background thread so we don't need to add this to the command queue.
    KeepAliveMuxImpl keepAliveImpl(keepAlive);
    GetInternalDevice()->SetFeatureReport(keepAliveImpl.Buffer, KeepAliveMuxImpl::PacketSize);

    // Read the temperature  data from the device
    pCalibration->Initialize();
}

bool Sensor2DeviceImpl::SetTrackingReport(const TrackingReport& data)
{ 
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::setTrackingReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::setTrackingReport(const TrackingReport& data)
{
    TrackingImpl ci(data);
    return GetInternalDevice()->SetFeatureReport(ci.Buffer, TrackingImpl::PacketSize);
}

bool Sensor2DeviceImpl::GetTrackingReport(TrackingReport* data)
{
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::getTrackingReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::getTrackingReport(TrackingReport* data)
{
    TrackingImpl ci;
    if (GetInternalDevice()->GetFeatureReport(ci.Buffer, TrackingImpl::PacketSize))
    {
        ci.Unpack();
        *data = ci.Settings;
        return true;
    }

    return false;
}

bool Sensor2DeviceImpl::SetDisplayReport(const DisplayReport& data)
{ 
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::setDisplayReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::setDisplayReport(const DisplayReport& data)
{
    DisplayImpl di(data);
    return GetInternalDevice()->SetFeatureReport(di.Buffer, DisplayImpl::PacketSize);
}

bool Sensor2DeviceImpl::GetDisplayReport(DisplayReport* data)
{
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::getDisplayReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::getDisplayReport(DisplayReport* data)
{
    DisplayImpl di;
    if (GetInternalDevice()->GetFeatureReport(di.Buffer, DisplayImpl::PacketSize))
    {
        di.Unpack();
        *data = di.Settings;
        return true;
    }

    return false;
}

bool Sensor2DeviceImpl::SetMagCalibrationReport(const MagCalibrationReport& data)
{ 
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::setMagCalibrationReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::setMagCalibrationReport(const MagCalibrationReport& data)
{
    MagCalibrationImpl mci(data);
    return GetInternalDevice()->SetFeatureReport(mci.Buffer, MagCalibrationImpl::PacketSize);
}

bool Sensor2DeviceImpl::GetMagCalibrationReport(MagCalibrationReport* data)
{
    // direct call if we are already on the device manager thread
    if (GetCurrentThreadId() == GetManagerImpl()->GetThreadId())
    {
        return getMagCalibrationReport(data);
    }

	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::getMagCalibrationReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::getMagCalibrationReport(MagCalibrationReport* data)
{
    MagCalibrationImpl mci;
    if (GetInternalDevice()->GetFeatureReport(mci.Buffer, MagCalibrationImpl::PacketSize))
    {
        mci.Unpack();
        *data = mci.Settings;
        return true;
    }

    return false;
}

bool Sensor2DeviceImpl::SetPositionCalibrationReport(const PositionCalibrationReport& data)
{ 
    Lock::Locker lock(&IndexedReportLock);

	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::setPositionCalibrationReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::setPositionCalibrationReport(const PositionCalibrationReport& data)
{
	UByte version = GetDeviceInterfaceVersion();
	if (version < 5)
	{
		PositionCalibrationImpl_Pre5 pci(data);
		return GetInternalDevice()->SetFeatureReport(pci.Buffer, PositionCalibrationImpl_Pre5::PacketSize);
	}
	
	PositionCalibrationImpl pci(data);
    return GetInternalDevice()->SetFeatureReport(pci.Buffer, PositionCalibrationImpl::PacketSize);
}

bool Sensor2DeviceImpl::GetPositionCalibrationReport(PositionCalibrationReport* data)
{
    Lock::Locker lock(&IndexedReportLock);

	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::getPositionCalibrationReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::getPositionCalibrationReport(PositionCalibrationReport* data)
{
	UByte version = GetDeviceInterfaceVersion();
	if (version < 5)
	{
		PositionCalibrationImpl_Pre5 pci;
		if (GetInternalDevice()->GetFeatureReport(pci.Buffer, PositionCalibrationImpl_Pre5::PacketSize))
		{
			pci.Unpack();
			*data = pci.Settings;
			return true;
		}

		return false;
	}

    PositionCalibrationImpl pci;
    if (GetInternalDevice()->GetFeatureReport(pci.Buffer, PositionCalibrationImpl::PacketSize))
    {
        pci.Unpack();
        *data = pci.Settings;
        return true;
    }

    return false;
}

bool Sensor2DeviceImpl::GetAllPositionCalibrationReports(Array<PositionCalibrationReport>* data)
{
    Lock::Locker lock(&IndexedReportLock);

    PositionCalibrationReport pc;
    bool result = GetPositionCalibrationReport(&pc);
    if (!result)
        return false;

    int positions = pc.NumPositions;
    data->Clear();
    data->Resize(positions);

    for (int i = 0; i < positions; i++)
    {
        result = GetPositionCalibrationReport(&pc);
        if (!result)
            return false;
        OVR_ASSERT(pc.NumPositions == positions);

        (*data)[pc.PositionIndex] = pc;
        // IMU should be the last one
        OVR_ASSERT(pc.PositionType == (pc.PositionIndex == positions - 1) ? 
            PositionCalibrationReport::PositionType_IMU : PositionCalibrationReport::PositionType_LED);
    }
    return true;
}

bool Sensor2DeviceImpl::SetCustomPatternReport(const CustomPatternReport& data)
{ 
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::setCustomPatternReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::setCustomPatternReport(const CustomPatternReport& data)
{
    CustomPatternImpl cpi(data);
    return GetInternalDevice()->SetFeatureReport(cpi.Buffer, CustomPatternImpl::PacketSize);
}

bool Sensor2DeviceImpl::GetCustomPatternReport(CustomPatternReport* data)
{
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::getCustomPatternReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::getCustomPatternReport(CustomPatternReport* data)
{
    CustomPatternImpl cpi;
    if (GetInternalDevice()->GetFeatureReport(cpi.Buffer, CustomPatternImpl::PacketSize))
    {
        cpi.Unpack();
        *data = cpi.Settings;
        return true;
    }

    return false;
}

bool Sensor2DeviceImpl::SetManufacturingReport(const ManufacturingReport& data)
{ 
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::setManufacturingReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::setManufacturingReport(const ManufacturingReport& data)
{
    ManufacturingImpl mi(data);
    return GetInternalDevice()->SetFeatureReport(mi.Buffer, ManufacturingImpl::PacketSize);
}

bool Sensor2DeviceImpl::GetManufacturingReport(ManufacturingReport* data)
{
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::getManufacturingReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::getManufacturingReport(ManufacturingReport* data)
{
    ManufacturingImpl mi;
    if (GetInternalDevice()->GetFeatureReport(mi.Buffer, ManufacturingImpl::PacketSize))
    {
        mi.Unpack();
        *data = mi.Settings;
        return true;
    }

    return false;
}

bool Sensor2DeviceImpl::SetLensDistortionReport(const LensDistortionReport& data)
{ 
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::setLensDistortionReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::setLensDistortionReport(const LensDistortionReport& data)
{
    LensDistortionImpl ui(data);
    return GetInternalDevice()->SetFeatureReport(ui.Buffer, LensDistortionImpl::PacketSize);
}

bool Sensor2DeviceImpl::GetLensDistortionReport(LensDistortionReport* data)
{
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::getLensDistortionReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::getLensDistortionReport(LensDistortionReport* data)
{
    LensDistortionImpl ui;
    if (GetInternalDevice()->GetFeatureReport(ui.Buffer, LensDistortionImpl::PacketSize))
    {
        ui.Unpack();
        *data = ui.Settings;
        return true;
    }

    return false;
}

bool Sensor2DeviceImpl::SetUUIDReport(const UUIDReport& data)
{ 
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::setUUIDReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::setUUIDReport(const UUIDReport& data)
{
    UUIDImpl ui(data);
    return GetInternalDevice()->SetFeatureReport(ui.Buffer, UUIDImpl::PacketSize);
}

bool Sensor2DeviceImpl::GetUUIDReport(UUIDReport* data)
{
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::getUUIDReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::getUUIDReport(UUIDReport* data)
{
    UUIDImpl ui;
    if (GetInternalDevice()->GetFeatureReport(ui.Buffer, UUIDImpl::PacketSize))
    {
        ui.Unpack();
        *data = ui.Settings;
        return true;
    }

    return false;
}

bool Sensor2DeviceImpl::SetKeepAliveMuxReport(const KeepAliveMuxReport& data)
{ 
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::setKeepAliveMuxReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::setKeepAliveMuxReport(const KeepAliveMuxReport& data)
{
    KeepAliveMuxImpl kami(data);
    return GetInternalDevice()->SetFeatureReport(kami.Buffer, KeepAliveMuxImpl::PacketSize);
}

bool Sensor2DeviceImpl::GetKeepAliveMuxReport(KeepAliveMuxReport* data)
{
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::getKeepAliveMuxReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::getKeepAliveMuxReport(KeepAliveMuxReport* data)
{
    KeepAliveMuxImpl kami;
    if (GetInternalDevice()->GetFeatureReport(kami.Buffer, KeepAliveMuxImpl::PacketSize))
    {
        kami.Unpack();
        *data = kami.Settings;
        return true;
    }

    return false;
}

bool Sensor2DeviceImpl::SetTemperatureReport(const TemperatureReport& data)
{
    Lock::Locker lock(&IndexedReportLock);

    // direct call if we are already on the device manager thread
    if (GetCurrentThreadId() == GetManagerImpl()->GetThreadId())
    {
        return setTemperatureReport(data);
    }

    bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::setTemperatureReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::setTemperatureReport(const TemperatureReport& data)
{
    TemperatureImpl ti(data);
    return GetInternalDevice()->SetFeatureReport(ti.Buffer, TemperatureImpl::PacketSize);
}

bool Sensor2DeviceImpl::GetTemperatureReport(TemperatureReport* data)
{
    Lock::Locker lock(&IndexedReportLock);

    // direct call if we are already on the device manager thread
    if (GetCurrentThreadId() == GetManagerImpl()->GetThreadId())
    {
        return getTemperatureReport(data);
    }

	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::getTemperatureReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::GetAllTemperatureReports(Array<Array<TemperatureReport> >* data)
{
    Lock::Locker lock(&IndexedReportLock);

    TemperatureReport t;
    bool result = GetTemperatureReport(&t);
    if (!result)
        return false;

    int bins = t.NumBins, samples = t.NumSamples;
    data->Clear();
    data->Resize(bins);
    for (int i = 0; i < bins; i++)
        (*data)[i].Resize(samples);

    for (int i = 0; i < bins; i++)
        for (int j = 0; j < samples; j++)
        {
            result = GetTemperatureReport(&t);
            if (!result)
                return false;
            OVR_ASSERT(t.NumBins == bins && t.NumSamples == samples);

            (*data)[t.Bin][t.Sample] = t;
        }
    return true;
}

bool Sensor2DeviceImpl::getTemperatureReport(TemperatureReport* data)
{
    TemperatureImpl ti;
    if (GetInternalDevice()->GetFeatureReport(ti.Buffer, TemperatureImpl::PacketSize))
    {
        ti.Unpack();
        *data = ti.Settings;
        return true;
    }

    return false;
}

bool Sensor2DeviceImpl::GetGyroOffsetReport(GyroOffsetReport* data)
{
    // direct call if we are already on the device manager thread
    if (GetCurrentThreadId() == GetManagerImpl()->GetThreadId())
    {
        return getGyroOffsetReport(data);
    }

    bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &Sensor2DeviceImpl::getGyroOffsetReport, &result, data))
	{
		return false;
	}

	return result;
}

bool Sensor2DeviceImpl::getGyroOffsetReport(GyroOffsetReport* data)
{
    GyroOffsetImpl goi;
    if (GetInternalDevice()->GetFeatureReport(goi.Buffer, GyroOffsetImpl::PacketSize))
    {
        goi.Unpack();
        *data = goi.Settings;
        return true;
    }

    return false;
}

void Sensor2DeviceImpl::onTrackerMessage(Tracker2Message* message)
{
    if (message->Type != Tracker2Message_Sensors)
        return;
    
    const float     sampleIntervalTimeUnit   = (1.0f / 1000.f);
    double          scaledSampleIntervalTimeUnit  = sampleIntervalTimeUnit;
    Tracker2Sensors& s = message->Sensors;
    
    double       absoluteTimeSeconds = 0.0;

    if (SequenceValid)
    {
        UInt32 runningSampleCountDelta;

        if (s.RunningSampleCount < LastRunningSampleCount)
        {
            // The running sample count on the device rolled around the 16 bit counter
            // (expect to happen about once per minute), so RunningSampleCount 
            // needs a high word increment.
            runningSampleCountDelta = ((((int)s.RunningSampleCount) + 0x10000) - (int)LastRunningSampleCount);
        }
        else
        {
            runningSampleCountDelta = (s.RunningSampleCount - LastRunningSampleCount);
        }

        absoluteTimeSeconds = LastSensorTime.TimeSeconds;
        scaledSampleIntervalTimeUnit = TimeFilter.ScaleTimeUnit(sampleIntervalTimeUnit);
 
        // If we missed a small number of samples, replicate the last sample.
        if ((runningSampleCountDelta > LastNumSamples) && (runningSampleCountDelta <= 254))
        {
            if (HandlerRef.HasHandlers())
            {
                MessageBodyFrame sensors(this);

                sensors.AbsoluteTimeSeconds = absoluteTimeSeconds - s.NumSamples * scaledSampleIntervalTimeUnit;
                sensors.TimeDelta     = (float) ((runningSampleCountDelta - LastNumSamples) * scaledSampleIntervalTimeUnit);
                sensors.Acceleration  = LastAcceleration;
                sensors.RotationRate  = LastRotationRate;
                sensors.MagneticField = LastMagneticField;
                sensors.Temperature   = LastTemperature;

                pCalibration->Apply(sensors);
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
    }

    LastNumSamples = s.NumSamples;
    LastRunningSampleCount = s.RunningSampleCount;

    if (HandlerRef.HasHandlers())
    {
        MessageBodyFrame sensors(this);        
        UByte            iterations = s.NumSamples;

        if (s.NumSamples > 2)
        {
            iterations        = 2;
            sensors.TimeDelta = (float) ((s.NumSamples - 1) * scaledSampleIntervalTimeUnit);
        }
        else
        {
            sensors.TimeDelta = (float) scaledSampleIntervalTimeUnit;
        }

        for (UByte i = 0; i < iterations; i++)
        {            
            sensors.AbsoluteTimeSeconds = absoluteTimeSeconds - ( iterations - 1 - i ) * scaledSampleIntervalTimeUnit;
            sensors.Acceleration = AccelFromBodyFrameUpdate(s, i);
            sensors.RotationRate = EulerFromBodyFrameUpdate(s, i);
            sensors.MagneticField= MagFromBodyFrameUpdate(s);
            sensors.Temperature  = s.Temperature * 0.01f;

            pCalibration->Apply(sensors);
            HandlerRef.Call(sensors);

            // TimeDelta for the last two sample is always fixed.
            sensors.TimeDelta = (float) scaledSampleIntervalTimeUnit;
        }

        // Send pixel read only when frame timestamp changes.
        if (LastFrameTimestamp != s.FrameTimestamp)
        {
            MessagePixelRead pixelRead(this);
            // Prepare message for pixel read
            pixelRead.PixelReadValue    = s.FrameID;
            pixelRead.RawFrameTime      = s.FrameTimestamp;
            pixelRead.RawSensorTime     = s.SampleTimestamp;
            pixelRead.SensorTimeSeconds = LastSensorTime.TimeSeconds;
            pixelRead.FrameTimeSeconds  = LastFrameTime.TimeSeconds;

            HandlerRef.Call(pixelRead);
            LastFrameTimestamp = s.FrameTimestamp;
        }

        UInt16 lowFrameCount = (UInt16) FullCameraFrameCount;
        // Send message only when frame counter changes
        if (lowFrameCount != s.CameraFrameCount)
        {
            // check for the rollover in the counter
            if (s.CameraFrameCount < lowFrameCount)
                FullCameraFrameCount += 0x10000;
            // update the low bits
            FullCameraFrameCount = (FullCameraFrameCount & ~0xFFFF) | s.CameraFrameCount;

            MessageExposureFrame vision(this);
            vision.CameraPattern = s.CameraPattern;
            vision.CameraFrameCount = FullCameraFrameCount;
            vision.CameraTimeSeconds = LastCameraTime.TimeSeconds;

            HandlerRef.Call(vision);
        }

        LastAcceleration = sensors.Acceleration;
        LastRotationRate = sensors.RotationRate;
        LastMagneticField= sensors.MagneticField;
        LastTemperature  = sensors.Temperature;

        //LastPixelRead = pixelRead.PixelReadValue;
        //LastPixelReadTimeStamp = LastFrameTime;
    }
    else
    {
        if (s.NumSamples != 0)
		{
			UByte i = (s.NumSamples > 1) ? 1 : 0;
			LastAcceleration  = AccelFromBodyFrameUpdate(s, i);
			LastRotationRate  = EulerFromBodyFrameUpdate(s, i);
			LastMagneticField = MagFromBodyFrameUpdate(s);
			LastTemperature   = s.Temperature * 0.01f;
		}
    }
}

// Helper function to handle wrap-around of timestamps from Tracker2Message and convert them
// to system time.
//   - Any timestamps that didn't increment keep their old system time.
//   - This is a bit tricky since we don't know which one of timestamps has most recent time.
//   - The first timestamp must be the IMU one; we assume that others can't be too much ahead of it

void UpdateDK2Timestamps(SensorTimeFilter& tf,
                         SensorTimestampMapping** timestamps, UInt32 *rawValues, int count)
{
    int     updateIndices[4];
    int     updateCount = 0;
    int     i;
    double  now = Timer::GetSeconds();

    OVR_ASSERT(count <= sizeof(updateIndices)/sizeof(int));

    // Update timestamp wrapping for any values that changed.
    for (i = 0; i < count; i++)
    {        
        UInt32 lowMks = (UInt32)timestamps[i]->TimestampMks;  // Low 32-bits are raw old timestamp.

        if (rawValues[i] != lowMks)
        {
            if (i == 0)
            {
                // Only check for rollover in the IMU timestamp
                if (rawValues[i] < lowMks)
                {
                    LogText("Timestamp %d rollover, was: %u, now: %u\n", i, lowMks, rawValues[i]);
                    timestamps[i]->TimestampMks += 0x100000000;
                }
                // Update the low bits
                timestamps[i]->TimestampMks = (timestamps[i]->TimestampMks & 0xFFFFFFFF00000000) | rawValues[i];
            }
            else
            {
                // Take the high bits from the main timestamp first (not a typo in the first argument!)
                timestamps[i]->TimestampMks = 
                    (timestamps[0]->TimestampMks & 0xFFFFFFFF00000000) | rawValues[i];
                // Now force it into the reasonable range around the expanded main timestamp
                if (timestamps[i]->TimestampMks > timestamps[0]->TimestampMks + 0x1000000)
                    timestamps[i]->TimestampMks -= 0x100000000;
                else if (timestamps[i]->TimestampMks + 0x100000000 < timestamps[0]->TimestampMks + 0x1000000)
                    timestamps[i]->TimestampMks += 0x100000000;
            }

            updateIndices[updateCount] = i;
            updateCount++;
        }
    }


    // TBD: Simplify. Update indices should no longer be needed with new TimeFilter accepting
    //      previous values.
    // We might want to have multi-element checking time roll-over.

    static const double mksToSec = 1.0 / 1000000.0;

    for (int i = 0; i < updateCount; i++)
    {
        SensorTimestampMapping& ts  = *timestamps[updateIndices[i]];

        ts.TimeSeconds = tf.SampleToSystemTime(((double)ts.TimestampMks) * mksToSec,
                                               now, ts.TimeSeconds, ts.DebugTag);
    }
}


void Sensor2DeviceImpl::OnInputReport(UByte* pData, UInt32 length)
{
	bool processed = false;
    if (!processed)
    {
		Tracker2Message message;
        if (decodeTracker2Message(&message, pData, length))
        {
            processed = true;

            // Process microsecond timestamps from DK2 tracker.
            // Mapped and raw values must correspond to one another in each array.
            // IMU timestamp must be the first one!
            SensorTimestampMapping* tsMaps[3] =
            {                
                &LastSensorTime,
                &LastCameraTime,
                &LastFrameTime
            };
            UInt32 tsRawMks[3] =
            {                
                message.Sensors.SampleTimestamp,
                message.Sensors.CameraTimestamp,
                message.Sensors.FrameTimestamp
            };
            // Handle wrap-around and convert samples to system time for any samples that changed.
            UpdateDK2Timestamps(TimeFilter, tsMaps, tsRawMks, sizeof(tsRawMks)/sizeof(tsRawMks[0]));            

            onTrackerMessage(&message);

            /*
            if (SF_LOG_fp)
            {
                static UInt32 lastFrameTs  = 0;
                static UInt32 lastCameraTs = 0;
                
                if ((lastFrameTs != message.Sensors.FrameTimestamp) ||
                    (lastCameraTs = message.Sensors.CameraTimestamp))
                    fprintf(SF_LOG_fp, "msg cameraTs: 0x%X frameTs: 0x%X sensorTs: 0x%X\n",
                            message.Sensors.CameraTimestamp, message.Sensors.FrameTimestamp,
                            message.Sensors.SampleTimestamp);

                lastFrameTs  = message.Sensors.FrameTimestamp;
                lastCameraTs = message.Sensors.CameraTimestamp;
            }
            */            

#if 0
            // Checks for DK2 firmware bug.
            static unsigned SLastSampleTime = 0;
            if ((SLastSampleTime >  message.Sensors.SampleTimestamp) && message.Sensors.SampleTimestamp > 1000000 )
            {
                fprintf(SF_LOG_fp, "*** Sample Timestamp Wrap! ***\n");
                OVR_ASSERT (SLastSampleTime <= message.Sensors.SampleTimestamp);
            }            
            SLastSampleTime = message.Sensors.SampleTimestamp;

            static unsigned SLastCameraTime = 0;
            if ((SLastCameraTime > message.Sensors.CameraTimestamp) && message.Sensors.CameraTimestamp > 1000000 )
            {
                fprintf(SF_LOG_fp, "*** Camera Timestamp Wrap! ***\n");
                OVR_ASSERT (SLastCameraTime <= message.Sensors.CameraTimestamp);
            }            
            SLastCameraTime = message.Sensors.CameraTimestamp;

            static unsigned SLastFrameTime = 0;
            if ((SLastFrameTime > message.Sensors.FrameTimestamp) && message.Sensors.FrameTimestamp > 1000000 )
            {
                fprintf(SF_LOG_fp, "*** Frame Timestamp Wrap! ***\n");
                OVR_ASSERT (SLastFrameTime <= message.Sensors.FrameTimestamp);
            }                        
            SLastFrameTime = message.Sensors.FrameTimestamp;
#endif            
        }       
    }
}

double Sensor2DeviceImpl::OnTicks(double tickSeconds)
{

    if (tickSeconds >= NextKeepAliveTickSeconds)
    {
        // Must send DK2 keep-alive. Set Keep-alive at 10 seconds.
        KeepAliveMuxReport keepAlive;
        keepAlive.CommandId = 0;
        keepAlive.INReport = 11;
        keepAlive.Interval = 10 * 1000;

        // Device creation is done from background thread so we don't need to add this to the command queue.
        KeepAliveMuxImpl keepAliveImpl(keepAlive);
        GetInternalDevice()->SetFeatureReport(keepAliveImpl.Buffer, KeepAliveMuxImpl::PacketSize);

		// Emit keep-alive every few seconds.
        double keepAliveDelta = 3.0;        // Use 3-second interval.
        NextKeepAliveTickSeconds = tickSeconds + keepAliveDelta;
    }
    return NextKeepAliveTickSeconds - tickSeconds;
}

} // namespace OVR
