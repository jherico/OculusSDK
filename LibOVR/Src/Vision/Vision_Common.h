/************************************************************************************

Filename    :   OVR_Vision_Common.h
Content     :   Common data structures that are used in multiple vision files
Created     :   November 25, 2014
Authors     :   Max Katsev

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

#ifndef OVR_Vision_Common_h
#define OVR_Vision_Common_h

#include "Extras/OVR_Math.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_Log.h"
#include "Sensors/OVR_DeviceConstants.h"
#include "Util/Util_Stopwatch.h"

// Compatible types (these are declared in global namespace)
typedef struct ovrPoseStatef_ ovrPoseStatef;
typedef struct ovrPoseStated_ ovrPoseStated;

namespace OVR { namespace Vision {
    
// Global "calibration mode" used by calibration tools to change
// the behavior of the SDK for calibration/experimentation purposes.
// This flag is set at system startup by calibration tools, and never changed.

extern int BundleCalibrationMode;

// Vision <-> OVR transform functions
//
// These transforms are required across the interface to many of the
// matching and reconstruction functions.
//
// OVR system is x+ right, y+ up, z+ back.
// Vision system is x+ right, y+ down, z+ forward.
// This is a 180 degree rotation about X axis.
//
template<typename T> inline Vector3<T> VisionFromOvr(const Vector3<T>& ovr)       { return Vector3<T>(ovr.x, -ovr.y, -ovr.z); }
template<typename T> inline Vector3<T> OvrFromVision(const Vector3<T>& vision)    { return Vector3<T>(vision.x, -vision.y, -vision.z); }
    
template<typename T> inline Quat<T> VisionFromOvr(const Quat<T>& ovr)       { return Quat<T>(ovr.x, -ovr.y, -ovr.z, ovr.w); }
template<typename T> inline Quat<T> OvrFromVision(const Quat<T>& vision)    { return Quat<T>(vision.x, -vision.y, -vision.z, vision.w); }

template<typename T> inline Pose<T> VisionFromOvr(const Pose<T>& ovr)       { return Pose<T>(VisionFromOvr(ovr.Rotation), VisionFromOvr(ovr.Translation)); }
template<typename T> inline Pose<T> OvrFromVision(const Pose<T>& vision)    { return Pose<T>(OvrFromVision(vision.Rotation), OvrFromVision(vision.Translation)); }

struct ImuSample
{
    double Time;

    Vector3d Accelerometer;
    Vector3d Gyro;
    Vector3d Magnetometer;
    double Temperature;

    ImuSample() : 
        Time(-1),
        Accelerometer(),
        Gyro(),
        Magnetometer(),
        Temperature(-1) {}

    ImuSample(const SensorDataType& data) : 
        Time(data.AbsoluteTimeSeconds),
        Accelerometer(data.Acceleration),
        Gyro(data.RotationRate),
        Magnetometer(data.MagneticField),
        Temperature(data.Temperature) {}
};

// vision pose result in the camera frame
struct PoseSample
{
    double Time; // capture time in seconds
    Posed CameraFromImu;
    Vector3d Velocity; // linear velocity in m/s in the camera frame
    Vector3d Omega; // angular velocity, Rodrigues representation, in rad/s

    double ObjectSpaceError;
    int MatchCount; // number of LEDs used for reconstruction

    bool HasPosition, HasOrientation, HasVelocities;
    Matrix3d PositionCovariance, OrientationCovariance;

    // Extra information that is logged but not otherwise used by tracking system
    double Stats[8];

OVR_DISABLE_MSVC_WARNING(4351)
    PoseSample() :
        Time(-1),
        CameraFromImu(),
        Velocity(),
        Omega(),
        ObjectSpaceError(DBL_MAX),
        MatchCount(-1),
        HasPosition(false),
        HasOrientation(false),
        HasVelocities(false),
        PositionCovariance(),
        OrientationCovariance(),
        Stats()
    {
    }
OVR_RESTORE_MSVC_WARNING()
};

// vision pose result in the world frame
struct PoseSampleWorld
{
    double Time; // capture time in seconds
    Posed WorldFromImu;
    Vector3d Velocity; // linear velocity in m/s
    Vector3d Omega; // angular velocity, Rodrigues representation, in rad/s

    bool HasPosition, HasOrientation, HasVelocities;
    Matrix3d PositionCovariance, OrientationCovariance;

    PoseSampleWorld() :
        Time(-1),
        WorldFromImu(),
        Velocity(),
        Omega(),
        HasPosition(false),
        HasOrientation(false),
        HasVelocities(false),
        PositionCovariance(),
        OrientationCovariance()
    {

    }

    PoseSampleWorld(const PoseSample& sample, const Posed& worldFromCamera) :
        Time(sample.Time),
        HasPosition(sample.HasPosition),
        HasOrientation(sample.HasOrientation),
        HasVelocities(sample.HasVelocities)
    {
        WorldFromImu = worldFromCamera * sample.CameraFromImu;
        Velocity = worldFromCamera.Rotate(sample.Velocity);
        Omega = worldFromCamera.Rotate(sample.Omega);

        Matrix3d R = (Matrix3d)worldFromCamera.Rotation;
        Matrix3d Rt = R.Transposed();
        PositionCovariance = R * sample.PositionCovariance * Rt;
        OrientationCovariance = R * sample.OrientationCovariance * Rt;
    }
};

// vision pose prediction in the world frame
struct PoseEstimateWorld
{
    double Time;        // capture time in seconds
    Posed WorldFromImu; // Gravity aligned

    bool HasPosition;       // WorldFromImu.Translation is valid
    bool HasOrientation;    // WorldFromImu.Rotation is valid and gravity aligned
    bool HasUp;             // WorldFromImu.Rotation is gravity aligned, but yaw is unknown

    PoseEstimateWorld() :
        Time(-1),
        WorldFromImu(),
        HasPosition(false),
        HasOrientation(false),
        HasUp(false)
    {
    }
};

// vision pose prediction in the camera frame (computed from PoseEstimateWorld)
struct PoseEstimate
{
    double Time;            // capture time in seconds
    Posed CameraFromImu;    // Gravity aligned
    Vector3d UpInImu;       // direction of gravity in the IMU frame

    bool HasPosition;       // WorldFromImu.Translation is valid
    bool HasOrientation;    // WorldFromImu.Rotation is valid and gravity aligned
    bool HasUp;             // WorldFromImu.Rotation is gravity aligned, but yaw is unknown

    PoseEstimate() :
        Time(-1),
        CameraFromImu(),
        UpInImu(),
        HasPosition(false),
        HasOrientation(false),
        HasUp(false)
    {
    }

    PoseEstimate(const PoseEstimateWorld& estimateWorld, const Posed& worldFromCamera)
    {
        Time = estimateWorld.Time;
        CameraFromImu = worldFromCamera.Inverted() * estimateWorld.WorldFromImu;
        UpInImu = estimateWorld.WorldFromImu.InverseRotate(Vector3d(0, 1, 0));
        HasPosition = estimateWorld.HasPosition;
        HasOrientation = estimateWorld.HasOrientation;
        HasUp = estimateWorld.HasUp;
    }
};

// PoseDistance functions
//
// Returns a distance between two poses, taking into account translation and rotation difference.
// This is an estimate of the worst-case distance between any two points of a model of size scale
// after being transformed by the two poses.
// These functions return the sum of the translation distance and the displacement due
// to the difference in rotation acting at a distance of arm, which should be roughly
// half the distance across the object.
// FIXME: Might be nice to add to Pose<T> in OVR_Math.h

template<typename T>
T PoseDistance(const Pose<T>& p1, const Pose<T>& p2, const T arm)
{
    return (p1.Translation.Distance(p2.Translation) + p1.Rotation.Angle(p2.Rotation) * arm);
}

template<typename T>
T PoseDistance(const Matrix3<T>& R1, const Vector3<T>& t1, const Matrix3<T>& R2, const Vector3<T>& t2, const T arm)
{
    return (t1.Distance(t2) + R1.Angle(R2) * arm);
}

} // namespace OVR::Vision

// PoseState describes the complete pose, or a rigid body configuration, at a
// point in time, including first and second derivatives. It is used to specify
// instantaneous location and movement of the headset.
// SensorState is returned as a part of the sensor state.
//
// This structure needs to be the same size and layout on 32-bit and 64-bit arch.
// Update OVR_PadCheck.cpp when updating this object.
template<class T>
class PoseState
{
public:
	typedef typename CompatibleTypes<Pose<T> >::Type CompatibleType;

	PoseState() : TimeInSeconds(0.0) { }

	// float <-> double conversion constructor.
	explicit PoseState(const PoseState<typename Math<T>::OtherFloatType> &src)
		: ThePose(src.ThePose),
		AngularVelocity(src.AngularVelocity), LinearVelocity(src.LinearVelocity),
		AngularAcceleration(src.AngularAcceleration), LinearAcceleration(src.LinearAcceleration),
		TimeInSeconds(src.TimeInSeconds)
	{ }

	// C-interop support: PoseStatef <-> ovrPoseStatef
	PoseState(const typename CompatibleTypes<PoseState<T> >::Type& src)
		: ThePose(src.ThePose),
		AngularVelocity(src.AngularVelocity), LinearVelocity(src.LinearVelocity),
		AngularAcceleration(src.AngularAcceleration), LinearAcceleration(src.LinearAcceleration),
		TimeInSeconds(src.TimeInSeconds)
	{ }

	operator typename CompatibleTypes<PoseState<T> >::Type() const
	{
		typename CompatibleTypes<PoseState<T> >::Type result;
		result.ThePose = ThePose;
		result.AngularVelocity = AngularVelocity;
		result.LinearVelocity = LinearVelocity;
		result.AngularAcceleration = AngularAcceleration;
		result.LinearAcceleration = LinearAcceleration;
		result.TimeInSeconds = TimeInSeconds;
		return result;
	}

	Pose<T> ThePose;
	Vector3<T>  AngularVelocity;
	Vector3<T>  LinearVelocity;
	Vector3<T>  AngularAcceleration;
	Vector3<T>  LinearAcceleration;
	// Absolute time of this state sample; always a double measured in seconds.
	double      TimeInSeconds;

	// ***** Helpers for Pose integration

    // Stores and integrates gyro angular velocity reading for a given time step.
    void StoreAndIntegrateGyro(Vector3d angVel, double dt)
    {
        AngularVelocity = angVel;
        ThePose.Rotation *= Quatd::FromRotationVector(angVel * dt);
    }

    void StoreAndIntegrateAccelerometer(Vector3d linearAccel, double dt)
    {
        LinearAcceleration = linearAccel;
        ThePose.Translation += LinearVelocity * dt + LinearAcceleration * (dt * dt * 0.5);
        LinearVelocity += LinearAcceleration * dt;
    }

    friend PoseState operator*(const Pose<T>& trans, const PoseState& poseState)
    {
        PoseState result;
        result.ThePose = trans * poseState.ThePose;
        result.LinearVelocity = trans.Rotate(poseState.LinearVelocity);
        result.LinearAcceleration = trans.Rotate(poseState.LinearAcceleration);
        result.AngularVelocity = trans.Rotate(poseState.AngularVelocity);
        result.AngularAcceleration = trans.Rotate(poseState.AngularAcceleration);
        return result;
    }
};

// External API returns pose as float, but uses doubles internally for quaternion precision.
typedef PoseState<float>  PoseStatef;
typedef PoseState<double> PoseStated;

// Compatible types
template<> struct CompatibleTypes<PoseState<float> > { typedef ovrPoseStatef Type; };
template<> struct CompatibleTypes<PoseState<double> > { typedef ovrPoseStated Type; };

// Handy debug output functions
template<typename T>
void Dump(const char* label, const Pose<T>& pose)
{
    auto t = pose.Translation * 1000;
    auto r = pose.Rotation.ToRotationVector();
    double angle = RadToDegree(r.Length());
    if (r.LengthSq() > 0)
        r.Normalize();
    LogText("%s: %.2f, %.2f, %.2f mm, %.2f deg %.2f, %.2f, %.2f\n",
            label, t.x, t.y, t.z, angle, r.x, r.y, r.z);
}

template<typename T>
void Dump(const char* label, const Vector3<T>& v)
{
    LogText("%s %.5g, %.5g, %.5g (%.5g)\n", label, v.x, v.y, v.z, v.Length());
}

template<typename T>
void Dump(const char* label, const Quat<T>& q)
{
    auto r = q.ToRotationVector();
    auto axis = r.Normalized();
    auto angle = RadToDegree(r.Length());
    LogText("%s %.2f (%.2f, %.2f, %.2f)\n", label, angle, axis.x, axis.y, axis.z);
}

template<typename T>
void Dump(const char* label, double time, const Pose<T>& p)
{
    LogText("%.4f: ", time);
    Dump(label, p);
}

static_assert((sizeof(PoseState<double>) == sizeof(Pose<double>) + 4 * sizeof(Vector3<double>) + sizeof(double)), "sizeof(PoseState<double>) failure");
#ifdef OVR_CPU_X86_64
static_assert((sizeof(PoseState<float>) == sizeof(Pose<float>) + 4 * sizeof(Vector3<float>) + sizeof(uint32_t)+sizeof(double)), "sizeof(PoseState<float>) failure"); //TODO: Manually pad template.
#elif defined(OVR_OS_WIN32) // The Windows 32 bit ABI aligns 64 bit values on 64 bit boundaries
static_assert((sizeof(PoseState<float>) == sizeof(Pose<float>) + 4 * sizeof(Vector3<float>) + sizeof(uint32_t)+sizeof(double)), "sizeof(PoseState<float>) failure");
#elif defined(OVR_CPU_ARM) // ARM aligns 64 bit values to 64 bit boundaries: http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0472k/chr1359125009502.html
static_assert((sizeof(PoseState<float>) == sizeof(Pose<float>) + 4 * sizeof(Vector3<float>) + sizeof(uint32_t)+sizeof(double)), "sizeof(PoseState<float>) failure");
#else // Else Unix/Apple 32 bit ABI, which aligns 64 bit values on 32 bit boundaries.
static_assert((sizeof(PoseState<float>) == sizeof(Pose<float>) + 4 * sizeof(Vector3<float>) + sizeof(double)), "sizeof(PoseState<float>) failure:");
#endif

} // namespace OVR

#endif // OVR_Vision_Common_h
