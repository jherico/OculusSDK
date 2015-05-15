/************************************************************************************

Filename    :   Vision_SensorState.h
Content     :   Sensor state information shared by tracking system with games
Created     :   May 13, 2014
Authors     :   Dov Katz, Chris Taylor

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

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

*************************************************************************************/

#ifndef OVR_Vision_SensorState_h
#define OVR_Vision_SensorState_h

#include "Vision/Vision_Common.h"
#include "Kernel/OVR_SharedMemory.h"
#include "Kernel/OVR_Lockless.h"
#include "Kernel/OVR_String.h"
#include "Sensors/OVR_DeviceConstants.h"
#include "Util/Util_LatencyTest2_Legacy.h"


namespace OVR { namespace Vision {


// Bit flags describing the current status of sensor tracking.
// These values must be the same as ovrStatusBits
enum StatusBits
{
    // Tracked bits: Toggled by SensorFusion
    Status_OrientationTracked = 0x0001, // Orientation is currently tracked (connected and in use)
    Status_PositionTracked    = 0x0002, // Position is currently tracked (false if out of range)
    Status_CameraPoseTracked  = 0x0004, // Camera pose is currently tracked

    // Connected bits: Toggled by TrackingManager
    Status_PositionConnected  = 0x0020, // Position tracking HW is connected
    Status_BuiltinConnected   = 0x0040, // Builtin tracking HW is connected
    Status_HMDConnected       = 0x0080, // HMD is available & connected

    // Masks
    Status_AllMask = 0xffff,
    Status_TrackingMask = Status_PositionTracked | Status_OrientationTracked | Status_CameraPoseTracked,
    Status_ConnectedMask = Status_PositionConnected | Status_HMDConnected,
};

// TrackedObject state stored in lockless updater "queue" and used for 
// prediction by SensorStateReader
//
// This structure needs to be the same size and layout on 32-bit and 64-bit arch.
// Update OVR_PadCheck.cpp when updating this object.
struct OVR_ALIGNAS(8) LocklessSensorState
{
    PoseState<double> WorldFromImu;
    SensorDataType    RawSensorData;

    // DO NOT USE
    // only preserved for backwards compatibility
    Pose<double>      WorldFromCamera_DEPRECATED;

    uint32_t          StatusFlags;

    OVR_UNUSED_STRUCT_PAD(pad0, 4); // Indicates there is implicit padding added.

    // ImuFromCpf for HMD pose tracking
    Posed             ImuFromCpf;

    // Initialized to invalid state
    LocklessSensorState() :
       WorldFromImu()
     , RawSensorData()
     , WorldFromCamera_DEPRECATED()
     , StatusFlags(0)
     , ImuFromCpf()
    {
    }
};

static_assert((sizeof(LocklessSensorState) == sizeof(PoseState<double>) + sizeof(SensorDataType) + sizeof(Pose<double>) + 4 + 4 + sizeof(Posed)), "size mismatch");

// This structure needs to be the same size and layout on 32-bit and 64-bit arch.
// Update OVR_PadCheck.cpp when updating this object.
struct OVR_ALIGNAS(8) LocklessCameraState
{
    Pose<double>      WorldFromCamera;

    uint32_t          StatusFlags;

    OVR_UNUSED_STRUCT_PAD(pad0, 4); // Indicates there is implicit padding added.

    // Initialized to invalid state
    LocklessCameraState() :
        WorldFromCamera()
        , StatusFlags(0)
    {
    }
};

static_assert(sizeof(LocklessCameraState) == sizeof(Pose<double>) + 2 * 4, "size mismatch");



//// Lockless updaters

struct CombinedHmdUpdater
{
    // IMPORTANT: do not add more data to this struct
    // new objects should have their own shared memory blocks
    LocklessUpdater<LocklessSensorState, LocklessPadding<LocklessSensorState, 512> > SensorState;

    // For 0.4/0.5 backwards compatibility.
    // We write to this shared memory object for old applications
    // but we do not use it for 0.6 since the DK2 latency testing
    // is done entirely in the OVRServer now.
    LocklessUpdater<Util::FrameTimeRecordSet, Util::FrameTimeRecordSet> LatencyTest;
};


typedef LocklessUpdater<LocklessCameraState, LocklessPadding<LocklessCameraState, 512> > CameraStateUpdater;


}} // namespace OVR::Vision

#endif
