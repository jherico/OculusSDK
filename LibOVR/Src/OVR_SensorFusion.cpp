/************************************************************************************

Filename    :   OVR_SensorFusion.cpp
Content     :   Methods that determine head orientation from sensor data over time
Created     :   October 9, 2012
Authors     :   Michael Antonov, Steve LaValle, Dov Katz, Max Katsev, Dan Gierl

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

#include "OVR_SensorFusion.h"
#include "Kernel/OVR_Log.h"
#include "Kernel/OVR_System.h"
#include "OVR_JSON.h"
#include "OVR_Profile.h"
#include "OVR_Stereo.h"
#include "OVR_Recording.h"

// Temporary for debugging
bool Global_Flag_1 = true;

//Convenient global variables to temporarily extract this data.
float  TPH_CameraPoseOrientationWxyz[4];
double TPH_CameraPoseConfidence;
double TPH_CameraPoseConfidenceThresholdOverrideIfNonZero = 0;
bool   TPH_IsPositionTracked = false;


namespace OVR {

const Transformd DefaultWorldFromCamera(Quatd(), Vector3d(0, 0, -1));

//-------------------------------------------------------------------------------------
// ***** Sensor Fusion

SensorFusion::SensorFusion(SensorDevice* sensor)
  : ExposureRecordHistory(100), LastMessageExposureFrame(NULL),
    FocusDirection(Vector3d(0, 0, 0)), FocusFOV(0.0),
    FAccelInImuFrame(1000), FAccelInCameraFrame(1000), FAngV(20),
    EnableGravity(true), EnableYawCorrection(true), MagCalibrated(false),
    EnableCameraTiltCorrection(true),
    MotionTrackingEnabled(true), VisionPositionEnabled(true),
    CenterPupilDepth(0.0)
{
   pHandler = new BodyFrameHandler(this);

   // And the clock is running...
   LogText("*** SensorFusion Startup: TimeSeconds = %f\n", Timer::GetSeconds());

   if (sensor)
       AttachToSensor(sensor);
   
   Reset();
}

SensorFusion::~SensorFusion()
{   
    delete(pHandler);
}

bool SensorFusion::AttachToSensor(SensorDevice* sensor)
{
    pHandler->RemoveHandlerFromDevices();
    Reset();

    if (sensor != NULL)
    {
        // cache mag calibration state
        MagCalibrated = sensor->IsMagCalibrated();
        
        // Load IMU position
        Array<PositionCalibrationReport> reports;
        bool result = sensor->GetAllPositionCalibrationReports(&reports);
        if (result)
        {
            PositionCalibrationReport imu = reports[reports.GetSize() - 1];
            OVR_ASSERT(imu.PositionType == PositionCalibrationReport::PositionType_IMU);
            // convert from vision to the world frame
            // TBD convert rotation as necessary?
            imu.Position.x *= -1.0;
            imu.Position.z *= -1.0;

            ImuFromScreen = Transformd(Quatd(imu.Normal, imu.Angle), imu.Position).Inverted();

            Recording::GetRecorder().RecordLedPositions(reports);
            Recording::GetRecorder().RecordDeviceIfcVersion(sensor->GetDeviceInterfaceVersion());
		}
        
        // Repopulate CPFOrigin
        SetCenterPupilDepth(CenterPupilDepth);

        // Subscribe to sensor updates
        sensor->AddMessageHandler(pHandler);

        // Initialize the sensor state
        // TBD: This is a hack to avoid a race condition if sensor status is checked immediately 
        // after sensor creation but before any data has flowed through.  We should probably
        // not depend strictly on data flow to determine capabilities like orientation and position
        // tracking, or else use some sort of synchronous method to wait for data
        LocklessState init;
        init.StatusFlags = Status_OrientationTracked;   
        UpdatedState.SetState(init);
    }

    return true;
}

// Resets the current orientation
void SensorFusion::Reset()
{
    Lock::Locker lockScope(pHandler->GetHandlerLock());

    UpdatedState.SetState(LocklessState());
    WorldFromImu                        = PoseState<double>();
    WorldFromImu.Pose                   = ImuFromCpf.Inverted(); // place CPF at the origin, not the IMU
    CameraFromImu                       = PoseState<double>();
    VisionError                         = PoseState<double>();
    WorldFromCamera                     = DefaultWorldFromCamera;
    WorldFromCameraConfidence           = -1;

    ExposureRecordHistory.Clear();
    NextExposureRecord                  = ExposureRecord();
    LastMessageExposureFrame            = MessageExposureFrame(NULL);
    LastVisionAbsoluteTime              = 0;
    Stage                               = 0;
    
    MagRefs.Clear();
    MagRefIdx                           = -1;
    MagCorrectionIntegralTerm           = Quatd();
    AccelOffset                         = Vector3d();

    FAccelInCameraFrame.Clear();
    FAccelInImuFrame.Clear();
    FAngV.Clear();

    setNeckPivotFromPose ( WorldFromImu.Pose );
}

//-------------------------------------------------------------------------------------
//  Vision & message processing

void SensorFusion::OnVisionFailure()
{
    // do nothing
    Recording::GetRecorder().RecordVisionSuccess(false);
}

void SensorFusion::OnVisionPreviousFrame(const Transform<double>& cameraFromImu)
{
    // simply save the observation for use in the next OnVisionSuccess call;
    // this should not have unintended side-effects for position filtering, 
    // since the vision time is not updated and the system keeps thinking we don't have vision yet
    CameraFromImu.Pose = cameraFromImu;
}

void SensorFusion::OnVisionSuccess(const Transform<double>& cameraFromImu, UInt32 exposureCounter)
{
    Lock::Locker lockScope(pHandler->GetHandlerLock());

    Recording::GetRecorder().RecordVisionSuccess(true);

    LastVisionAbsoluteTime = GetTime();

    // ********* LastVisionExposureRecord *********

    // Skip old data and use the record that matches the exposure counter
    while (!ExposureRecordHistory.IsEmpty() &&
           (ExposureRecordHistory.PeekFront().ExposureCounter <= exposureCounter))
    {
        LastVisionExposureRecord = ExposureRecordHistory.PopFront();
    }

    // Use current values if we don't have historical data
    // Right now, this will happen if we get first frame after prediction failure,
    // and this exposure wasn't in the buffer.  (TBD: Unlikely.. unless IMU message wasn't sent?)
    if (LastVisionExposureRecord.ExposureCounter != exposureCounter)
        LastVisionExposureRecord = ExposureRecord(exposureCounter, GetTime(), WorldFromImu, PoseState<double>());

    // ********* CameraFromImu *********
    
    // This is stored in the camera frame, so need to be careful when combining with the IMU data,
    // which is in the world frame

    Transformd cameraFromImuPrev = CameraFromImu.Pose;
    CameraFromImu.Pose = cameraFromImu; 
    CameraFromImu.TimeInSeconds = LastVisionExposureRecord.ExposureTime;

    // Check LastVisionExposureRecord.Delta.TimeInSeconds to avoid divide by zero, which we could (rarely)
    // get if we didn't have exposures delta for history (skipped exposure counters
    // due to video mode change that stalls USB, etc).
    if (LastVisionExposureRecord.ImuOnlyDelta.TimeInSeconds > 0.001)
    {
        Vector3d visionVelocityInImuFrame = (cameraFromImu.Translation - cameraFromImuPrev.Translation) /
                                            LastVisionExposureRecord.ImuOnlyDelta.TimeInSeconds;
        // Use the accel data to estimate the velocity at the exposure time
        // (as opposed to the average velocity between exposures)
        Vector3d imuVelocityInWorldFrame = LastVisionExposureRecord.ImuOnlyDelta.LinearVelocity -
            LastVisionExposureRecord.ImuOnlyDelta.Pose.Translation / LastVisionExposureRecord.ImuOnlyDelta.TimeInSeconds;
        CameraFromImu.LinearVelocity = visionVelocityInImuFrame + 
            WorldFromCamera.Inverted().Rotate(imuVelocityInWorldFrame);
    }
    else
    {
        CameraFromImu.LinearVelocity = Vector3d(0,0,0);
    }
}

PoseStated SensorFusion::computeVisionError()
{
    PoseStated worldFromImuVision = WorldFromCamera * CameraFromImu;
    // Here we need to compute the difference between worldFromImuVision and WorldFromImu.
    // However this difference needs to be represented in the World frame, not IMU frame.
    // Therefore the computation is different from simply worldFromImuVision.Pose * WorldFromImu.Pose.Inverted().
    PoseStated err;
    err.Pose.Rotation = worldFromImuVision.Pose.Rotation * 
        LastVisionExposureRecord.WorldFromImu.Pose.Rotation.Inverted();
    err.Pose.Translation = worldFromImuVision.Pose.Translation - 
        LastVisionExposureRecord.WorldFromImu.Pose.Translation;
    err.LinearVelocity = worldFromImuVision.LinearVelocity - 
        LastVisionExposureRecord.WorldFromImu.LinearVelocity;
    return err;
}

Transform<double> SensorFusion::GetVisionPrediction(UInt32 exposureCounter)
{
    Lock::Locker lockScope(pHandler->GetHandlerLock());

    // Combine the small deltas together
    // Should only be one iteration, unless we are skipping camera frames
    ExposureRecord record;
    PoseState<double> delta = PoseState<double>();

    while (!ExposureRecordHistory.IsEmpty() && 
           (ExposureRecordHistory.PeekFront().ExposureCounter <= exposureCounter))
    {
        record = ExposureRecordHistory.PopFront();
        delta.AdvanceByDelta(record.ImuOnlyDelta);
    }
    // Put the combine exposure record back in the history, for use in HandleVisionSuccess(...)
    record.ImuOnlyDelta = delta;
    ExposureRecordHistory.PushFront(record);

    Transformd result;
    if (record.VisionTrackingAvailable)
    {
        // if the tracking is working normally, use the change in the main state (SFusion output)
        // to compute the prediction
        result = CameraFromImu.Pose *
            LastVisionExposureRecord.WorldFromImu.Pose.Inverted() * record.WorldFromImu.Pose;
    }
    else
    {
        // if we just acquired vision, the main state probably doesn't have the correct position,
        // so can't rely on it for prediction

        // solution: use the accelerometer and vision velocity to propagate the previous sample forward
        // (don't forget to transform IMU to the camera frame)
        result = Transform<double>
            (
                CameraFromImu.Pose.Rotation * delta.Pose.Rotation,
                CameraFromImu.Pose.Translation + CameraFromImu.LinearVelocity * delta.TimeInSeconds +
                WorldFromCamera.Inverted().Rotate(delta.Pose.Translation)
            );
    }

    return result;
}

void SensorFusion::handleMessage(const MessageBodyFrame& msg)
{
    if (msg.Type != Message_BodyFrame || !IsMotionTrackingEnabled())
        return;

    // Put the sensor readings into convenient local variables
    Vector3d gyro(msg.RotationRate); 
    Vector3d accel(msg.Acceleration); 
    Vector3d mag(msg.MagneticField);
    double DeltaT = msg.TimeDelta;

    // Keep track of time
    WorldFromImu.TimeInSeconds = msg.AbsoluteTimeSeconds;
    // We got an update in the last 60ms and the data is not very old
    bool visionIsRecent = (GetTime() - LastVisionAbsoluteTime < 0.07) && (GetVisionLatency() < 0.25);
    Stage++;

    // Insert current sensor data into filter history
    FAngV.PushBack(gyro);
    FAccelInImuFrame.Update(accel, DeltaT, Quatd(gyro, gyro.Length() * DeltaT));

    // Process raw inputs
    // in the future the gravity offset can be calibrated using vision feedback
    Vector3d accelInWorldFrame = WorldFromImu.Pose.Rotate(accel) - Vector3d(0, 9.8, 0);

    // Recompute the vision error to account for all the corrections and the new data
    VisionError = computeVisionError();

    // Update headset orientation   
    WorldFromImu.StoreAndIntegrateGyro(gyro, DeltaT);
    // Tilt correction based on accelerometer
    if (EnableGravity)
        applyTiltCorrection(DeltaT);
    // Yaw correction based on camera
    if (EnableYawCorrection && visionIsRecent)
        applyVisionYawCorrection(DeltaT);
    // Yaw correction based on magnetometer
	if (EnableYawCorrection && MagCalibrated) // MagCalibrated is always false for DK2 for now
		applyMagYawCorrection(mag, DeltaT);
	// Focus Correction
	if ((FocusDirection.x != 0.0f || FocusDirection.z != 0.0f) && FocusFOV < Mathf::Pi)
		applyFocusCorrection(DeltaT);

    // Update camera orientation
    if (EnableCameraTiltCorrection && visionIsRecent)
        applyCameraTiltCorrection(accel, DeltaT);

    // The quaternion magnitude may slowly drift due to numerical error,
    // so it is periodically normalized.
    if ((Stage & 0xFF) == 0)
    {
        WorldFromImu.Pose.Rotation.Normalize();
        WorldFromCamera.Rotation.Normalize();
    }

    // Update headset position
    if (VisionPositionEnabled && visionIsRecent)
    {
        // Integrate UMI and velocity here up to a fixed amount of time after vision. 
        WorldFromImu.StoreAndIntegrateAccelerometer(accelInWorldFrame + AccelOffset, DeltaT);
        // Position correction based on camera
        applyPositionCorrection(DeltaT); 
        // Compute where the neck pivot would be.
        setNeckPivotFromPose(WorldFromImu.Pose);
    }
    else
    {
        // Fall back onto internal head model
        // Use the last-known neck pivot position to figure out the expected IMU position.
        // (should be the opposite of SensorFusion::setNeckPivotFromPose)
        WorldFromNeck.Rotation = WorldFromImu.Pose.Rotation;
        WorldFromImu.Pose = WorldFromNeck * (ImuFromCpf * CpfFromNeck).Inverted();

        // We can't trust velocity past this point.
        WorldFromImu.LinearVelocity = Vector3d(0,0,0);
        WorldFromImu.LinearAcceleration = accelInWorldFrame;
    }

    // Compute the angular acceleration
    WorldFromImu.AngularAcceleration = (FAngV.GetSize() >= 12 && DeltaT > 0) ? 
        (FAngV.SavitzkyGolayDerivative12() / DeltaT) : Vector3d();

    // Update the dead reckoning state used for incremental vision tracking
    NextExposureRecord.ImuOnlyDelta.StoreAndIntegrateGyro(gyro, DeltaT);
    NextExposureRecord.ImuOnlyDelta.StoreAndIntegrateAccelerometer(accelInWorldFrame, DeltaT);
    NextExposureRecord.ImuOnlyDelta.TimeInSeconds = WorldFromImu.TimeInSeconds - LastMessageExposureFrame.CameraTimeSeconds;
    NextExposureRecord.VisionTrackingAvailable &= (VisionPositionEnabled && visionIsRecent);

	Recording::GetRecorder().LogData("sfTimeSeconds", WorldFromImu.TimeInSeconds);
    Recording::GetRecorder().LogData("sfStage", (double)Stage);
	Recording::GetRecorder().LogData("sfPose", WorldFromImu.Pose);
	//Recorder::LogData("sfAngAcc", State.AngularAcceleration);
	//Recorder::LogData("sfAngVel", State.AngularVelocity);
	//Recorder::LogData("sfLinAcc", State.LinearAcceleration);
	//Recorder::LogData("sfLinVel", State.LinearVelocity);

    // Store the lockless state.    
    LocklessState lstate;
    lstate.StatusFlags       = Status_OrientationTracked;
    if (VisionPositionEnabled)
        lstate.StatusFlags  |= Status_PositionConnected;
    if (VisionPositionEnabled && visionIsRecent)
        lstate.StatusFlags  |= Status_PositionTracked;

	//A convenient means to temporarily extract this flag
	TPH_IsPositionTracked = visionIsRecent;

    lstate.State        = WorldFromImu;
    lstate.Temperature  = msg.Temperature;
    lstate.Magnetometer = mag;    
    UpdatedState.SetState(lstate);
}

void SensorFusion::handleExposure(const MessageExposureFrame& msg)
{
    NextExposureRecord.ExposureCounter = msg.CameraFrameCount;
    NextExposureRecord.ExposureTime = msg.CameraTimeSeconds;
    NextExposureRecord.WorldFromImu = WorldFromImu;
    NextExposureRecord.ImuOnlyDelta.TimeInSeconds = msg.CameraTimeSeconds - LastMessageExposureFrame.CameraTimeSeconds;
    ExposureRecordHistory.PushBack(NextExposureRecord);

    // Every new exposure starts from zero
    NextExposureRecord = ExposureRecord(); 
    LastMessageExposureFrame = msg;
}

// If you have a known-good pose, this sets the neck pivot position.
void SensorFusion::setNeckPivotFromPose(Transformd const &worldFromImu)
{
    WorldFromNeck = worldFromImu * ImuFromCpf * CpfFromNeck;
}

// These two functions need to be moved into Quat class
// Compute a rotation required to transform "from" into "to". 
Quatd vectorAlignmentRotation(const Vector3d &from, const Vector3d &to)
{
    Vector3d axis = from.Cross(to);
    if (axis.LengthSq() == 0)
        // this handles both collinear and zero-length input cases
        return Quatd();
    double angle = from.Angle(to);
    return Quatd(axis, angle);
}

// Compute the part of the quaternion that rotates around Y axis
Quatd extractYawRotation(const Quatd &error)
{
    if (error.y == 0)
        return Quatd();
    double phi = atan2(error.w, error.y);
    double alpha = Mathd::Pi - 2 * phi;
    return Quatd(Axis_Y, alpha);
}

void SensorFusion::applyPositionCorrection(double deltaT)
{
    // Each component of gainPos is equivalent to a Kalman gain of (sigma_process / sigma_observation)
	const Vector3d gainPos = Vector3d(10, 10, 8);
    const Vector3d gainVel = gainPos.EntrywiseMultiply(gainPos) * 0.5;
    const Vector3d gainAccel = gainVel * 0.5;
    const double snapThreshold = 0.1; // Large value (previously 0.01, which caused frequent jumping)

    Vector3d correctionPos, correctionVel;
    if (VisionError.Pose.Translation.LengthSq() > (snapThreshold * snapThreshold) ||
        !(UpdatedState.GetState().StatusFlags & Status_PositionTracked))
    {
        // high error or just reacquired position from vision - apply full correction

        // to know where we are right now, take the vision pose (which is slightly old)
        // and update it using the imu data since then
        PoseStated worldFromImuVision = WorldFromCamera * CameraFromImu;
        for (unsigned int i = 0; i < ExposureRecordHistory.GetSize(); i++)
            worldFromImuVision.AdvanceByDelta(ExposureRecordHistory.PeekFront(i).ImuOnlyDelta);
        worldFromImuVision.AdvanceByDelta(NextExposureRecord.ImuOnlyDelta);

        correctionPos = worldFromImuVision.Pose.Translation - WorldFromImu.Pose.Translation;
        correctionVel = worldFromImuVision.LinearVelocity - WorldFromImu.LinearVelocity;
        AccelOffset   = Vector3d();
    }
    else
    {
        correctionPos = VisionError.Pose.Translation.EntrywiseMultiply(gainPos) * deltaT;
        correctionVel = VisionError.Pose.Translation.EntrywiseMultiply(gainVel) * deltaT;
        AccelOffset  += VisionError.Pose.Translation.EntrywiseMultiply(gainAccel) * deltaT;
    }

    WorldFromImu.Pose.Translation += correctionPos;
    WorldFromImu.LinearVelocity += correctionVel;

    // Update the exposure records so that we don't apply the same correction twice
    LastVisionExposureRecord.WorldFromImu.Pose.Translation += correctionPos;
    LastVisionExposureRecord.WorldFromImu.LinearVelocity += correctionVel;
    for (unsigned int i = 0; i < ExposureRecordHistory.GetSize(); i++)
    {
        PoseStated& state = ExposureRecordHistory.PeekBack(i).WorldFromImu;
        state.Pose.Translation += correctionPos;
        state.LinearVelocity += correctionVel;
    }
}

void SensorFusion::applyVisionYawCorrection(double deltaT)
{
    const double gain = 0.25;
    const double snapThreshold = 0.1;

    Quatd yawError = extractYawRotation(VisionError.Pose.Rotation);

    Quatd correction;
    if (Alg::Abs(yawError.w) < cos(snapThreshold / 2)) // angle(yawError) > snapThreshold
        // high error, jump to the vision position
        correction = yawError;
    else
        correction = yawError.Nlerp(Quatd(), gain * deltaT);

    WorldFromImu.Pose.Rotation = correction * WorldFromImu.Pose.Rotation;

    // Update the exposure records so that we don't apply the same correction twice
    LastVisionExposureRecord.WorldFromImu.Pose.Rotation = correction * LastVisionExposureRecord.WorldFromImu.Pose.Rotation;
    for (unsigned int i = 0; i < ExposureRecordHistory.GetSize(); i++)
    {
        PoseStated& state = ExposureRecordHistory.PeekBack(i).WorldFromImu;
        state.Pose.Rotation = correction * state.Pose.Rotation;
    }
}

void SensorFusion::applyMagYawCorrection(Vector3d mag, double deltaT)
{
    const double minMagLengthSq   = Mathd::Tolerance; // need to use a real value to discard very weak fields
    const double maxMagRefDist    = 0.1;
    const double maxTiltError     = 0.05;
    const double proportionalGain = 0.01;
    const double integralGain     = 0.0005;

    Vector3d magInWorldFrame = WorldFromImu.Pose.Rotate(mag);
    // verify that the horizontal component is sufficient
    if (magInWorldFrame.x * magInWorldFrame.x + magInWorldFrame.z * magInWorldFrame.z < minMagLengthSq)
        return;
    magInWorldFrame.Normalize();

    // Delete a bad point
    if (MagRefIdx >= 0 && MagRefs[MagRefIdx].Score < 0)
    {
        MagRefs.RemoveAtUnordered(MagRefIdx);
        MagRefIdx = -1;
    }

    // Update the reference point if needed
    if (MagRefIdx < 0 || mag.Distance(MagRefs[MagRefIdx].InImuFrame) > maxMagRefDist)
    {
        // Find a new one
        MagRefIdx = -1;
        double bestDist = maxMagRefDist;
        for (unsigned int i = 0; i < MagRefs.GetSize(); i++)
        {
            double dist = mag.Distance(MagRefs[i].InImuFrame);
            if (bestDist > dist)
            {
                bestDist = dist;
                MagRefIdx = i;
            }
        }

        // Create one if needed
        if (MagRefIdx < 0 && MagRefs.GetSize() < MagMaxReferences)
		{
            MagRefs.PushBack(MagReferencePoint(mag, WorldFromImu.Pose, 1000));
		}
    }

    if (MagRefIdx >= 0)
    {
        Vector3d magRefInWorldFrame = MagRefs[MagRefIdx].WorldFromImu.Rotate(MagRefs[MagRefIdx].InImuFrame);
        magRefInWorldFrame.Normalize();

        // If the vertical angle is wrong, decrease the score and do nothing
        if (Alg::Abs(magRefInWorldFrame.y - magInWorldFrame.y) > maxTiltError)
        {
            MagRefs[MagRefIdx].Score -= 1;
            return;
        }

        MagRefs[MagRefIdx].Score += 2;
#if 0
        // this doesn't seem to work properly, need to investigate
        Quatd error = vectorAlignmentRotation(magW, magRefW);
        Quatd yawError = extractYawRotation(error);
#else
        // Correction is computed in the horizontal plane
        magInWorldFrame.y = magRefInWorldFrame.y = 0;
        Quatd yawError = vectorAlignmentRotation(magInWorldFrame, magRefInWorldFrame);
#endif                                                 
        Quatd correction = yawError.Nlerp(Quatd(), proportionalGain * deltaT) *
                           MagCorrectionIntegralTerm.Nlerp(Quatd(), deltaT);
        MagCorrectionIntegralTerm = MagCorrectionIntegralTerm * yawError.Nlerp(Quatd(), integralGain * deltaT);

        WorldFromImu.Pose.Rotation = correction * WorldFromImu.Pose.Rotation;
    }
}

void SensorFusion::applyTiltCorrection(double deltaT)
{
    const double gain = 0.25;
    const double snapThreshold = 0.1;
    const Vector3d up(0, 1, 0);

    Vector3d accelInWorldFrame = WorldFromImu.Pose.Rotate(FAccelInImuFrame.GetFilteredValue());
    Quatd error = vectorAlignmentRotation(accelInWorldFrame, up);

    Quatd correction;
    if (FAccelInImuFrame.GetSize() == 1 || 
        ((Alg::Abs(error.w) < cos(snapThreshold / 2) && FAccelInImuFrame.Confidence() > 0.75)))
        // full correction for start-up
        // or large error with high confidence
        correction = error;
    else if (FAccelInImuFrame.Confidence() > 0.5)
        correction = error.Nlerp(Quatd(), gain * deltaT);
    else
        // accelerometer is unreliable due to movement
        return;

    WorldFromImu.Pose.Rotation = correction * WorldFromImu.Pose.Rotation;
}

void SensorFusion::applyCameraTiltCorrection(Vector3d accel, double deltaT)
{
    const double snapThreshold = 0.02; // in radians
    const double maxCameraPositionOffset = 0.2;
    const Vector3d up(0, 1, 0), forward(0, 0, -1);

    // for startup use filtered value instead of instantaneous for stability
    if (FAccelInCameraFrame.IsEmpty())
        accel = FAccelInImuFrame.GetFilteredValue();

    Transformd cameraFromImu = WorldFromCamera.Inverted() * VisionError.Pose * WorldFromImu.Pose;
    // this is what the hypothetical camera-mounted accelerometer would show
    Vector3d accelInCameraFrame = cameraFromImu.Rotate(accel);
    FAccelInCameraFrame.Update(accelInCameraFrame, deltaT);
    Vector3d cameraAccelInWorldFrame = WorldFromCamera.Rotate(FAccelInCameraFrame.GetFilteredValue());
   
    Quatd error1 = vectorAlignmentRotation(cameraAccelInWorldFrame, up);
    // cancel out yaw rotation
    Vector3d forwardCamera = (error1 * WorldFromCamera.Rotation).Rotate(forward);
    forwardCamera.y = 0;
    Quatd error2 = vectorAlignmentRotation(forwardCamera, forward);
    // combined error
    Quatd error = error2 * error1;

    double confidence = FAccelInCameraFrame.Confidence();
    // penalize the confidence if looking away from the camera
    // TODO: smooth fall-off
    if (CameraFromImu.Pose.Rotate(forward).Angle(forward) > 1)
        confidence *= 0.5;

	//Convenient global variable to temporarily extract this data.
	TPH_CameraPoseConfidence = confidence;
	//Allow override of confidence threshold
	double confidenceThreshold = 0.75f;
	if (TPH_CameraPoseConfidenceThresholdOverrideIfNonZero)
    {
		confidenceThreshold = TPH_CameraPoseConfidenceThresholdOverrideIfNonZero;
	}

    Quatd correction;
    if (FAccelInCameraFrame.GetSize() == 1 ||
        confidence > WorldFromCameraConfidence + 0.2 ||
        // disabled due to false positives when moving side to side
//        (Alg::Abs(error.w) < cos(5 * snapThreshold / 2) && confidence > 0.55) ||    
        (Alg::Abs(error.w) < cos(snapThreshold / 2) && confidence > confidenceThreshold))
    {
        // large error with high confidence
        correction = error;
        // update the confidence level
        WorldFromCameraConfidence = confidence;
    }
    else
    {
        // accelerometer is unreliable due to movement
        return;
    }

    Transformd newWorldFromCamera(correction * WorldFromCamera.Rotation, Vector3d());

    // compute a camera position change that together with the camera rotation would result in zero player movement
    newWorldFromCamera.Translation += (WorldFromCamera * CameraFromImu.Pose).Translation -
        (newWorldFromCamera * CameraFromImu.Pose).Translation;
    // if the new position is too far, reset to default 
    // (can't hide the rotation, might as well use it to reset the position)
    if (newWorldFromCamera.Translation.DistanceSq(DefaultWorldFromCamera.Translation) > maxCameraPositionOffset * maxCameraPositionOffset)
        newWorldFromCamera.Translation = DefaultWorldFromCamera.Translation;

    WorldFromCamera = newWorldFromCamera;

	//Convenient global variable to temporarily extract this data.
	TPH_CameraPoseOrientationWxyz[0] = (float) WorldFromCamera.Rotation.w;
	TPH_CameraPoseOrientationWxyz[1] = (float) WorldFromCamera.Rotation.x;
	TPH_CameraPoseOrientationWxyz[2] = (float) WorldFromCamera.Rotation.y;
	TPH_CameraPoseOrientationWxyz[3] = (float) WorldFromCamera.Rotation.z;
}

void SensorFusion::applyFocusCorrection(double deltaT)
{
	Vector3d up = Vector3d(0, 1, 0);
	double gain = 0.01;
	Vector3d currentDir = WorldFromImu.Pose.Rotate(Vector3d(0, 0, 1));

	Vector3d focusYawComponent = FocusDirection.ProjectToPlane(up);
	Vector3d currentYawComponent = currentDir.ProjectToPlane(up);

	double angle = focusYawComponent.Angle(currentYawComponent);

	if( angle > FocusFOV )
	{
		Quatd yawError;
		if ( FocusFOV != 0.0f)
		{
			Vector3d lFocus = Quatd(up, -FocusFOV).Rotate(focusYawComponent);
			Vector3d rFocus = Quatd(up, FocusFOV).Rotate(focusYawComponent);
			double lAngle = lFocus.Angle(currentYawComponent);
			double rAngle = rFocus.Angle(currentYawComponent);
			if(lAngle < rAngle)
			{
				yawError = vectorAlignmentRotation(currentDir, lFocus);
			} 
            else
			{
				yawError = vectorAlignmentRotation(currentDir, rFocus);
			}
		} 
        else
		{
			yawError = vectorAlignmentRotation(currentYawComponent, focusYawComponent);
		}

		Quatd correction = yawError.Nlerp(Quatd(), gain * deltaT);
        WorldFromImu.Pose.Rotation = correction * WorldFromImu.Pose.Rotation;
	}
}

//------------------------------------------------------------------------------------
// Focus filter setting functions

void SensorFusion::SetFocusDirection()
{
	SetFocusDirection(WorldFromImu.Pose.Rotate(Vector3d(0.0, 0.0, 1.0)));
}

void SensorFusion::SetFocusDirection(Vector3d direction)
{
	FocusDirection = direction;
}

void SensorFusion::SetFocusFOV(double fov)
{
	OVR_ASSERT(fov >= 0.0);
	FocusFOV = fov;
}

void SensorFusion::ClearFocus()
{
	FocusDirection = Vector3d(0.0, 0.0, 0.0);
	FocusFOV = 0.0f;
}

//-------------------------------------------------------------------------------------
// Head model functions.

// Sets up head-and-neck model and device-to-pupil dimensions from the user's profile.
void SensorFusion::SetUserHeadDimensions(Profile const &profile, HmdRenderInfo const &hmdRenderInfo)
{
    float neckeye[2];
    int count = profile.GetFloatValues(OVR_KEY_NECK_TO_EYE_DISTANCE, neckeye, 2);
    // Make sure these are vaguely sensible values.
    if (count == 2)
    {
        OVR_ASSERT ( ( neckeye[0] > 0.05f ) && ( neckeye[0] < 0.5f ) );
        OVR_ASSERT ( ( neckeye[1] > 0.05f ) && ( neckeye[1] < 0.5f ) );
        SetHeadModel ( Vector3f ( 0.0, neckeye[1], -neckeye[0] ) );
    }

    // Find the distance from the center of the screen to the "center eye"
    // This center eye is used by systems like rendering & audio to represent the player,
    // and they will handle the offsets needed from there to each actual eye.

    // HACK HACK HACK
    // We know for DK1 the screen->lens surface distance is roughly 0.049f, and that the faceplate->lens is 0.02357f.
    // We're going to assume(!!!!) that all HMDs have the same screen->faceplate distance.
    // Crystal Cove was measured to be roughly 0.025 screen->faceplate which agrees with this assumption.
    // TODO: do this properly!  Update:  Measured this at 0.02733 with a CC prototype, CES era (PT7), on 2/19/14 -Steve
    float screenCenterToMidplate = 0.02733f;
    float centerEyeRelief = hmdRenderInfo.GetEyeCenter().ReliefInMeters;
    float centerPupilDepth = screenCenterToMidplate + hmdRenderInfo.LensSurfaceToMidplateInMeters + centerEyeRelief;
    SetCenterPupilDepth ( centerPupilDepth );

    Recording::GetRecorder().RecordUserParams(GetHeadModel(), GetCenterPupilDepth());
}

Vector3f SensorFusion::GetHeadModel() const
{
    return (Vector3f)CpfFromNeck.Inverted().Translation;
}

void SensorFusion::SetHeadModel(const Vector3f &headModel, bool resetNeckPivot /*= true*/ )
{
    Lock::Locker lockScope(pHandler->GetHandlerLock());
    // The head model should look something like (0, 0.12, -0.12), so
    // these asserts are to try to prevent sign problems, as
    // they can be subtle but nauseating!
    OVR_ASSERT ( headModel.y > 0.0f );
    OVR_ASSERT ( headModel.z < 0.0f );
    CpfFromNeck = Transformd(Quatd(), (Vector3d)headModel).Inverted();
    if ( resetNeckPivot )
    {
        setNeckPivotFromPose ( WorldFromImu.Pose );
    }
}

