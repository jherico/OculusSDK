/********************************************************************************//**

\file OVR_CAPI_0_6_0.h
\brief C Interface to the Oculus PC SDK tracking and rendering library.

\copyright Copyright 2014 Oculus VR, LLC All Rights reserved.
\n
Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.
\n
You may obtain a copy of the License at
\n
http://www.oculusvr.com/licenses/LICENSE-3.2
\n
Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_CAPI_h  //   We don't use version numbers within this name, as all versioned variations of this file are currently mutually exclusive.
#define OVR_CAPI_h  ///< Header include guard


#include "OVR_CAPI_Keys.h"
#include "OVR_Version.h"
#include "OVR_ErrorCode.h"


#include <stdint.h>

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4324) // structure was padded due to __declspec(align())
#endif



//-----------------------------------------------------------------------------------
// ***** OVR_OS
//
#if !defined(OVR_OS_WIN32) && defined(_WIN32) 
    #define OVR_OS_WIN32
#endif

#if !defined(OVR_OS_MAC) && defined(__APPLE__)
    #define OVR_OS_MAC
#endif

#if !defined(OVR_OS_LINUX) && defined(__linux__)
    #define OVR_OS_LINUX
#endif



//-----------------------------------------------------------------------------------
// ***** OVR_CPP
//
#if !defined(OVR_CPP)
    #if defined(__cplusplus)
        #define OVR_CPP(x) x
    #else
        #define OVR_CPP(x) /* Not C++ */
    #endif
#endif



//-----------------------------------------------------------------------------------
// ***** OVR_CDECL
//
/// LibOVR calling convention for 32-bit Windows builds.
//
#if !defined(OVR_CDECL)
    #if defined(_WIN32)
        #define OVR_CDECL __cdecl
    #else
        #define OVR_CDECL
    #endif
#endif



//-----------------------------------------------------------------------------------
// ***** OVR_EXTERN_C
//
/// Defined as extern "C" when built from C++ code.
//
#if !defined(OVR_EXTERN_C)
    #ifdef __cplusplus 
        #define OVR_EXTERN_C extern "C"
    #else
        #define OVR_EXTERN_C
    #endif
#endif



//-----------------------------------------------------------------------------------
// ***** OVR_PUBLIC_FUNCTION / OVR_PRIVATE_FUNCTION
//
// OVR_PUBLIC_FUNCTION  - Functions that externally visible from a shared library. Corresponds to Microsoft __dllexport.
// OVR_PUBLIC_CLASS     - C++ structs and classes that are externally visible from a shared library. Corresponds to Microsoft __dllexport.
// OVR_PRIVATE_FUNCTION - Functions that are not visible outside of a shared library. They are private to the shared library.
// OVR_PRIVATE_CLASS    - C++ structs and classes that are not visible outside of a shared library. They are private to the shared library.
//
// OVR_DLL_BUILD        - Used to indicate that the current compilation unit is of a shared library.
// OVR_DLL_IMPORT       - Used to indicate that the current compilation unit is a user of the corresponding shared library.
// OVR_DLL_BUILD        - used to indicate that the current compilation unit is not a shared library but rather statically linked code.
//
#if !defined(OVR_PUBLIC_FUNCTION)
    #if defined(OVR_DLL_BUILD)
        #if defined(_WIN32)
            #define OVR_PUBLIC_FUNCTION(rval) OVR_EXTERN_C __declspec(dllexport) rval OVR_CDECL
            #define OVR_PUBLIC_CLASS          __declspec(dllexport)
            #define OVR_PRIVATE_FUNCTION
            #define OVR_PRIVATE_CLASS
        #else
            #define OVR_PUBLIC_FUNCTION(rval) OVR_EXTERN_C __attribute__((visibility("default"))) rval OVR_CDECL /* Requires GCC 4.0+ */
            #define OVR_PUBLIC_CLASS          __attribute__((visibility("default"))) /* Requires GCC 4.0+ */
            #define OVR_PRIVATE_FUNCTION      __attribute__((visibility("hidden")))
            #define OVR_PRIVATE_CLASS         __attribute__((visibility("hidden")))
        #endif
    #elif defined(OVR_DLL_IMPORT)
        #if defined(_WIN32)
            #define OVR_PUBLIC_FUNCTION(rval) OVR_EXTERN_C __declspec(dllimport) rval OVR_CDECL
            #define OVR_PUBLIC_CLASS          __declspec(dllimport)
            #define OVR_PRIVATE_FUNCTION
            #define OVR_PRIVATE_CLASS
        #else
            #define OVR_PUBLIC_FUNCTION(rval) OVR_EXTERN_C rval OVR_CDECL
            #define OVR_PUBLIC_CLASS
            #define OVR_PRIVATE_FUNCTION
            #define OVR_PRIVATE_CLASS
        #endif
    #else // OVR_STATIC_BUILD
        #define OVR_PUBLIC_FUNCTION(rval)     OVR_EXTERN_C rval OVR_CDECL
        #define OVR_PUBLIC_CLASS
        #define OVR_PRIVATE_FUNCTION
        #define OVR_PRIVATE_CLASS
    #endif
#endif


//-----------------------------------------------------------------------------------
// ***** OVR_EXPORT
//
/// Provided for backward compatibility with older versions of this library.
//
#if !defined(OVR_EXPORT)
    #ifdef OVR_OS_WIN32
        #define OVR_EXPORT __declspec(dllexport)
    #else
        #define OVR_EXPORT
    #endif
#endif



//-----------------------------------------------------------------------------------
// ***** OVR_ALIGNAS
//
#if !defined(OVR_ALIGNAS)
    // C++11 alignas
    #if defined(__GNUC__) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 408) && (defined(__GXX_EXPERIMENTAL_CXX0X__) || (__cplusplus >= 201103L))
        #define OVR_ALIGNAS(n) alignas(n)
    #elif defined(__clang__) && !defined(__APPLE__) && (((__clang_major__ * 100) + __clang_minor__) >= 300) && (__cplusplus >= 201103L)
        #define OVR_ALIGNAS(n) alignas(n)
    #elif defined(__clang__) && defined(__APPLE__) && (((__clang_major__ * 100) + __clang_minor__) >= 401) && (__cplusplus >= 201103L)
        #define OVR_ALIGNAS(n) alignas(n)
    #elif defined(_MSC_VER) && (_MSC_VER >= 1900)
        #define OVR_ALIGNAS(n) alignas(n)
    #elif defined(__EDG_VERSION__) && (__EDG_VERSION__ >= 408)
        #define OVR_ALIGNAS(n) alignas(n)

    // Pre-C++11 alignas fallbacks
    #elif defined(__GNUC__) || defined(__clang__)
        #define OVR_ALIGNAS(n) __attribute__((aligned(n)))
    #elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
        #define OVR_ALIGNAS(n) __declspec(align(n))             // For Microsoft the alignment must be a literal integer.
    #elif defined(__CC_ARM)
        #define OVR_ALIGNAS(n) __align(n)
    #else
        #error Need to define OVR_ALIGNAS
    #endif
#endif



//-----------------------------------------------------------------------------------
// ***** Padding
//
/// Defines explicitly unused space for a struct.
/// When used correcly, usage of this macro should not change the size of the struct.
/// Compile-time and runtime behavior with and without this defined should be identical.
///
#if !defined(OVR_UNUSED_STRUCT_PAD)
    #define OVR_UNUSED_STRUCT_PAD(padName, size) char padName[size];
#endif


//-----------------------------------------------------------------------------------
// ***** Word Size
//
/// Specifies the size of a pointer on the given platform.
///
#if !defined(OVR_PTR_SIZE)
    #if defined(__WORDSIZE)
        #define OVR_PTR_SIZE ((__WORDSIZE) / 8)
    #elif defined(_WIN64) || defined(__LP64__) || defined(_LP64) || defined(_M_IA64) || defined(__ia64__) || defined(__arch64__) || defined(__64BIT__) || defined(__Ptr_Is_64)
        #define OVR_PTR_SIZE 8
    #elif defined(__CC_ARM) && (__sizeof_ptr == 8)
        #define OVR_PTR_SIZE 8
    #else
        #define OVR_PTR_SIZE 4
    #endif
#endif


//-----------------------------------------------------------------------------------
// ***** OVR_ON32 / OVR_ON64
//
#if OVR_PTR_SIZE == 8
    #define OVR_ON32(x)
    #define OVR_ON64(x) x
#else
    #define OVR_ON32(x) x
    #define OVR_ON64(x)
#endif


//-----------------------------------------------------------------------------------
// ***** ovrBool

typedef char ovrBool;   ///< Boolean type
#define ovrFalse 0      ///< ovrBool value of false.
#define ovrTrue  1      ///< ovrBool value of true.


