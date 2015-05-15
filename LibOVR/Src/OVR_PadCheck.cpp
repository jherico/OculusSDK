/************************************************************************************

Filename    :   OVR_PadCheck.cpp
Content     :   Validates the structure padding at runtime to validate back-compat.
Created     :   March 18, 2015
Authors     :   Chris Taylor

Copyright   :   Copyright 2014 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.2 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "OVR_PadCheck.h"

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Log.h"

// Headers containing software to check.
#include "OVR_CAPI.h"
#include "CAPI/CAPI_DistortionTiming.h"
#include "Util/Util_LatencyTest2.h"
#include "Vision/Vision_Common.h"
#include "Vision/SensorFusion/Vision_SensorState.h"
#include "Extras/OVR_Math.h"
#include "Sensors/OVR_DeviceConstants.h"
#include "OVR_CAPI_GL.h"
#if defined(OVR_OS_WIN32)
#include "OVR_CAPI_D3D.h"
#endif


namespace OVR {


// Check at runtime that our shared structures do not get broken.
// This function should be done after System::Init() is called.
bool VerifyBackwardsCompatibility()
{
    bool failure = false;

    // If any of these assertions fail, it means we have broken backwards
    // compatibility with a previous version of the SDK.
    // This also validates the OVR_UNUSED_STRUCT_PAD() done in OVR_CAPI.h

#if (OVR_PTR_SIZE == 8) // 64-bit builds:

    OVR_PAD_CHECK(ovrHmdDesc, Handle, 0);
    OVR_PAD_CHECK(ovrHmdDesc, Type, 8);
    OVR_PAD_CHECK(ovrHmdDesc, ProductName, 16);
    OVR_PAD_CHECK(ovrHmdDesc, Manufacturer, 24);
    OVR_PAD_CHECK(ovrHmdDesc, VendorId, 32);
    OVR_PAD_CHECK(ovrHmdDesc, ProductId, 34);
    OVR_PAD_CHECK(ovrHmdDesc, SerialNumber, 36);
    OVR_PAD_CHECK(ovrHmdDesc, FirmwareMajor, 60);
    OVR_PAD_CHECK(ovrHmdDesc, FirmwareMinor, 62);
    OVR_PAD_CHECK(ovrHmdDesc, CameraFrustumHFovInRadians, 64);
    OVR_PAD_CHECK(ovrHmdDesc, CameraFrustumVFovInRadians, 68);
    OVR_PAD_CHECK(ovrHmdDesc, CameraFrustumNearZInMeters, 72);
    OVR_PAD_CHECK(ovrHmdDesc, CameraFrustumFarZInMeters, 76);
    OVR_PAD_CHECK(ovrHmdDesc, HmdCaps, 80);
    OVR_PAD_CHECK(ovrHmdDesc, TrackingCaps, 84);    
    OVR_PAD_CHECK(ovrHmdDesc, DefaultEyeFov, 88);
    OVR_PAD_CHECK(ovrHmdDesc, MaxEyeFov, 120);
    OVR_PAD_CHECK(ovrHmdDesc, EyeRenderOrder, 152);
    OVR_PAD_CHECK(ovrHmdDesc, Resolution, 160);
    OVR_SIZE_CHECK(ovrHmdDesc, 168);

    OVR_PAD_CHECK(ovrTexture, Header, 0);
    OVR_PAD_CHECK(ovrTexture, PlatformData, 16);
    OVR_SIZE_CHECK(ovrTexture, 80);

    OVR_PAD_CHECK(ovrInitParams, Flags, 0);
    OVR_PAD_CHECK(ovrInitParams, RequestedMinorVersion, 4);
    OVR_PAD_CHECK(ovrInitParams, LogCallback, 8);
    OVR_PAD_CHECK(ovrInitParams, ConnectionTimeoutMS, 16);
    OVR_SIZE_CHECK(ovrInitParams, 24);

#else // 32-bit builds:

    OVR_PAD_CHECK(ovrHmdDesc, Handle, 0);
    OVR_PAD_CHECK(ovrHmdDesc, Type, 4);
    OVR_PAD_CHECK(ovrHmdDesc, ProductName, 8);
    OVR_PAD_CHECK(ovrHmdDesc, Manufacturer, 12);
    OVR_PAD_CHECK(ovrHmdDesc, VendorId, 16);
    OVR_PAD_CHECK(ovrHmdDesc, ProductId, 18);
    OVR_PAD_CHECK(ovrHmdDesc, SerialNumber, 20);
    OVR_PAD_CHECK(ovrHmdDesc, FirmwareMajor, 44);
    OVR_PAD_CHECK(ovrHmdDesc, FirmwareMinor, 46);
    OVR_PAD_CHECK(ovrHmdDesc, CameraFrustumHFovInRadians, 48);
    OVR_PAD_CHECK(ovrHmdDesc, CameraFrustumVFovInRadians, 52);
    OVR_PAD_CHECK(ovrHmdDesc, CameraFrustumNearZInMeters, 56);
    OVR_PAD_CHECK(ovrHmdDesc, CameraFrustumFarZInMeters, 60);
    OVR_PAD_CHECK(ovrHmdDesc, HmdCaps, 64);
    OVR_PAD_CHECK(ovrHmdDesc, TrackingCaps, 68);    
    OVR_PAD_CHECK(ovrHmdDesc, DefaultEyeFov, 72);
    OVR_PAD_CHECK(ovrHmdDesc, MaxEyeFov, 104);
    OVR_PAD_CHECK(ovrHmdDesc, EyeRenderOrder, 136);
    OVR_PAD_CHECK(ovrHmdDesc, Resolution, 144);
    OVR_SIZE_CHECK(ovrHmdDesc, 152);

    OVR_PAD_CHECK(ovrTexture, Header, 0);
    OVR_PAD_CHECK(ovrTexture, PlatformData, 12);
    OVR_SIZE_CHECK(ovrTexture, 44);

    OVR_PAD_CHECK(ovrInitParams, Flags, 0);
    OVR_PAD_CHECK(ovrInitParams, RequestedMinorVersion, 4);
    OVR_PAD_CHECK(ovrInitParams, LogCallback, 8);
    OVR_PAD_CHECK(ovrInitParams, ConnectionTimeoutMS, 12);
    OVR_SIZE_CHECK(ovrInitParams, 16);

#endif // 32/64-bit builds

    OVR_PAD_CHECK(ovrPosef, Orientation, 0);
    OVR_PAD_CHECK(ovrPosef, Position, 16);
    OVR_SIZE_CHECK(ovrPosef, 28);

    OVR_PAD_CHECK(ovrPoseStatef, ThePose, 0);
    OVR_PAD_CHECK(ovrPoseStatef, AngularVelocity, 28);
    OVR_PAD_CHECK(ovrPoseStatef, LinearVelocity, 40);
    OVR_PAD_CHECK(ovrPoseStatef, AngularAcceleration, 52);
    OVR_PAD_CHECK(ovrPoseStatef, LinearAcceleration, 64);
    OVR_PAD_CHECK(ovrPoseStatef, TimeInSeconds, 80);
    OVR_SIZE_CHECK(ovrPoseStatef, 88);

    OVR_PAD_CHECK(ovrFovPort, UpTan, 0);
    OVR_PAD_CHECK(ovrFovPort, DownTan, 4);
    OVR_PAD_CHECK(ovrFovPort, LeftTan, 8);
    OVR_PAD_CHECK(ovrFovPort, RightTan, 12);
    OVR_SIZE_CHECK(ovrFovPort, 16);

    OVR_PAD_CHECK(ovrSensorData, Accelerometer, 0);
    OVR_PAD_CHECK(ovrSensorData, Gyro, 12);
    OVR_PAD_CHECK(ovrSensorData, Magnetometer, 24);
    OVR_PAD_CHECK(ovrSensorData, Temperature, 36);
    OVR_PAD_CHECK(ovrSensorData, TimeInSeconds, 40);
    OVR_SIZE_CHECK(ovrSensorData, 44);


    OVR_PAD_CHECK(ovrTrackingState, HeadPose, 0);
    OVR_PAD_CHECK(ovrTrackingState, CameraPose, 88);
    OVR_PAD_CHECK(ovrTrackingState, LeveledCameraPose, 116);
    OVR_PAD_CHECK(ovrTrackingState, RawSensorData, 144);
    OVR_PAD_CHECK(ovrTrackingState, StatusFlags, 188);
    OVR_PAD_CHECK(ovrTrackingState, LastCameraFrameCounter, 192);
    OVR_SIZE_CHECK(ovrTrackingState, 200);

    // 0.6.0 version of the ovrFrameTiming structure.
    // It changed significantly from the 0.4/0.5 version.
    OVR_PAD_CHECK(ovrFrameTiming, DisplayMidpointSeconds, 0); /* 8 bytes */
    OVR_PAD_CHECK(ovrFrameTiming, FrameIntervalSeconds, 8); /* 8 bytes */
    OVR_PAD_CHECK(ovrFrameTiming, AppFrameIndex, 16); /* 4 bytes */
    OVR_PAD_CHECK(ovrFrameTiming, DisplayFrameIndex, 20); /* 4 bytes */
    OVR_SIZE_CHECK(ovrFrameTiming, 24);

    OVR_PAD_CHECK(ovrEyeRenderDesc, Eye, 0);
    OVR_PAD_CHECK(ovrEyeRenderDesc, Fov, 4);
    OVR_PAD_CHECK(ovrEyeRenderDesc, DistortedViewport, 20);
    OVR_PAD_CHECK(ovrEyeRenderDesc, PixelsPerTanAngleAtCenter, 36);
    OVR_PAD_CHECK(ovrEyeRenderDesc, HmdToEyeViewOffset, 44);
    OVR_SIZE_CHECK(ovrEyeRenderDesc, 56);

    OVR_PAD_CHECK(ovrTimewarpProjectionDesc, Projection22, 0);
    OVR_PAD_CHECK(ovrTimewarpProjectionDesc, Projection23, 4);
    OVR_PAD_CHECK(ovrTimewarpProjectionDesc, Projection32, 8);
    OVR_SIZE_CHECK(ovrTimewarpProjectionDesc, 12);

    OVR_PAD_CHECK(ovrViewScaleDesc, HmdToEyeViewOffset, 0);
    OVR_PAD_CHECK(ovrViewScaleDesc, HmdSpaceToWorldScaleInMeters, 24);
    OVR_SIZE_CHECK(ovrViewScaleDesc, 28);

    OVR_PAD_CHECK(ovrTextureHeader, API, 0);
    OVR_PAD_CHECK(ovrTextureHeader, TextureSize, 4);
    OVR_SIZE_CHECK(ovrTextureHeader, 12);

    // Render structures:
    // These are casted by the app from this specific format to a generic container.

    // OpenGL:
    OVR_PAD_CHECK(ovrGLTextureData, Header, 0); /* 12 bytes */
    OVR_PAD_CHECK(ovrGLTextureData, TexId, 12); /* 4 bytes */
    OVR_SIZE_CHECK(ovrGLTextureData, 16);

