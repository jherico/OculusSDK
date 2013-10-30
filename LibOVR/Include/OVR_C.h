/************************************************************************************

Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#pragma once

#if defined(WIN32)
    #define OVR_STDCALL         __stdcall
#else
    #define OVR_STDCALL
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define OVR_VENDOR 0x2833
#define OVR_PRODUCT 0x0001

// Error codes
#define OVR_NO_ERROR 0x00
#define OVR_INVALID_PARAM 0x01

typedef struct {
    // HID Vendor and ProductId of the device.
    unsigned short VendorId;
    unsigned short ProductId;
    // Rift serial number.
    char SerialNumber[20];
} OvrRiftInfo;

typedef union {
    float v[3];
    struct {
        float x, y ,z;
        float r, g, b;
        float s, t, u;
    };
} OvrVector;

typedef struct {
    float TimeDelta;
    float Temperature;
    OvrVector Accel;
    OvrVector Gyro;
    OvrVector Mag;
} OvrSensorMessage;

//////////////////////////////////////////////////////////////////////////////////////////////
// DisplayInfo struct
//////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    unsigned char DistortionType;
    float HResolution, VResolution;
    float HScreenSize, VScreenSize;
    float VCenter;
    float LensSeparation;
    float EyeToScreenDistance[2];
    float DistortionK[6];
} OvrDisplayInfo;

typedef struct {
    float x;
    float y;
    float z;
    float w;
} OvrQuaternionf;

typedef struct {
    float x;
    float y;
    float z;
} OvrVector3f;

typedef unsigned int OVR_HANDLE;
typedef void (OVR_STDCALL *OVR_SENSOR_CALLBACK)
        (const OvrSensorMessage * message);

void OVR_STDCALL ovrInit();

void OVR_STDCALL ovrDestroy();

unsigned int OVR_STDCALL ovrGetError();

OVR_HANDLE OVR_STDCALL ovrOpenFirstAvailableRift();

void OVR_STDCALL ovrCloseRift(
        OVR_HANDLE device);

void OVR_STDCALL ovrGetDisplayInfo(
        OVR_HANDLE device,
        OvrDisplayInfo * out);

OVR_SENSOR_CALLBACK OVR_STDCALL ovrRegisterSampleHandler(
        OVR_HANDLE device,
        OVR_SENSOR_CALLBACK callback);

void OVR_STDCALL ovrEnableSensorFusion(
        OVR_HANDLE device,
        int enableGravityCorrection,
        int enableMagneticCorrection,
        int enablePrediction);

void OVR_STDCALL ovrResetSensorFusion(
        OVR_HANDLE device);

void OVR_STDCALL ovrGetPredictedOrientation(
        OVR_HANDLE device,
        float predictionDelta,
        OvrQuaternionf * out);

void OVR_STDCALL ovrGetPredictedEulerAngles(
        OVR_HANDLE device,
        float predictionDelta,
        OvrVector3f * out);

void OVR_STDCALL ovrGetOrientation(
        OVR_HANDLE device,
        OvrQuaternionf * out);

void OVR_STDCALL ovrGetEulerAngles(
        OVR_HANDLE device,
        OvrVector3f * out);

#ifdef __cplusplus
}
#endif