//-----------------------------------------------------------------------------------
// ***** Simple Math Structures

/// A 2D vector with integer components.
typedef struct OVR_ALIGNAS(4) ovrVector2i_
{
    int x, y;
} ovrVector2i;

/// A 2D size with integer components.
typedef struct OVR_ALIGNAS(4) ovrSizei_
{
    int w, h;
} ovrSizei;

/// A 2D rectangle with a position and size.
/// All components are integers.
typedef struct OVR_ALIGNAS(4) ovrRecti_
{
    ovrVector2i Pos;
    ovrSizei    Size;
} ovrRecti;

/// A quaternion rotation.
typedef struct OVR_ALIGNAS(4) ovrQuatf_
{
    float x, y, z, w;
} ovrQuatf;

/// A 2D vector with float components.
typedef struct OVR_ALIGNAS(4) ovrVector2f_
{
    float x, y;
} ovrVector2f;

/// A 3D vector with float components.
typedef struct OVR_ALIGNAS(4) ovrVector3f_
{
    float x, y, z;
} ovrVector3f;

/// A 4x4 matrix with float elements.
typedef struct OVR_ALIGNAS(4) ovrMatrix4f_
{
    float M[4][4];
} ovrMatrix4f;


/// Position and orientation together.
typedef struct OVR_ALIGNAS(4) ovrPosef_
{
    ovrQuatf     Orientation;
    ovrVector3f  Position;
} ovrPosef;

/// A full pose (rigid body) configuration with first and second derivatives.
///
/// Body refers to any object for which ovrPoseStatef is providing data.
/// It can be the camera or something else; the context depends on the usage of the struct.
typedef struct OVR_ALIGNAS(8) ovrPoseStatef_
{
    ovrPosef     ThePose;               ///< The body's position and orientation.
    ovrVector3f  AngularVelocity;       ///< The body's angular velocity in radians per second.
    ovrVector3f  LinearVelocity;        ///< The body's velocity in meters per second.
    ovrVector3f  AngularAcceleration;   ///< The body's angular acceleration in radians per second per second.
    ovrVector3f  LinearAcceleration;    ///< The body's acceleration in meters per second per second.
    OVR_UNUSED_STRUCT_PAD(pad0, 4)      ///< \internal struct pad.
    double       TimeInSeconds;         ///< Absolute time of this state sample.
} ovrPoseStatef;

/// Describes the up, down, left, and right angles of the field of view.
///
/// Field Of View (FOV) tangent of the angle units.
/// \note For a standard 90 degree vertical FOV, we would
/// have: { UpTan = tan(90 degrees / 2), DownTan = tan(90 degrees / 2) }.
typedef struct OVR_ALIGNAS(4) ovrFovPort_
{
    float UpTan;    ///< The tangent of the angle between the viewing vector and the top edge of the field of view.
    float DownTan;  ///< The tangent of the angle between the viewing vector and the bottom edge of the field of view.
    float LeftTan;  ///< The tangent of the angle between the viewing vector and the left edge of the field of view.
    float RightTan; ///< The tangent of the angle between the viewing vector and the right edge of the field of view.
} ovrFovPort;


//-----------------------------------------------------------------------------------
// ***** HMD Types

/// Enumerates all HMD types that we support.
///
/// The currently released developer kits are ovrHmd_DK1 and ovrHmd_DK2. The other enumerations are for internal use only.
typedef enum ovrHmdType_
{
    ovrHmd_None      = 0,
    ovrHmd_DK1       = 3,
    ovrHmd_DKHD      = 4,
    ovrHmd_DK2       = 6,
    ovrHmd_BlackStar = 7,
    ovrHmd_CB        = 8,
    ovrHmd_Other     = 9,
    ovrHmd_EnumSize  = 0x7fffffff ///< \internal Force type int32_t.
} ovrHmdType;


/// HMD capability bits reported by device.
///
/// Set <B>(read/write)</B> flags through ovrHmd_SetEnabledCaps()
typedef enum ovrHmdCaps_
{
    // Read-only flags.
    ovrHmdCap_DebugDevice       = 0x0010,   ///< <B>(read only)</B> Specifies that the HMD is a virtual debug device.

    /// \brief <B>(read/write)</B> Toggles low persistence mode on or off.
    /// \details This setting reduces eye-tracking based motion blur. Eye-tracking based motion blur is caused by the viewer's focal point
    /// moving more pixels than have refreshed in the same period of time.\n
    /// The disadvantage of this setting is that this reduces the average brightness of the display and causes some users to perceive flicker.\n
    /// <I>There is no performance cost for this option. Oculus recommends exposing it to the user as an optional setting.</I>
    ovrHmdCap_LowPersistence    = 0x0080,
    ovrHmdCap_DynamicPrediction = 0x0200,   ///< <B>(read/write)</B> Adjusts prediction dynamically based on internally measured latency.
    ovrHmdCap_NoVSync           = 0x1000,   ///< <B>(read/write)</B> Supports rendering without VSync for debugging.

    /// Indicates to the developer what caps they can and cannot modify. These are processed by the client.
    ovrHmdCap_Writable_Mask     = ovrHmdCap_LowPersistence |
                                  ovrHmdCap_DynamicPrediction |
                                  ovrHmdCap_NoVSync,

    /// \internal Indicates to the developer what caps they can and cannot modify. These are processed by the service.
    ovrHmdCap_Service_Mask      = ovrHmdCap_LowPersistence |
                                  ovrHmdCap_DynamicPrediction
  , ovrHmdCap_EnumSize          = 0x7fffffff ///< \internal Force type int32_t.
} ovrHmdCaps;


/// Tracking capability bits reported by the device.
/// Used with ovrHmd_ConfigureTracking.
typedef enum ovrTrackingCaps_
{
    ovrTrackingCap_Orientation      = 0x0010,   ///< Supports orientation tracking (IMU).
    ovrTrackingCap_MagYawCorrection = 0x0020,   ///< Supports yaw drift correction via a magnetometer or other means.
    ovrTrackingCap_Position         = 0x0040,   ///< Supports positional tracking.
    /// Overriding the other flags, this causes the application
    /// to ignore tracking settings. This is the internal
    /// default before ovrHmd_ConfigureTracking is called.
    ovrTrackingCap_Idle             = 0x0100,
    ovrTrackingCap_EnumSize         = 0x7fffffff ///< \internal Force type int32_t.
} ovrTrackingCaps;


/// Specifies which eye is being used for rendering.
/// This type explicitly does not include a third "NoStereo" monoscopic option, as such is
/// not required for an HMD-centered API.
typedef enum ovrEyeType_
{
    ovrEye_Left     = 0,         ///< The left eye, from the viewer's perspective.
    ovrEye_Right    = 1,         ///< The right eye, from the viewer's perspective.
    ovrEye_Count    = 2,         ///< \internal Count of enumerated elements.
    ovrEye_EnumSize = 0x7fffffff ///< \internal Force type int32_t.
} ovrEyeType;


/// This is a complete descriptor of the HMD.
typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) ovrHmdDesc_
{
    struct ovrHmdStruct* Handle;               ///< Internal handle of this HMD.
    ovrHmdType   Type;                         ///< The type of HMD.
    OVR_ON64(OVR_UNUSED_STRUCT_PAD(pad0, 4))   ///< \internal struct paddding.
    const char*  ProductName;                  ///< UTF8-encoded product identification string (e.g. "Oculus Rift DK1").
    const char*  Manufacturer;                 ///< UTF8-encoded HMD manufacturer identification string.
    short        VendorId;                     ///< HID (USB) vendor identifier of the device.
    short        ProductId;                    ///< HID (USB) product identifier of the device.
    char         SerialNumber[24];             ///< Sensor (and display) serial number.
    short        FirmwareMajor;                ///< Sensor firmware major version.
    short        FirmwareMinor;                ///< Sensor firmware minor version.
    float        CameraFrustumHFovInRadians;   ///< External tracking camera frustum horizontal field-of-view (if present).
    float        CameraFrustumVFovInRadians;   ///< External tracking camera frustum vertical field-of-view (if present).
    float        CameraFrustumNearZInMeters;   ///< External tracking camera frustum near Z (if present).
    float        CameraFrustumFarZInMeters;    ///< External tracking camera frustum near Z (if present).
    unsigned int HmdCaps;                      ///< Capability bits described by ovrHmdCaps.
    unsigned int TrackingCaps;                 ///< Capability bits described by ovrTrackingCaps.
    ovrFovPort   DefaultEyeFov[ovrEye_Count];  ///< Defines the recommended FOVs for the HMD.
    ovrFovPort   MaxEyeFov[ovrEye_Count];      ///< Defines the maximum FOVs for the HMD.
    ovrEyeType   EyeRenderOrder[ovrEye_Count]; ///< Preferred eye rendering order for best performance. Can help reduce latency on sideways-scanned screens.
    ovrSizei     Resolution;                   ///< Resolution of the full HMD screen (both eyes) in pixels.

} ovrHmdDesc;

