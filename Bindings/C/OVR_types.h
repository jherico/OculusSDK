/************************************************************************************

Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#pragma once

#ifndef OVR_HEADER
#error "Don't include OVR_types.h directly.  Include OculusVR.h"
#endif

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OVR_VENDOR 0x2833
#define OVR_PRODUCT 0x0001

typedef struct {
    // HID Vendor and ProductId of the device.
    uint16_t VendorId;
    uint16_t ProductId;
    // Rift serial number.
    char SerialNumber[20];
} OvrRiftInfo;

typedef union {
    int32_t v[3];
    struct {
        int32_t x, y ,z;
        int32_t r, g, b;
        int32_t s, t, u;
    };
} OvrVector;

// The structure of messages received from the head tracker.
// Conversion to floating point values is avoided
typedef struct {
    uint8_t SampleCount;
    uint16_t Timestamp;
    uint16_t LastCommandID;
    int16_t Temperature;
    struct {
        OvrVector Accel;
        OvrVector Gyro;
    } Samples[3];
    OvrVector Mag;
} OvrSensorMessage;

//////////////////////////////////////////////////////////////////////////////////////////////
// Scale Range
// SensorScaleRange provides buffer packing logic for the Sensor Range
// record that can be applied to DK1 sensor through Get/SetFeature. We expose this
// through SensorRange class, which has different units.
//////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    // Maximum detected acceleration in m/s^2. Up to 8*G equivalent support guaranteed,
    // where G is ~9.81 m/s^2.
    // Oculus DK1 HW has thresholds near: 2, 4 (default), 8, 16 G.
    float MaxAcceleration;
    // Maximum detected angular velocity in rad/s. Up to 8*Pi support guaranteed.
    // Oculus DK1 HW thresholds near: 1, 2, 4, 8 Pi (default).
    float MaxRotationRate;
    // Maximum detectable Magnetic field strength in Gauss. Up to 2.5 Gauss support guaranteed.
    // Oculus DK1 HW thresholds near: 0.88, 1.3, 1.9, 2.5 gauss.
    float MaxMagneticField;
} OvrSensorRange;


//////////////////////////////////////////////////////////////////////////////////////////////
// DisplayInfo struct
//////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    uint8_t DistortionType;
    float HResolution, VResolution;
    float HScreenSize, VScreenSize;
    float VCenter;
    float LensSeparation;
    float EyeToScreenDistance[2];
    float DistortionK[6];
} OvrDisplayInfo;

typedef unsigned int OVR_HANDLE;
typedef void (*OVR_SENSOR_CALLBACK)(const OvrSensorMessage * message);

#ifdef __cplusplus
}
#endif