#if defined(OVR_OS_MS)

    // D3D:
    #if OVR_PTR_SIZE == 8
        OVR_PAD_CHECK(ovrD3D11TextureData, Header, 0); /* 28 bytes */
        OVR_PAD_CHECK(ovrD3D11TextureData, pTexture, 16); /* 8 bytes */
        OVR_PAD_CHECK(ovrD3D11TextureData, pSRView, 24); /* 8 bytes */
        OVR_SIZE_CHECK(ovrD3D11TextureData, 32);
    #else
        OVR_PAD_CHECK(ovrD3D11TextureData, Header, 0); /* 28 bytes */
        OVR_PAD_CHECK(ovrD3D11TextureData, pTexture, 12); /* 4 bytes */
        OVR_PAD_CHECK(ovrD3D11TextureData, pSRView, 16); /* 4 bytes */
        OVR_SIZE_CHECK(ovrD3D11TextureData, 20);
    #endif
#endif

    // Lockless structures:
    // These are shared via shared-memory between old and new versions of software
    // compiled in 32-bit and 64-bit modes.
    // These need to be the same between 32-bit and 64-bit builds.
    // If these assertions fail it means we have broken backwards-compatibility.
    // To add new members to these structures, add them at the end and add those
    // new members to these assertions.

    // LocklessAppTimingBase:
    // Shared memory region for 0.6.0 Compositor design.

    OVR_PAD_CHECK(CAPI::LocklessAppTimingBase, IsValid, 0); /* 4 bytes */
    OVR_PAD_CHECK(CAPI::LocklessAppTimingBase, LastEndFrameIndex, 4); /* 4 bytes */
    OVR_PAD_CHECK(CAPI::LocklessAppTimingBase, LastStartFrameTime, 8); /* 8 bytes */
    OVR_PAD_CHECK(CAPI::LocklessAppTimingBase, LastKnownVsyncTime, 16); /* 8 bytes */
    OVR_PAD_CHECK(CAPI::LocklessAppTimingBase, VsyncFuzzFactor, 24); /* 8 bytes */
    OVR_PAD_CHECK(CAPI::LocklessAppTimingBase, FrameInterval, 32); /* 8 bytes */
    OVR_PAD_CHECK(CAPI::LocklessAppTimingBase, ScanoutDelay, 40); /* 8 bytes */
    OVR_PAD_CHECK(CAPI::LocklessAppTimingBase, ScreenSwitchingDelay, 48); /* 8 bytes */
    OVR_SIZE_CHECK(CAPI::LocklessAppTimingBase, 56);

    // FrameTimeRecordSet:
    // Shared memory region for 0.5.0 and earlier DK2 latency tester.
    // 0.6.0 no longer uses this as it is all in-process with the Compositor design.

    OVR_PAD_CHECK(Util::FrameTimeRecord, ReadbackIndex, 0); /* 4 bytes */
    OVR_PAD_CHECK(Util::FrameTimeRecord, TimeSeconds, 8); /* 8 bytes */
    OVR_SIZE_CHECK(Util::FrameTimeRecord, 16);

    OVR_PAD_CHECK(Util::FrameTimeRecordSet, Records, 0); /* 64 bytes */
    OVR_PAD_CHECK(Util::FrameTimeRecordSet, NextWriteIndex, 64); /* 4 bytes */
    OVR_SIZE_CHECK(Util::FrameTimeRecordSet, 72);

    // LocklessSensorState:
    // Shared memory region for 0.6.0 and earlier.

    OVR_PAD_CHECK(Pose<double>, Rotation, 0); /* 32 bytes */
    OVR_PAD_CHECK(Pose<double>, Translation, 32); /* 24 bytes */
    OVR_SIZE_CHECK(Pose<double>, 56);

    OVR_PAD_CHECK(PoseState<double>, ThePose, 0); /* 56 bytes */
    OVR_PAD_CHECK(PoseState<double>, AngularVelocity, 56); /* 24 bytes */
    OVR_PAD_CHECK(PoseState<double>, LinearVelocity, 80); /* 24 bytes */
    OVR_PAD_CHECK(PoseState<double>, AngularAcceleration, 104); /* 24 bytes */
    OVR_PAD_CHECK(PoseState<double>, LinearAcceleration, 128); /* 24 bytes */
    OVR_PAD_CHECK(PoseState<double>, TimeInSeconds, 152); /* 8 bytes */
    OVR_SIZE_CHECK(PoseState<double>, 160);

    OVR_PAD_CHECK(SensorDataType, Acceleration, 0); /* 12 bytes */
    OVR_PAD_CHECK(SensorDataType, RotationRate, 12); /* 12 bytes */
    OVR_PAD_CHECK(SensorDataType, MagneticField, 24); /* 12 bytes */
    OVR_PAD_CHECK(SensorDataType, Temperature, 36); /* 4 bytes */
    OVR_PAD_CHECK(SensorDataType, AbsoluteTimeSeconds, 40); /* 8 bytes */
    OVR_SIZE_CHECK(SensorDataType, 48);

    OVR_PAD_CHECK(Vision::LocklessSensorState, WorldFromImu, 0); /* 160 bytes */
    OVR_PAD_CHECK(Vision::LocklessSensorState, RawSensorData, 160); /* 48 bytes */
    OVR_PAD_CHECK(Vision::LocklessSensorState, WorldFromCamera_DEPRECATED, 208); /* 56 bytes */
    OVR_PAD_CHECK(Vision::LocklessSensorState, StatusFlags, 264); /* 4 bytes */
    OVR_PAD_CHECK(Vision::LocklessSensorState, ImuFromCpf, 272); /* 56 bytes */
    OVR_SIZE_CHECK(Vision::LocklessSensorState, 328);

    // LocklessCameraState:
    // Shared memory region for 0.5.0 and newer.

    OVR_PAD_CHECK(Vision::LocklessCameraState, WorldFromCamera, 0); /* 56 bytes */
    OVR_PAD_CHECK(Vision::LocklessCameraState, StatusFlags, 56); /* 4 bytes */
    OVR_SIZE_CHECK(Vision::LocklessCameraState, 64);


    OVR_ASSERT(!failure);
    return !failure;
}


} // namespace OVR