/// Type used by ovrHmd_* functions.
typedef const ovrHmdDesc* ovrHmd;


/// Bit flags describing the current status of sensor tracking.
//  The values must be the same as in enum StatusBits
///
/// \see ovrTrackingState
///
typedef enum ovrStatusBits_
{
    ovrStatus_OrientationTracked    = 0x0001,    ///< Orientation is currently tracked (connected and in use).
    ovrStatus_PositionTracked       = 0x0002,    ///< Position is currently tracked (false if out of range).
    ovrStatus_CameraPoseTracked     = 0x0004,    ///< Camera pose is currently tracked.
    ovrStatus_PositionConnected     = 0x0020,    ///< Position tracking hardware is connected.
    ovrStatus_HmdConnected          = 0x0080,    ///< HMD Display is available and connected.
    ovrStatus_EnumSize              = 0x7fffffff ///< \internal Force type int32_t.
} ovrStatusBits;


/// Specifies a reading we can query from the sensor.
///
/// \see ovrTrackingState
///
typedef struct OVR_ALIGNAS(4) ovrSensorData_
{
    ovrVector3f    Accelerometer;    ///< Acceleration reading in meters/second^2.
    ovrVector3f    Gyro;             ///< Rotation rate in radians/second.
    ovrVector3f    Magnetometer;     ///< Magnetic field in Gauss.
    float          Temperature;      ///< Temperature of the sensor in degrees Celsius.
    float          TimeInSeconds;    ///< Time when the reported IMU reading took place in seconds. \see ovr_GetTimeInSeconds
} ovrSensorData;



/// @cond DoxygenIgnore
/// @endcond



/// Tracking state at a given absolute time (describes predicted HMD pose, etc.).
/// Returned by ovrHmd_GetTrackingState.
///
/// \see ovrHmd_GetTrackingState
///
typedef struct OVR_ALIGNAS(8) ovrTrackingState_
{
    /// Predicted head pose (and derivatives) at the requested absolute time.
    /// The look-ahead interval is equal to (HeadPose.TimeInSeconds - RawSensorData.TimeInSeconds).
    ovrPoseStatef  HeadPose;

    /// Current pose of the external camera (if present).
    /// This pose includes camera tilt (roll and pitch). For a leveled coordinate
    /// system use LeveledCameraPose.
    ovrPosef       CameraPose;

    /// Camera frame aligned with gravity.
    /// This value includes position and yaw of the camera, but not roll and pitch.
    /// It can be used as a reference point to render real-world objects in the correct location.
    ovrPosef       LeveledCameraPose;

    /// The most recent sensor data received from the HMD.
    ovrSensorData  RawSensorData;

    /// Tracking status described by ovrStatusBits.
    unsigned int   StatusFlags;

    /// Tags the vision processing results to a certain frame counter number.
    uint32_t LastCameraFrameCounter;

    OVR_UNUSED_STRUCT_PAD(pad0, 4) ///< \internal struct padding
} ovrTrackingState;


/// Frame timing data reported by ovrHmd_GetFrameTiming.
///
/// \see ovrHmd_GetFrameTiming
///
typedef struct OVR_ALIGNAS(8) ovrFrameTiming_
{
    /// A point in time when the middle of the screen will be displayed. For global shutter,
    /// this will be the display time. For rolling shutter this is a point at which half the image has
    /// been displayed. This value can be passed as an absolute time to ovrHmd_GetTrackingState
    /// to get the best predicted pose for rendering the scene.
    double      DisplayMidpointSeconds;

    /// Display interval between the frames. This will generally be 1 / RefreshRate of the HMD;
    /// however, it may vary slightly during runtime based on video cart scan-out timing.
    double      FrameIntervalSeconds;

    /// Application frame index for which we requested timing.
    unsigned    AppFrameIndex;

    /// HW display frame index that we expect this application frame will hit; this is the frame that
    /// will be displayed at DisplayMidpointSeconds. This value is monotonically increasing with each v-sync.
    unsigned    DisplayFrameIndex;
} ovrFrameTiming;


/// Rendering information for each eye. Computed by ovrHmd_GetRenderDesc() based on the
/// specified FOV. Note that the rendering viewport is not included
/// here as it can be specified separately and modified per frame by
/// passing different Viewport values in the layer structure.
///
/// \see ovrHmd_GetRenderDesc
///
typedef struct OVR_ALIGNAS(4) ovrEyeRenderDesc_
{
    ovrEyeType  Eye;                        ///< The eye index to which this instance corresponds.
    ovrFovPort  Fov;                        ///< The field of view.
    ovrRecti    DistortedViewport;          ///< Distortion viewport.
    ovrVector2f PixelsPerTanAngleAtCenter;  ///< How many display pixels will fit in tan(angle) = 1.
    ovrVector3f HmdToEyeViewOffset;         ///< Translation of each eye.
} ovrEyeRenderDesc;


/// Projection information for ovrLayerEyeFovDepth.
///
/// Use the utility function ovrTimewarpProjectionDesc_FromProjection to
/// generate this structure from the application's projection matrix.
///
/// \see ovrLayerEyeFovDepth, ovrTimewarpProjectionDesc_FromProjection
///
typedef struct OVR_ALIGNAS(4) ovrTimewarpProjectionDesc_
{
    float Projection22;     ///< Projection matrix element [2][2].
    float Projection23;     ///< Projection matrix element [2][3].
    float Projection32;     ///< Projection matrix element [3][2].
} ovrTimewarpProjectionDesc;


/// Contains the data necessary to properly calculate position info for various layer types.
/// - HmdToEyeViewOffset is the same value pair provided in ovrEyeRenderDesc.
/// - HmdSpaceToWorldScaleInMeters is used to scale player motion into in-application units.
///   In other words, it is how big an in-application unit is in the player's physical meters.
///   For example, if the application uses inches as its units then HmdSpaceToWorldScaleInMeters would be 0.0254.
///   Note that if you are scaling the player in size, this must also scale. So if your application
///   units are inches, but you're shrinking the player to half their normal size, then
///   HmdSpaceToWorldScaleInMeters would be 0.0254*2.0.
///
/// \see ovrEyeRenderDesc, ovrHmd_SubmitFrame
///
typedef struct OVR_ALIGNAS(4) ovrViewScaleDesc_
{
    ovrVector3f HmdToEyeViewOffset[ovrEye_Count];   ///< Translation of each eye.
    float       HmdSpaceToWorldScaleInMeters;       ///< Ratio of viewer units to meter units.
} ovrViewScaleDesc;


//-----------------------------------------------------------------------------------
// ***** Platform-independent Rendering Configuration

/// These types are used to hide platform-specific details when passing
/// render device, OS, and texture data to the API.
///
/// The benefit of having these wrappers versus platform-specific API functions is
/// that they allow application glue code to be portable. A typical example is an
/// engine that has multiple back ends, such as GL and D3D. Portable code that calls
/// these back ends can also use LibOVR. To do this, back ends can be modified
/// to return portable types such as ovrTexture and ovrRenderAPIConfig.
typedef enum ovrRenderAPIType_
{
    ovrRenderAPI_None,                  ///< No API
    ovrRenderAPI_OpenGL,                ///< OpenGL
    ovrRenderAPI_Android_GLES,          ///< OpenGL ES
    ovrRenderAPI_D3D9_Obsolete,         ///< DirectX 9. Obsolete
    ovrRenderAPI_D3D10_Obsolete,        ///< DirectX 10. Obsolete
    ovrRenderAPI_D3D11,                 ///< DirectX 11.
    ovrRenderAPI_Count,                 ///< \internal Count of enumerated elements.
    ovrRenderAPI_EnumSize = 0x7fffffff  ///< \internal Force type int32_t.
} ovrRenderAPIType;


/// API-independent part of a texture descriptor.
///
/// ovrTextureHeader is a common struct present in all ovrTexture struct types.
///
typedef struct OVR_ALIGNAS(4) ovrTextureHeader_
{
    ovrRenderAPIType API;           ///< The API type to which this texture belongs.
    ovrSizei         TextureSize;   ///< Size of this texture in pixels.
} ovrTextureHeader;


