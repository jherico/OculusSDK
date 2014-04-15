/************************************************************************************

Filename    :   OVR_SensorFusion.cpp
Content     :   Methods that determine head orientation from sensor data over time
Created     :   October 9, 2012
Authors     :   Michael Antonov, Steve LaValle, Dov Katz, Max Katsev

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
#include "Recording/Recorder.h"

// Temporary for debugging
bool Global_Flag_1 = true;

//Convenient global variable to temporarily extract this data.
float TPH_CameraPoseOrientationWxyz[4];


namespace OVR {

//-------------------------------------------------------------------------------------
// ***** Sensor Fusion

SensorFusion::SensorFusion(SensorDevice* sensor)
  : MotionTrackingEnabled(true), VisionPositionEnabled(true),
    EnableGravity(true), EnableYawCorrection(true), MagCalibrated(true), EnableCameraTiltCorrection(true), 
    FAngV(20), FAccelHeadset(1000), FAccelCamera(1000),
    ExposureRecordHistory(100), LastMessageExposureFrame(NULL),
    VisionMaxIMUTrackTime(4.0/60.0), // Integrate IMU up to 4 frames
    HeadModel(0, OVR_DEFAULT_NECK_TO_EYE_VERTICAL, -OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL),
    DefaultCameraPosition(0, 0, -1)
{
   pHandler = new BodyFrameHandler(this);

   // And the clock is running...
   LogText("*** SensorFusion Startup: TimeSeconds = %f\n", Timer::GetSeconds());

   if (sensor)
       AttachToSensor(sensor);
   
   // MA: 1/25/2014 for DK2
   SetCenterPupilDepth(0.076f);

   Reset();
}

SensorFusion::~SensorFusion()
{   
    delete(pHandler);
}

bool SensorFusion::AttachToSensor(SensorDevice* sensor)
{
    if (sensor != NULL)
    {
        // Load IMU position
        Array<PositionCalibrationReport> reports;

        bool result = sensor->GetAllPositionCalibrationReports(&reports);
        if(result)
        {
            PositionCalibrationReport const& imu = reports[reports.GetSize() - 1];
            OVR_ASSERT(imu.PositionType == PositionCalibrationReport::PositionType_IMU);
            IMUPosition = imu.Position;

            Recorder::Buffer(imu);
            Recorder::Buffer(reports);

            // convert from vision to the world frame
            IMUPosition.x *= -1.0;
            IMUPosition.z *= -1.0;
		}
        else
        {
            // TODO: set up IMUPosition for devices that don't have this report.
        }
        // Repopulate CPFOrigin
        SetCenterPupilDepth(CenterPupilDepth);
    }

    pHandler->RemoveHandlerFromDevices();

    if (sensor != NULL)
    {
        sensor->AddMessageHandler(pHandler);
    }

    Reset();

    // Initialize the sensor state
    // TBD: This is a hack to avoid a race condition if sensor status is checked immediately 
    // after sensor creation but before any data has flowed through.  We should probably
    // not depend strictly on data flow to determine capabilites like orientation and position
    // tracking, or else use some sort of synchronous method to wait for data
    LocklessState init;
    init.StatusFlags = Status_OrientationTracked;   
    UpdatedState.SetState(init);

    return true;
}

// Resets the current orientation
void SensorFusion::Reset()
{
    Lock::Locker lockScope(pHandler->GetHandlerLock());

    UpdatedState.SetState(LocklessState());
    State                               = PoseState<double>();
    State.Transform.Position                 = -CPFPositionInIMUFrame; // place CPF at the origin, not the IMU
    VisionState                         = PoseState<double>();
    VisionError                         = PoseState<double>();
    CurrentExposureIMUDelta             = PoseState<double>();
    CameraPose                          = Pose<double>(Quatd(), DefaultCameraPosition);
    CameraPoseConfidence                = -1;

    ExposureRecordHistory.Clear();
    LastMessageExposureFrame            = MessageExposureFrame(NULL);
    LastVisionAbsoluteTime              = 0;
    FullVisionCorrectionExposureCounter = 0;
    Stage                               = 0;
    
    MagNumReferences                    = 0;
    MagRefIdx                           = -1;
    MagRefScore                         = 0;
    MagCorrectionIntegralTerm           = Quatd();
    AccelOffset                         = Vector3d();

    FAccelCamera.Clear();
    FAccelHeadset.Clear();
    FAngV.Clear();

    setNeckPivotFromPose ( State.Transform );
}

//-------------------------------------------------------------------------------------
//  Vision & message processing

void SensorFusion::OnVisionFailure()
{
    // do nothing
}

void SensorFusion::OnVisionPreviousFrame(const Pose<double>& pose)
{
    // simply save the observation for use in the next OnVisionSuccess call;
    // this should not have unintended side-effects for position filtering, 
    // since the vision time is not updated and the system keeps thinking we don't have vision yet
    VisionState.Transform = pose;
}

void SensorFusion::OnVisionSuccess(const Pose<double>& pose, UInt32 exposureCounter)
{
    Lock::Locker lockScope(pHandler->GetHandlerLock());

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
        LastVisionExposureRecord = ExposureRecord(exposureCounter, GetTime(), State, PoseState<double>());

    // ********* VisionState *********
    
    // This is stored in the camera frame, so need to be careful when combining with the IMU data,
    // which is in the world frame

    // convert to the world frame
    Vector3d positionChangeW = CameraPose.Orientation.Rotate(pose.Position - VisionState.Transform.Position);

    VisionState.TimeInSeconds = LastVisionExposureRecord.ExposureTime;
    VisionState.Transform = pose; 

    // Check LastVisionExposureRecord.Delta.TimeInSeconds to avoid divide by zero, which we could (rarely)
    // get if we didn't have exposures delta for history (skipped exposure counters
    // due to video mode change that stalls USB, etc).
    if (LastVisionExposureRecord.Delta.TimeInSeconds > 0.001)
    {
        // Use the accel data to estimate the velocity at the exposure time
        // (as opposed to the average velocity between exposures)
        Vector3d velocityW = LastVisionExposureRecord.Delta.LinearVelocity + 
                             (positionChangeW - LastVisionExposureRecord.Delta.Transform.Position) / 
                             LastVisionExposureRecord.Delta.TimeInSeconds;
        VisionState.LinearVelocity = CameraPose.Orientation.Inverted().Rotate(velocityW);
    }
    else
    {
        VisionState.LinearVelocity = Vector3d(0,0,0);
    } 

    // ********* VisionError *********

    // This is in the world frame, so transform the vision data appropriately

    VisionError.Transform.Position    = CameraPose.Orientation.Rotate(VisionState.Transform.Position) + CameraPose.Position - 
						           LastVisionExposureRecord.State.Transform.Position;
    VisionError.LinearVelocity   = CameraPose.Orientation.Rotate(VisionState.LinearVelocity) - 
        						   LastVisionExposureRecord.State.LinearVelocity;
    VisionError.Transform.Orientation = CameraPose.Orientation * VisionState.Transform.Orientation * 
	                               LastVisionExposureRecord.State.Transform.Orientation.Inverted();
}

Pose<double> SensorFusion::GetVisionPrediction(UInt32 exposureCounter)
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
        delta.AdvanceByDelta(record.Delta);
    }
    // Put the combine exposure record back in the history, for use in HandleVisionSuccess(...)
    record.Delta = delta;
    ExposureRecordHistory.PushFront(record);

    // Add the effect of initial pose and velocity from vision.
    // Don't forget to transform IMU to the camera frame
    Pose<double> c(VisionState.Transform.Orientation * delta.Transform.Orientation,
                   VisionState.Transform.Position + VisionState.LinearVelocity * delta.TimeInSeconds +
                   CameraPose.Orientation.Inverted().Rotate(delta.Transform.Position));

    return c;
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
    MagCalibrated = msg.MagCalibrated;

    // Keep track of time
    State.TimeInSeconds = msg.AbsoluteTimeSeconds;
    // We got an update in the last 60ms and the data is not very old
    bool visionIsRecent = (GetTime() - LastVisionAbsoluteTime < VisionMaxIMUTrackTime) && (GetVisionLatency() < 0.25);
    Stage++;

    // Insert current sensor data into filter history
    FAngV.PushBack(gyro);
    FAccelHeadset.Update(accel, DeltaT, Quatd(gyro, gyro.Length() * DeltaT));

    // Process raw inputs
    // in the future the gravity offset can be calibrated using vision feedback
    Vector3d accelW = State.Transform.Orientation.Rotate(accel) - Vector3d(0, 9.8, 0);

    // Update headset orientation   
    State.StoreAndIntegrateGyro(gyro, DeltaT);
    // Tilt correction based on accelerometer
    if (EnableGravity)
        applyTiltCorrection(DeltaT);
    // Yaw correction based on camera
    if (EnableYawCorrection && visionIsRecent)
        applyVisionYawCorrection(DeltaT);
    // Yaw correction based on magnetometer
	if (EnableYawCorrection && MagCalibrated) // MagCalibrated is always false for DK2 for now
		applyMagYawCorrection(mag, DeltaT);

    // Update camera orientation
    if (EnableCameraTiltCorrection && visionIsRecent)
        applyCameraTiltCorrection(accel, DeltaT);

    // The quaternion magnitude may slowly drift due to numerical error,
    // so it is periodically normalized.
    if ((Stage & 0xFF) == 0)
    {
        State.Transform.Orientation.Normalize();
        CameraPose.Orientation.Normalize();
    }

    // Update headset position
    if (VisionPositionEnabled && visionIsRecent)
    {
        // Integrate UMI and velocity here up to a fixed amount of time after vision. 
        State.StoreAndIntegrateAccelerometer(accelW + AccelOffset, DeltaT);
        // Position correction based on camera
        applyPositionCorrection(DeltaT); 
        // Compute where the neck pivot would be.
        setNeckPivotFromPose(State.Transform);
    }
    else
    {
        // Fall back onto internal head model
        // Use the last-known neck pivot position to figure out the expected IMU position.
        // (should be the opposite of SensorFusion::setNeckPivotFromPose)
        Vector3d imuInNeckPivotFrame = HeadModel - CPFPositionInIMUFrame;
        State.Transform.Position = NeckPivotPosition + State.Transform.Orientation.Rotate(imuInNeckPivotFrame);

        // We can't trust velocity past this point.
        State.LinearVelocity = Vector3d(0,0,0);
        State.LinearAcceleration = accelW;
    }

    // Compute the angular acceleration
    State.AngularAcceleration = (FAngV.GetSize() >= 12 && DeltaT > 0) ? 
        (FAngV.SavitzkyGolayDerivative12() / DeltaT) : Vector3d();

    // Update the dead reckoning state used for incremental vision tracking
    CurrentExposureIMUDelta.StoreAndIntegrateGyro(gyro, DeltaT);
    CurrentExposureIMUDelta.StoreAndIntegrateAccelerometer(accelW, DeltaT);

	// If we only compiled the stub version of Recorder, then branch prediction shouldn't
	// have any problem with this if statement. Actually, it should be optimized out, but need to verify.
	if(Recorder::GetRecorder())
	{
        Posed savePose = static_cast<Posed>(GetPoseAtTime(GetTime()));
		Recorder::LogData("sfTimeSeconds", State.TimeInSeconds);
		Recorder::LogData("sfPose", savePose);
	}

    // Store the lockless state.    
    LocklessState lstate;
    lstate.StatusFlags       = Status_OrientationTracked;
    if (VisionPositionEnabled)
        lstate.StatusFlags  |= Status_PositionConnected;
    if (VisionPositionEnabled && visionIsRecent)
        lstate.StatusFlags  |= Status_PositionTracked;
    lstate.State        = State;
    lstate.Temperature  = msg.Temperature;
    lstate.Magnetometer = mag;    
    UpdatedState.SetState(lstate);
}

void SensorFusion::handleExposure(const MessageExposureFrame& msg)
{
    if (msg.CameraFrameCount > LastMessageExposureFrame.CameraFrameCount + 1)
    {
        LogText("Skipped %d tracker exposure counters\n",
            msg.CameraFrameCount - (LastMessageExposureFrame.CameraFrameCount + 1));
    }
    else
    {
        // MA: Check timing deltas
        //     Is seems repetitive tracking loss occurs when timing gets out of sync
        //     Could be caused by some bug in HW timing + time filter?
        if (fabs(State.TimeInSeconds - msg.CameraTimeSeconds) > 0.1f)
        {
            static int logLimiter = 0;
            if ((logLimiter & 0x3F) == 0)
            {
                LogText("Timing out of sync: State.T=%f, ExposureT=%f, delta=%f, Time()=%f\n",
                    State.TimeInSeconds, msg.CameraTimeSeconds,
                    State.TimeInSeconds - msg.CameraTimeSeconds, GetTime());
            }
            logLimiter++;
        }

    }

    CurrentExposureIMUDelta.TimeInSeconds = msg.CameraTimeSeconds - LastMessageExposureFrame.CameraTimeSeconds;
    ExposureRecordHistory.PushBack(ExposureRecord(
        msg.CameraFrameCount, msg.CameraTimeSeconds, State, CurrentExposureIMUDelta));

    // Every new exposure starts from zero
    CurrentExposureIMUDelta = PoseState<double>(); 
    LastMessageExposureFrame = msg;
}

// If you have a known-good pose, this sets the neck pivot position.
void SensorFusion::setNeckPivotFromPose(Posed const &pose)
{
    Vector3d imuInNeckPivotFrame = HeadModel - CPFPositionInIMUFrame;
    NeckPivotPosition = pose.Position - pose.Orientation.Rotate(imuInNeckPivotFrame);
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
    const double snapThreshold = 0.1; // Large value (previously 0.01, which caused frequent jumping)

    if (LastVisionExposureRecord.ExposureCounter <= FullVisionCorrectionExposureCounter)
        return;

    if (VisionError.Transform.Position.LengthSq() > (snapThreshold * snapThreshold) ||
        !(UpdatedState.GetState().StatusFlags & Status_PositionTracked))
    {
        // high error or just reacquired position from vision - apply full correction
        State.Transform.Position += VisionError.Transform.Position;
        State.LinearVelocity += VisionError.LinearVelocity;
        // record the frame counter to avoid additional correction until we see the new data
        FullVisionCorrectionExposureCounter = LastMessageExposureFrame.CameraFrameCount;        
    }
    else
    {
        State.Transform.Position += VisionError.Transform.Position.EntrywiseMultiply(gainPos) * deltaT;
        State.LinearVelocity += VisionError.Transform.Position.EntrywiseMultiply(gainVel) * deltaT;
		// Uncomment the line below to try acclerometer bias estimation in filter
        //AccelOffset += VisionError.Pose.Position * gainAccel * deltaT;
    }
}

void SensorFusion::applyVisionYawCorrection(double deltaT)
{
    const double gain = 0.25;
    const double snapThreshold = 0.1;

    if (LastVisionExposureRecord.ExposureCounter <= FullVisionCorrectionExposureCounter)
        return;

    Quatd yawError = extractYawRotation(VisionError.Transform.Orientation);

    Quatd correction;
    if (Alg::Abs(yawError.w) < cos(snapThreshold / 2)) // angle(yawError) > snapThreshold
    {
        // high error, jump to the vision position
        correction = yawError;
        // record the frame counter to avoid additional correction until we see the new data
        FullVisionCorrectionExposureCounter = LastMessageExposureFrame.CameraFrameCount;        
    }
    else
        correction = yawError.Nlerp(Quatd(), gain * deltaT);

    State.Transform.Orientation = correction * State.Transform.Orientation;
}

void SensorFusion::applyMagYawCorrection(Vector3d mag, double deltaT)
{
    const double minMagLengthSq   = Mathd::Tolerance; // need to use a real value to discard very weak fields
    const double maxMagRefDist    = 0.1;
    const double maxTiltError     = 0.05;
    const double proportionalGain = 0.01;
    const double integralGain     = 0.0005;

    Vector3d magW = State.Transform.Orientation.Rotate(mag);
    // verify that the horizontal component is sufficient
    if (magW.x * magW.x + magW.z * magW.z < minMagLengthSq)
        return;
    magW.Normalize();

    // Update the reference point if needed
    if (MagRefScore < 0 || MagRefIdx < 0 ||
		mag.Distance(MagRefsInBodyFrame[MagRefIdx]) > maxMagRefDist)
    {
        // Delete a bad point
        if (MagRefIdx >= 0 && MagRefScore < 0)
        {
            MagNumReferences--;
            MagRefsInBodyFrame[MagRefIdx]  = MagRefsInBodyFrame[MagNumReferences];
            MagRefsInWorldFrame[MagRefIdx] = MagRefsInWorldFrame[MagNumReferences];
            MagRefsPoses[MagRefIdx] = MagRefsPoses[MagRefIdx];
        }
        // Find a new one
        MagRefIdx = -1;
        MagRefScore = 1000;
        double bestDist = maxMagRefDist;
        for (int i = 0; i < MagNumReferences; i++)
        {
            double dist = mag.Distance(MagRefsInBodyFrame[i]);
            if (bestDist > dist)
            {
                bestDist = dist;
                MagRefIdx = i;
            }
        }
        // Create one if needed
        if (MagRefIdx < 0 && MagNumReferences < MagMaxReferences)
        {
            MagRefIdx = MagNumReferences;
            MagRefsInBodyFrame[MagRefIdx] = mag;
            MagRefsInWorldFrame[MagRefIdx] = magW;
            MagRefsPoses[MagRefIdx] = State.Transform.Orientation;
            MagNumReferences++;
        }
    }

    if (MagRefIdx >= 0)
    {
        Vector3d magRefW = MagRefsInWorldFrame[MagRefIdx];

        // If the vertical angle is wrong, decrease the score and do nothing
        if (Alg::Abs(magRefW.y - magW.y) > maxTiltError)
        {
            MagRefScore -= 1;
            return;
        }

        MagRefScore += 2;
#if 0
        // this doesn't seem to work properly, need to investigate
        Quatd error = vectorAlignmentRotation(magW, magRefW);
        Quatd yawError = extractYawRotation(error);
#else
        // Correction is computed in the horizontal plane
        magW.y = magRefW.y = 0;
        Quatd yawError = vectorAlignmentRotation(magW, magRefW);
#endif                                                 
        Quatd correction = yawError.Nlerp(Quatd(), proportionalGain * deltaT) *
                           MagCorrectionIntegralTerm.Nlerp(Quatd(), deltaT);
        MagCorrectionIntegralTerm = MagCorrectionIntegralTerm * yawError.Nlerp(Quatd(), integralGain * deltaT);

        State.Transform.Orientation = correction * State.Transform.Orientation;
    }
}

void SensorFusion::applyTiltCorrection(double deltaT)
{
    const double gain = 0.25;
    const double snapThreshold = 0.1;
    const Vector3d up(0, 1, 0);

    Vector3d accelW = State.Transform.Orientation.Rotate(FAccelHeadset.GetFilteredValue());
    Quatd error = vectorAlignmentRotation(accelW, up);

    Quatd correction;
    if (FAccelHeadset.GetSize() == 1 || 
        ((Alg::Abs(error.w) < cos(snapThreshold / 2) && FAccelHeadset.Confidence() > 0.75)))
        // full correction for start-up
        // or large error with high confidence
        correction = error;
    else if (FAccelHeadset.Confidence() > 0.5)
        correction = error.Nlerp(Quatd(), gain * deltaT);
    else
        // accelerometer is unreliable due to movement
        return;

    State.Transform.Orientation = correction * State.Transform.Orientation;
}

void SensorFusion::applyCameraTiltCorrection(Vector3d accel, double deltaT)
{
    const double snapThreshold = 0.02; // in radians
    const double maxCameraPositionOffset = 0.2;
    const Vector3d up(0, 1, 0), forward(0, 0, -1);

    if (LastVisionExposureRecord.ExposureCounter <= FullVisionCorrectionExposureCounter)
        return;

    // for startup use filtered value instead of instantaneous for stability
    if (FAccelCamera.IsEmpty())
        accel = FAccelHeadset.GetFilteredValue();

    Quatd headsetToCamera = CameraPose.Orientation.Inverted() * VisionError.Transform.Orientation * State.Transform.Orientation;
    // this is what the hypothetical camera-mounted accelerometer would show
    Vector3d accelCamera = headsetToCamera.Rotate(accel);
    FAccelCamera.Update(accelCamera, deltaT);
    Vector3d accelCameraW = CameraPose.Orientation.Rotate(FAccelCamera.GetFilteredValue());
   
    Quatd error1 = vectorAlignmentRotation(accelCameraW, up);
    // cancel out yaw rotation
    Vector3d forwardCamera = (error1 * CameraPose.Orientation).Rotate(forward);
    forwardCamera.y = 0;
    Quatd error2 = vectorAlignmentRotation(forwardCamera, forward);
    // combined error
    Quatd error = error2 * error1;

    double confidence = FAccelCamera.Confidence();
    // penalize the confidence if looking away from the camera
    // TODO: smooth fall-off
    if (VisionState.Transform.Orientation.Rotate(forward).Angle(forward) > 1)
        confidence *= 0.5;

    Quatd correction;
    if (FAccelCamera.GetSize() == 1 ||
        confidence > CameraPoseConfidence + 0.2 ||
        // disabled due to false positives when moving side to side
//        (Alg::Abs(error.w) < cos(5 * snapThreshold / 2) && confidence > 0.55) ||    
        (Alg::Abs(error.w) < cos(snapThreshold / 2) && confidence > 0.75))
    {
        // large error with high confidence
        correction = error;
        // update the confidence level
        CameraPoseConfidence = confidence;
        // record the frame counter to avoid additional correction until we see the new data
        FullVisionCorrectionExposureCounter = LastMessageExposureFrame.CameraFrameCount;

        LogText("adjust camera tilt confidence %f angle %f\n",
                CameraPoseConfidence, RadToDegree(correction.Angle(Quatd())));
    }
    else
    {
        // accelerometer is unreliable due to movement
        return;
    }

    Quatd newOrientation = correction * CameraPose.Orientation;
    // compute a camera position change that together with the camera rotation would result in zero player movement
    Vector3d newPosition = CameraPose.Orientation.Rotate(VisionState.Transform.Position) + CameraPose.Position - 
                            newOrientation.Rotate(VisionState.Transform.Position);
    // if the new position is too far, reset to default 
    // (can't hide the rotation, might as well use it to reset the position)
    if (newPosition.DistanceSq(DefaultCameraPosition) > maxCameraPositionOffset * maxCameraPositionOffset)
        newPosition = DefaultCameraPosition;

    CameraPose.Orientation = newOrientation;
    CameraPose.Position    = newPosition;

	//Convenient global variable to temporarily extract this data.
	TPH_CameraPoseOrientationWxyz[0] = (float) newOrientation.w;
	TPH_CameraPoseOrientationWxyz[1] = (float) newOrientation.x;
	TPH_CameraPoseOrientationWxyz[2] = (float) newOrientation.y;
	TPH_CameraPoseOrientationWxyz[3] = (float) newOrientation.z;


    LogText("adjust camera position %f %f %f\n", newPosition.x, newPosition.y, newPosition.z);
}

//-------------------------------------------------------------------------------------
// Head model functions.

// Sets up head-and-neck model and device-to-pupil dimensions from the user's profile.
void SensorFusion::SetUserHeadDimensions(Profile const *profile, HmdRenderInfo const &hmdRenderInfo)
{
    float neckEyeHori = OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL;
    float neckEyeVert = OVR_DEFAULT_NECK_TO_EYE_VERTICAL;
    if ( profile != NULL )
    {
        float neckeye[2];
        if (profile->GetFloatValues(OVR_KEY_NECK_TO_EYE_DISTANCE, neckeye, 2) == 2)
        {
            neckEyeHori = neckeye[0];
            neckEyeVert = neckeye[1];
        }
    }
    // Make sure these are vaguely sensible values.
    OVR_ASSERT ( ( neckEyeHori > 0.05f ) && ( neckEyeHori < 0.5f ) );
    OVR_ASSERT ( ( neckEyeVert > 0.05f ) && ( neckEyeVert < 0.5f ) );
    SetHeadModel ( Vector3f ( 0.0, neckEyeVert, -neckEyeHori ) );

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
    if ( profile == NULL )
    {
        // No valid profile, so the eye-relief won't be correct either, so fill in a default that feels good
        centerEyeRelief = 0.020f;
    }
    float centerPupilDepth = screenCenterToMidplate + hmdRenderInfo.LensSurfaceToMidplateInMeters + centerEyeRelief;
    SetCenterPupilDepth ( centerPupilDepth );
}

Vector3f SensorFusion::GetHeadModel() const
{
    return (Vector3f)HeadModel;
}

void SensorFusion::SetHeadModel(const Vector3f &headModel, bool resetNeckPivot /*= true*/ )
{
    Lock::Locker lockScope(pHandler->GetHandlerLock());
    // The head model should look something like (0, 0.12, -0.12), so
    // these asserts are to try to prevent sign problems, as
    // they can be subtle but nauseating!
    OVR_ASSERT ( headModel.y > 0.0f );
    OVR_ASSERT ( headModel.z < 0.0f );
    HeadModel = (Vector3d)headModel;
    if ( resetNeckPivot )
    {
        setNeckPivotFromPose ( State.Transform );
    }
}

