/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_SensorFusion.h
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

#ifndef OVR_SensorFusion_h
#define OVR_SensorFusion_h

#include "OVR_Device.h"
#include "OVR_SensorFilter.h"
#include <time.h>
#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_Threads.h"
#include "Kernel/OVR_Lockless.h"

// CAPI forward declarations.
typedef struct ovrPoseStatef_ ovrPoseStatef;
typedef struct ovrSensorState_ ovrSensorState;

namespace OVR {

struct HmdRenderInfo;

//-------------------------------------------------------------------------------------
// ***** Sensor State

// These values are reported as compatible with C API.


// PoseState describes the complete pose, or a rigid body configuration, at a
// point in time, including first and second derivatives. It is used to specify
// instantaneous location and movement of the headset.
// SensorState is returned as a part of the sensor state.

template<class T>
class PoseState
{
public:
    typedef typename CompatibleTypes<Pose<T> >::Type CompatibleType;

    PoseState() : TimeInSeconds(0.0) { }
    // float <-> double conversion constructor.
    explicit PoseState(const PoseState<typename Math<T>::OtherFloatType> &src)
        : Transform(src.Transform),
          AngularVelocity(src.AngularVelocity), LinearVelocity(src.LinearVelocity),
          AngularAcceleration(src.AngularAcceleration), LinearAcceleration(src.LinearAcceleration),
          TimeInSeconds(src.TimeInSeconds)
    { }

    // C-interop support: PoseStatef <-> ovrPoseStatef
    PoseState(const typename CompatibleTypes<PoseState<T> >::Type& src)
        : Transform(src.Pose),
          AngularVelocity(src.AngularVelocity), LinearVelocity(src.LinearVelocity),
          AngularAcceleration(src.AngularAcceleration), LinearAcceleration(src.LinearAcceleration),
          TimeInSeconds(src.TimeInSeconds)
    { }

    operator const typename CompatibleTypes<PoseState<T> >::Type () const
    {
        typename CompatibleTypes<PoseState<T> >::Type result;
        result.Pose		            = Transform;
        result.AngularVelocity      = AngularVelocity;
        result.LinearVelocity       = LinearVelocity;
        result.AngularAcceleration  = AngularAcceleration;
        result.LinearAcceleration   = LinearAcceleration;
        result.TimeInSeconds        = TimeInSeconds;
        return result;
    }


    Pose<T>     Transform;
    Vector3<T>  AngularVelocity;
    Vector3<T>  LinearVelocity;
    Vector3<T>  AngularAcceleration;
    Vector3<T>  LinearAcceleration;
    // Absolute time of this state sample; always a double measured in seconds.
    double      TimeInSeconds;

    
    // ***** Helpers for Pose integration

    // Stores and integrates gyro angular velocity reading for a given time step.
    void StoreAndIntegrateGyro(Vector3d angVel, double dt);
    // Stores and integrates position/velocity from accelerometer reading for a given time step.
    void StoreAndIntegrateAccelerometer(Vector3d linearAccel, double dt);
    
    // Performs integration of state by adding next state delta to it
    // to produce a combined state change
    void AdvanceByDelta(const PoseState<T>& delta);
};



// External API returns pose as float, but uses doubles internally for quaternion precision.
typedef PoseState<float>  PoseStatef;
typedef PoseState<double> PoseStated;


//-------------------------------------------------------------------------------------
// ***** Sensor State


// Bit flags describing the current status of sensor tracking.
enum StatusBits
{
    Status_OrientationTracked    = 0x0001,   // Orientation is currently tracked (connected and in use).
    Status_PositionTracked       = 0x0002,   // Position is currently tracked (false if out of range).
    Status_PositionConnected     = 0x0020,   // Position tracking HW is conceded.
   // Status_HMDConnected          = 0x0080    // HMD Display is available & connected.
};


// Full state of of the sensor reported by GetSensorState() at a given absolute time.
class SensorState
{
public:
    SensorState() : Temperature(0), StatusFlags(0) { }

    // C-interop support
    SensorState(const ovrSensorState& s);
    operator const ovrSensorState () const;

    // Pose state at the time that SensorState was requested.
    PoseStatef   Predicted;
    // Actual recorded pose configuration based on sensor sample at a 
    // moment closest to the requested time.
    PoseStatef   Recorded;

    // Calibrated magnetometer reading, in Gauss, at sample time.
    Vector3f     Magnetometer;
    // Sensor temperature reading, in degrees Celsius, at sample time.
    float        Temperature;
    // Sensor status described by ovrStatusBits.
    unsigned int StatusFlags;
};



//-------------------------------------------------------------------------------------

class VisionHandler
{
public:
    virtual void        OnVisionSuccess(const Pose<double>& pose, UInt32 exposureCounter) = 0;
    virtual void        OnVisionPreviousFrame(const Pose<double>& pose) = 0;
	virtual void		OnVisionFailure() = 0;