/// Contains platform-specific information about a texture.
/// Aliases to one of ovrD3D11Texture or ovrGLTexture.
///
/// \see ovrD3D11Texture, ovrGLTexture.
///
typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) ovrTexture_
{
    ovrTextureHeader Header;                    ///< API-independent header.
    OVR_ON64(OVR_UNUSED_STRUCT_PAD(pad0, 4))    ///< \internal struct padding
    uintptr_t        PlatformData[8];           ///< Specialized in ovrGLTextureData, ovrD3D11TextureData etc.
} ovrTexture;


/// Describes a set of textures that act as a rendered flip chain.
///
/// An ovrSwapTextureSet per layer is passed to ovrHmd_SubmitFrame via one of the ovrLayer types.
/// The TextureCount refers to the flip chain count and not an eye count. 
/// See the layer structs and functions for information about how to use ovrSwapTextureSet.
/// 
/// ovrSwapTextureSets must be created by either the ovrHmd_CreateSwapTextureSetD3D11 or 
/// ovrHmd_CreateSwapTextureSetGL factory function, and must be destroyed by ovrHmd_DestroySwapTextureSet.
///
/// \see ovrHmd_CreateSwapTextureSetD3D11, ovrHmd_CreateSwapTextureSetGL, ovrHmd_DestroySwapTextureSet.
///
typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) ovrSwapTextureSet_
{
    ovrTexture* Textures;       ///< Points to an array of ovrTextures.
    int         TextureCount;   ///< The number of textures referenced by the Textures array. 

    /// CurrentIndex specifies which of the Textures will be used by the ovrHmd_SubmitFrame call.
    /// This is manually incremented by the application, typically in a round-robin manner.
    ///
    /// Before selecting a Texture as a rendertarget, the application should increment CurrentIndex by
    /// 1 and wrap it back to 0 if CurrentIndex == TextureCount, so that it gets a fresh rendertarget,
    /// one that is not currently being used for display. It can then render to Textures[CurrentIndex].
    ///
    /// After rendering, the application calls ovrHmd_SubmitFrame using that same CurrentIndex value
    /// to display the new rendertarget.
    ///
    /// The application can submit multiple frames with the same ovrSwapTextureSet and CurrentIndex 
    /// value if the rendertarget does not need to be updated, for example when displaying an
    /// information display whose text has not changed since the previous frame.
    ///
    /// Multiple layers can use the same ovrSwapTextureSet at the same time - there is no need to 
    /// create a unique ovrSwapTextureSet for each layer. However, all the layers using a particular
    /// ovrSwapTextureSet will share the same value of CurrentIndex, so they cannot use different
    /// textures within the ovrSwapTextureSet.
    ///
    /// Once a particular Textures[CurrentIndex] has been sent to ovrHmd_SubmitFrame, that texture
    /// should not be rendered to until a subsequent ovrHmd_SubmitFrame is made (either with a
    /// different CurrentIndex value, or with a different ovrSwapTextureSet, or disabling the layer).
    int         CurrentIndex;
} ovrSwapTextureSet;



//-----------------------------------------------------------------------------------
// ***** Initialize structures

/// Initialization flags.
///
/// \see ovrInitParams, ovr_Initialize
///
typedef enum ovrInitFlags_
{
    /// When a debug library is requested, a slower debugging version of the library will
    /// run which can be used to help solve problems in the library and debug application code.
    ovrInit_Debug          = 0x00000001,

    /// When ServerOptional is set, the ovr_Initialize() call not will block waiting for
    /// the server to respond. If the server is not reachable, it might still succeed.
    ovrInit_ServerOptional = 0x00000002,

    /// When a version is requested, the LibOVR runtime respects the RequestedMinorVersion
    /// field and verifies that the RequestedMinorVersion is supported.
    ovrInit_RequestVersion = 0x00000004,

    /// Forces debug features of LibOVR off explicitly, even if it is built in debug mode.
    ovrInit_ForceNoDebug   = 0x00000008,


    ovrInit_EnumSize       = 0x7fffffff ///< \internal Force type int32_t.
} ovrInitFlags;


/// Logging levels
///
/// \see ovrInitParams, ovrLogCallback
///
typedef enum ovrLogLevel_
{
    ovrLogLevel_Debug    = 0, ///< Debug-level log event.
    ovrLogLevel_Info     = 1, ///< Info-level log event.
    ovrLogLevel_Error    = 2, ///< Error-level log event.

    ovrLogLevel_EnumSize = 0x7fffffff ///< \internal Force type int32_t.
} ovrLogLevel;


/// Signature of the logging callback function pointer type.
///
/// \param[in] level is one of the ovrLogLevel constants.
/// \param[in] message is a UTF8-encoded null-terminated string.
/// \see ovrInitParams, ovrLogLevel, ovr_Initialize
///
typedef void (OVR_CDECL* ovrLogCallback)(int level, const char* message);


/// Parameters for ovr_Initialize.
///
/// \see ovr_Initialize
///
typedef struct OVR_ALIGNAS(8) ovrInitParams_
{
    /// Flags from ovrInitFlags to override default behavior.
    /// Use 0 for the defaults.
    uint32_t       Flags;

    /// Requests a specific minimum minor version of the LibOVR runtime.
    /// Flags must include ovrInit_RequestVersion or this will be ignored
    /// and OVR_MINOR_VERSION will be used.
    uint32_t       RequestedMinorVersion;

    /// User-supplied log callback function, which may be called at any time
    /// asynchronously from multiple threads until ovr_Shutdown completes.
    /// Use NULL to specify no log callback.
    ovrLogCallback LogCallback;

    /// Relative number of milliseconds to wait for a connection to the server 
    /// before failing. Use 0 for the default timeout.
    uint32_t       ConnectionTimeoutMS;

    OVR_ON64(OVR_UNUSED_STRUCT_PAD(pad0, 4)) ///< \internal

} ovrInitParams;


