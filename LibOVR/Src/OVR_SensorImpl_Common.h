/************************************************************************************

Filename    :   OVR_SensorImpl_Common.h
Content     :   Source common to SensorImpl and Sensor2Impl.
Created     :   January 21, 2014
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

#ifndef OVR_SensorImpl_Common_h
#define OVR_SensorImpl_Common_h

#include "Kernel/OVR_System.h"
#include "OVR_Device.h"

namespace OVR 
{

void UnpackSensor(const UByte* buffer, SInt32* x, SInt32* y, SInt32* z);
void PackSensor(UByte* buffer, SInt32 x, SInt32 y, SInt32 z);

// Sensor HW only accepts specific maximum range values, used to maximize
// the 16-bit sensor outputs. Use these ramps to specify and report appropriate values.
const UInt16 AccelRangeRamp[] = { 2, 4, 8, 16 };
const UInt16 GyroRangeRamp[]  = { 250, 500, 1000, 2000 };
const UInt16 MagRangeRamp[]   = { 880, 1300, 1900, 2500 };

UInt16 SelectSensorRampValue(const UInt16* ramp, unsigned count,
                                    float val, float factor, const char* label);

// SensorScaleImpl provides buffer packing logic for the Sensor Range
// record that can be applied to DK1 sensor through Get/SetFeature. We expose this
// through SensorRange class, which has different units.
struct SensorRangeImpl
{
    enum  { PacketSize = 8 };
    UByte   Buffer[PacketSize];
    
    UInt16  CommandId;
    UInt16  AccelScale;
    UInt16  GyroScale;
    UInt16  MagScale;

    SensorRangeImpl(const SensorRange& r, UInt16 commandId = 0);

    void SetSensorRange(const SensorRange& r, UInt16 commandId = 0);
    void GetSensorRange(SensorRange* r);

    static SensorRange GetMaxSensorRange();

    void  Pack();
    void Unpack();
};

struct SensorConfigImpl
{
    enum  { PacketSize = 7 };
    UByte   Buffer[PacketSize];

    // Flag values for Flags.
    enum {
        Flag_RawMode            = 0x01,
        Flag_CalibrationTest	= 0x02, // Internal test mode
        Flag_UseCalibration		= 0x04,
        Flag_AutoCalibration	= 0x08,
        Flag_MotionKeepAlive    = 0x10,
        Flag_CommandKeepAlive   = 0x20,
        Flag_SensorCoordinates  = 0x40
    };

    UInt16  CommandId;
    UByte   Flags;
    UInt16  PacketInterval;
    UInt16  KeepAliveIntervalMs;

    SensorConfigImpl();

    void    SetSensorCoordinates(bool sensorCoordinates);
    bool    IsUsingSensorCoordinates() const;

    void Pack();
    void Unpack();
};

struct SensorFactoryCalibrationImpl
{
    enum  { PacketSize = 69 };
    UByte   Buffer[PacketSize];
    
    Vector3f AccelOffset;
    Vector3f GyroOffset;
    Matrix4f AccelMatrix;
    Matrix4f GyroMatrix;
    float    Temperature;

    SensorFactoryCalibrationImpl();

    void     Pack();    // Not yet implemented.
    void     Unpack();
};


// SensorKeepAlive - feature report that needs to be sent at regular intervals for sensor
// to receive commands.
struct SensorKeepAliveImpl
{
    enum  { PacketSize = 5 };
    UByte   Buffer[PacketSize];

    UInt16  CommandId;
    UInt16  KeepAliveIntervalMs;

    SensorKeepAliveImpl(UInt16 interval = 0, UInt16 commandId = 0);

    void Pack();
    void Unpack();
};

struct TrackerSample
{
    SInt32 AccelX, AccelY, AccelZ;
    SInt32 GyroX, GyroY, GyroZ;
};

enum LastCommandIdFlags
{
    LastCommandId_Shutter = 1,
    LastCommandId_LEDs    = 2
};

} // namespace OVR

#endif // OVR_SensorImpl_Common_h
