/************************************************************************************

Filename    :   OVR_Sensor2Impl.h
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

#ifndef OVR_Sensor2Impl_h
#define OVR_Sensor2Impl_h

#include "OVR_SensorImpl.h"
#include "OVR_SensorCalibration.h"

namespace OVR {
    
struct Tracker2Message;

//-------------------------------------------------------------------------------------
// Used to convert DK2 Mks timestamps to system TimeSeconds
struct SensorTimestampMapping
{        
    UInt64      TimestampMks;
    double      TimeSeconds;
    const char* DebugTag;

    SensorTimestampMapping(const char* debugTag)
        : TimestampMks(0), TimeSeconds(0.0), DebugTag(debugTag) { }
};

//-------------------------------------------------------------------------------------
// ***** OVR::Sensor2DeviceImpl

// Oculus Sensor2 interface.
class Sensor2DeviceImpl : public SensorDeviceImpl
{
public:
     Sensor2DeviceImpl(SensorDeviceCreateDesc* createDesc);
    ~Sensor2DeviceImpl();

    // HIDDevice::Notifier interface.
    virtual void        OnInputReport(UByte* pData, UInt32 length);
    virtual double      OnTicks(double tickSeconds);        

    // Get/set feature reports added for DK2. See 'DK2 Firmware Specification' document details.
    virtual bool        SetTrackingReport(const TrackingReport& data);
	virtual bool        GetTrackingReport(TrackingReport* data);

    virtual bool        SetDisplayReport(const DisplayReport& data);
	virtual bool		GetDisplayReport(DisplayReport* data);

    virtual bool		SetMagCalibrationReport(const MagCalibrationReport& data);
	virtual bool		GetMagCalibrationReport(MagCalibrationReport* data);

    virtual bool		SetPositionCalibrationReport(const PositionCalibrationReport& data);
	        bool        GetPositionCalibrationReport(PositionCalibrationReport* data);
	virtual bool        GetAllPositionCalibrationReports(Array<PositionCalibrationReport>* data);

    virtual bool		SetCustomPatternReport(const CustomPatternReport& data);
	virtual bool		GetCustomPatternReport(CustomPatternReport* data);

    virtual bool		SetKeepAliveMuxReport(const KeepAliveMuxReport& data);
	virtual bool		GetKeepAliveMuxReport(KeepAliveMuxReport* data);

    virtual bool		SetManufacturingReport(const ManufacturingReport& data);
	virtual bool		GetManufacturingReport(ManufacturingReport* data);

    virtual bool		SetUUIDReport(const UUIDReport& data);
    virtual bool		GetUUIDReport(UUIDReport* data);

    virtual bool		SetTemperatureReport(const TemperatureReport& data);
            bool        GetTemperatureReport(TemperatureReport* data);
    virtual bool        GetAllTemperatureReports(Array<Array<TemperatureReport> >*);

    virtual bool        GetGyroOffsetReport(GyroOffsetReport* data);

    virtual bool		SetLensDistortionReport(const LensDistortionReport& data);
	virtual bool		GetLensDistortionReport(LensDistortionReport* data);

protected:
    virtual void        openDevice();

    bool                decodeTracker2Message(Tracker2Message* message, UByte* buffer, int size);

    bool	            setTrackingReport(const TrackingReport& data);
    bool                getTrackingReport(TrackingReport* data);

    bool	            setDisplayReport(const DisplayReport& data);
    bool                getDisplayReport(DisplayReport* data);

    bool	            setMagCalibrationReport(const MagCalibrationReport& data);
    bool                getMagCalibrationReport(MagCalibrationReport* data);

    bool	            setPositionCalibrationReport(const PositionCalibrationReport& data);
    bool                getPositionCalibrationReport(PositionCalibrationReport* data);

    bool	            setCustomPatternReport(const CustomPatternReport& data);
    bool                getCustomPatternReport(CustomPatternReport* data);

    bool	            setKeepAliveMuxReport(const KeepAliveMuxReport& data);
    bool                getKeepAliveMuxReport(KeepAliveMuxReport* data);

    bool	            setManufacturingReport(const ManufacturingReport& data);
    bool                getManufacturingReport(ManufacturingReport* data);

    bool	            setUUIDReport(const UUIDReport& data);
    bool                getUUIDReport(UUIDReport* data);

    bool		        setTemperatureReport(const TemperatureReport& data);
    bool		        getTemperatureReport(TemperatureReport* data);

    bool                getGyroOffsetReport(GyroOffsetReport* data);

    bool	            setLensDistortionReport(const LensDistortionReport& data);
    bool                getLensDistortionReport(LensDistortionReport* data);

    // Called for decoded messages
    void                onTrackerMessage(Tracker2Message* message);

    UByte                   LastNumSamples;
    UInt16		            LastRunningSampleCount;
    UInt32                  FullCameraFrameCount;

    SensorTimestampMapping  LastCameraTime;
    SensorTimestampMapping  LastFrameTime;
    SensorTimestampMapping  LastSensorTime;
    // Record last frame timestamp to know when to send pixelRead messages.
    UInt32                  LastFrameTimestamp;

    SensorCalibration       *pCalibration;

    // This lock is used to protect operations with auto-incrementing indices 
    // (see TemperatureReport and PositionCalibrationReport)
    Lock                    IndexedReportLock;
};

} // namespace OVR

#endif // OVR_Sensor2Impl_h