#ifdef __cplusplus
extern "C" {
#endif


// -----------------------------------------------------------------------------------
// ***** API Interfaces

// Overview of the API
//
// Setup:
//  - ovr_Initialize().
//  - ovrHmd_Create(0, &hmd).
//  - Call ovrHmd_ConfigureTracking() to configure and initialize tracking.
//  - Use hmd members and ovrHmd_GetFovTextureSize() to determine graphics configuration
//    and ovrHmd_GetRenderDesc() to get per-eye rendering parameters.
//  - Allocate render target texture sets with ovrHmd_CreateSwapTextureSetD3D11() or
//    ovrHmd_CreateSwapTextureSetGL().
//
// Application Loop:
//  - Call ovrHmd_GetFrameTiming() to get the current frame timing information.
//  - Call ovrHmd_GetTrackingState() and ovr_CalcEyePoses() to obtain the predicted
//    rendering pose for each eye based on timing.
//  - Render the scene content into CurrentIndex of ovrTextureSet for each eye and layer
//    you plan to update this frame. Increment texture set CurrentIndex.
//  - Call ovrHmd_SubmitFrame() to render the distorted layers to the back buffer
//    and present them on the HMD. If ovrHmd_SubmitFrame returns ovrSuccess_NotVisible,
//    there is no need to render the scene for the next loop iteration. Instead, 
//    just call ovrHmd_SubmitFrame again until it returns ovrSuccess.
//
// Shutdown:
//  - ovrHmd_Destroy().
//  - ovr_Shutdown().


/// Initializes LibOVR
///
/// Initialize LibOVR for application usage. This includes finding and loading the LibOVRRT  
/// shared library. No LibOVR API functions, other than ovr_GetLastErrorInfo, can be called
/// unless ovr_Initialize succeeds. A successful call to ovr_Initialize must be eventually
/// followed by a call to ovr_Shutdown. ovr_Initialize calls are idempotent. 
/// Calling ovr_Initialize twice does not require two matching calls to ovr_Shutdown. 
/// If already initialized, the return value is ovr_Success.
/// 
/// LibOVRRT shared library search order:
///      -# Current working directory (often the same as the application directory).
///      -# Module directory (usually the same as the application directory,
///         but not if the module is a separate shared library).
///      -# Application directory
///      -# Development directory (only if OVR_ENABLE_DEVELOPER_SEARCH is enabled,
///         which is off by default).
///      -# Standard OS shared library search location(s) (OS-specific).
///
/// \param params Specifies custom initialization options. May be NULL to indicate default options.
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information. Example failed results include:
///     - ovrError_Initialize: Generic initialization error.
///     - ovrError_LibLoad: Couldn't load LibOVRRT.
///     - ovrError_LibVersion: LibOVRRT version incompatibility.
///     - ovrError_ServiceConnection: Couldn't connect to the OVR Service.
///     - ovrError_ServiceVersion: OVR Service version incompatibility.
///     - ovrError_IncompatibleOS: The operating system version is incompatible.
///     - ovrError_DisplayInit: Unable to initialize the HMD display.
///     - ovrError_ServerStart:  Unable to start the server. Is it already running?
///     - ovrError_Reinitialization: Attempted to re-initialize with a different version.
///
/// \see ovr_Shutdown
///
OVR_PUBLIC_FUNCTION(ovrResult) ovr_Initialize(const ovrInitParams* params);


/// Shuts down LibOVR
///
/// A successful call to ovr_Initialize must be eventually matched by a call to ovr_Shutdown.
/// After calling ovr_Shutdown, no LibOVR functions can be called except ovr_GetLastErrorInfo
/// or another ovr_Initialize. ovr_Shutdown invalidates all pointers, references, and created objects 
/// previously returned by LibOVR functions. The LibOVRRT shared library can be unloaded by 
/// ovr_Shutdown. 
///
/// \see ovr_Initialize
///
OVR_PUBLIC_FUNCTION(void) ovr_Shutdown();


/// Provides information about the last error.
/// \see ovr_GetLastErrorInfo
typedef struct ovrErrorInfo_
{
    ovrResult Result;               ///< The result from the last API call that generated an error ovrResult.
    char      ErrorString[512];     ///< A UTF8-encoded null-terminated English string describing the problem. The format of this string is subject to change in future versions.
} ovrErrorInfo;


/// Returns information about the most recent failed return value by the
/// current thread for this library.
///
/// This function itself can never generate an error.
/// The last error is never cleared by LibOVR, but will be overwritten by new errors.
/// Do not use this call to determine if there was an error in the last API 
/// call as successful API calls don't clear the last ovrErrorInfo.
/// To avoid any inconsistency, ovr_GetLastErrorInfo should be called immediately
/// after an API function that returned a failed ovrResult, with no other API
/// functions called in the interim.
///
/// \param[out] errorInfo The last ovrErrorInfo for the current thread.
///
/// \see ovrErrorInfo
///
OVR_PUBLIC_FUNCTION(void) ovr_GetLastErrorInfo(ovrErrorInfo* errorInfo);


/// Returns the version string representing the LibOVRRT version.
///
/// The returned string pointer is valid until the next call to ovr_Shutdown.
///
/// Note that the returned version string doesn't necessarily match the current
/// OVR_MAJOR_VERSION, etc., as the returned string refers to the LibOVRRT shared
/// library version and not the locally compiled interface version.
///
/// The format of this string is subject to change in future versions and its contents
/// should not be interpreted.
///
/// \return Returns a UTF8-encoded null-terminated version string.
///
OVR_PUBLIC_FUNCTION(const char*) ovr_GetVersionString();


/// Writes a message string to the LibOVR tracing mechanism (if enabled).
///
/// This message will be passed back to the application via the ovrLogCallback if 
/// it was registered.
///
/// \param[in] level One of the ovrLogLevel constants.
/// \param[in] message A UTF8-encoded null-terminated string.
/// \return returns the strlen of the message or a negative value if the message is too large.
///
/// \see ovrLogLevel, ovrLogCallback
///
OVR_PUBLIC_FUNCTION(int) ovr_TraceMessage(int level, const char* message);


//-------------------------------------------------------------------------------------
/// @name HMD Management
///
/// Handles the enumeration, creation, destruction, and properties of an HMD (head-mounted display).
///@{

/// Detects or re-detects HMDs and reports the total number detected.
///
/// This function is useful to determine if an HMD can be created without committing to 
/// creating it. For example, an application can use this information to present an HMD selection GUI.
///
/// If one or more HMDs are present, an integer value is returned which indicates
/// the number present. The number present indicates the range of valid indexes that 
/// can be passed to ovrHmd_Create. If no HMDs are present, the return 
/// value is zero. If there is an error, a negative error ovrResult value is
/// returned.
///
/// \return Returns an integer that specifies the number of HMDs currently present. Upon failure, OVR_SUCCESS(result) is false.
///
/// \see ovrHmd_Create
///
OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_Detect();

/// Creates a handle to an HMD which doubles as a description structure.
///
/// Upon success the returned ovrHmd* must be freed with ovrHmd_Destroy.
/// A second call to ovrHmd_Create with the same index as a previously
/// successful call will result in an error return value.
///
/// \param[in] index A value in the range of [0 .. ovrHmd_Detect()-1].
/// \param[out] pHmd Provides a pointer to an ovrHmd which will be written to upon success.
/// \return Returns an ovrResult indicating success or failure.
///
/// \see ovrHmd_Destroy
///
OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_Create(int index, ovrHmd* pHmd);

/// Creates a fake HMD used for debugging only.
///
/// This is not tied to specific hardware, but may be used to debug some of the related rendering.
/// \param[in] type Indicates the HMD type to emulate.
/// \param[out] pHmd Provides a pointer to an ovrHmd which will be written to upon success.
/// \return Returns an ovrResult indicating success or failure.
///
/// \see ovrHmd_Create
///
OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_CreateDebug(ovrHmdType type, ovrHmd* pHmd);

/// Destroys the HMD.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \see ovrHmd_Create
///
OVR_PUBLIC_FUNCTION(void) ovrHmd_Destroy(ovrHmd hmd);

/// Returns ovrHmdCaps bits that are currently enabled.
///
/// Note that this value is different from ovrHmdDesc::HmdCaps, which describes what
/// capabilities are available for that HMD.
///
/// \return Returns a combination of zero or more ovrHmdCaps.
/// \see ovrHmdCaps
///
OVR_PUBLIC_FUNCTION(unsigned int) ovrHmd_GetEnabledCaps(ovrHmd hmd);

/// Modifies capability bits described by ovrHmdCaps that can be modified,
/// such as ovrHmdCap_LowPersistance.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] hmdCaps A combination of 0 or more ovrHmdCaps.
///
/// \see ovrHmdCaps
///
OVR_PUBLIC_FUNCTION(void) ovrHmd_SetEnabledCaps(ovrHmd hmd, unsigned int hmdCaps);

//@}



//-------------------------------------------------------------------------------------
/// @name Tracking
///
/// Tracking functions handle the position, orientation, and movement of the HMD in space.
///
/// All tracking interface functions are thread-safe, allowing tracking state to be sampled
/// from different threads.
///
///@{

/// Starts sensor sampling, enabling specified capabilities, described by ovrTrackingCaps.
///
/// Use 0 for both supportedTrackingCaps and requiredTrackingCaps to disable tracking.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
///
/// \param[in] supportedTrackingCaps Specifies support that is requested. The function will succeed
///            even if these caps are not available (i.e. sensor or camera is unplugged). Support
///            will automatically be enabled if the device is plugged in later. Software should
///            check ovrTrackingState.StatusFlags for real-time status.
///
/// \param[in] requiredTrackingCaps Specifies sensor capabilities required at the time of the call.
///            If they are not available, the function will fail. Pass 0 if only specifying
///            supportedTrackingCaps.
///
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information.
///
/// \see ovrTrackingCaps
///
OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_ConfigureTracking(ovrHmd hmd, unsigned int supportedTrackingCaps,
                                                      unsigned int requiredTrackingCaps);

/// Re-centers the sensor position and orientation.
///
/// This resets the (x,y,z) positional components and the yaw orientation component.
/// The Roll and pitch orientation components are always determined by gravity and cannot
/// be redefined. All future tracking will report values relative to this new reference position.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
///
OVR_PUBLIC_FUNCTION(void) ovrHmd_RecenterPose(ovrHmd hmd);


/// Returns tracking state reading based on the specified absolute system time.
///
/// Pass an absTime value of 0.0 to request the most recent sensor reading. In this case
/// both PredictedPose and SamplePose will have the same value.
///
/// This may also be used for more refined timing of front buffer rendering logic, and so on.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] absTime Specifies the absolute future time to predict the return 
///            ovrTrackingState value. Use 0 to request the most recent tracking state.
/// \return Returns the ovrTrackingState that is predicted for the given absTime.
///
/// \see ovrTrackingState, ovrHmd_GetEyePoses, ovr_GetTimeInSeconds
///
OVR_PUBLIC_FUNCTION(ovrTrackingState) ovrHmd_GetTrackingState(ovrHmd hmd, double absTime);



///@}



//-------------------------------------------------------------------------------------
// @name Layers
//
///@{

/// Describes layer types that can be passed to ovrHmd_SubmitFrame.
/// Each layer type has an associated struct, such as ovrLayerEyeFov.
///
/// \see ovrLayerHeader
///
typedef enum ovrLayerType_
{
    ovrLayerType_Disabled       = 0,         ///< Layer is disabled.
    ovrLayerType_EyeFov         = 1,         ///< Described by ovrLayerEyeFov.
    ovrLayerType_EyeFovDepth    = 2,         ///< Described by ovrLayerEyeFovDepth.
    ovrLayerType_QuadInWorld    = 3,         ///< Described by ovrLayerQuad.
    ovrLayerType_QuadHeadLocked = 4,         ///< Described by ovrLayerQuad. Displayed in front of your face, moving with the head.
    ovrLayerType_Direct         = 6,         ///< Described by ovrLayerDirect. Passthrough for debugging and custom rendering.
    ovrLayerType_EnumSize       = 0x7fffffff ///< Force type int32_t.
} ovrLayerType;


/// Identifies flags used by ovrLayerHeader and which are passed to ovrHmd_SubmitFrame.
///
/// \see ovrLayerHeader
///
typedef enum ovrLayerFlags_
{
    /// ovrLayerFlag_HighQuality mode costs performance, but looks better.
    ovrLayerFlag_HighQuality               = 0x01,

    /// ovrLayerFlag_TextureOriginAtBottomLeft: the opposite is TopLeft.
    /// Generally this is false for D3D, true for OpenGL.
    ovrLayerFlag_TextureOriginAtBottomLeft = 0x02

} ovrLayerFlags;


/// Defines properties shared by all ovrLayer structs, such as ovrLayerEyeFov.
///
/// ovrLayerHeader is used as a base member in these larger structs.
/// This struct cannot be used by itself except for the case that Type is ovrLayerType_Disabled.
/// 
/// \see ovrLayerType, ovrLayerFlags
///
typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) ovrLayerHeader_
{
    ovrLayerType    Type;   ///< Described by ovrLayerType.
    unsigned        Flags;  ///< Described by ovrLayerFlags.
} ovrLayerHeader;