float SensorFusion::GetCenterPupilDepth() const
{
    return CenterPupilDepth;
}

void SensorFusion::SetCenterPupilDepth(float centerPupilDepth)
{
    CenterPupilDepth = centerPupilDepth;

    Transformd screenFromCpf(Quatd(), Vector3d(0, 0, centerPupilDepth));
    ImuFromCpf = ImuFromScreen * screenFromCpf;
	
    setNeckPivotFromPose ( WorldFromImu.Pose );
}

//-------------------------------------------------------------------------------------

// This is a "perceptually tuned predictive filter", which means that it is optimized
// for improvements in the VR experience, rather than pure error.  In particular,
// jitter is more perceptible at lower speeds whereas latency is more perceptible
// after a high-speed motion.  Therefore, the prediction interval is dynamically
// adjusted based on speed.  Significant more research is needed to further improve
// this family of filters.
static Transform<double> calcPredictedPose(const PoseState<double>& poseState, double predictionDt)
{
    Transform<double> pose   = poseState.Pose;
	const double linearCoef  = 1.0;
	Vector3d angularVelocity = poseState.AngularVelocity;
	double   angularSpeed    = angularVelocity.Length();

	// This could be tuned so that linear and angular are combined with different coefficients
	double speed             = angularSpeed + linearCoef * poseState.LinearVelocity.Length();

	const double slope       = 0.2; // The rate at which the dynamic prediction interval varies
	double candidateDt       = slope * speed; // TODO: Replace with smoothstep function

	double dynamicDt         = predictionDt;

	// Choose the candidate if it is shorter, to improve stability
	if (candidateDt < predictionDt)
		dynamicDt = candidateDt;

    if (angularSpeed > 0.001)
        pose.Rotation = pose.Rotation * Quatd(angularVelocity, angularSpeed * dynamicDt);

    pose.Translation += poseState.LinearVelocity * dynamicDt;

    return pose;
}