float SensorFusion::GetCenterPupilDepth() const
{
    return CenterPupilDepth;
}


void SensorFusion::SetCenterPupilDepth(float centerPupilDepth)
{
    CenterPupilDepth = centerPupilDepth;

    CPFPositionInIMUFrame = -IMUPosition;
    CPFPositionInIMUFrame.z += CenterPupilDepth;
	
    setNeckPivotFromPose ( State.Transform );
}

//-------------------------------------------------------------------------------------

// This is a "perceptually tuned predictive filter", which means that it is optimized
// for improvements in the VR experience, rather than pure error.  In particular,
// jitter is more perceptible at lower speeds whereas latency is more perceptable
// after a high-speed motion.  Therefore, the prediction interval is dynamically
// adjusted based on speed.  Significant more research is needed to further improve
// this family of filters.
static Pose<double> calcPredictedPose(const PoseState<double>& poseState, double predictionDt)
{
    Pose<double> pose        = poseState.Transform;
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
        pose.Orientation = pose.Orientation * Quatd(angularVelocity, angularSpeed * dynamicDt);

    pose.Position += poseState.LinearVelocity * dynamicDt;

    return pose;
}


Posef SensorFusion::GetPoseAtTime(double absoluteTime) const
{
    SensorState ss = GetSensorStateAtTime ( absoluteTime );
    return ss.Predicted.Transform;
}