/// Describes a layer that specifies a monoscopic or stereoscopic view.
/// This is the kind of layer that's typically used as layer 0 to ovrHmd_SubmitFrame,
/// as it is the kind of layer used to render a 3D stereoscopic view.
///
/// Three options exist with respect to mono/stereo texture usage:
///    - ColorTexture[0] and ColorTexture[1] contain the left and right stereo renderings, respectively. 
///      Viewport[0] and Viewport[1] refer to ColorTexture[0] and ColorTexture[1], respectively.
///    - ColorTexture[0] contains both the left and right renderings, ColorTexture[1] is NULL, 
///      and Viewport[0] and Viewport[1] refer to sub-rects with ColorTexture[0].
///    - ColorTexture[0] contains a single monoscopic rendering, and Viewport[0] and 
///      Viewport[1] both refer to that rendering.
///
/// \see ovrSwapTextureSet, ovrHmd_SubmitFrame
///
typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) ovrLayerEyeFov_
{
    /// Header.Type must be ovrLayerType_EyeFov.
    ovrLayerHeader      Header;

    /// ovrSwapTextureSets for the left and right eye respectively. 
    /// The second one of which can be NULL for cases described above.
    ovrSwapTextureSet*  ColorTexture[ovrEye_Count];
 
    /// Specifies the ColorTexture sub-rect UV coordinates.
    /// Both Viewport[0] and Viewport[1] must be valid.
    ovrRecti            Viewport[ovrEye_Count];

    /// The viewport field of view.
    ovrFovPort          Fov[ovrEye_Count];

    /// Specifies the position and orientation of each eye view, with the position specified in meters.
    /// RenderPose will typically be the value returned from ovr_CalcEyePoses,
    /// but can be different in special cases if a different head pose is used for rendering.
    ovrPosef            RenderPose[ovrEye_Count];

} ovrLayerEyeFov;


/// Describes a layer that specifies a monoscopic or stereoscopic view, 
/// with depth textures in addition to color textures. This is typically used to support
/// positional time warp. This struct is the same as ovrLayerEyeFov, but with the addition
/// of DepthTexture and ProjectionDesc.
///
/// ProjectionDesc can be created using ovrTimewarpProjectionDesc_FromProjection.
///
/// Three options exist with respect to mono/stereo texture usage:
///    - ColorTexture[0] and ColorTexture[1] contain the left and right stereo renderings, respectively. 
///      Viewport[0] and Viewport[1] refer to ColorTexture[0] and ColorTexture[1], respectively.
///    - ColorTexture[0] contains both the left and right renderings, ColorTexture[1] is NULL, 
///      and Viewport[0] and Viewport[1] refer to sub-rects with ColorTexture[0].
///    - ColorTexture[0] contains a single monoscopic rendering, and Viewport[0] and 
///      Viewport[1] both refer to that rendering.
///
/// \see ovrSwapTextureSet, ovrHmd_SubmitFrame
///
typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) ovrLayerEyeFovDepth_
{
    /// Header.Type must be ovrLayerType_EyeFovDepth.
    ovrLayerHeader      Header;

    /// ovrSwapTextureSets for the left and right eye respectively. 
    /// The second one can be NULL in cases described above.
    ovrSwapTextureSet*  ColorTexture[ovrEye_Count];

    /// Specifies the ColorTexture sub-rect UV coordinates.
    /// Both Viewport[0] and Viewport[1] must be valid.
    ovrRecti            Viewport[ovrEye_Count];

    /// The viewport field of view.
    ovrFovPort          Fov[ovrEye_Count];

    /// Specifies the position and orientation of each eye view, with the position specified in meters.
    /// RenderPose will typically be the value returned from ovr_CalcEyePoses,
    /// but can be different in special cases if a different head pose is used for rendering.
    ovrPosef            RenderPose[ovrEye_Count];

    /// Depth texture for positional timewarp.
    /// Must map 1:1 to the ColorTexture.
    ovrSwapTextureSet*  DepthTexture[ovrEye_Count];

    /// Specifies how to convert DepthTexture information into meters.
    /// \see ovrTimewarpProjectionDesc_FromProjection
    ovrTimewarpProjectionDesc ProjectionDesc;

} ovrLayerEyeFovDepth;


/// Describes a layer of Quad type, which is a single quad in world or viewer space.
/// It is used for both ovrLayerType_QuadInWorld and ovrLayerType_QuadHeadLocked.
/// This type of layer represents a single object placed in the world and not a stereo
/// view of the world itself. 
///
/// A typical use of ovrLayerType_QuadInWorld is to draw a television screen in a room
/// that for some reason is more convenient to draw as a layer than as part of the main
/// view in layer 0. For example, it could implement a 3D popup GUI that is drawn at a 
/// higher resolution than layer 0 to improve fidelity of the GUI.
///
/// A use of ovrLayerType_QuadHeadLocked might be to implement a debug HUD visible in 
/// the HMD.
///
/// Quad layers are visible from both sides; they are not back-face culled.
///
/// \see ovrSwapTextureSet, ovrHmd_SubmitFrame
///
typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) ovrLayerQuad_
{
    /// Header.Type must be ovrLayerType_QuadInWorld or ovrLayerType_QuadHeadLocked.
    ovrLayerHeader      Header;

    /// Contains a single image, never with any stereo view.
    ovrSwapTextureSet*  ColorTexture;

    /// Specifies the ColorTexture sub-rect UV coordinates.
    ovrRecti            Viewport;

    /// Position and orientation of the center of the quad. Position is specified in meters.
    ovrPosef            QuadPoseCenter;

    /// Width and height (respectively) of the quad in meters.
    ovrVector2f         QuadSize;

} ovrLayerQuad;


/// Describes a layer which is copied to the HMD as-is. Neither distortion, time warp, 
/// nor vignetting is applied to ColorTexture before it's copied to the HMD. The application
/// can, however implement these kinds of effects itself before submitting the layer.
/// This layer can be used for application-based distortion rendering and can also be 
/// used for implementing a debug HUD that's viewed on the mirror texture.
///
/// \see ovrSwapTextureSet, ovrHmd_SubmitFrame
///
typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) ovrLayerDirect_
{
    /// Header.Type must be ovrLayerType_EyeDirect.
    ovrLayerHeader      Header;

    /// ovrSwapTextureSets for the left and right eye respectively. 
    /// The second one of which can be NULL for cases described above.
    ovrSwapTextureSet*  ColorTexture[ovrEye_Count];

    /// Specifies the ColorTexture sub-rect UV coordinates.
    /// Both Viewport[0] and Viewport[1] must be valid.
    ovrRecti            Viewport[ovrEye_Count];

} ovrLayerDirect;



/// Union that combines ovrLayer types in a way that allows them
/// to be used in a polymorphic way.
typedef union ovrLayer_Union_
{
    ovrLayerHeader      Header;
    ovrLayerEyeFov      EyeFov;
    ovrLayerEyeFovDepth EyeFovDepth;
    ovrLayerQuad        Quad;
    ovrLayerDirect      Direct;

} ovrLayer_Union;

//@}



