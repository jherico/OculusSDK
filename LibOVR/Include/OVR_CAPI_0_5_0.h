/************************************************************************************

Filename    :   OVR_CAPI_0_5_0.h
Content     :   C Interface to Oculus tracking and rendering.
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

/// @file OVR_CAPI_0_5_0.h
/// Exposes all general Rift functionality.

#ifndef OVR_CAPI_h  // We don't use version numbers within this, as all versioned variations of
#define OVR_CAPI_h  // this file are currently mutually exclusive.


#include "OVR_CAPI_Keys.h"
#include "OVR_Version.h"


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
// LibOVR calling convention for 32-bit Windows builds.
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
// Defined as extern "C" when built from C++ code.
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
// Provided for backward compatibility with older usage.

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
// ***** ovrBool

typedef char ovrBool;
#define ovrFalse 0
#define ovrTrue  1


//-----------------------------------------------------------------------------------
// ***** Simple Math Structures

/// A 2D vector with integer components.
typedef struct ovrVector2i_
{
    int x, y;
} ovrVector2i;

/// A 2D size with integer components.
typedef struct ovrSizei_
{
    int w, h;
} ovrSizei;
/// A 2D rectangle with a position and size.
/// All components are integers.
typedef struct ovrRecti_
{
    ovrVector2i Pos;
    ovrSizei    Size;
} ovrRecti;

/// A quaternion rotation.
typedef struct ovrQuatf_
{
    float x, y, z, w;
} ovrQuatf;

/// A 2D vector with float components.
typedef struct ovrVector2f_
{
    float x, y;
} ovrVector2f;

/// A 3D vector with float components.
typedef struct ovrVector3f_
{
    float x, y, z;
} ovrVector3f;

/// A 4x4 matrix with float elements.
typedef struct ovrMatrix4f_
{
    float M[4][4];
} ovrMatrix4f;

/// Position and orientation together.
typedef struct ovrPosef_
{
    ovrQuatf     Orientation;
    ovrVector3f  Position;
} ovrPosef;

/// A full pose (rigid body) configuration with first and second derivatives.
typedef struct OVR_ALIGNAS(8) ovrPoseStatef_
{
    ovrPosef     ThePose;               ///< The body's position and orientation.
    ovrVector3f  AngularVelocity;       ///< The body's angular velocity in radians per second.
    ovrVector3f  LinearVelocity;        ///< The body's velocity in meters per second.
    ovrVector3f  AngularAcceleration;   ///< The body's angular acceleration in radians per second per second.
    ovrVector3f  LinearAcceleration;    ///< The body's acceleration in meters per second per second.
    float        Pad;                   ///< Unused struct padding.
    double       TimeInSeconds;         ///< Absolute time of this state sample.
} ovrPoseStatef;

/// Field Of View (FOV) in tangent of the angle units.
/// As an example, for a standard 90 degree vertical FOV, we would
/// have: { UpTan = tan(90 degrees / 2), DownTan = tan(90 degrees / 2) }.
typedef struct ovrFovPort_
{
    float UpTan;    ///< The tangent of the angle between the viewing vector and the top edge of the field of view.
    float DownTan;  ///< The tangent of the angle between the viewing vector and the bottom edge of the field of view.
    float LeftTan;  ///< The tangent of the angle between the viewing vector and the left edge of the field of view.
    float RightTan; ///< The tangent of the angle between the viewing vector and the right edge of the field of view.
} ovrFovPort;

//-----------------------------------------------------------------------------------
// ***** HMD Types

/// Enumerates all HMD types that we support.
typedef enum ovrHmdType_
{
    ovrHmd_None             = 0,
    ovrHmd_DK1              = 3,
    ovrHmd_DKHD             = 4,
    ovrHmd_DK2              = 6,
    ovrHmd_BlackStar        = 7,
    ovrHmd_CB               = 8,
    ovrHmd_Other      = 9,
    ovrHmd_EnumSize   = 0x7fffffff ///< Force type int32_t.
} ovrHmdType;

/// HMD capability bits reported by device.
typedef enum ovrHmdCaps_
{
    // Read-only flags.
    ovrHmdCap_Present           = 0x0001,   ///< (read only) The HMD is plugged in and detected by the system.
    ovrHmdCap_Available         = 0x0002,   ///< (read only) The HMD and its sensor are available for ownership use.
                                            ///<             i.e. it is not already owned by another application.
    ovrHmdCap_Captured          = 0x0004,   ///< (read only) Set to 'true' if we captured ownership of this HMD.
    ovrHmdCap_ExtendDesktop     = 0x0008,   ///< (read only) Means the display driver works via acting as an addition display monitor.
    ovrHmdCap_DebugDevice       = 0x0010,   ///< (read only) Means HMD device is a virtual debug device.

    // Modifiable flags (through ovrHmd_SetEnabledCaps).
    ovrHmdCap_NoMirrorToWindow  = 0x2000,   ///< Disables mirroring of HMD output to the window. This may improve 
                                            ///< rendering performance slightly (only if 'ExtendDesktop' is off).
    ovrHmdCap_DisplayOff        = 0x0040,   ///< Turns off HMD screen and output (only if 'ExtendDesktop' is off).
    ovrHmdCap_LowPersistence    = 0x0080,   ///< HMD supports low persistence mode.
    ovrHmdCap_DynamicPrediction = 0x0200,   ///< Adjust prediction dynamically based on internally measured latency.
    ovrHmdCap_NoVSync           = 0x1000,   ///< Support rendering without VSync for debugging.

    // These bits can be modified by ovrHmd_SetEnabledCaps.
    ovrHmdCap_Writable_Mask     = ovrHmdCap_NoMirrorToWindow |
                                  ovrHmdCap_DisplayOff |
                                  ovrHmdCap_LowPersistence |
                                  ovrHmdCap_DynamicPrediction |
                                  ovrHmdCap_NoVSync,

    /// These flags are currently passed into the service. May change without notice.
    ovrHmdCap_Service_Mask      = ovrHmdCap_NoMirrorToWindow |
                                  ovrHmdCap_DisplayOff |
                                  ovrHmdCap_LowPersistence |
                                  ovrHmdCap_DynamicPrediction
  , ovrHmdCap_EnumSize          = 0x7fffffff ///< Force type int32_t.
} ovrHmdCaps;


/// Tracking capability bits reported by the device.
/// Used with ovrHmd_ConfigureTracking.
typedef enum ovrTrackingCaps_
{
    ovrTrackingCap_Orientation      = 0x0010,   ///< Supports orientation tracking (IMU).
    ovrTrackingCap_MagYawCorrection = 0x0020,   ///< Supports yaw drift correction via a magnetometer or other means.
    ovrTrackingCap_Position         = 0x0040,   ///< Supports positional tracking.
    /// Overrides the other flags. Indicates that the application
    /// doesn't care about tracking settings. This is the internal
    /// default before ovrHmd_ConfigureTracking is called.
    ovrTrackingCap_Idle             = 0x0100,
    ovrTrackingCap_EnumSize         = 0x7fffffff    ///< Force type int32_t.
} ovrTrackingCaps;

/// Distortion capability bits reported by device.
/// Used with ovrHmd_ConfigureRendering and ovrHmd_CreateDistortionMesh.
typedef enum ovrDistortionCaps_
{
    // 0x01 unused - Previously ovrDistortionCap_Chromatic now enabled permanently.
    ovrDistortionCap_TimeWarp           =    0x02,     ///< Supports timewarp.
    // 0x04 unused

    ovrDistortionCap_Vignette           =    0x08,     ///< Supports vignetting around the edges of the view.
    ovrDistortionCap_NoRestore          =    0x10,     ///< Do not save and restore the graphics and compute state when rendering distortion.
    ovrDistortionCap_FlipInput          =    0x20,     ///< Flip the vertical texture coordinate of input images.
    ovrDistortionCap_SRGB               =    0x40,     ///< Assume input images are in sRGB gamma-corrected color space.
    ovrDistortionCap_Overdrive          =    0x80,     ///< Overdrive brightness transitions to reduce artifacts on DK2+ displays
    ovrDistortionCap_HqDistortion       =   0x100,     ///< High-quality sampling of distortion buffer for anti-aliasing
    ovrDistortionCap_LinuxDevFullscreen =   0x200,     ///< Indicates window is fullscreen on a device when set. The SDK will automatically apply distortion mesh rotation if needed.
    ovrDistortionCap_ComputeShader      =   0x400,     ///< Using compute shader (DX11+ only)
    //ovrDistortionCap_NoTimewarpJit    =   0x800      RETIRED - do not reuse this bit without major versioning changes.
    ovrDistortionCap_TimewarpJitDelay   =  0x1000,     ///< Enables a spin-wait that tries to push time-warp to be as close to V-sync as possible. WARNING - this may backfire and cause framerate loss - use with caution.

    ovrDistortionCap_ProfileNoSpinWaits = 0x10000,    ///< Use when profiling with timewarp to remove false positives
    ovrDistortionCap_EnumSize           = 0x7fffffff  ///< Force type int32_t.
} ovrDistortionCaps;

/// Specifies which eye is being used for rendering.
/// This type explicitly does not include a third "NoStereo" option, as such is
/// not required for an HMD-centered API.
typedef enum ovrEyeType_
{
    ovrEye_Left  = 0,
    ovrEye_Right = 1,
    ovrEye_Count    = 2,
    ovrEye_EnumSize = 0x7fffffff ///< Force type int32_t.
} ovrEyeType;

/// This is a complete descriptor of the HMD.
typedef struct ovrHmdDesc_
{
    /// Internal handle of this HMD.
    struct ovrHmdStruct* Handle;

    /// This HMD's type.
    ovrHmdType  Type;
    
    /// Name string describing the product: "Oculus Rift DK1", etc.
    const char* ProductName;
    /// String describing the manufacturer. Usually "Oculus".
    const char* Manufacturer;
    
    /// HID Vendor ID of the device.
    short       VendorId;
    /// HID Product ID of the device.
    short       ProductId;
    /// Sensor (and display) serial number.
    char        SerialNumber[24];
    /// Sensor firmware major version number.
    short       FirmwareMajor;
    /// Sensor firmware minor version number.
    short       FirmwareMinor;
    // External tracking camera frustum dimensions (if present).
    float       CameraFrustumHFovInRadians; ///< Horizontal field-of-view
    float       CameraFrustumVFovInRadians; ///< Vertical field-of-view
    float       CameraFrustumNearZInMeters; ///< Near clip distance
    float       CameraFrustumFarZInMeters; ///< Far clip distance

    /// Capability bits described by ovrHmdCaps.
    unsigned int HmdCaps;
    /// Capability bits described by ovrTrackingCaps.
    unsigned int TrackingCaps;
    /// Capability bits described by ovrDistortionCaps.
    unsigned int DistortionCaps;

    /// The recommended optical FOV for the HMD.
    ovrFovPort  DefaultEyeFov[ovrEye_Count];
    /// The maximum optical FOV for the HMD.
    ovrFovPort  MaxEyeFov[ovrEye_Count];

    /// Preferred eye rendering order for best performance.
    /// Can help reduce latency on sideways-scanned screens.
    ovrEyeType  EyeRenderOrder[ovrEye_Count];

    /// Resolution of the full HMD screen (both eyes) in pixels.
    ovrSizei    Resolution;
    /// Location of the application window on the desktop (or 0,0).
    ovrVector2i WindowsPos;

    /// Display that the HMD should present on.
    /// TBD: It may be good to remove this information relying on WindowPos instead.
    /// Ultimately, we may need to come up with a more convenient alternative,
    /// such as API-specific functions that return adapter, or something that will
    /// work with our monitor driver.
    /// Windows: (e.g. "\\\\.\\DISPLAY3", can be used in EnumDisplaySettings/CreateDC).
    const char* DisplayDeviceName;
    /// MacOS:
    int         DisplayId;

} ovrHmdDesc;

/// Simple type ovrHmd is used in ovrHmd_* calls.
typedef const ovrHmdDesc * ovrHmd;

/// Bit flags describing the current status of sensor tracking.
// The values must be the same as in enum StatusBits
typedef enum ovrStatusBits_
{
    ovrStatus_OrientationTracked    = 0x0001,    ///< Orientation is currently tracked (connected and in use).
    ovrStatus_PositionTracked       = 0x0002,    ///< Position is currently tracked (false if out of range).
    ovrStatus_CameraPoseTracked     = 0x0004,    ///< Camera pose is currently tracked.
    ovrStatus_PositionConnected     = 0x0020,    ///< Position tracking hardware is connected.
    ovrStatus_HmdConnected          = 0x0080,    ///< HMD Display is available and connected.
    ovrStatus_EnumSize              = 0x7fffffff ///< Force type int32_t.
} ovrStatusBits;

/// Specifies a reading we can query from the sensor.
typedef struct ovrSensorData_
{
    ovrVector3f    Accelerometer;    /// Acceleration reading in m/s^2.
    ovrVector3f    Gyro;             /// Rotation rate in rad/s.
    ovrVector3f    Magnetometer;     /// Magnetic field in Gauss.
    float          Temperature;      /// Temperature of the sensor in degrees Celsius.
    float          TimeInSeconds;    /// Time when the reported IMU reading took place, in seconds.
} ovrSensorData;


/// Tracking state at a given absolute time (describes predicted HMD pose etc).
/// Returned by ovrHmd_GetTrackingState.
typedef struct ovrTrackingState_
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

    /// Tag the vision processing results to a certain frame counter number.
    uint32_t LastCameraFrameCounter;

    /// Unused struct padding.
    uint32_t Pad;
} ovrTrackingState;



/// Frame timing data reported by ovrHmd_BeginFrameTiming() or ovrHmd_BeginFrame().
typedef struct OVR_ALIGNAS(8) ovrFrameTiming_
{
    /// The amount of time that has passed since the previous frame's
    /// ThisFrameSeconds value (usable for movement scaling).
    /// This will be clamped to no more than 0.1 seconds to prevent
    /// excessive movement after pauses due to loading or initialization.
    float DeltaSeconds;

    /// Unused struct padding.
    float Pad; 

    /// It is generally expected that the following holds:
    /// ThisFrameSeconds < TimewarpPointSeconds < NextFrameSeconds < 
    /// EyeScanoutSeconds[EyeOrder[0]] <= ScanoutMidpointSeconds <= EyeScanoutSeconds[EyeOrder[1]].

    /// Absolute time value when rendering of this frame began or is expected to
    /// begin. Generally equal to NextFrameSeconds of the previous frame. Can be used
    /// for animation timing.
    double ThisFrameSeconds;

    /// Absolute point when IMU expects to be sampled for this frame.
    double TimewarpPointSeconds;

    /// Absolute time when frame Present followed by GPU Flush will finish and the next frame begins.
    double NextFrameSeconds;

    /// Time when half of the screen will be scanned out. Can be passed as an absolute time
    /// to ovrHmd_GetTrackingState() to get the predicted general orientation.
    double ScanoutMidpointSeconds;

    /// Timing points when each eye will be scanned out to display. Used when rendering each eye.
    double EyeScanoutSeconds[2];
} ovrFrameTiming;

/// Rendering information for each eye. Computed by either ovrHmd_ConfigureRendering()
/// or ovrHmd_GetRenderDesc() based on the specified FOV. Note that the rendering viewport
/// is not included here as it can be specified separately and modified per frame through:
///    (a) ovrHmd_GetRenderScaleAndOffset in the case of client rendered distortion,
/// or (b) passing different values via ovrTexture in the case of SDK rendered distortion.
typedef struct ovrEyeRenderDesc_
{
    ovrEyeType  Eye;                        ///< The eye index this instance corresponds to.
    ovrFovPort  Fov;                        ///< The field of view.
    ovrRecti    DistortedViewport;          ///< Distortion viewport.
    ovrVector2f PixelsPerTanAngleAtCenter;  ///< How many display pixels will fit in tan(angle) = 1.
    ovrVector3f HmdToEyeViewOffset;         ///< Translation to be applied to view matrix for each eye offset.
} ovrEyeRenderDesc;

/// Rendering information for positional TimeWarp.
/// Contains the data necessary to properly calculate position info for timewarp matrices
/// and also interpret depth info provided via the depth buffer to the timewarp shader
typedef struct ovrPositionTimewarpDesc_
{
    /// The same offset value pair provided in ovrEyeRenderDesc.
    ovrVector3f      HmdToEyeViewOffset[2];
    /// The near clip distance used in the projection matrix.
    float            NearClip;
    /// The far clip distance used in the projection matrix
    /// utilized when rendering the eye depth textures provided in ovrHmd_EndFrame
    float            FarClip;
} ovrPositionTimewarpDesc;

//-----------------------------------------------------------------------------------
// ***** Platform-independent Rendering Configuration

/// These types are used to hide platform-specific details when passing
/// render device, OS, and texture data to the API.
///
/// The benefit of having these wrappers versus platform-specific API functions is
/// that they allow game glue code to be portable. A typical example is an
/// engine that has multiple back ends, say GL and D3D. Portable code that calls
/// these back ends may also use LibOVR. To do this, back ends can be modified
/// to return portable types such as ovrTexture and ovrRenderAPIConfig.
typedef enum ovrRenderAPIType_
{
    ovrRenderAPI_None,
    ovrRenderAPI_OpenGL,
    ovrRenderAPI_Android_GLES,  // May include extra native window pointers, etc.
    ovrRenderAPI_D3D9,          // Deprecated: Not supported for SDK rendering
    ovrRenderAPI_D3D10, // Deprecated: Not supported for SDK rendering
    ovrRenderAPI_D3D11,
    ovrRenderAPI_Count,
    ovrRenderAPI_EnumSize = 0x7fffffff ///< Force type int32_t.
} ovrRenderAPIType;

/// Platform-independent part of rendering API-configuration data.
/// It is a part of ovrRenderAPIConfig, passed to ovrHmd_Configure.
typedef struct ovrRenderAPIConfigHeader_
{
    ovrRenderAPIType API;               ///< The graphics API in use.
    ovrSizei         BackBufferSize;    ///< Previously named RTSize.
    int              Multisample;       ///< The number of samples per pixel.
} ovrRenderAPIConfigHeader;

/// Contains platform-specific information for rendering.
typedef struct ovrRenderAPIConfig_
{
    ovrRenderAPIConfigHeader Header;          ///< Platform-independent rendering information.
    uintptr_t                PlatformData[8]; ///< Platform-specific rendering information.
} ovrRenderAPIConfig;

/// Platform-independent part of the eye texture descriptor.
/// It is a part of ovrTexture, passed to ovrHmd_EndFrame.
/// If RenderViewport is all zeros then the full texture will be used.
typedef struct ovrTextureHeader_
{
    ovrRenderAPIType API;             ///< The graphics API in use.
    ovrSizei         TextureSize;     ///< The size of the texture.
    ovrRecti         RenderViewport;  ///< Pixel viewport in texture that holds eye image.
} ovrTextureHeader;

/// Contains platform-specific information about a texture.
/// Specialized for different rendering APIs in:
///     ovrGLTexture, ovrD3D11Texture
typedef struct ovrTexture_
{
    ovrTextureHeader Header;          ///< Platform-independent data about the texture.
    uintptr_t        PlatformData[8]; ///< Specialized in ovrGLTextureData, ovrD3D11TextureData etc.
} ovrTexture;


// -----------------------------------------------------------------------------------
// ***** API Interfaces

// Basic steps to use the API:
//
// Setup:
//  * ovrInitialize()
//  * ovrHMD hmd = ovrHmd_Create(0)
//  * Use hmd members and ovrHmd_GetFovTextureSize() to determine graphics configuration.
//  * Call ovrHmd_ConfigureTracking() to configure and initialize tracking.
//  * Call ovrHmd_ConfigureRendering() to setup graphics for SDK rendering,
//    which is the preferred approach.
//    Please refer to "Client Distortion Rendering" below if you prefer to do that instead.
//  * If the ovrHmdCap_ExtendDesktop flag is not set, then use ovrHmd_AttachToWindow to
//    associate the relevant application window with the hmd.
//  * Allocate render target textures as needed.
//
// Game Loop:
//  * Call ovrHmd_BeginFrame() to get the current frame timing information.
//  * Render each eye using ovrHmd_GetEyePoses() to get each eye pose.
//  * Call ovrHmd_EndFrame() to render the distorted textures to the back buffer
//    and present them on the hmd.
//
// Shutdown:
//  * ovrHmd_Destroy(hmd)
//  * ovr_Shutdown()
//

#ifdef __cplusplus
extern "C" {
#endif


/// ovr_InitializeRenderingShim initializes the rendering shim apart from everything
/// else in LibOVR. This may be helpful if the application prefers to avoid
/// creating any OVR resources (allocations, service connections, etc) at this point.
/// ovr_InitializeRenderingShim does not bring up anything within LibOVR except the
/// necessary hooks to enable the Direct-to-Rift functionality.
///
/// Either ovr_InitializeRenderingShim() or ovr_Initialize() must be called before any
/// Direct3D or OpenGL initialization is done by application (creation of devices, etc).
/// ovr_Initialize() must still be called after to use the rest of LibOVR APIs.
///
/// Same as ovr_InitializeRenderingShim except it requests to support at least the
/// given minor LibOVR library version.
OVR_PUBLIC_FUNCTION(ovrBool) ovr_InitializeRenderingShimVersion(int requestedMinorVersion);

OVR_PUBLIC_FUNCTION(ovrBool) ovr_InitializeRenderingShim();


/// Library init/shutdown, must be called around all other OVR code.
/// No other functions calls besides ovr_InitializeRenderingShim are allowed before
/// ovr_Initialize succeeds or after ovr_Shutdown.
/// Initializes all Oculus functionality.
/// A second call to ovr_Initialize after successful second call returns ovrTrue.

/// Flags for Initialize()
typedef enum ovrInitFlags_
{
    // When a debug library is requested, a slower debugging version of the library will
    // be run which can be used to help solve problems in the library and debug game code.
    ovrInit_Debug          = 0x00000001,

    // When ServerOptional is set, the ovr_Initialize() call not will block waiting for
    // the server to respond.  If the server is not reachable it may still succeed.
    ovrInit_ServerOptional = 0x00000002,

    // When a version is requested, LibOVR runtime will respect the RequestedMinorVersion
    // field and will verify that the RequestedMinorVersion is supported.
    ovrInit_RequestVersion = 0x00000004,

    // Forces debug features of LibOVR off explicitly, even if it is built in debug mode.
    ovrInit_ForceNoDebug   = 0x00000008,

} ovrInitFlags;

/// Logging levels
typedef enum ovrLogLevel_
{
    ovrLogLevel_Debug = 0,
    ovrLogLevel_Info  = 1,
    ovrLogLevel_Error = 2
} ovrLogLevel;

/// Signature for the logging callback.
/// Level is one of the ovrLogLevel constants.
typedef void (OVR_CDECL *ovrLogCallback)(int level, const char* message);

/// Parameters for the ovr_Initialize() call.
typedef struct
{
    /// Flags from ovrInitFlags to override default behavior.
    /// Pass 0 for the defaults.
    uint32_t       Flags;               ///< Combination of ovrInitFlags or 0

    /// Request a specific minimum minor version of the LibOVR runtime.
    /// Flags must include ovrInit_RequestVersion or this will be ignored.
    uint32_t       RequestedMinorVersion;

    /// Log callback function, which may be called at any time asynchronously from
    /// multiple threads until ovr_Shutdown() completes.
    /// Pass 0 for no log callback.
    ovrLogCallback LogCallback;         ///< Function pointer or 0

    /// Number of milliseconds to wait for a connection to the server.
    /// Pass 0 for the default timeout.
    uint32_t       ConnectionTimeoutMS; ///< Timeout in Milliseconds or 0
} ovrInitParams;

/// Initialize with extra parameters.
/// Pass 0 to initialize with default parameters, suitable for released games.
/// LibOVRRT shared library search order:
///      1) Current working directory (often the same as the application directory).
///      2) Module directory (usually the same as the application directory, but not if the module is a separate shared library).
///      3) Application directory
///      4) Development directory (only if OVR_ENABLE_DEVELOPER_SEARCH is enabled, which is off by default).
///      5) Standard OS shared library search location(s) (OS-specific).
OVR_PUBLIC_FUNCTION(ovrBool) ovr_Initialize(ovrInitParams const* params OVR_CPP(= 0));

/// Shuts down all Oculus functionality.
OVR_PUBLIC_FUNCTION(void) ovr_Shutdown();

/// Returns version string representing libOVR version. Static, so
/// string remains valid for app lifespan
OVR_PUBLIC_FUNCTION(const char*) ovr_GetVersionString();

/// Detects or re-detects HMDs and reports the total number detected.
/// Users can get information about each HMD by calling ovrHmd_Create with an index.
/// Returns -1 when the service is unreachable.
OVR_PUBLIC_FUNCTION(int) ovrHmd_Detect();

/// Creates a handle to an HMD which doubles as a description structure.
/// Index can [0 .. ovrHmd_Detect()-1]. Index mappings can cange after each ovrHmd_Detect call.
/// If not null, then the returned handle must be freed with ovrHmd_Destroy.
OVR_PUBLIC_FUNCTION(ovrHmd) ovrHmd_Create(int index);
OVR_PUBLIC_FUNCTION(void) ovrHmd_Destroy(ovrHmd hmd);

/// Creates a 'fake' HMD used for debugging only. This is not tied to specific hardware,
/// but may be used to debug some of the related rendering.
OVR_PUBLIC_FUNCTION(ovrHmd) ovrHmd_CreateDebug(ovrHmdType type);

/// Returns last error for HMD state. Returns null for no error.
/// String is valid until next call or GetLastError or HMD is destroyed.
/// Pass null hmd to get global errors (during create etc).
OVR_PUBLIC_FUNCTION(const char*) ovrHmd_GetLastError(ovrHmd hmd);

/// Platform specific function to specify the application window whose output will be 
/// displayed on the HMD. Only used if the ovrHmdCap_ExtendDesktop flag is false.
///   Windows: SwapChain associated with this window will be displayed on the HMD.
///            Specify 'destMirrorRect' in window coordinates to indicate an area
///            of the render target output that will be mirrored from 'sourceRenderTargetRect'.
///            Null pointers mean "full size".
/// @note Source and dest mirror rects are not yet implemented.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_AttachToWindow(ovrHmd hmd, void* window,
                                         const ovrRecti* destMirrorRect,
                                         const ovrRecti* sourceRenderTargetRect);

/// Returns capability bits that are enabled at this time as described by ovrHmdCaps.
/// Note that this value is different font ovrHmdDesc::HmdCaps, which describes what
/// capabilities are available for that HMD.
OVR_PUBLIC_FUNCTION(unsigned int) ovrHmd_GetEnabledCaps(ovrHmd hmd);

/// Modifies capability bits described by ovrHmdCaps that can be modified,
/// such as ovrHmdCap_LowPersistance.
OVR_PUBLIC_FUNCTION(void) ovrHmd_SetEnabledCaps(ovrHmd hmd, unsigned int hmdCaps);

//-------------------------------------------------------------------------------------
// ***** Tracking Interface

/// All tracking interface functions are thread-safe, allowing tracking state to be sampled
/// from different threads.
/// ConfigureTracking starts sensor sampling, enabling specified capabilities,
///    described by ovrTrackingCaps.
///  - supportedTrackingCaps specifies support that is requested. The function will succeed
///   even if these caps are not available (i.e. sensor or camera is unplugged). Support
///    will automatically be enabled if such device is plugged in later. Software should
///    check ovrTrackingState.StatusFlags for real-time status.
///  - requiredTrackingCaps specify sensor capabilities required at the time of the call.
///    If they are not available, the function will fail. Pass 0 if only specifying
///    supportedTrackingCaps.
///  - Pass 0 for both supportedTrackingCaps and requiredTrackingCaps to disable tracking.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_ConfigureTracking(ovrHmd hmd, unsigned int supportedTrackingCaps,
                                                         unsigned int requiredTrackingCaps);

/// Re-centers the sensor orientation.
/// Normally this will recenter the (x,y,z) translational components and the yaw
/// component of orientation.
OVR_PUBLIC_FUNCTION(void) ovrHmd_RecenterPose(ovrHmd hmd);

/// Returns tracking state reading based on the specified absolute system time.
/// Pass an absTime value of 0.0 to request the most recent sensor reading. In this case
/// both PredictedPose and SamplePose will have the same value.
/// ovrHmd_GetEyePoses relies on a valid ovrTrackingState.
/// This may also be used for more refined timing of FrontBuffer rendering logic, etc.
OVR_PUBLIC_FUNCTION(ovrTrackingState) ovrHmd_GetTrackingState(ovrHmd hmd, double absTime);





//-------------------------------------------------------------------------------------
// ***** Graphics Setup

/// Calculates the recommended viewport size for rendering a given eye within the HMD
/// with a given FOV cone. Higher FOV will generally require larger textures to
/// maintain quality.
///  - pixelsPerDisplayPixel specifies the ratio of the number of render target pixels
///    to display pixels at the center of distortion. 1.0 is the default value. Lower
///    values can improve performance, higher values give improved quality.
/// Apps packing multiple eye views together on the same textue should ensure there is
/// roughly 8 pixels of padding between them to prevent texture filtering and chromatic
/// aberration causing images to "leak" between the two eye views.
OVR_PUBLIC_FUNCTION(ovrSizei) ovrHmd_GetFovTextureSize(ovrHmd hmd, ovrEyeType eye, ovrFovPort fov,
                                             float pixelsPerDisplayPixel);

//-------------------------------------------------------------------------------------
// *****  Rendering API Thread Safety

//  All of rendering functions including the configure and frame functions
// are *NOT thread safe*. It is ok to use ConfigureRendering on one thread and handle
//  frames on another thread, but explicit synchronization must be done since
//  functions that depend on configured state are not reentrant.
//
//  As an extra requirement, any of the following calls must be done on
//  the render thread, which is the same thread that calls ovrHmd_BeginFrame
//  or ovrHmd_BeginFrameTiming.
//    - ovrHmd_EndFrame
//    - ovrHmd_GetEyeTimewarpMatrices

//-------------------------------------------------------------------------------------
// *****  SDK Distortion Rendering Functions

// These functions support rendering of distortion by the SDK through direct
// access to the underlying rendering API, such as D3D or GL.
// This is the recommended approach since it allows better support for future
// Oculus hardware, and enables a range of low-level optimizations.

/// Configures rendering and fills in computed render parameters.
/// This function can be called multiple times to change rendering settings.
/// eyeRenderDescOut is a pointer to an array of two ovrEyeRenderDesc structs
/// that are used to return complete rendering information for each eye.
///  - apiConfig provides D3D/OpenGL specific parameters. Pass null
///    to shutdown rendering and release all resources.
///  - distortionCaps describe desired distortion settings.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_ConfigureRendering(ovrHmd hmd,
                                              const ovrRenderAPIConfig* apiConfig,
                                              unsigned int distortionCaps,
                                              const ovrFovPort eyeFovIn[2],
                                              ovrEyeRenderDesc eyeRenderDescOut[2] );


/// Begins a frame, returning timing information.
/// This should be called at the beginning of the game rendering loop (on the render thread).
/// Pass 0 for the frame index if not using ovrHmd_GetFrameTiming.
OVR_PUBLIC_FUNCTION(ovrFrameTiming) ovrHmd_BeginFrame(ovrHmd hmd, unsigned int frameIndex);

/// Ends a frame, submitting the rendered textures to the frame buffer.
/// - RenderViewport within each eyeTexture can change per frame if necessary.
/// - 'renderPose' will typically be the value returned from ovrHmd_GetEyePoses
///   but can be different if a different head pose was used for rendering.
/// - This may perform distortion and scaling internally, assuming is it not
///   delegated to another thread.
/// - Must be called on the same thread as BeginFrame.
/// - If ovrDistortionCap_DepthProjectedTimeWarp is enabled, then app must provide eyeDepthTexture
///   and posTimewarpDesc. Otherwise both can be NULL.
/// - *** This Function will call Present/SwapBuffers and potentially wait for GPU Sync ***.
OVR_PUBLIC_FUNCTION(void) ovrHmd_EndFrame(ovrHmd hmd,
                                    const ovrPosef renderPose[2],
                                    const ovrTexture eyeTexture[2]);

/// Returns predicted head pose in outHmdTrackingState and offset eye poses in outEyePoses
/// as an atomic operation. Caller need not worry about applying HmdToEyeViewOffset to the
/// returned outEyePoses variables.
/// - Thread-safe function where caller should increment frameIndex with every frame
///   and pass the index where applicable to functions called on the  rendering thread.
/// - hmdToEyeViewOffset[2] can be ovrEyeRenderDesc.HmdToEyeViewOffset returned from 
///   ovrHmd_ConfigureRendering or ovrHmd_GetRenderDesc. For monoscopic rendering,
///   use a vector that is the average of the two vectors for both eyes.
/// - If frameIndex is not being utilized, pass in 0.
/// - Assuming outEyePoses are used for rendering, it should be passed into ovrHmd_EndFrame.
/// - If caller doesn't need outHmdTrackingState, it can be passed in as NULL
OVR_PUBLIC_FUNCTION(void) ovrHmd_GetEyePoses(ovrHmd hmd, unsigned int frameIndex,
                                             const ovrVector3f hmdToEyeViewOffset[2],
                                             ovrPosef outEyePoses[2],
                                             ovrTrackingState* outHmdTrackingState);

/// Function was previously called ovrHmd_GetEyePose
/// Returns the predicted head pose to use when rendering the specified eye.
/// - Important: Caller must apply HmdToEyeViewOffset before using ovrPosef for rendering
/// - Must be called between ovrHmd_BeginFrameTiming and ovrHmd_EndFrameTiming.
/// - If returned pose is used for rendering the eye, it should be passed to ovrHmd_EndFrame.
/// - Parameter 'eye' is used internally for prediction timing only
OVR_PUBLIC_FUNCTION(ovrPosef) ovrHmd_GetHmdPosePerEye(ovrHmd hmd, ovrEyeType eye);


//-------------------------------------------------------------------------------------
// *****  Client Distortion Rendering Functions

// These functions provide the distortion data and render timing support necessary to allow
// client rendering of distortion. Client-side rendering involves the following steps:
//
//  1. Setup ovrEyeDesc based on the desired texture size and FOV.
//     Call ovrHmd_GetRenderDesc to get the necessary rendering parameters for each eye.
//
//  2. Use ovrHmd_CreateDistortionMesh to generate the distortion mesh.
//
//  3. Use ovrHmd_BeginFrameTiming, ovrHmd_GetEyePoses, and ovrHmd_BeginFrameTiming in
//     the rendering loop to obtain timing and predicted head orientation when rendering each eye.
//      - When using timewarp, use ovr_WaitTillTime after the rendering and gpu flush, followed
//        by ovrHmd_GetEyeTimewarpMatrices to obtain the timewarp matrices used
//        by the distortion pixel shader. This will minimize latency.
//

/// Computes the distortion viewport, view adjust, and other rendering parameters for
/// the specified eye. This can be used instead of ovrHmd_ConfigureRendering to do
/// setup for client rendered distortion.
OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc) ovrHmd_GetRenderDesc(ovrHmd hmd,
                                                 ovrEyeType eyeType, ovrFovPort fov);


/// Describes a vertex used by the distortion mesh. This is intended to be converted into
/// the engine-specific format. Some fields may be unused based on the ovrDistortionCaps
/// flags selected. TexG and TexB, for example, are not used if chromatic correction is
/// not requested.
typedef struct ovrDistortionVertex_
{
    ovrVector2f ScreenPosNDC;    ///< [-1,+1],[-1,+1] over the entire framebuffer.
    float       TimeWarpFactor;  ///< Lerp factor between time-warp matrices. Can be encoded in Pos.z.
    float       VignetteFactor;  ///< Vignette fade factor. Can be encoded in Pos.w.
    ovrVector2f TanEyeAnglesR;   ///< The tangents of the horizontal and vertical eye angles for the red channel.
    ovrVector2f TanEyeAnglesG;   ///< The tangents of the horizontal and vertical eye angles for the green channel.
    ovrVector2f TanEyeAnglesB;   ///< The tangents of the horizontal and vertical eye angles for the blue channel.
} ovrDistortionVertex;

/// Describes a full set of distortion mesh data, filled in by ovrHmd_CreateDistortionMesh.
/// Contents of this data structure, if not null, should be freed by ovrHmd_DestroyDistortionMesh.
typedef struct ovrDistortionMesh_
{
    ovrDistortionVertex* pVertexData; ///< The distortion vertices representing each point in the mesh.
    unsigned short*      pIndexData;  ///< Indices for connecting the mesh vertices into polygons.
    unsigned int         VertexCount; ///< The number of vertices in the mesh.
    unsigned int         IndexCount;  ///< The number of indices in the mesh.
} ovrDistortionMesh;

/// Generate distortion mesh per eye.
/// Distortion capabilities will depend on 'distortionCaps' flags. Users should 
/// render using the appropriate shaders based on their settings.
/// Distortion mesh data will be allocated and written into the ovrDistortionMesh data structure,
/// which should be explicitly freed with ovrHmd_DestroyDistortionMesh.
/// Users should call ovrHmd_GetRenderScaleAndOffset to get uvScale and Offset values for rendering.
/// The function shouldn't fail unless theres is a configuration or memory error, in which case
/// ovrDistortionMesh values will be set to null.
/// This is the only function in the SDK reliant on eye relief, currently imported from profiles,
/// or overridden here.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_CreateDistortionMesh(ovrHmd hmd,
                                                 ovrEyeType eyeType, ovrFovPort fov,
                                                 unsigned int distortionCaps,
                                                 ovrDistortionMesh *meshData);
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_CreateDistortionMeshDebug(ovrHmd hmddesc,
                                                     ovrEyeType eyeType, ovrFovPort fov,
                                                     unsigned int distortionCaps,
                                                     ovrDistortionMesh *meshData,
                                                     float debugEyeReliefOverrideInMetres);


/// Used to free the distortion mesh allocated by ovrHmd_GenerateDistortionMesh. meshData elements
/// are set to null and zeroes after the call.
OVR_PUBLIC_FUNCTION(void) ovrHmd_DestroyDistortionMesh(ovrDistortionMesh* meshData);

/// Computes updated 'uvScaleOffsetOut' to be used with a distortion if render target size or
/// viewport changes after the fact. This can be used to adjust render size every frame if desired.
OVR_PUBLIC_FUNCTION(void) ovrHmd_GetRenderScaleAndOffset(ovrFovPort fov,
                                                    ovrSizei textureSize, ovrRecti renderViewport,
                                                    ovrVector2f uvScaleOffsetOut[2] );

/// Thread-safe timing function for the main thread. Caller should increment frameIndex
/// with every frame and pass the index where applicable to functions called on the
/// rendering thread.
OVR_PUBLIC_FUNCTION(ovrFrameTiming) ovrHmd_GetFrameTiming(ovrHmd hmd, unsigned int frameIndex);

/// Called at the beginning of the frame on the rendering thread.
/// Pass frameIndex == 0 if ovrHmd_GetFrameTiming isn't being used. Otherwise,
/// pass the same frame index as was used for GetFrameTiming on the main thread.
OVR_PUBLIC_FUNCTION(ovrFrameTiming) ovrHmd_BeginFrameTiming(ovrHmd hmd, unsigned int frameIndex);

/// Marks the end of client distortion rendered frame, tracking the necessary timing information.
/// This function must be called immediately after Present/SwapBuffers + GPU sync. GPU sync is
/// important before this call to reduce latency and ensure proper timing.
OVR_PUBLIC_FUNCTION(void) ovrHmd_EndFrameTiming(ovrHmd hmd);

/// Initializes and resets frame time tracking. This is typically not necessary, but
/// is helpful if game changes vsync state or video mode. vsync is assumed to be on if this
/// isn't called. Resets internal frame index to the specified number.
OVR_PUBLIC_FUNCTION(void) ovrHmd_ResetFrameTiming(ovrHmd hmd, unsigned int frameIndex);

/// Computes timewarp matrices used by distortion mesh shader, these are used to adjust
/// for head orientation change since the last call to ovrHmd_GetEyePoses
/// when rendering this eye. The ovrDistortionVertex::TimeWarpFactor is used to blend between the
/// matrices, usually representing two different sides of the screen.
/// Set 'calcPosition' to true when using depth based positional timewarp
/// Must be called on the same thread as ovrHmd_BeginFrameTiming.
OVR_PUBLIC_FUNCTION(void) ovrHmd_GetEyeTimewarpMatrices(ovrHmd hmd, ovrEyeType eye, ovrPosef renderPose,
                                                           ovrMatrix4f twmOut[2]);
OVR_PUBLIC_FUNCTION(void) ovrHmd_GetEyeTimewarpMatricesDebug(ovrHmd hmddesc, ovrEyeType eye, ovrPosef renderPose,
                                                             ovrQuatf playerTorsoMotion, ovrMatrix4f twmOut[2],
                                                             double debugTimingOffsetInSeconds);




//-------------------------------------------------------------------------------------
// ***** Stateless math setup functions

/// Returns global, absolute high-resolution time in seconds. This is the same
/// value as used in sensor messages.
OVR_PUBLIC_FUNCTION(double) ovr_GetTimeInSeconds();


// -----------------------------------------------------------------------------------
// ***** Latency Test interface

/// Does latency test processing and returns 'TRUE' if specified rgb color should
/// be used to clear the screen.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_ProcessLatencyTest(ovrHmd hmd, unsigned char rgbColorOut[3]);

/// Returns non-null string once with latency test result, when it is available.
/// Buffer is valid until next call.
OVR_PUBLIC_FUNCTION(const char*) ovrHmd_GetLatencyTestResult(ovrHmd hmd);

/// Returns the latency testing color in rgbColorOut to render when using a DK2
/// Returns false if this feature is disabled or not-applicable (e.g. using a DK1)
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_GetLatencyTest2DrawColor(ovrHmd hmddesc, unsigned char rgbColorOut[3]);

//-------------------------------------------------------------------------------------
// ***** Health and Safety Warning Display interface
//

/// Used by ovrhmd_GetHSWDisplayState to report the current display state.
typedef struct OVR_ALIGNAS(8) ovrHSWDisplayState_
{
    /// If true then the warning should be currently visible
    /// and the following variables have meaning. Else there is no
    /// warning being displayed for this application on the given HMD.
    ovrBool Displayed;              ///< True if the Health&Safety Warning is currently displayed.
    char    Pad[8-sizeof(ovrBool)]; ///< Unused struct padding.
    double  StartTime;              ///< Absolute time when the warning was first displayed. See ovr_GetTimeInSeconds().
    double  DismissibleTime;        ///< Earliest absolute time when the warning can be dismissed. May be a time in the past.
} ovrHSWDisplayState;

/// Returns the current state of the HSW display. If the application is doing the rendering of
/// the HSW display then this function serves to indicate that the warning should be
/// currently displayed. If the application is using SDK-based eye rendering then the SDK by
/// default automatically handles the drawing of the HSW display. An application that uses
/// application-based eye rendering should use this function to know when to start drawing the
/// HSW display itself and can optionally use it in conjunction with ovrhmd_DismissHSWDisplay
/// as described below.
///
/// Example usage for application-based rendering:
///    bool HSWDisplayCurrentlyDisplayed = false; // global or class member variable
///    ovrHSWDisplayState hswDisplayState;
///    ovrhmd_GetHSWDisplayState(Hmd, &hswDisplayState);
///
///    if (hswDisplayState.Displayed && !HSWDisplayCurrentlyDisplayed) {
///        <insert model into the scene that stays in front of the user>
///        HSWDisplayCurrentlyDisplayed = true;
///    }
OVR_PUBLIC_FUNCTION(void) ovrHmd_GetHSWDisplayState(ovrHmd hmd, ovrHSWDisplayState *hasWarningState);

/// Requests a dismissal of the HSWDisplay at the earliest possible time, which may be seconds
/// into the future due to display longevity requirements.
/// Returns true if the display is valid, in which case the request can always be honored.
///
/// Example usage :
///    void ProcessEvent(int key) {
///        if (key == escape)
///            ovrhmd_DismissHSWDisplay(hmd);
///    }
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_DismissHSWDisplay(ovrHmd hmd);

/// Get boolean property. Returns first element if property is a boolean array.
/// Returns defaultValue if property doesn't exist.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_GetBool(ovrHmd hmd, const char* propertyName, ovrBool defaultVal);

/// Modify bool property; false if property doesn't exist or is readonly.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetBool(ovrHmd hmd, const char* propertyName, ovrBool value);

/// Get integer property. Returns first element if property is an integer array.
/// Returns defaultValue if property doesn't exist.
OVR_PUBLIC_FUNCTION(int) ovrHmd_GetInt(ovrHmd hmd, const char* propertyName, int defaultVal);

/// Modify integer property; false if property doesn't exist or is readonly.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetInt(ovrHmd hmd, const char* propertyName, int value);

/// Get float property. Returns first element if property is a float array.
/// Returns defaultValue if property doesn't exist.
OVR_PUBLIC_FUNCTION(float) ovrHmd_GetFloat(ovrHmd hmd, const char* propertyName, float defaultVal);

/// Modify float property; false if property doesn't exist or is readonly.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetFloat(ovrHmd hmd, const char* propertyName, float value);

/// Get float[] property. Returns the number of elements filled in, 0 if property doesn't exist.
/// Maximum of arraySize elements will be written.
OVR_PUBLIC_FUNCTION(unsigned int) ovrHmd_GetFloatArray(ovrHmd hmd, const char* propertyName,
                                            float values[], unsigned int arraySize);

/// Modify float[] property; false if property doesn't exist or is readonly.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetFloatArray(ovrHmd hmd, const char* propertyName,
                                             float values[], unsigned int arraySize);

/// Get string property. Returns first element if property is a string array.
/// Returns defaultValue if property doesn't exist.
/// String memory is guaranteed to exist until next call to GetString or GetStringArray, or HMD is destroyed.
OVR_PUBLIC_FUNCTION(const char*) ovrHmd_GetString(ovrHmd hmd, const char* propertyName,
                                        const char* defaultVal);

/// Set string property
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetString(ovrHmd hmddesc, const char* propertyName,
                                    const char* value);

// -----------------------------------------------------------------------------------
// ***** Logging

/// Send a message string to the system tracing mechanism if enabled (currently Event Tracing for Windows)
/// Level is one of the ovrLogLevel constants.
/// returns the length of the message, or -1 if message is too large
OVR_PUBLIC_FUNCTION(int) ovr_TraceMessage(int level, const char* message);


// DEPRECATED: These functions are being phased out in favor of a more comprehensive logging system.
// These functions will return false and do nothing.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_StartPerfLog(ovrHmd hmd, const char* fileName, const char* userData1);
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_StopPerfLog(ovrHmd hmd);


#ifdef __cplusplus
} // extern "C"
#endif


#if defined(_MSC_VER)
    #pragma warning(pop)
#endif



// -----------------------------------------------------------------------------------
// ***** Backward compatibility #includes
//
// This is at the bottom of this file because the following is dependent on the 
// declarations above. 

#if !defined(OVR_CAPI_NO_UTILS)
	#include "OVR_CAPI_Util.h"
#endif


#endif // OVR_CAPI_h
