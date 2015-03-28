/************************************************************************************

Filename    :   Vision_SensorStateReader.h
Content     :   Separate reader component that is able to recover sensor pose
Created     :   June 4, 2014
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

*************************************************************************************/

#ifndef OVR_Vision_SensorStateReader_h
#define OVR_Vision_SensorStateReader_h

#include "Kernel/OVR_Lockless.h"
#include "Vision_SensorState.h"

#include "OVR_Profile.h"


// CAPI forward declarations.
struct ovrTrackingState_;
typedef struct ovrTrackingState_ ovrTrackingState;

namespace OVR { namespace Vision {


//-----------------------------------------------------------------------------
// TrackingStateReader

// Full output of tracking reported by GetSensorStateAtTime()
class TrackingState
{
public:
    TrackingState() : StatusFlags(0) { }

    // C-interop support
    TrackingState(const ovrTrackingState& s);
    operator ovrTrackingState () const;

    // HMD pose information for the requested time.
    PoseStatef   HeadPose;

    // Orientation and position of the external camera, if present.
    Posef        CameraPose;
    // Orientation and position of the camera after alignment with gravity 
    Posef        LeveledCameraPose;

    // Most recent sensor data received from the HMD
    SensorDataType RawSensorData;

    // Sensor status described by ovrStatusBits.
    uint32_t     StatusFlags;

};

// User interface to retrieve pose from the sensor fusion subsystem
class TrackingStateReader : public NewOverrideBase
{
protected:
    const CombinedHmdUpdater* HmdUpdater;
    const CameraStateUpdater* CameraUpdater;

    // Transform from real-world coordinates to centered coordinates
    Posed CenteredFromWorld; 
    static Posed DefaultWorldFromCentered;

public:
    TrackingStateReader();

    // Initialize the updaters
    void SetUpdaters(const CombinedHmdUpdater *hmd, const CameraStateUpdater *camera);


    // Re-centers on the current yaw and translation, taking
    // the head-neck model into account.
    bool         RecenterPose(const Vector3d& neckModeloffset);

    // Computes CenteredFromWorld from a worldFromCpf pose and neck model offset
    bool         ComputeCenteredFromWorld(const Posed& worldFromCpf, const Vector3d& neckModelOffset);

    // Get the full dynamical system state of the CPF, which includes velocities and accelerations,
    // predicted at a specified absolute point in time.
    bool         GetTrackingStateAtTime(double absoluteTime, TrackingState& state) const;

    // Get the predicted pose (orientation, position) of the center pupil frame (CPF) at a specific point in time.
    bool         GetPoseAtTime(double absoluteTime, Posef& transform) const;

    // Get the sensor status (same as GetSensorStateAtTime(...).Status)
    uint32_t     GetStatus() const;

    Posed GetCenteredFromWorld() const
    {
        return CenteredFromWorld;
    }

    Posed GetDefaultCenteredFromWorld() const
    {
        return DefaultWorldFromCentered.Inverted();
    }
};


}} // namespace OVR::Vision

#endif // Vision_SensorStateReader_h