    // Get a configuration that represents the change over a short time interval
    virtual Pose<double> GetVisionPrediction(UInt32 exposureCounter) = 0;
};

//-------------------------------------------------------------------------------------
// ***** SensorFusion

// SensorFusion class accumulates Sensor notification messages to keep track of
// orientation, which involves integrating the gyro and doing correction with gravity.
// Magnetometer based yaw drift correction is also supported; it is usually enabled
// automatically based on loaded magnetometer configuration.
// Orientation is reported as a quaternion, from which users can obtain either the
// rotation matrix or Euler angles.
//
// The class can operate in two ways:
//  - By user manually passing MessageBodyFrame messages to the OnMessage() function. 
//  - By attaching SensorFusion to a SensorDevice, in which case it will
//    automatically handle notifications from that device.


class SensorFusion : public NewOverrideBase, public VisionHandler
{
    enum
    {
        MagMaxReferences = 1000
    };        

public:

	// -------------------------------------------------------------------------------
	// Critical components for tiny API

    SensorFusion(SensorDevice* sensor = 0);
    ~SensorFusion();

    // Attaches this SensorFusion to the IMU sensor device, from which it will receive
    // notification messages. If a sensor is attached, manual message notification
    // is not necessary. Calling this function also resets SensorFusion state.
    bool                        AttachToSensor(SensorDevice* sensor);

    // Returns true if this Sensor fusion object is attached to the IMU.
    bool                        IsAttachedToSensor() const;

    // Sets up head-and-neck model and device-to-pupil dimensions from the user's profile and the HMD stats.
    // This copes elegantly if profile is NULL.
    void                        SetUserHeadDimensions(Profile const *profile, HmdRenderInfo const &hmdRenderInfo);

	// Get the predicted pose (orientation, position) of the center pupil frame (CPF) at a specific point in time.
	Posef                       GetPoseAtTime(double absoluteTime) const;

    // Get the full dynamical system state of the CPF, which includes velocities and accelerations,
    // predicted at a specified absolute point in time.
    SensorState                 GetSensorStateAtTime(double absoluteTime) const;

    // Get the sensor status (same as GetSensorStateAtTime(...).Status)
    unsigned int                GetStatus() const;

	// End tiny API components
    // -------------------------------------------------------------------------------

    // Resets the current orientation.
    void        Reset                        ();

    // Configuration
    void        EnableMotionTracking(bool enable = true)    { MotionTrackingEnabled = enable; }
    bool        IsMotionTrackingEnabled() const             { return MotionTrackingEnabled;   }
    
    // Accelerometer/Gravity Correction Control
    // Enables/disables gravity correction (on by default).
    void        SetGravityEnabled   (bool enableGravity);
    bool        IsGravityEnabled    () const;

	// Vision Position and Orientation Configuration
    // -----------------------------------------------
	bool        IsVisionPositionEnabled       () const;
	void        SetVisionPositionEnabled      (bool enableVisionPosition);
    
    // compensates for a tilted camera
	void        SetCameraTiltCorrectionEnabled(bool enable);
    bool        IsCameraTiltCorrectionEnabled () const;

    // Message Handling Logic
    // -----------------------------------------------
    // Notifies SensorFusion object about a new BodyFrame 
    // message from a sensor.
    // Should be called by user if not attached to sensor.
    void        OnMessage                (const MessageBodyFrame& msg);
   

	// Interaction with vision
    // -----------------------------------------------
	// Handle observation from vision system (orientation, position, time)
    virtual void        OnVisionSuccess(const Pose<double>& pose, UInt32 exposureCounter);
    virtual void        OnVisionPreviousFrame(const Pose<double>& pose);
	virtual void		OnVisionFailure();
    // Get a configuration that represents the change over a short time interval
    virtual Pose<double> GetVisionPrediction(UInt32 exposureCounter);

    double              GetTime                ()     const;
    double              GetVisionLatency       ()     const;


	// Detailed head dimension control
    // -----------------------------------------------
	// These are now deprecated in favour of SetUserHeadDimensions()
	Vector3f                    GetHeadModel() const;
    void                        SetHeadModel(const Vector3f &headModel, bool resetNeckPivot = true );
	float                       GetCenterPupilDepth() const;
    void                        SetCenterPupilDepth(float centerPupilDepth);


    // Magnetometer and Yaw Drift Section:
    // ---------------------------------------

    // Enables/disables magnetometer based yaw drift correction. 
    // Must also have mag calibration data for this correction to work.
	void        SetYawCorrectionEnabled(bool enable);
    // Determines if yaw correction is enabled.
    bool        IsYawCorrectionEnabled () const;