/// @name SDK Distortion Rendering
///
/// All of rendering functions including the configure and frame functions
/// are not thread safe. It is OK to use ConfigureRendering on one thread and handle
/// frames on another thread, but explicit synchronization must be done since
/// functions that depend on configured state are not reentrant.
///
/// These functions support rendering of distortion by the SDK.
///
//@{

// TextureSet creation is rendering API-specific, so the ovrHmd_CreateSwapTextureSetXX
// methods can be found in the rendering API-specific headers, such as OVR_CAPI_D3D.h and OVR_CAPI_GL.h


/// Destroys an ovrSwapTextureSet and frees all the resources associated with it.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] textureSet Specifies the ovrSwapTextureSet to destroy.
///
/// \see ovrHmd_CreateSwapTextureSetD3D11, ovrHmd_CreateSwapTextureSetGL
///
OVR_PUBLIC_FUNCTION(void) ovrHmd_DestroySwapTextureSet(ovrHmd hmd, ovrSwapTextureSet* textureSet);


/// Destroys a mirror texture previously created by one of the mirror texture creation functions.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] mirrorTexture Specifies the ovrTexture to destroy.
///
/// \see ovrHmd_CreateMirrorTextureD3D11, ovrHmd_CreateMirrorTextureGL
///
OVR_PUBLIC_FUNCTION(void) ovrHmd_DestroyMirrorTexture(ovrHmd hmd, ovrTexture* mirrorTexture);


/// Calculates the recommended viewport size for rendering a given eye within the HMD
/// with a given FOV cone. 
///
/// Higher FOV will generally require larger textures to maintain quality.
/// Apps packing multiple eye views together on the same texture should ensure there are
/// at least 8 pixels of padding between them to prevent texture filtering and chromatic
/// aberration causing images to leak between the two eye views.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] eye Specifies which eye (left or right) to calculate for.
/// \param[in] fov Specifies the ovrFovPort to use.
/// \param[in] pixelsPerDisplayPixel Specifies the ratio of the number of render target pixels
///            to display pixels at the center of distortion. 1.0 is the default value. Lower
///            values can improve performance, higher values give improved quality.
/// \return Returns the texture width and height size.
///
OVR_PUBLIC_FUNCTION(ovrSizei) ovrHmd_GetFovTextureSize(ovrHmd hmd, ovrEyeType eye, ovrFovPort fov,
                                                       float pixelsPerDisplayPixel);

/// Computes the distortion viewport, view adjust, and other rendering parameters for
/// the specified eye.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] eyeType Specifies which eye (left or right) for which to perform calculations.
/// \param[in] fov Specifies the ovrFovPort to use.
/// \return Returns the computed ovrEyeRenderDesc for the given eyeType and field of view.
///
/// \see ovrEyeRenderDesc
///
OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc) ovrHmd_GetRenderDesc(ovrHmd hmd,
                                                           ovrEyeType eyeType, ovrFovPort fov);

/// Submits layers for distortion and display.
///
/// ovrHmd_SubmitFrame triggers distortion and processing which might happen asynchronously. 
/// The function will return when there is room in the submission queue and surfaces
/// are available. Distortion might or might not have completed.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
///
/// \param[in] frameIndex Specifies the targeted frame index, or 0, to refer to one frame after the last
///        time ovrHmd_SubmitFrame was called.
///
/// \param[in] viewScaleDesc Provides additional information needed only if layerPtrList contains
///        a ovrLayerType_QuadInWorld or ovrLayerType_QuadHeadLocked. If NULL, a default
///        version is used based on the current configuration and a 1.0 world scale.
///
/// \param[in] layerPtrList Specifies a list of ovrLayer pointers, which can include NULL entries to
///        indicate that any previously shown layer at that index is to not be displayed.
///        Each layer header must be a part of a layer structure such as ovrLayerEyeFov or ovrLayerQuad,
///        with Header.Type identifying its type. A NULL layerPtrList entry in the array indicates the 
//         absence of the given layer.
///
/// \param[in] layerCount Indicates the number of valid elements in layerPtrList. The maximum
///        supported layerCount is not currently specified, but may be specified in a future version.
///
/// - Layers are drawn in the order they are specified in the array, regardless of the layer type.
///
/// - Layers are not remembered between successive calls to ovrHmd_SubmitFrame. A layer must be 
///   specified in every call to ovrHmd_SubmitFrame or it won't be displayed.
///
/// - If a layerPtrList entry that was specified in a previous call to ovrHmd_SubmitFrame is
///   passed as NULL or is of type ovrLayerType_Disabled, that layer is no longer displayed.
///
/// - A layerPtrList entry can be of any layer type and multiple entries of the same layer type
///   are allowed. No layerPtrList entry may be duplicated (i.e. the same pointer as an earlier entry).
///
/// <b>Example code</b>
///     \code{.cpp}
///         ovrLayerEyeFov  layer0;
///         ovrLayerQuad    layer1;
///           ...
///         ovrLayerHeader* layers[2] = { &layer0.Header, &layer1.Header };
///         ovrResult result = ovrHmd_SubmitFrame(hmd, frameIndex, nullptr, layers, 2);
///     \endcode
///
/// \return Returns an ovrResult for which OVR_SUCCESS(result) is false upon error and true
///         upon one of the possible success values:
///     - ovrSuccess: rendering completed successfully.
///     - ovrSuccess_NotVisible: rendering completed successfully but was not displayed on the HMD,
///       usually because another application currently has ownership of the HMD. Applications receiving
///       this result should stop rendering new content, but continue to call ovrHmd_SubmitFrame periodically
///       until it returns a value other than ovrSuccess_NotVisible.
///
/// \see ovrHmd_GetFrameTiming, ovrViewScaleDesc, ovrLayerHeader
///
OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_SubmitFrame(ovrHmd hmd, unsigned int frameIndex,
                                                  const ovrViewScaleDesc* viewScaleDesc,
                                                  ovrLayerHeader const * const * layerPtrList, unsigned int layerCount);
///@}



//-------------------------------------------------------------------------------------
/// @name Frame Timing
///
//@{

/// Gets the ovrFrameTiming for the given frame index.
///
/// The application should increment frameIndex for each successively targeted frame,
/// and pass that index to any relevent OVR functions that need to apply to the frame
/// identified by that index. 
///
/// This function is thread-safe and allows for multiple application threads to target 
/// their processing to the same displayed frame.
/// 
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] frameIndex Identifies the frame the caller wishes to target.
/// \return Returns the ovrFrameTiming for the given frameIndex.
/// \see ovrFrameTiming, ovrHmd_ResetFrameTiming
///
OVR_PUBLIC_FUNCTION(ovrFrameTiming) ovrHmd_GetFrameTiming(ovrHmd hmd, unsigned int frameIndex);

/// Initializes and resets frame time tracking. 
///
/// This is typically not necessary, but is helpful if the application changes vsync state or 
/// video mode. vsync is assumed to be on if this isn't called. Resets internal frame index to 
/// the specified number.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] frameIndex Identifies the frame the caller wishes to target.
/// \see ovrHmd_GetFrameTiming
///
OVR_PUBLIC_FUNCTION(void) ovrHmd_ResetFrameTiming(ovrHmd hmd, unsigned int frameIndex);

/// Returns global, absolute high-resolution time in seconds. 
///
/// The time frame of reference for this function is not specified and should not be
/// depended upon.
///
/// \return Returns seconds as a floating point value.
/// \see ovrPoseStatef, ovrSensorData, ovrFrameTiming
///
OVR_PUBLIC_FUNCTION(double) ovr_GetTimeInSeconds();

///@}



// -----------------------------------------------------------------------------------
/// @name Property Access
///
/// These functions read and write OVR properties. Supported properties
/// are defined in OVR_CAPI_Keys.h
///
//@{

/// Reads a boolean property.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] propertyName The name of the property, which needs to be valid for only the call.
/// \param[in] defaultVal specifes the value to return if the property couldn't be read.
/// \return Returns the property interpreted as a boolean value. Returns defaultVal if
///         the property doesn't exist.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_GetBool(ovrHmd hmd, const char* propertyName, ovrBool defaultVal);

/// Writes or creates a boolean property.
/// If the property wasn't previously a boolean property, it is changed to a boolean property.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] propertyName The name of the property, which needs to be valid only for the call.
/// \param[in] value The value to write.
/// \return Returns true if successful, otherwise false. A false result should only occur if the property  
///         name is empty or if the property is read-only.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetBool(ovrHmd hmd, const char* propertyName, ovrBool value);


