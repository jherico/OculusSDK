/************************************************************************************

Filename    :   OVR_Sensor2ImplUtil.h
Content     :   DK2 sensor device feature report utils.
Created     :   January 27, 2014
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

#ifndef OVR_Sensor2ImplUtil_h
#define OVR_Sensor2ImplUtil_h

#include "OVR_Device.h"
#include "OVR_SensorImpl_Common.h"
#include "Kernel/OVR_Alg.h"

namespace OVR {

using namespace Alg;

// Tracking feature report.
struct TrackingImpl
{
    enum  { PacketSize = 13 };
    UByte   Buffer[PacketSize];
    
    TrackingReport  Settings;

	TrackingImpl()
	{
		for (int i=0; i<PacketSize; i++)
		{
			Buffer[i] = 0;
		}

		Buffer[0] = 12;
	}

    TrackingImpl(const TrackingReport& settings)
		:	Settings(settings)
    {
		Pack();
	}

    void  Pack()
    {

        Buffer[0] = 12;
        EncodeUInt16 ( Buffer+1, Settings.CommandId );
        Buffer[3] = Settings.Pattern;
        Buffer[4] = UByte(Settings.Enable << 0 |
                          Settings.Autoincrement << 1 |
                          Settings.UseCarrier << 2 |
                          Settings.SyncInput << 3 |
                          Settings.VsyncLock << 4 |
                          Settings.CustomPattern << 5);
        Buffer[5] = 0;
        EncodeUInt16 ( Buffer+6, Settings.ExposureLength );
        EncodeUInt16 ( Buffer+8, Settings.FrameInterval );
        EncodeUInt16 ( Buffer+10, Settings.VsyncOffset );
        Buffer[12] = Settings.DutyCycle;
    }

    void Unpack()
    {
        Settings.CommandId = DecodeUInt16(Buffer+1);
        Settings.Pattern = Buffer[3];
        Settings.Enable = (Buffer[4] & 0x01) != 0;
        Settings.Autoincrement = (Buffer[4] & 0x02) != 0;
        Settings.UseCarrier = (Buffer[4] & 0x04) != 0;
        Settings.SyncInput = (Buffer[4] & 0x08) != 0;
        Settings.VsyncLock = (Buffer[4] & 0x10) != 0;
        Settings.CustomPattern = (Buffer[4] & 0x20) != 0;
        Settings.ExposureLength = DecodeUInt16(Buffer+6);
        Settings.FrameInterval = DecodeUInt16(Buffer+8);
        Settings.VsyncOffset = DecodeUInt16(Buffer+10);
        Settings.DutyCycle = Buffer[12];
    }
};

// Display feature report.
struct DisplayImpl
{
    enum  { PacketSize = 16 };
    UByte   Buffer[PacketSize];
    
    DisplayReport Settings;

	DisplayImpl()
	{
		for (int i=0; i<PacketSize; i++)
		{
			Buffer[i] = 0;
		}

		Buffer[0] = 13;
	}

    DisplayImpl(const DisplayReport& settings)
		:	Settings(settings)
    {
		Pack();
	}

    void  Pack()
    {

        Buffer[0] = 13;
        EncodeUInt16 ( Buffer+1, Settings.CommandId );
        Buffer[3] = Settings.Brightness;
        Buffer[4] = UByte(  (Settings.ShutterType & 0x0F) |
                            (Settings.CurrentLimit & 0x03) << 4 |
                            (Settings.UseRolling ? 0x40 : 0) |
                            (Settings.ReverseRolling ? 0x80 : 0));
        Buffer[5] = UByte(  (Settings.HighBrightness ? 0x01 : 0) |
                            (Settings.SelfRefresh ? 0x02 : 0) |
                            (Settings.ReadPixel ? 0x04 : 0) |
                            (Settings.DirectPentile ? 0x08 : 0));
        EncodeUInt16 ( Buffer+8, Settings.Persistence );
        EncodeUInt16 ( Buffer+10, Settings.LightingOffset );
        EncodeUInt16 ( Buffer+12, Settings.PixelSettle );
        EncodeUInt16 ( Buffer+14, Settings.TotalRows );
    }

    void Unpack()
    {

        Settings.CommandId = DecodeUInt16(Buffer+1);
        Settings.Brightness = Buffer[3];
        Settings.ShutterType = DisplayReport::ShutterTypeEnum(Buffer[4] & 0x0F);
        Settings.CurrentLimit = DisplayReport::CurrentLimitEnum((Buffer[4] >> 4) & 0x02);
        Settings.UseRolling = (Buffer[4] & 0x40) != 0;
        Settings.ReverseRolling = (Buffer[4] & 0x80) != 0;
        Settings.HighBrightness = (Buffer[5] & 0x01) != 0;
        Settings.SelfRefresh = (Buffer[5] & 0x02) != 0;
        Settings.ReadPixel = (Buffer[5] & 0x04) != 0;
        Settings.DirectPentile = (Buffer[5] & 0x08) != 0;
        Settings.Persistence = DecodeUInt16(Buffer+8);
        Settings.LightingOffset = DecodeUInt16(Buffer+10);
        Settings.PixelSettle = DecodeUInt16(Buffer+12);
        Settings.TotalRows = DecodeUInt16(Buffer+14);
    }
};

// MagCalibration feature report.
struct MagCalibrationImpl
{
    enum  { PacketSize = 52 };
    UByte   Buffer[PacketSize];

    MagCalibrationReport Settings;

    MagCalibrationImpl()
    {
        memset(Buffer, 0, sizeof(Buffer));
        Buffer[0] = 14;
    }

    MagCalibrationImpl(const MagCalibrationReport& settings)
        :	Settings(settings)
    {
        Pack();
    }

    void  Pack()
    {
        Buffer[0] = 14;
        EncodeUInt16(Buffer+1, Settings.CommandId);
        Buffer[3] = Settings.Version;

        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 4; j++)
            {
                SInt32 value = SInt32(Settings.Calibration.M[i][j] * 1e4f);
                EncodeSInt32(Buffer + 4 + 4 * (4 * i + j), value);
            }
    }

    void Unpack()
    {
        Settings.CommandId = DecodeUInt16(Buffer+1);
        Settings.Version = Buffer[3];

        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 4; j++)
            {
                SInt32 value = DecodeSInt32(Buffer + 4 + 4 * (4 * i + j));
                Settings.Calibration.M[i][j] = (float)value * 1e-4f;
            }
    }
};

//-------------------------------------------------------------------------------------
// PositionCalibration feature report.
// - Sensor interface versions before 5 do not support Normal and Rotation.

struct PositionCalibrationImpl
{
	enum  { PacketSize = 30 };
	UByte   Buffer[PacketSize];

	PositionCalibrationReport Settings;

	PositionCalibrationImpl()
	{
		for (int i=0; i<PacketSize; i++)
		{
			Buffer[i] = 0;
		}

		Buffer[0] = 15;
	}

	PositionCalibrationImpl(const PositionCalibrationReport& settings)
		:	Settings(settings)
	{
		Pack();
	}

	void  Pack()
	{

		Buffer[0] = 15;
		EncodeUInt16(Buffer+1, Settings.CommandId);
		Buffer[3] = Settings.Version;

        Vector3d position = Settings.Position * 1e6;
		EncodeSInt32(Buffer+4,  (SInt32) position.x);
		EncodeSInt32(Buffer+8,  (SInt32) position.y);
		EncodeSInt32(Buffer+12, (SInt32) position.z);

        Vector3d normal = Settings.Normal * 1e6;
        EncodeSInt16(Buffer+16, (SInt16) normal.x);
		EncodeSInt16(Buffer+18, (SInt16) normal.y);
		EncodeSInt16(Buffer+20, (SInt16) normal.z);

        double rotation = Settings.Angle * 1e4;
		EncodeSInt16(Buffer+22, (SInt16) rotation);

		EncodeUInt16(Buffer+24, Settings.PositionIndex);
		EncodeUInt16(Buffer+26, Settings.NumPositions);
		EncodeUInt16(Buffer+28, UInt16(Settings.PositionType));
	}

	void Unpack()
	{
		Settings.CommandId = DecodeUInt16(Buffer+1);
		Settings.Version = Buffer[3];

		Settings.Position.x = DecodeSInt32(Buffer + 4) * 1e-6;
		Settings.Position.y = DecodeSInt32(Buffer + 8) * 1e-6;
		Settings.Position.z = DecodeSInt32(Buffer + 12) * 1e-6;

		Settings.Normal.x = DecodeSInt16(Buffer + 16) * 1e-6;
		Settings.Normal.y = DecodeSInt16(Buffer + 18) * 1e-6;
		Settings.Normal.z = DecodeSInt16(Buffer + 20) * 1e-6;

		Settings.Angle = DecodeSInt16(Buffer + 22) * 1e-4;

		Settings.PositionIndex = DecodeUInt16(Buffer + 24);
		Settings.NumPositions  = DecodeUInt16(Buffer + 26);

		Settings.PositionType  = PositionCalibrationReport::PositionTypeEnum(DecodeUInt16(Buffer + 28));
	}
};

struct PositionCalibrationImpl_Pre5
{
	enum  { PacketSize = 22 };
	UByte   Buffer[PacketSize];

	PositionCalibrationReport Settings;

	PositionCalibrationImpl_Pre5()
	{
		for (int i=0; i<PacketSize; i++)
		{
			Buffer[i] = 0;
		}

		Buffer[0] = 15;
	}

	PositionCalibrationImpl_Pre5(const PositionCalibrationReport& settings)
		:	Settings(settings)
	{
		Pack();
	}

	void  Pack()
	{

		Buffer[0] = 15;
		EncodeUInt16(Buffer+1, Settings.CommandId);
		Buffer[3] = Settings.Version;

        Vector3d position = Settings.Position * 1e6;
        EncodeSInt32(Buffer+4 , (SInt32) position.x);
        EncodeSInt32(Buffer+8 , (SInt32) position.y);
        EncodeSInt32(Buffer+12, (SInt32) position.z);

		EncodeUInt16(Buffer+16, Settings.PositionIndex);
		EncodeUInt16(Buffer+18, Settings.NumPositions);
		EncodeUInt16(Buffer+20, UInt16(Settings.PositionType));
	}

	void Unpack()
	{

		Settings.CommandId = DecodeUInt16(Buffer+1);
		Settings.Version = Buffer[3];

		Settings.Position.x = DecodeSInt32(Buffer + 4) * 1e-6;
		Settings.Position.y = DecodeSInt32(Buffer + 8) * 1e-6;
		Settings.Position.z = DecodeSInt32(Buffer + 12) * 1e-6;

		Settings.PositionIndex = DecodeUInt16(Buffer + 16);
		Settings.NumPositions  = DecodeUInt16(Buffer + 18);
		Settings.PositionType  = PositionCalibrationReport::PositionTypeEnum(DecodeUInt16(Buffer + 20));
	}
};

// CustomPattern feature report.
struct CustomPatternImpl
{
    enum  { PacketSize = 12 };
    UByte   Buffer[PacketSize];
    
    CustomPatternReport Settings;

	CustomPatternImpl()
	{
		for (int i=0; i<PacketSize; i++)
		{
			Buffer[i] = 0;
		}

		Buffer[0] = 16;
	}

    CustomPatternImpl(const CustomPatternReport& settings)
		:	Settings(settings)
    {
		Pack();
	}

    void  Pack()
    {

        Buffer[0] = 16;
        EncodeUInt16(Buffer+1, Settings.CommandId);
        Buffer[3] = Settings.SequenceLength;
        EncodeUInt32(Buffer+4 , Settings.Sequence);
        EncodeUInt16(Buffer+8 , Settings.LEDIndex);
        EncodeUInt16(Buffer+10, Settings.NumLEDs);
    }

    void Unpack()
    {
        Settings.CommandId = DecodeUInt16(Buffer+1);
        Settings.SequenceLength = Buffer[3];
        Settings.Sequence = DecodeUInt32(Buffer+4);
        Settings.LEDIndex = DecodeUInt16(Buffer+8);
        Settings.NumLEDs = DecodeUInt16(Buffer+10);
    }
};

// Manufacturing feature report.
struct ManufacturingImpl
{
    enum  { PacketSize = 16 };
    UByte   Buffer[PacketSize];

    ManufacturingReport Settings;

	ManufacturingImpl()
	{
		memset(Buffer, 0, sizeof(Buffer));
		Buffer[0] = 18;
	}

    ManufacturingImpl(const ManufacturingReport& settings)
        :   Settings(settings)
    {
        Pack();
    }

    void  Pack()
    {
        Buffer[0] = 18;
        EncodeUInt16(Buffer+1, Settings.CommandId);
        Buffer[3] = Settings.NumStages;
        Buffer[4] = Settings.Stage;
        Buffer[5] = Settings.StageVersion;
        EncodeUInt16(Buffer+6, Settings.StageLocation);
        EncodeUInt32(Buffer+8, Settings.StageTime);
        EncodeUInt32(Buffer+12, Settings.Result);
    }

    void Unpack()
    {
        Settings.CommandId = DecodeUInt16(Buffer+1);
        Settings.NumStages = Buffer[3];
        Settings.Stage = Buffer[4];
        Settings.StageVersion = Buffer[5];
        Settings.StageLocation = DecodeUInt16(Buffer+6);
        Settings.StageTime = DecodeUInt32(Buffer+8);
        Settings.Result = DecodeUInt32(Buffer+12);
    }
};

// UUID feature report.
struct UUIDImpl
{
    enum  { PacketSize = 23 };
    UByte   Buffer[PacketSize];

    UUIDReport Settings;

	UUIDImpl()
	{
		memset(Buffer, 0, sizeof(Buffer));
		Buffer[0] = 19;
	}

    UUIDImpl(const UUIDReport& settings)
        :   Settings(settings)
    {
        Pack();
    }

    void  Pack()
    {
        Buffer[0] = 19;
        EncodeUInt16(Buffer+1, Settings.CommandId);
		for (int i = 0; i < 20; ++i)
			Buffer[3 + i] = Settings.UUIDValue[i];
    }

    void Unpack()
    {
        Settings.CommandId = DecodeUInt16(Buffer+1);
		for (int i = 0; i < 20; ++i)
			Settings.UUIDValue[i] = Buffer[3 + i];
    }
};

// LensDistortion feature report.
struct LensDistortionImpl
{
    enum  { PacketSize = 64 };
    UByte   Buffer[PacketSize];

    LensDistortionReport Settings;

	LensDistortionImpl()
	{
		memset(Buffer, 0, sizeof(Buffer));
		Buffer[0] = 22;
	}

    LensDistortionImpl(const LensDistortionReport& settings)
        :   Settings(settings)
    {
        Pack();
    }

    void  Pack()
    {
        Buffer[0] = 19;
        EncodeUInt16(Buffer+1, Settings.CommandId);
		
		Buffer[3] = Settings.NumDistortions;
		Buffer[4] = Settings.DistortionIndex;
		Buffer[5] = Settings.Bitmask;
		EncodeUInt16(Buffer+6, Settings.LensType);
		EncodeUInt16(Buffer+8, Settings.Version);
		EncodeUInt16(Buffer+10, Settings.EyeRelief);

		for (int i = 0; i < 11; ++i)
			EncodeUInt16(Buffer+12+2*i, Settings.KCoefficients[i]);

		EncodeUInt16(Buffer+34, Settings.MaxR);
		EncodeUInt16(Buffer+36, Settings.MetersPerTanAngleAtCenter);
				
		for (int i = 0; i < 4; ++i)
			EncodeUInt16(Buffer+38+2*i, Settings.ChromaticAberration[i]);
    }

    void Unpack()
    {
        Settings.CommandId = DecodeUInt16(Buffer+1);
		
		Settings.NumDistortions = Buffer[3];
		Settings.DistortionIndex = Buffer[4];
		Settings.Bitmask = Buffer[5];
		Settings.LensType = DecodeUInt16(Buffer+6);
		Settings.Version = DecodeUInt16(Buffer+8);
		Settings.EyeRelief = DecodeUInt16(Buffer+10);

		for (int i = 0; i < 11; ++i)
			Settings.KCoefficients[i] = DecodeUInt16(Buffer+12+2*i);

		Settings.MaxR = DecodeUInt16(Buffer+34);
		Settings.MetersPerTanAngleAtCenter = DecodeUInt16(Buffer+36);
				
		for (int i = 0; i < 4; ++i)
			Settings.ChromaticAberration[i] = DecodeUInt16(Buffer+38+2*i);
    }
};

// KeepAliveMux feature report.
struct KeepAliveMuxImpl
{
    enum  { PacketSize = 6 };
    UByte   Buffer[PacketSize];

    KeepAliveMuxReport Settings;

	KeepAliveMuxImpl()
	{
		memset(Buffer, 0, sizeof(Buffer));
		Buffer[0] = 17;
	}

    KeepAliveMuxImpl(const KeepAliveMuxReport& settings)
        :   Settings(settings)
    {
        Pack();
    }

    void  Pack()
    {
        Buffer[0] = 17;
        EncodeUInt16(Buffer+1, Settings.CommandId);
        Buffer[3] = Settings.INReport;
        EncodeUInt16(Buffer+4, Settings.Interval);
    }

    void Unpack()
    {
        Settings.CommandId = DecodeUInt16(Buffer+1);
        Settings.INReport = Buffer[3];
        Settings.Interval = DecodeUInt16(Buffer+4);
    }
};

// Temperature feature report.
struct TemperatureImpl
{
    enum  { PacketSize = 24 };
    UByte   Buffer[PacketSize];
    
    TemperatureReport Settings;

	TemperatureImpl()
	{
		memset(Buffer, 0, sizeof(Buffer));
		Buffer[0] = 20;
	}

    TemperatureImpl(const TemperatureReport& settings)
		:	Settings(settings)
    {
		Pack();
	}

    void  Pack()
    {

        Buffer[0] = 20;
        EncodeUInt16(Buffer + 1, Settings.CommandId);
        Buffer[3] = Settings.Version;

        Buffer[4] = Settings.NumBins;
        Buffer[5] = Settings.Bin;
        Buffer[6] = Settings.NumSamples;
        Buffer[7] = Settings.Sample;

        EncodeSInt16(Buffer + 8 , SInt16(Settings.TargetTemperature * 1e2));
        EncodeSInt16(Buffer + 10, SInt16(Settings.ActualTemperature * 1e2));

        EncodeUInt32(Buffer + 12, Settings.Time);

        Vector3d offset = Settings.Offset * 1e4;
        PackSensor(Buffer + 16, (SInt16) offset.x, (SInt16) offset.y, (SInt16) offset.z);
    }

    void Unpack()
    {
        Settings.CommandId = DecodeUInt16(Buffer + 1);
        Settings.Version = Buffer[3];

        Settings.NumBins    = Buffer[4];
        Settings.Bin        = Buffer[5];
        Settings.NumSamples = Buffer[6];
        Settings.Sample     = Buffer[7];

        Settings.TargetTemperature = DecodeSInt16(Buffer + 8) * 1e-2;
        Settings.ActualTemperature = DecodeSInt16(Buffer + 10) * 1e-2;

        Settings.Time = DecodeUInt32(Buffer + 12);

        SInt32 x, y, z;
        UnpackSensor(Buffer + 16, &x, &y, &z);
        Settings.Offset = Vector3d(x, y, z) * 1e-4;
    }
};

// GyroOffset feature report.
struct GyroOffsetImpl
{
    enum  { PacketSize = 18 };
    UByte   Buffer[PacketSize];

    GyroOffsetReport Settings;

    GyroOffsetImpl()
    {
        memset(Buffer, 0, sizeof(Buffer));
        Buffer[0] = 21;
    }

   GyroOffsetImpl(const GyroOffsetReport& settings)
		:	Settings(settings)
    {
		Pack();
	}

    void  Pack()
    {

        Buffer[0] = 21;
        Buffer[1] = UByte(Settings.CommandId & 0xFF);
        Buffer[2] = UByte(Settings.CommandId >> 8);
        Buffer[3] = UByte(Settings.Version);

		Vector3d offset = Settings.Offset * 1e4;
		PackSensor(Buffer + 4, (SInt32) offset.x, (SInt32) offset.y, (SInt32) offset.z);

        EncodeSInt16(Buffer + 16, SInt16(Settings.Temperature * 1e2));
    }

    void Unpack()
    {
        Settings.CommandId   = DecodeUInt16(Buffer + 1);
        Settings.Version     = GyroOffsetReport::VersionEnum(Buffer[3]);

        SInt32 x, y, z;
        UnpackSensor(Buffer + 4, &x, &y, &z);
        Settings.Offset      = Vector3d(x, y, z) * 1e-4f;

        Settings.Temperature = DecodeSInt16(Buffer + 16) * 1e-2;
    }
};

} // namespace OVR

#endif // OVR_Sensor2ImplUtil_h