Transformf SensorFusion::GetPoseAtTime(double absoluteTime) const
{
    SensorState ss = GetSensorStateAtTime ( absoluteTime );
    return ss.Predicted.Pose;
}


SensorState SensorFusion::GetSensorStateAtTime(double absoluteTime) const
{          
     const LocklessState lstate = UpdatedState.GetState();
     // Delta time from the last available data
     const double pdt = absoluteTime - lstate.State.TimeInSeconds;
     
     SensorState ss;
     ss.Recorded     = PoseStatef(lstate.State);
     ss.Temperature  = lstate.Temperature;
     ss.Magnetometer = Vector3f(lstate.Magnetometer);
     ss.StatusFlags  = lstate.StatusFlags;

     ss.Predicted               = ss.Recorded;
     ss.Predicted.TimeInSeconds = absoluteTime;
    
     // Do prediction logic and ImuFromCpf transformation
     ss.Recorded.Pose  = Transformf(lstate.State.Pose * ImuFromCpf);
     ss.Predicted.Pose = Transformf(calcPredictedPose(lstate.State, pdt) * ImuFromCpf);
     return ss;
}

unsigned SensorFusion::GetStatus() const
{
    return UpdatedState.GetState().StatusFlags;
}

//-------------------------------------------------------------------------------------

void SensorFusion::OnMessage(const MessageBodyFrame& msg)
{
    OVR_ASSERT(!IsAttachedToSensor());
    handleMessage(msg);
}

//-------------------------------------------------------------------------------------

void SensorFusion::BodyFrameHandler::OnMessage(const Message& msg)
{
	Recording::GetRecorder().RecordMessage(msg);
    if (msg.Type == Message_BodyFrame)
        pFusion->handleMessage(static_cast<const MessageBodyFrame&>(msg));
    if (msg.Type == Message_ExposureFrame)
        pFusion->handleExposure(static_cast<const MessageExposureFrame&>(msg));
}

} // namespace OVR