SensorState SensorFusion::GetSensorStateAtTime(double absoluteTime) const
{          
     const LocklessState lstate = UpdatedState.GetState();
     // Delta time from the last processed message
     const double        pdt   = absoluteTime - lstate.State.TimeInSeconds;
     
     SensorState ss;
     ss.Recorded     = PoseStatef(lstate.State);
     ss.Temperature  = lstate.Temperature;
     ss.Magnetometer = Vector3f(lstate.Magnetometer);
     ss.StatusFlags       = lstate.StatusFlags;

     // Do prediction logic
     ss.Predicted               = ss.Recorded;
     ss.Predicted.TimeInSeconds = absoluteTime;
     ss.Predicted.Transform          = Posef(calcPredictedPose(lstate.State, pdt));
    
     // CPFOriginInIMUFrame transformation
     const Vector3f cpfOriginInIMUFrame(CPFPositionInIMUFrame);
     ss.Recorded.Transform.Position  += ss.Recorded.Transform.Orientation.Rotate(cpfOriginInIMUFrame);
     ss.Predicted.Transform.Position += ss.Predicted.Transform.Orientation.Rotate(cpfOriginInIMUFrame);
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
	Recorder::Buffer(msg);
    if (msg.Type == Message_BodyFrame)
        pFusion->handleMessage(static_cast<const MessageBodyFrame&>(msg));
    if (msg.Type == Message_ExposureFrame)
        pFusion->handleExposure(static_cast<const MessageExposureFrame&>(msg));
}

} // namespace OVR