    // True if mag has calibration values stored
    bool        HasMagCalibration      () const;
	// Clear the reference points associating
    // mag readings with orientations
	void        ClearMagReferences     ();

private:
  
    // -----------------------------------------------
    
	class BodyFrameHandler : public NewOverrideBase, public MessageHandler
    {
        SensorFusion* pFusion;
    public:
        BodyFrameHandler(SensorFusion* fusion) 
            : pFusion(fusion) {}

        ~BodyFrameHandler();

        virtual void OnMessage(const Message& msg);
        virtual bool SupportsMessageType(MessageType type) const;
    };   


    // -----------------------------------------------

    // State version stored in lockless updater "queue" and used for 
    // prediction by GetPoseAtTime/GetSensorStateAtTime
    struct LocklessState
    {        
        PoseState<double>  State;
        float              Temperature;
        Vector3d           Magnetometer;
        unsigned int       StatusFlags;

        LocklessState() : Temperature(0.0), StatusFlags(0) { };
    };


    // -----------------------------------------------

    // Entry describing the state of the headset at the time of an exposure as reported by the DK2 board.
    // This is used in combination with the vision data for 
    // incremental tracking based on IMU change and for drift correction
    struct ExposureRecord
    {
		UInt32            ExposureCounter;
        double            ExposureTime;
        PoseState<double> State;  // State of the headset at the time of exposure.
        PoseState<double> Delta;  // IMU Delta between previous exposure (or a vision frame) and this one.

		ExposureRecord() : ExposureCounter(0), ExposureTime(0.0) { }
		ExposureRecord(UInt32 exposureCounter, double exposureTime,
                      const PoseState<double>& state,
                      const PoseState<double>& stateDelta)
            : ExposureCounter(exposureCounter), ExposureTime(exposureTime),
              State(state), Delta(stateDelta) { }
    };

    // -----------------------------------------------

    // The phase of the head as estimated by sensor fusion
	PoseState<double> State;

    // State that can be read without any locks, so that high priority rendering thread
    // doesn't have to worry about being blocked by a sensor/vision threads that got preempted.
    LocklessUpdater<LocklessState>	UpdatedState;

    // The pose we got from Vision, augmented with velocity information from numerical derivatives
    // This is the only state that is stored in the camera reference frame; the rest are in the world frame
    PoseState<double> VisionState;    
    // Difference between the state from vision and the main State at the time of exposure
    PoseState<double> VisionError;
    // ExposureRecord that corresponds to the same exposure/frame as VisionState
    ExposureRecord    LastVisionExposureRecord; 
    // Change in state since the last exposure based on IMU data only 
    // (used for incremental tracking predictions)
    PoseState<double> CurrentExposureIMUDelta;
    // Past exposure records between the last update from vision and now
    // (should only be one record unless vision latency is high)
    CircularBuffer<ExposureRecord> ExposureRecordHistory;
    // Timings of the previous exposure, used to populate ExposureRecordHistory
    MessageExposureFrame LastMessageExposureFrame;
    // Time of the last vision update
    double            LastVisionAbsoluteTime;
    // Used by the head model.
    Vector3d          NeckPivotPosition;

    bool              EnableCameraTiltCorrection;
    // Pose of the camera in the world coordinate system
    Pose<double>      CameraPose;
    double            CameraPoseConfidence;
    Vector3d          DefaultCameraPosition;

    UInt32            FullVisionCorrectionExposureCounter;

    // This is a signed distance, but positive because Z increases looking inward.
    // This is expressed relative to the IMU in the HMD and corresponds to the location
	// of the cyclopean virtual camera focal point if both the physical and virtual 
	// worlds are isometrically mapped onto each other.  -Steve
    float                   CenterPupilDepth;
    Vector3d                CPFPositionInIMUFrame;
    // Position of the IMU relative to the center of the screen (loaded from the headset firmware)
    Vector3d                IMUPosition;
    // Origin of the positional coordinate system in the real world relative to the camera.
    Vector3d                PositionOrigin;

	double			        VisionMaxIMUTrackTime;
        
    unsigned int            Stage;
    BodyFrameHandler        *pHandler;
    volatile bool           EnableGravity;

    SensorFilterBodyFrame   FAccelHeadset, FAccelCamera;
    SensorFilterd           FAngV;

    Vector3d                AccelOffset;

    bool                     EnableYawCorrection;
    bool                     MagCalibrated;
public: // The below made public for access during rendering for debugging
    int                      MagNumReferences;
    Vector3d                 MagRefsInBodyFrame[MagMaxReferences];
    Vector3d                 MagRefsInWorldFrame[MagMaxReferences];
	Quatd					 MagRefsPoses[MagMaxReferences];
    int                      MagRefIdx;
private:
    int                      MagRefScore;
    Quatd                    MagCorrectionIntegralTerm;