/// Reads an integer property.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] propertyName The name of the property, which needs to be valid only for the call.
/// \param[in] defaultVal Specifes the value to return if the property couldn't be read.
/// \return Returns the property interpreted as an integer value. Returns defaultVal if
///         the property doesn't exist.
OVR_PUBLIC_FUNCTION(int) ovrHmd_GetInt(ovrHmd hmd, const char* propertyName, int defaultVal);

/// Writes or creates an integer property.
///
/// If the property wasn't previously a boolean property, it is changed to an integer property.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] propertyName The name of the property, which needs to be valid only for the call.
/// \param[in] value The value to write.
/// \return Returns true if successful, otherwise false. A false result should only occur if the property  
///         name is empty or if the property is read-only.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetInt(ovrHmd hmd, const char* propertyName, int value);


/// Reads a float property.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] propertyName The name of the property, which needs to be valid only for the call.
/// \param[in] defaultVal specifes the value to return if the property couldn't be read.
/// \return Returns the property interpreted as an float value. Returns defaultVal if
///         the property doesn't exist.
OVR_PUBLIC_FUNCTION(float) ovrHmd_GetFloat(ovrHmd hmd, const char* propertyName, float defaultVal);

/// Writes or creates a float property.
/// If the property wasn't previously a float property, it's changed to a float property.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] propertyName The name of the property, which needs to be valid only for the call.
/// \param[in] value The value to write.
/// \return Returns true if successful, otherwise false. A false result should only occur if the property  
///         name is empty or if the property is read-only.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetFloat(ovrHmd hmd, const char* propertyName, float value);


/// Reads a float array property.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] propertyName The name of the property, which needs to be valid only for the call.
/// \param[in] values An array of float to write to.
/// \param[in] valuesCapacity Specifies the maximum number of elements to write to the values array.
/// \return Returns the number of elements read, or 0 if property doesn't exist or is empty.
OVR_PUBLIC_FUNCTION(unsigned int) ovrHmd_GetFloatArray(ovrHmd hmd, const char* propertyName,
                                                       float values[], unsigned int valuesCapacity);

/// Writes or creates a float array property.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] propertyName The name of the property, which needs to be valid only for the call.
/// \param[in] values An array of float to write from.
/// \param[in] valuesSize Specifies the number of elements to write.
/// \return Returns true if successful, otherwise false. A false result should only occur if the property  
///         name is empty or if the property is read-only.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetFloatArray(ovrHmd hmd, const char* propertyName,
                                                  const float values[], unsigned int valuesSize);


/// Reads a string property.
/// Strings are UTF8-encoded and null-terminated.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] propertyName The name of the property, which needs to be valid only for the call.
/// \param[in] defaultVal Specifes the value to return if the property couldn't be read.
/// \return Returns the string property if it exists. Otherwise returns defaultVal, which can be specified as NULL.
///         The return memory is guaranteed to be valid until next call to ovrHmd_GetString or 
///         until the HMD is destroyed, whichever occurs first.
OVR_PUBLIC_FUNCTION(const char*) ovrHmd_GetString(ovrHmd hmd, const char* propertyName,
                                                  const char* defaultVal);

/// Writes or creates a string property.
/// Strings are UTF8-encoded and null-terminated.
///
/// \param[in] hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in] propertyName The name of the property, which needs to be valid only for the call.
/// \param[in] value The string property, which only needs to be valid for the duration of the call.
/// \return Returns true if successful, otherwise false. A false result should only occur if the property  
///         name is empty or if the property is read-only.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetString(ovrHmd hmd, const char* propertyName,
                                              const char* value);

///@}




#ifdef __cplusplus
} // extern "C"
#endif


#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

/// @cond DoxygenIgnore
//-----------------------------------------------------------------------------
// ***** Compiler packing validation
//
// These checks ensure that the compiler settings being used will be compatible
// with with pre-built dynamic library provided with the runtime.

OVR_CPP(static_assert(sizeof(ovrBool) == 1,         "ovrBool size mismatch"));
OVR_CPP(static_assert(sizeof(ovrVector2i) == 4 * 2, "ovrVector2i size mismatch"));
OVR_CPP(static_assert(sizeof(ovrSizei) == 4 * 2,    "ovrSizei size mismatch"));
OVR_CPP(static_assert(sizeof(ovrRecti) == sizeof(ovrVector2i) + sizeof(ovrSizei), "ovrRecti size mismatch"));
OVR_CPP(static_assert(sizeof(ovrQuatf) == 4 * 4,    "ovrQuatf size mismatch"));
OVR_CPP(static_assert(sizeof(ovrVector2f) == 4 * 2, "ovrVector2f size mismatch"));
OVR_CPP(static_assert(sizeof(ovrVector3f) == 4 * 3, "ovrVector3f size mismatch"));
OVR_CPP(static_assert(sizeof(ovrMatrix4f) == 4 * 16, "ovrMatrix4f size mismatch"));

OVR_CPP(static_assert(sizeof(ovrPosef) == (7 * 4),       "ovrPosef size mismatch"));
OVR_CPP(static_assert(sizeof(ovrPoseStatef) == (22 * 4), "ovrPoseStatef size mismatch"));
OVR_CPP(static_assert(sizeof(ovrFovPort) == (4 * 4),     "ovrFovPort size mismatch"));

OVR_CPP(static_assert(sizeof(ovrHmdCaps) == 4,      "ovrHmdCaps size mismatch"));
OVR_CPP(static_assert(sizeof(ovrTrackingCaps) == 4, "ovrTrackingCaps size mismatch"));
OVR_CPP(static_assert(sizeof(ovrEyeType) == 4,      "ovrEyeType size mismatch"));
OVR_CPP(static_assert(sizeof(ovrHmdType) == 4,      "ovrHmdType size mismatch"));

OVR_CPP(static_assert(sizeof(ovrSensorData) == (11 * 4), "ovrSensorData size mismatch"));
OVR_CPP(static_assert(sizeof(ovrTrackingState) == 
                      sizeof(ovrPoseStatef) + 4 + 2 * sizeof(ovrPosef) + sizeof(ovrSensorData) + 2 * 4,
                      "ovrTrackingState size mismatch"));
OVR_CPP(static_assert(sizeof(ovrFrameTiming) == 3 * 8, "ovrFrameTiming size mismatch"));

OVR_CPP(static_assert(sizeof(ovrRenderAPIType) == 4, "ovrRenderAPIType size mismatch"));

OVR_CPP(static_assert(sizeof(ovrTextureHeader) == sizeof(ovrRenderAPIType) + sizeof(ovrSizei),
                      "ovrTextureHeader size mismatch"));
OVR_CPP(static_assert(sizeof(ovrTexture) == sizeof(ovrTextureHeader) OVR_ON64(+4) + sizeof(uintptr_t) * 8, 
                      "ovrTexture size mismatch"));

OVR_CPP(static_assert(sizeof(ovrStatusBits) == 4, "ovrStatusBits size mismatch"));

OVR_CPP(static_assert(sizeof(ovrEyeRenderDesc) == sizeof(ovrEyeType) + sizeof(ovrFovPort) + sizeof(ovrRecti) +
                                                  sizeof(ovrVector2f) + sizeof(ovrVector3f),
                      "ovrEyeRenderDesc size mismatch"));
OVR_CPP(static_assert(sizeof(ovrTimewarpProjectionDesc) == 4 * 3, "ovrTimewarpProjectionDesc size mismatch"));

OVR_CPP(static_assert(sizeof(ovrInitFlags) == 4, "ovrInitFlags size mismatch"));
OVR_CPP(static_assert(sizeof(ovrLogLevel) == 4, "ovrLogLevel size mismatch"));

OVR_CPP(static_assert(sizeof(ovrInitParams) == sizeof(ovrLogCallback) + 4 * 3 OVR_ON64(+4),
                      "ovrInitParams size mismatch"));

OVR_CPP(static_assert(sizeof(ovrHmdDesc) == OVR_ON64(4 +)
    sizeof(struct ovrHmdStruct*) + sizeof(ovrHmdType) // Handle - Type
    + sizeof(void*) * 2 + 2 + 2 + 24 // ProductName - SerialNumber
    + 2 + 2 + 4 * 4 // FirmwareMajor - CameraFrustumFarZInMeters
    + 4 * 2 + sizeof(ovrFovPort) * 4 // HmdCaps - MaxEyeFov
    + sizeof(ovrEyeType)* 2 + sizeof(ovrSizei)  // EyeRenderOrder - Resolution
    , "ovrHmdDesc size mismatch"));


// -----------------------------------------------------------------------------------
// ***** Backward compatibility #includes
//
// This is at the bottom of this file because the following is dependent on the 
// declarations above. 

#if !defined(OVR_CAPI_NO_UTILS)
    #include "OVR_CAPI_Util.h"
#endif

/// @endcond

#endif // OVR_CAPI_h
