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
#include "Util/Util_LatencyTest2State.h"
#include "Sensors/OVR_DeviceConstants.h"


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

#pragma pack(push, 8)

// TrackedObject state stored in lockless updater "queue" and used for 
// prediction by SensorStateReader
struct LocklessSensorState
{
    PoseState<double> WorldFromImu;
    SensorDataType    RawSensorData;

    // DO NOT USE
    // only preserved for backwards compatibility
    Pose<double>      WorldFromCamera_DEPRECATED;

    uint32_t          StatusFlags;
    uint32_t          _PAD_0_;

    // ImuFromCpf for HMD pose tracking
    Posed             ImuFromCpf;

    // Initialized to invalid state
    LocklessSensorState() :
       WorldFromImu()
     , RawSensorData()
     , WorldFromCamera_DEPRECATED()
     , StatusFlags(0)
     , _PAD_0_(0) // This assignment should be irrelevant, but it quells static/runtime analysis complaints.
     , ImuFromCpf()
    {
    }
};

static_assert((sizeof(LocklessSensorState) == sizeof(PoseState<double>) + sizeof(SensorDataType) + sizeof(Pose<double>) + 2*sizeof(uint32_t) + sizeof(Posed)), "sizeof(LocklessSensorState) failure");

struct LocklessCameraState
{
    Pose<double>      WorldFromCamera;

    uint32_t          StatusFlags;
    uint32_t          _PAD_0_;

    // Initialized to invalid state
    LocklessCameraState() :
        WorldFromCamera()
        , StatusFlags(0)
        , _PAD_0_(0) // This assignment should be irrelevant, but it quells static/runtime analysis complaints.
    {
    }
};

static_assert((sizeof(LocklessCameraState) == sizeof(Pose<double>) + 2 * sizeof(uint32_t)), "sizeof(LocklessCameraState) failure");


// Padded out version stored in the updater slots
// Designed to be a larger fixed size to allow the data to grow in the future
// without breaking older compiled code.
OVR_DISABLE_MSVC_WARNING(4351)
template <class Payload, int PaddingSize>
struct LocklessPadding
{
    uint8_t buffer[PaddingSize];
    
    LocklessPadding() : buffer() { }

    LocklessPadding& operator=(const Payload& rhs)
    {
        // if this fires off, then increase PaddingSize
        // IMPORTANT: this WILL break backwards compatibility
        static_assert(sizeof(buffer) >= sizeof(Payload), "PaddingSize is too small");

        memcpy(buffer, &rhs, sizeof(Payload));
        return *this;
    }

    operator Payload() const
    {
        Payload result;
        memcpy(&result, buffer, sizeof(Payload));
        return result;
    }
};
OVR_RESTORE_MSVC_WARNING()

#pragma pack(pop)

//// Lockless updaters

struct CombinedHmdUpdater
{
    // IMPORTANT: do not add more data to this struct
    // new objects should have their own shared memory blocks
    LocklessUpdater<LocklessSensorState, LocklessPadding<LocklessSensorState, 512> > SensorState;
    LocklessUpdater<Util::FrameTimeRecordSet, Util::FrameTimeRecordSet> LatencyTest;
};


typedef LocklessUpdater<LocklessCameraState, LocklessPadding<LocklessCameraState, 512> > CameraStateUpdater;


}} // namespace OVR::Vision

#endif
