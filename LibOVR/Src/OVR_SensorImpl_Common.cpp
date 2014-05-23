/************************************************************************************

Filename    :   OVR_SensorImpl_Common.cpp
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

#include "OVR_SensorImpl_Common.h"
#include "Kernel/OVR_Alg.h"

namespace OVR 
{

void UnpackSensor(const UByte* buffer, SInt32* x, SInt32* y, SInt32* z)
{
    // Sign extending trick
    // from http://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend
    struct {SInt32 x:21;} s;

    *x = s.x = (buffer[0] << 13) | (buffer[1] << 5) | ((buffer[2] & 0xF8) >> 3);
    *y = s.x = ((buffer[2] & 0x07) << 18) | (buffer[3] << 10) | (buffer[4] << 2) |
               ((buffer[5] & 0xC0) >> 6);
    *z = s.x = ((buffer[5] & 0x3F) << 15) | (buffer[6] << 7) | (buffer[7] >> 1);
}

void PackSensor(UByte* buffer, SInt32 x, SInt32 y, SInt32 z)
{
    // Pack 3 32 bit integers into 8 bytes
    buffer[0] = UByte(x >> 13);
    buffer[1] = UByte(x >> 5);
    buffer[2] = UByte((x << 3) | ((y >> 18) & 0x07));
    buffer[3] = UByte(y >> 10);
    buffer[4] = UByte(y >> 2);
    buffer[5] = UByte((y << 6) | ((z >> 15) & 0x3F));
    buffer[6] = UByte(z >> 7);
    buffer[7] = UByte(z << 1);
}

UInt16 SelectSensorRampValue(const UInt16* ramp, unsigned count,
                                    float val, float factor, const char* label)
{    
    UInt16 threshold = (UInt16)(val * factor);

    for (unsigned i = 0; i<count; i++)
    {
        if (ramp[i] >= threshold)
            return ramp[i];
    }
    OVR_DEBUG_LOG(("SensorDevice::SetRange - %s clamped to %0.4f",
                   label, float(ramp[count-1]) / factor));
    OVR_UNUSED2(factor, label);
    return ramp[count-1];
}

SensorRangeImpl::SensorRangeImpl(const SensorRange& r, UInt16 commandId)
{
    SetSensorRange(r, commandId);
}

void SensorRangeImpl::SetSensorRange(const SensorRange& r, UInt16 commandId)
{
    CommandId  = commandId;
    AccelScale = SelectSensorRampValue(AccelRangeRamp, sizeof(AccelRangeRamp)/sizeof(AccelRangeRamp[0]),
                                        r.MaxAcceleration, (1.0f / 9.81f), "MaxAcceleration");
    GyroScale  = SelectSensorRampValue(GyroRangeRamp, sizeof(GyroRangeRamp)/sizeof(GyroRangeRamp[0]),
                                        r.MaxRotationRate, Math<float>::RadToDegreeFactor, "MaxRotationRate");
    MagScale   = SelectSensorRampValue(MagRangeRamp, sizeof(MagRangeRamp)/sizeof(MagRangeRamp[0]),
                                        r.MaxMagneticField, 1000.0f, "MaxMagneticField");
    Pack();
}

void SensorRangeImpl::GetSensorRange(SensorRange* r)
{
    r->MaxAcceleration = AccelScale * 9.81f;
    r->MaxRotationRate = DegreeToRad((float)GyroScale);
    r->MaxMagneticField= MagScale * 0.001f;
}

SensorRange SensorRangeImpl::GetMaxSensorRange()
{
    return SensorRange(AccelRangeRamp[sizeof(AccelRangeRamp)/sizeof(AccelRangeRamp[0]) - 1] * 9.81f,
                        GyroRangeRamp[sizeof(GyroRangeRamp)/sizeof(GyroRangeRamp[0]) - 1] *
                            Math<float>::DegreeToRadFactor,
                        MagRangeRamp[sizeof(MagRangeRamp)/sizeof(MagRangeRamp[0]) - 1] * 0.001f);
}

void SensorRangeImpl::Pack()
{
    Buffer[0] = 4;
    Buffer[1] = UByte(CommandId & 0xFF);
    Buffer[2] = UByte(CommandId >> 8);
    Buffer[3] = UByte(AccelScale);
    Buffer[4] = UByte(GyroScale & 0xFF);
    Buffer[5] = UByte(GyroScale >> 8);
    Buffer[6] = UByte(MagScale & 0xFF);
    Buffer[7] = UByte(MagScale >> 8);
}

void SensorRangeImpl::Unpack()
{
    CommandId = Buffer[1] | (UInt16(Buffer[2]) << 8);
    AccelScale= Buffer[3];
    GyroScale = Buffer[4] | (UInt16(Buffer[5]) << 8);
    MagScale  = Buffer[6] | (UInt16(Buffer[7]) << 8);
}

SensorConfigImpl::SensorConfigImpl() 
    :   CommandId(0), Flags(0), PacketInterval(0), SampleRate(0)
{
    memset(Buffer, 0, PacketSize);
    Buffer[0] = 2;
}

void SensorConfigImpl::SetSensorCoordinates(bool sensorCoordinates)
{ 
    Flags = (Flags & ~Flag_SensorCoordinates) | (sensorCoordinates ? Flag_SensorCoordinates : 0); 
}

bool SensorConfigImpl::IsUsingSensorCoordinates() const
{ 
    return (Flags & Flag_SensorCoordinates) != 0; 
}

void SensorConfigImpl::Pack()
{
    Buffer[0] = 2;
    Buffer[1] = UByte(CommandId & 0xFF);
    Buffer[2] = UByte(CommandId >> 8);
    Buffer[3] = Flags;
    Buffer[4] = UByte(PacketInterval);
    Buffer[5] = UByte(SampleRate & 0xFF);
    Buffer[6] = UByte(SampleRate >> 8);
}

void SensorConfigImpl::Unpack()
{
    CommandId		= Buffer[1] | (UInt16(Buffer[2]) << 8);
    Flags			= Buffer[3];
    PacketInterval	= Buffer[4];
    SampleRate		= Buffer[5] | (UInt16(Buffer[6]) << 8);
}

SensorFactoryCalibrationImpl::SensorFactoryCalibrationImpl() 
    : AccelOffset(), GyroOffset(), AccelMatrix(), GyroMatrix(), Temperature(0)
{
    memset(Buffer, 0, PacketSize);
    Buffer[0] = 3;
}

void SensorFactoryCalibrationImpl::Pack()
{
    SInt32 x, y, z;

    Buffer[0] = 3;

    x = SInt32(AccelOffset.x * 1e4f);
    y = SInt32(AccelOffset.y * 1e4f);
    z = SInt32(AccelOffset.z * 1e4f);
    PackSensor(Buffer + 3, x, y, z);

    x = SInt32(GyroOffset.x * 1e4f);
    y = SInt32(GyroOffset.y * 1e4f);
    z = SInt32(GyroOffset.z * 1e4f);
    PackSensor(Buffer + 11, x, y, z);

    // ignore the scale matrices for now
}

void SensorFactoryCalibrationImpl::Unpack()
{
    static const float sensorMax = (1 << 20) - 1;
    SInt32 x, y, z;

    UnpackSensor(Buffer + 3, &x, &y, &z);
    AccelOffset.y = (float) y * 1e-4f;
    AccelOffset.z = (float) z * 1e-4f;
    AccelOffset.x = (float) x * 1e-4f;

    UnpackSensor(Buffer + 11, &x, &y, &z);
    GyroOffset.x = (float) x * 1e-4f;
    GyroOffset.y = (float) y * 1e-4f;
    GyroOffset.z = (float) z * 1e-4f;

    for (int i = 0; i < 3; i++)
    {
        UnpackSensor(Buffer + 19 + 8 * i, &x, &y, &z);
        AccelMatrix.M[i][0] = (float) x / sensorMax;
        AccelMatrix.M[i][1] = (float) y / sensorMax;
        AccelMatrix.M[i][2] = (float) z / sensorMax;
        AccelMatrix.M[i][i] += 1.0f;
    }

    for (int i = 0; i < 3; i++)
    {
        UnpackSensor(Buffer + 43 + 8 * i, &x, &y, &z);
        GyroMatrix.M[i][0] = (float) x / sensorMax;
        GyroMatrix.M[i][1] = (float) y / sensorMax;
        GyroMatrix.M[i][2] = (float) z / sensorMax;
        GyroMatrix.M[i][i] += 1.0f;
    }

    Temperature = (float) Alg::DecodeSInt16(Buffer + 67) / 100.0f;
}

SensorKeepAliveImpl::SensorKeepAliveImpl(UInt16 interval, UInt16 commandId)
    : CommandId(commandId), KeepAliveIntervalMs(interval)
{
    Pack();
}

void SensorKeepAliveImpl::Pack()
{
    Buffer[0] = 8;
    Buffer[1] = UByte(CommandId & 0xFF);
    Buffer[2] = UByte(CommandId >> 8);
    Buffer[3] = UByte(KeepAliveIntervalMs & 0xFF);
    Buffer[4] = UByte(KeepAliveIntervalMs >> 8);
}

void SensorKeepAliveImpl::Unpack()
{
    CommandId          = Buffer[1] | (UInt16(Buffer[2]) << 8);
    KeepAliveIntervalMs= Buffer[3] | (UInt16(Buffer[4]) << 8);
}

} // namespace OVR