    bool                     MotionTrackingEnabled;
    bool                     VisionPositionEnabled;

	// Built-in head model for faking 
    // position using orientation only 
	Vector3d                 HeadModel; 

    //---------------------------------------------    

    // Internal handler for messages
    // bypasses error checking.
    void        handleMessage(const MessageBodyFrame& msg);
    void        handleExposure(const MessageExposureFrame& msg);

    // Returns  new gyroCorrection
    Vector3d    calcMagYawCorrectionForMessage(Vector3d gyroCorrection,
                                               Quatd q, Quatd qInv,
                                               Vector3d calMag, Vector3d up, double deltaT);
	// Apply headset yaw correction from magnetometer
	// for models without camera or when camera isn't available
	void applyMagYawCorrection(Vector3d mag, double deltaT);
    // Apply headset tilt correction from the accelerometer
    void        applyTiltCorrection(double deltaT);
    // Apply headset yaw correction from the camera
    void        applyVisionYawCorrection(double deltaT);
    // Apply headset position correction from the camera
    void        applyPositionCorrection(double deltaT);
    // Apply camera tilt correction from the accelerometer
    void        applyCameraTiltCorrection(Vector3d accel, double deltaT);

    // If you have a known-good pose, this sets the neck pivot position.
    void        setNeckPivotFromPose ( Posed const &pose );
};



//-------------------------------------------------------------------------------------
// ***** SensorFusion - Inlines

inline bool SensorFusion::IsAttachedToSensor() const  
{ 
    return pHandler->IsHandlerInstalled(); 
}

inline void SensorFusion::SetGravityEnabled(bool enableGravity)     
{ 
    EnableGravity = enableGravity; 
}   

inline bool SensorFusion::IsGravityEnabled() const
{
    return EnableGravity;
}

inline void SensorFusion::SetYawCorrectionEnabled(bool enable)
{ 
    EnableYawCorrection = enable; 
}

inline bool SensorFusion::IsYawCorrectionEnabled() const          
{
    return EnableYawCorrection;
}

inline bool SensorFusion::HasMagCalibration() const 
{
    return MagCalibrated;
}  

inline void SensorFusion::ClearMagReferences()
{ 
    MagNumReferences = 0; 
}

inline bool SensorFusion::IsVisionPositionEnabled() const             
{ 
    return VisionPositionEnabled;
}

inline void SensorFusion::SetVisionPositionEnabled(bool enableVisionPosition)  
{ 
    VisionPositionEnabled = enableVisionPosition;
}

inline void SensorFusion::SetCameraTiltCorrectionEnabled(bool enable)   
{
    EnableCameraTiltCorrection = enable; 
}

inline bool SensorFusion::IsCameraTiltCorrectionEnabled() const       
{
    return EnableCameraTiltCorrection;
}

inline double SensorFusion::GetVisionLatency() const
{
    return LastVisionAbsoluteTime - VisionState.TimeInSeconds;
}

inline double SensorFusion::GetTime() const 
{ 
    return Timer::GetSeconds(); 
}

inline SensorFusion::BodyFrameHandler::~BodyFrameHandler()
{
    RemoveHandlerFromDevices();
}

inline bool SensorFusion::BodyFrameHandler::SupportsMessageType(MessageType type) const
{
    return (type == Message_BodyFrame || type == Message_ExposureFrame);
}


//-------------------------------------------------------------------------------------
// ***** PoseState - Inlines

// Stores and integrates gyro angular velocity reading for a given time step.
template<class T>
void PoseState<T>::StoreAndIntegrateGyro(Vector3d angVel, double dt)
{
    AngularVelocity = angVel;
    double angle = angVel.Length() * dt;
    if (angle > 0)
        Transform.Orientation = Transform.Orientation * Quatd(angVel, angle);
}

template<class T>
void PoseState<T>::StoreAndIntegrateAccelerometer(Vector3d linearAccel, double dt)
{
    LinearAcceleration = linearAccel;
    Transform.Position     += LinearVelocity * dt + LinearAcceleration * (dt * dt * 0.5);
    LinearVelocity    += LinearAcceleration * dt;
}

// Performs integration of state by adding next state delta to it
// to produce a combined state change
template<class T>
void PoseState<T>::AdvanceByDelta(const PoseState<T>& delta)
{
    Transform.Orientation  = Transform.Orientation * delta.Transform.Orientation;
    Transform.Position    += delta.Transform.Position + LinearVelocity * delta.TimeInSeconds;
    LinearVelocity   += delta.LinearVelocity;
    TimeInSeconds    += delta.TimeInSeconds;
}

} // namespace OVR
#endif
