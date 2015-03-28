/************************************************************************************

Filename    :   Vision_SensorStateReader.cpp
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

#include "Vision_SensorStateReader.h"


namespace OVR { namespace Vision {


//-------------------------------------------------------------------------------------

// This is a "perceptually tuned predictive filter", which means that it is optimized
// for improvements in the VR experience, rather than pure error.  In particular,
// jitter is more perceptible at lower speeds whereas latency is more perceptible
// after a high-speed motion.  Therefore, the prediction interval is dynamically
// adjusted based on speed.  Significant more research is needed to further improve
// this family of filters.
static Pose<double> calcPredictedPose(const PoseState<double>& poseState, double predictionDt)
{
    Pose<double> pose = poseState.ThePose;
    const double linearCoef = 1.0;
    Vector3d angularVelocity = poseState.AngularVelocity;
    double angularSpeed = angularVelocity.Length();

    // This could be tuned so that linear and angular are combined with different coefficients
    double speed = angularSpeed + linearCoef * poseState.LinearVelocity.Length();

    const double slope = 0.2; // The rate at which the dynamic prediction interval varies
    double candidateDt = slope * speed; // TODO: Replace with smoothstep function

    double dynamicDt = predictionDt;

    // Choose the candidate if it is shorter, to improve stability
    if (candidateDt < predictionDt)
    {
        dynamicDt = candidateDt;
    }

    if (angularSpeed > 0.001)
    {
        pose.Rotation = pose.Rotation * Quatd(angularVelocity, angularSpeed * dynamicDt);
    }

    pose.Translation += poseState.LinearVelocity * dynamicDt;

    return pose;
}

PoseState<float> calcPredictedPoseState(const LocklessSensorState& sensorState, double absoluteTime, const Posed& centeredFromWorld)
{
    // Delta time from the last available data
    double pdt = absoluteTime - sensorState.WorldFromImu.TimeInSeconds;
    static const double maxPdt = 0.1;

    // If delta went negative due to synchronization problems between processes or just a lag spike,
    if (pdt < 0)
    {
        pdt = 0;
    }
    else if (pdt > maxPdt)
    {
        pdt = maxPdt;
        static double lastLatWarnTime = 0;
        if (lastLatWarnTime != sensorState.WorldFromImu.TimeInSeconds)
        {
            lastLatWarnTime = sensorState.WorldFromImu.TimeInSeconds;
            LogText("[TrackingStateReader] Prediction interval too high: %f s, clamping at %f s\n", pdt, maxPdt);
        }
    }

    PoseStatef result;
    result = PoseStatef(sensorState.WorldFromImu);
    result.TimeInSeconds = absoluteTime;
    result.ThePose = Posef(centeredFromWorld * calcPredictedPose(sensorState.WorldFromImu, pdt) * sensorState.ImuFromCpf);
    return result;
}

//// TrackingStateReader

// Pre-0.5.0 applications assume that the initial WorldFromCentered
// pose is always identity, because the WorldFromImu pose has a 180-degree flip in Y 
// and a 1-meter offset in Z.  See CAPI_HMDState.cpp
Posed TrackingStateReader::DefaultWorldFromCentered(Quatd::Identity(), Vector3d(0, 0, 0));

// At startup, we want an identity pose when the user is looking along the positive camera Z axis, one meter in front of camera.
// That is a 180 degree rotation about Y, with a -1 meter translation (the inverse of this pose, CenteredFromWorld, is actually used)
// (NOTE: This pose should be the same as SensorFusionFilter::DefaultWorldFromImu)
//
//Posed TrackingStateReader::DefaultWorldFromCentered(Quatd(0, 1, 0, 0), Vector3d(0, 0, -1));

TrackingStateReader::TrackingStateReader() :
    HmdUpdater(nullptr),
    CameraUpdater(nullptr)
{
    CenteredFromWorld = GetDefaultCenteredFromWorld();
}

void TrackingStateReader::SetUpdaters(const CombinedHmdUpdater *hmd, const CameraStateUpdater *camera)
{
    HmdUpdater    = hmd;
    CameraUpdater = camera;
}


// This function centers tracking on the current pose, such that when the
// headset is positioned at the current pose and looking level in the current direction,
// the tracking system pose will be identity.
// In other words, tracking is relative to this centered pose.
//
bool TrackingStateReader::RecenterPose(const Vector3d& neckModelOffset)
{
    if (!HmdUpdater)
        return false;

    const LocklessSensorState lstate = HmdUpdater->SensorState.GetState();
    Posed worldFromCpf = (lstate.WorldFromImu.ThePose * lstate.ImuFromCpf);

    return ComputeCenteredFromWorld(worldFromCpf, neckModelOffset);
}

bool TrackingStateReader::ComputeCenteredFromWorld(const Posed& worldFromCpf, const Vector3d& neckModel)
{
    // Position of CPF in the head rotation center frame
    const Vector3d cpfInRotationCenter = neckModel;

    const Vector3d forward(0, 0, -1);
    const Vector3d up(0, 1, 0);
    Vector3d look = worldFromCpf.Rotate(forward);

    // If the headset is pointed straight up or straight down,
    // it may be face down on a tabletop.  In this case we
    // can't reliably extract a heading angle.
    // We assume straight ahead and return false so caller 
    // knows that recenter may not be reliable.
    bool headingValid = true;
    static const double lookTol = cos(DegreeToRad(20.0));
    if (fabs(look.Dot(up)) >= lookTol)    // fabs(lookBack.Dot(up))
    {
        look = forward;
        headingValid = false;
    }

    // Now compute the orientation of the headset when looking straight ahead:
    // Extract the heading (yaw) component of the pose
    Vector3d centeredLook = Vector3d(look.x, 0, look.z).Normalized();
    Quatd centeredOrientation = Quatd::Align(centeredLook, forward);

    // Compute the position in world space of the head rotation center:
    // we assume the head rotates about this point in space.
    Vector3d headRotationCenter = worldFromCpf.Transform(-cpfInRotationCenter);

    // Now apply the heading rotation to compute the reference position of the CPF
    // relative to the head rotation center.
    Vector3d centeredCpfPos = headRotationCenter + centeredOrientation.Rotate(cpfInRotationCenter);

    // Now compute the centered pose of the CPF.
    Posed worldFromCentered(centeredOrientation, centeredCpfPos);

    // For tracking, we use the inverse of the centered pose
    CenteredFromWorld = worldFromCentered.Inverted();

    return headingValid;
}

bool TrackingStateReader::GetTrackingStateAtTime(double absoluteTime, TrackingState& ss) const
{
    LocklessCameraState cameraState;
    LocklessSensorState sensorState;

    if (CameraUpdater)
        cameraState = CameraUpdater->GetState();
    if (HmdUpdater)
        sensorState = HmdUpdater->SensorState.GetState();

    // Update the status flags
    ss.StatusFlags = cameraState.StatusFlags | sensorState.StatusFlags;

    // If no hardware is connected, override the tracking flags
    if (0 == (ss.StatusFlags & Status_HMDConnected))
    {
        ss.StatusFlags &= ~Status_TrackingMask;
    }
    if (0 == (ss.StatusFlags & Status_PositionConnected))
    {
        ss.StatusFlags &= ~(Status_PositionTracked | Status_CameraPoseTracked);
    }

    // If tracking info is invalid,
    if (0 == (ss.StatusFlags & Status_TrackingMask))
    {
        return false;
    }
    
    ss.HeadPose = calcPredictedPoseState(sensorState, absoluteTime, CenteredFromWorld);

    ss.CameraPose        = Posef(CenteredFromWorld * cameraState.WorldFromCamera);
    ss.LeveledCameraPose = Posef(CenteredFromWorld * Posed(Quatd(), cameraState.WorldFromCamera.Translation));

    ss.RawSensorData = sensorState.RawSensorData;
    return true;
}

bool TrackingStateReader::GetPoseAtTime(double absoluteTime, Posef& transform) const
{
    TrackingState ss;

    if (!GetTrackingStateAtTime(absoluteTime, ss))
    {
        return false;
    }

    transform = ss.HeadPose.ThePose;

    return true;
}

uint32_t TrackingStateReader::GetStatus() const
{
    TrackingState ss;

    if (!GetTrackingStateAtTime(0, ss))
    {
        return 0;
    }

    return ss.StatusFlags;
}


}} // namespace OVR::Vision
