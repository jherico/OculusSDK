/************************************************************************************

Filename    :   OVR_CAPI.h
Content     :   C Interface to Oculus sensors and rendering.
Created     :   November 23, 2013
Authors     :   Michael Antonov

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

************************************************************************************/
#ifndef OVR_CAPI_h
#define OVR_CAPI_h

#include <stdint.h>

typedef char ovrBool;

//-----------------------------------------------------------------------------------
// ***** OVR_EXPORT definition

#if !defined(OVR_EXPORT)
    #if defined(WIN32)    
        #define OVR_EXPORT __declspec(dllexport)        
    #else
        #define OVR_EXPORT
    #endif
#endif

//-----------------------------------------------------------------------------------
// ***** Simple Math Structures

// 2D integer
typedef struct ovrVector2i_
{
    int x, y;
} ovrVector2i;
typedef struct ovrSizei_
{
    int w, h;
} ovrSizei;
typedef struct ovrRecti_
{
    ovrVector2i Pos;
    ovrSizei    Size;
} ovrRecti;

// 3D
typedef struct ovrQuatf_
{
    float x, y, z, w;  
} ovrQuatf;
typedef struct ovrVector2f_
{
    float x, y;
} ovrVector2f;
typedef struct ovrVector3f_
{
    float x, y, z;
} ovrVector3f;
typedef struct ovrMatrix4f_
{
    float M[4][4];
} ovrMatrix4f;
// Position and orientation together.
typedef struct ovrPosef_
{
    ovrQuatf     Orientation;
    ovrVector3f  Position;    
} ovrPosef;

// Full pose (rigid body) configuration with first and second derivatives.
typedef struct ovrPoseStatef_
{
    ovrPosef     Pose;
    ovrVector3f  AngularVelocity;
    ovrVector3f  LinearVelocity;
    ovrVector3f  AngularAcceleration;
    ovrVector3f  LinearAcceleration;
    double       TimeInSeconds;         // Absolute time of this state sample.
} ovrPoseStatef;

// Field Of View (FOV) in tangent of the angle units.
// As an example, for a standard 90 degree vertical FOV, we would 
// have: { UpTan = tan(90 degrees / 2), DownTan = tan(90 degrees / 2) }.
typedef struct ovrFovPort_
{
    float UpTan;
    float DownTan;
    float LeftTan;
    float RightTan;
} ovrFovPort;


//-----------------------------------------------------------------------------------
// ***** HMD Types

// Enumerates all HMD types that we support.
typedef enum
{
    ovrHmd_None             = 0,    
    ovrHmd_DK1              = 3,
    ovrHmd_DKHD             = 4,
    ovrHmd_CrystalCoveProto = 5,
    ovrHmd_DK2              = 6,
    ovrHmd_Other             // Some HMD other then the one in the enumeration.
} ovrHmdType;

// HMD capability bits reported by device.
typedef enum
{
    ovrHmdCap_Present           = 0x0001,   //  This HMD exists (as opposed to being unplugged).
    ovrHmdCap_Available         = 0x0002,   //  HMD and is sensor is available for use
                                            //  (if not owned by another app).    
    ovrHmdCap_Orientation       = 0x0010,   //  Support orientation tracking (IMU).
    ovrHmdCap_YawCorrection     = 0x0020,   //  Supports yaw correction through magnetometer or other means.
    ovrHmdCap_Position          = 0x0040,   //  Supports positional tracking.
    ovrHmdCap_LowPersistence    = 0x0080,   //  Supports low persistence mode.
	ovrHmdCap_LatencyTest       = 0x0100,   //  Supports pixel reading for continous latency testing.
    ovrHmdCap_DynamicPrediction = 0x0200,   //  Adjust prediction dynamically based on DK2 Latency.

    // Support rendering without VSync for debugging
    ovrHmdCap_NoVSync           = 0x1000
} ovrHmdCapBits;

// Describes distortion rendering parameters for ovrHmd_ConfigureRenderAPI or for
// ovrHmd_GenerateDistortionMesh.
typedef enum
{        
    ovrDistortion_Chromatic = 0x01,
    ovrDistortion_TimeWarp	= 0x02,
    ovrDistortion_Vignette  = 0x08
} ovrDistortionCaps;


// Specifies which eye is being used for rendering.
// This type explicitly does not include a third "NoStereo" option, as such is
// not required for an HMD-centered API.
typedef enum
{
    ovrEye_Left  = 0,
    ovrEye_Right = 1,
    ovrEye_Count = 2
} ovrEyeType;


// Handle to HMD; returned by ovrHmd_Create.
typedef struct ovrHmdStruct* ovrHmd;

// This is a complete descriptor of the HMD.
typedef struct ovrHmdDesc_
{
    ovrHmd      Handle;  // Handle of this HMD.
    ovrHmdType  Type;
    
    // Name string describing the product: "Oculus Rift DK1", etc.
    const char* ProductName;    
    const char* Manufacturer;

    // Capability bits described by ovrHmdCapBits.
    unsigned int Caps;
    unsigned int DistortionCaps;

    // Resolution of the entire HMD screen (for both eyes) in pixels.
    ovrSizei    Resolution;
    // Where monitor window should be on screen or (0,0).
    ovrVector2i WindowsPos;     

    // These define the recommended and maximum optical FOVs for the HMD.    
    ovrFovPort  DefaultEyeFov[ovrEye_Count];
    ovrFovPort  MaxEyeFov[ovrEye_Count];

    // Preferred eye rendering order for best performance.
    // Can help reduce latency on sideways-scanned screens.
    ovrEyeType  EyeRenderOrder[ovrEye_Count];
    
    // Display that HMD should present on.
    // TBD: It may be good to remove this information relying on WidowPos instead.
    // Ultimately, we may need to come up with a more convenient alternative,
    // such as a API-specific functions that return adapter, ot something that will
    // work with our monitor driver.

    // Windows: "\\\\.\\DISPLAY3", etc. Can be used in EnumDisplaySettings/CreateDC.    
    const char* DisplayDeviceName;
    // MacOS
    long        DisplayId;
} ovrHmdDesc;

// Describes the type of positional tracking being done.
/*
typedef enum
{
    ovrPose_None,
    ovrPose_HeadModel,
    ovrPose_Positional
} ovrPoseType;
*/


// Bit flags describing the current status of sensor tracking.
typedef enum
{
    ovrStatus_OrientationTracked    = 0x0001,   // Orientation is currently tracked (connected and in use).
    ovrStatus_PositionTracked       = 0x0002,   // Position is currently tracked (FALSE if out of range).
    ovrStatus_PositionConnected     = 0x0020,   // Position tracking HW is connected.
    ovrStatus_HmdConnected          = 0x0080    // HMD Display is available & connected.
} ovrStatusBits;


// State of the sensor at given a absolute time.
typedef struct ovrSensorState_
{
    // Predicted pose configuration at requested absolute time.
    // One can determine the time difference between predicted and actual
    // readings by comparing ovrPoseState.TimeInSeconds.
    ovrPoseStatef  Predicted;
    // Actual recorded pose configuration based on the sensor sample at a 
    // moment closest to the requested time.
    ovrPoseStatef  Recorded;

    // Sensor temperature reading, in degrees Celsius, as sample time.
    float          Temperature;    
    // Sensor status described by ovrStatusBits.
    unsigned int   StatusFlags;
} ovrSensorState;

// For now.
// TBD: Decide if this becomes a part of HMDDesc
typedef struct ovrSensorDesc_
{
    // HID Vendor and ProductId of the device.
    short   VendorId;
    short   ProductId;
    // Sensor (and display) serial number.
    char    SerialNumber[24];
} ovrSensorDesc;



// Frame data reported by ovrHmd_BeginFrameTiming().
typedef struct ovrFrameTiming_
{
    // The amount of time that has passed since the previous frame returned
    // BeginFrameSeconds value, usable for movement scaling.
    // This will be clamped to no more than 0.1 seconds to prevent
    // excessive movement after pauses for loading or initialization.
    float			DeltaSeconds;

    // It is generally expected that the following hold:
    // ThisFrameSeconds < TimewarpPointSeconds < NextFrameSeconds < 
    // EyeScanoutSeconds[EyeOrder[0]] <= ScanoutMidpointSeconds <= EyeScanoutSeconds[EyeOrder[1]]

    // Absolute time value of when rendering of this frame began or is expected to
    // begin; generally equal to NextFrameSeconds of the previous frame. Can be used
    // for animation timing.
    double			ThisFrameSeconds;
    // Absolute point when IMU expects to be sampled for this frame.
    double			TimewarpPointSeconds;
    // Absolute time when frame Present + GPU Flush will finish, and the next frame starts.
    double			NextFrameSeconds;

    // Time when when half of the screen will be scanned out. Can be passes as a prediction
    // value to ovrHmd_GetSensorState() go get general orientation.
    double		    ScanoutMidpointSeconds;
    // Timing points when each eye will be scanned out to display. Used for rendering each eye. 
    double			EyeScanoutSeconds[2];    

} ovrFrameTiming;




// Describes an eye for ovrHmd_Configure().
// Configure will generate more complete ovrEyeRenderDesc based on this data.
// Users must fill in both render target TextureSize and a RenderViewport within it
// to specify the rectangle from which pre-distorted eye image will be taken.
// A different RenderViewport may be used during rendering by specifying either
//    (a) calling ovrHmd_GetRenderScaleAndOffset with game-rendered api,
// or (b) passing different values in ovrTexture in case of SDK-rendered distortion.
typedef struct ovrEyeDesc_
{    
    ovrEyeType  Eye;
    ovrSizei    TextureSize;     // Absolute size of render texture.
    ovrRecti    RenderViewport;  // Viewport within texture where eye rendering takes place.
                                 // If specified as (0,0,0,0), it will be initialized to TextureSize.
    ovrFovPort  Fov;
} ovrEyeDesc;

// Rendering information for each eye, computed by ovrHmd_Configure().
typedef struct ovrEyeRenderDesc_
{    
    ovrEyeDesc  Desc;        
	ovrRecti	DistortedViewport; 	        // Distortion viewport 
    ovrVector2f PixelsPerTanAngleAtCenter;  // How many display pixels will fit in tan(angle) = 1.
    ovrVector3f ViewAdjust;  		        // Translation to be applied to view matrix.
} ovrEyeRenderDesc;


//-----------------------------------------------------------------------------------
// ***** Platform-independent Rendering Configuration

// These types are used to hide platform-specific details when passing
// render device, OS and texture data to the APIs.
//
// The benefit of having these wrappers vs. platform-specific API functions is
// that they allow game glue code to be portable. A typical example is an
// engine that has multiple back ends, say GL and D3D. Portable code that calls
// these back ends may also use LibOVR. To do this, back ends can be modified
// to return portable types such as ovrTexture and ovrRenderAPIConfig.

typedef enum
{
    ovrRenderAPI_None,
    ovrRenderAPI_OpenGL,
    ovrRenderAPI_Android_GLES,  // May include extra native window pointers, etc.
    ovrRenderAPI_D3D9,
    ovrRenderAPI_D3D10,
    ovrRenderAPI_D3D11,
    ovrRenderAPI_Count
} ovrRenderAPIType;

// Platform-independent part of rendering API-configuration data.
// It is a part of ovrRenderAPIConfig, passed to ovrHmd_Configure.
typedef struct ovrRenderAPIConfigHeader_
{
    ovrRenderAPIType API;
    ovrSizei         RTSize;
    int              Multisample;
} ovrRenderAPIConfigHeader;

typedef struct ovrRenderAPIConfig_
{
    ovrRenderAPIConfigHeader Header;
    uintptr_t                PlatformData[8];
} ovrRenderAPIConfig;

// Platform-independent part of eye texture descriptor.
// It is a part of ovrTexture, passed to ovrHmd_EndFrame.
//  - If RenderViewport is all zeros, will be used.
typedef struct ovrTextureHeader_
{
    ovrRenderAPIType API;
    ovrSizei         TextureSize;
    ovrRecti         RenderViewport;  // Pixel viewport in texture that holds eye image.
} ovrTextureHeader;

typedef struct ovrTexture_
{
    ovrTextureHeader Header;
    uintptr_t        PlatformData[8];
} ovrTexture;


// -----------------------------------------------------------------------------------
// ***** API Interfaces

// Basic steps to use the API:
//
// Setup:
//  1. ovrInitialize();
//  2. ovrHMD hmd = ovrHmd_Create(0);  ovrHmd_GetDesc(hmd, &hmdDesc);
//  3. Use hmdDesc and ovrHmd_GetFovTextureSize() to determine graphics configuration.
//  4. Call ovrHmd_StartSensor() to configure and initialize tracking.
//  5. Call ovrHmd_ConfigureRendering() to setup graphics for SDK rendering,
//     which is the preferred approach.
//     Please refer to "Game-Side Rendering" below if you prefer to do that instead.
//  5. Allocate textures as needed.
//
// Game Loop:
//  6. Call ovrHmd_BeginFrame() to get frame timing and orientation information.
//  7. Render each eye in between ovrHmd_BeginEyeRender and ovrHmd_EndEyeRender calls,
//     providing the result texture to the API.
//  8. Call ovrHmd_EndFrame() to render distorted textures to the back buffer
//     and present them on the Hmd.
//
// Shutdown:
//  9. ovrHmd_Destroy(hmd)
//  10. ovr_Shutdown()
//

#ifdef __cplusplus 
extern "C" {
#endif

// Library init/shutdown, must be called around all other OVR code.
// No other functions calls are allowed before ovr_Initialize succeeds or after ovr_Shutdown.
OVR_EXPORT ovrBool  ovr_Initialize();
OVR_EXPORT void     ovr_Shutdown();


// Detects or re-detects HMDs and reports the total number detected.
// Users can get information about each HMD by calling ovrHmd_Create with an index.
OVR_EXPORT int      ovrHmd_Detect();


// Creates a handle to an HMD and optionally fills in data about it.
// Index can [0 .. ovrHmd_Detect()-1]; index mappings can cange after each ovrHmd_Detect call.
// If not null, returned handle must be freed with ovrHmd_Destroy.
OVR_EXPORT ovrHmd   ovrHmd_Create(int index);
OVR_EXPORT void     ovrHmd_Destroy(ovrHmd hmd);

// Creates a "fake" HMD used for debugging only. This is not tied to specific hardware,
// but may be used to debug some of the related rendering.
OVR_EXPORT ovrHmd   ovrHmd_CreateDebug(ovrHmdType type);


// Returns last error for HMD state. Returns null for no error.
// String is valid until next call or GetLastError or HMD is destroyed.
// Pass null hmd to get global error (for create, etc).
OVR_EXPORT const char* ovrHmd_GetLastError(ovrHmd hmd);


//-------------------------------------------------------------------------------------
// ***** Sensor Interface

// All sensor interface functions are thread-safe, allowing sensor state to be sampled
// from different threads.

// Starts sensor sampling, enabling specified capabilities, described by ovrHmdCapBits.
//  - supportedCaps specifies support that is requested. The function will succeed even if,
//    if these caps are not available (i.e. sensor or camera is unplugged). Support will
//    automatically be enabled if such device is plugged in later. Software should check
//    ovrSensorState.StatusFlags for real-time status.
// - requiredCaps specify sensor capabilities required at the time of the call. If they
//   are not available, the function will fail. Pass 0 if only specifying SupportedCaps.
OVR_EXPORT ovrBool  ovrHmd_StartSensor(ovrHmd hmd, unsigned int supportedCaps,
                                       unsigned int requiredCaps);
// Stops sensor sampling, shutting down internal resources.
OVR_EXPORT void     ovrHmd_StopSensor(ovrHmd hmd);
// Resets sensor orientation.
OVR_EXPORT void     ovrHmd_ResetSensor(ovrHmd hmd);

// Returns sensor state reading based on the specified absolute system time.
// Pass absTime value of 0.0 to request the most recent sensor reading; in this case
// both PredictedPose and SamplePose will have the same value.
// ovrHmd_GetEyePredictedSensorState relies on this internally.
// This may also be used for more refined timing of FrontBuffer rendering logic, etc.
OVR_EXPORT ovrSensorState ovrHmd_GetSensorState(ovrHmd hmd, double absTime);

// Returns information about a sensor.
// Only valid after StartSensor.
OVR_EXPORT ovrBool     ovrHmd_GetSensorDesc(ovrHmd hmd, ovrSensorDesc* descOut);


//-------------------------------------------------------------------------------------
// ***** Graphics Setup

// Fills in description about HMD; this is the same as filled in by ovrHmd_Create.
OVR_EXPORT void     ovrHmd_GetDesc(ovrHmd hmd, ovrHmdDesc* desc);

// Calculates texture size recommended for rendering one eye within HMD, given FOV cone.
// Higher FOV will generally require larger textures to maintain quality.
//  - pixelsPerDisplayPixel specifies that number of render target pixels per display
//    pixel at center of distortion; 1.0 is the default value. Lower values
//    can improve performance.
OVR_EXPORT ovrSizei ovrHmd_GetFovTextureSize(ovrHmd hmd, ovrEyeType eye, ovrFovPort fov,
                                             float pixelsPerDisplayPixel);



//-------------------------------------------------------------------------------------
// *****  Rendering API Thread Safety

//  All of rendering APIs, inclusing Configure and frame functions are *NOT
//  Thread Safe*.  It is ok to use ConfigureRendering on one thread and handle
//  frames on another thread, but explicit synchronization must be done since
//  functions that depend on configured state are not reentrant.
//
//  As an extra requirement, any of the following calls must be done on
//  the render thread, which is the same thread that calls ovrHmd_BeginFrame
//  or ovrHmd_BeginFrameTiming.
//    - ovrHmd_EndFrame
//    - ovrHmd_BeginEyeRender
//    - ovrHmd_EndEyeRender
//    - ovrHmd_GetFramePointTime
//    - ovrHmd_GetEyePose
//    - ovrHmd_GetEyeTimewarpMatrices


//-------------------------------------------------------------------------------------
// *****  SDK-Rendering Functions

// These functions support rendering of distortion by the SDK through direct
// access to the underlying rendering HW, such as D3D or GL.
// This is the recommended approach, as it allows for better support or future
// Oculus hardware and a range of low-level optimizations.


// Configures rendering; fills in computed render parameters.
// This function can be called multiple times to change rendering settings.
// The users pass in two eye view descriptors that are used to
// generate complete rendering information for each eye in eyeRenderDescOut[2].
//
//  - apiConfig provides D3D/OpenGL specific parameters. Pass null
//    to shutdown rendering and release all resources.
//  - distortionCaps describe distortion settings that will be applied.
//
OVR_EXPORT ovrBool ovrHmd_ConfigureRendering( ovrHmd hmd,
                                              const ovrRenderAPIConfig* apiConfig,
                                              unsigned int hmdCaps,
                                              unsigned int distortionCaps,
                                              const ovrEyeDesc eyeDescIn[2],
                                              ovrEyeRenderDesc eyeRenderDescOut[2] );


// Begins a frame, returning timing and orientation information useful for simulation.
// This should be called in the beginning of game rendering loop (on render thread).
// This function relies on ovrHmd_BeginFrameTiming for some of its functionality.
// Pass 0 for frame index if not using GetFrameTiming.
OVR_EXPORT ovrFrameTiming ovrHmd_BeginFrame(ovrHmd hmd, unsigned int frameIndex);

// Ends frame, rendering textures to frame buffer. This may perform distortion and scaling
// internally, assuming is it not delegated to another thread. 
// Must be called on the same thread as BeginFrame. Calls ovrHmd_BeginEndTiming internally.
// *** This Function will to Present/SwapBuffers and potentially wait for GPU Sync ***.
OVR_EXPORT void     ovrHmd_EndFrame(ovrHmd hmd);


// Marks beginning of eye rendering. Must be called on the same thread as BeginFrame.
// This function uses ovrHmd_GetEyePose to predict sensor state that should be
// used rendering the specified eye.
// This combines current absolute time with prediction that is appropriate for this HMD.
// It is ok to call ovrHmd_BeginEyeRender() on both eyes before calling ovrHmd_EndEyeRender.
// If rendering one eye at a time, it is best to render eye specified by
// HmdDesc.EyeRenderOrder[0] first.
OVR_EXPORT ovrPosef ovrHmd_BeginEyeRender(ovrHmd hmd, ovrEyeType eye);

// Marks the end of eye rendering and submits eye texture for display after it is ready.
// Rendering viewport within the texture can change per frame if necessary.
// Specified texture  may be presented immediately or wait till ovrHmd_EndFrame based
// on implementation. The API may performs distortion and scaling internally.
// 'renderPose' will typically be the value returned from ovrHmd_BeginEyeRender, but can
// be different if different pose was used for rendering.
OVR_EXPORT void     ovrHmd_EndEyeRender(ovrHmd hmd, ovrEyeType eye,
                                        ovrPosef renderPose, ovrTexture* eyeTexture);



//-------------------------------------------------------------------------------------
// *****  Game-Side Rendering Functions

// These functions provide distortion data and render timing support necessary to allow
// game rendering of distortion. Game-side rendering involves the following steps:
//
//  1. Setup ovrEyeDesc based on desired texture size and Fov.
//     Call ovrHmd_GetRenderDesc to get the necessary rendering parameters for each eye.
// 
//  2. Use ovrHmd_CreateDistortionMesh to generate distortion mesh.
//
//  3. Use ovrHmd_BeginFrameTiming, ovrHmd_GetEyePose and ovrHmd_BeginFrameTiming
//     in the rendering loop to obtain timing and predicted view orientation for
//     each eye.
//      - If relying on timewarp, use ovr_WaitTillTime after rendering+flush, followed
//        by ovrHmd_GetEyeTimewarpMatrices to obtain timewarp matrices used 
//        in distortion pixel shader to reduce latency.
//

// Computes distortion viewport, view adjust and other rendering for the specified
// eye. This can be used instead of ovrHmd_ConfigureRendering to help setup rendering on
// the game side.
OVR_EXPORT ovrEyeRenderDesc ovrHmd_GetRenderDesc(ovrHmd hmd, ovrEyeDesc eyeDesc);


// Describes a vertex used for distortion; this is intended to be converted into
// the engine-specific format.
// Some fields may be unused based on ovrDistortionCaps selected. TexG and TexB, for example,
// are not used if chromatic correction is not requested.
typedef struct ovrDistortionVertex_
{
    ovrVector2f Pos;
    float       TimeWarpFactor;  // Lerp factor between time-warp matrices. Can be encoded in Pos.z.
    float       VignetteFactor;  // Vignette fade factor. Can be encoded in Pos.w.
    ovrVector2f TexR;
    ovrVector2f TexG;
    ovrVector2f TexB;    
} ovrDistortionVertex;

// Describes a full set of distortion mesh data, filled in by ovrHmd_CreateDistortionMesh.
// Contents of this data structure, if not null, should be freed by ovrHmd_DestroyDistortionMesh.
typedef struct ovrDistortionMesh_
{
    ovrDistortionVertex* pVertexData;
    unsigned short*      pIndexData;
    unsigned int         VertexCount;
    unsigned int         IndexCount;
} ovrDistortionMesh;

// Generate distortion mesh per eye.
// Distortion capabilities will depend on 'distortionCaps' flags; user should rely on
// appropriate shaders based on their settings.
// Distortion mesh data will be allocated and stored into the ovrDistortionMesh data structure,
// which should be explicitly freed with ovrHmd_DestroyDistortionMesh.
// uvScaleOffsetOut[] are filled in based on render target settings of eyeDesc.
// The function shouldn't fail unless theres is a configuration or memory error, in which case
// ovrDistortionMesh values will be set to null.
OVR_EXPORT ovrBool  ovrHmd_CreateDistortionMesh( ovrHmd hmd, ovrEyeDesc eyeDesc,
                                                 unsigned int distortionCaps,
                                                 ovrVector2f uvScaleOffsetOut[2], 
                                                 ovrDistortionMesh *meshData );

// Frees distortion mesh allocated by ovrHmd_GenerateDistortionMesh. meshData elements
// are set to null and zeroes after the call.
OVR_EXPORT void     ovrHmd_DestroyDistortionMesh( ovrDistortionMesh* meshData );

// Computes updated 'uvScaleOffsetOut' to be used with a distortion if render target size or
// viewport changes after the fact. This can be used to adjust render size every frame, if desired.
OVR_EXPORT void     ovrHmd_GetRenderScaleAndOffset( ovrHmd hmd, ovrEyeDesc eyeDesc,
                                                    unsigned int distortionCaps,
                                                    ovrVector2f uvScaleOffsetOut[2] );


// Thread-safe timing function for the main thread. Caller should increment frameIndex
// with every frame and pass the index to RenderThread for processing.
OVR_EXPORT ovrFrameTiming ovrHmd_GetFrameTiming(ovrHmd hmd, unsigned int frameIndex);

// Called at the beginning of the frame on the Render Thread.
// Pass frameIndex == 0 if ovrHmd_GetFrameTiming isn't being used. Otherwise,
// pass the same frame index as was used for GetFrameTiming on the main thread.
OVR_EXPORT ovrFrameTiming ovrHmd_BeginFrameTiming(ovrHmd hmd, unsigned int frameIndex);

// Marks the end of game-rendered frame, tracking the necessary timing information. This
// function must be called immediately after Present/SwapBuffers + GPU sync. GPU sync is important
// before this call to reduce latency and ensure proper timing.
OVR_EXPORT void     ovrHmd_EndFrameTiming(ovrHmd hmd);

// Initializes and resets frame time tracking. This is typically not necessary, but
// is helpful if game changes vsync state or video mode. vsync is assumed to be on if this
// isn't called. Resets internal frame index to the specified number.
OVR_EXPORT void     ovrHmd_ResetFrameTiming(ovrHmd hmd, unsigned int frameIndex, bool vsync);


// Predicts and returns Pose that should be used rendering the specified eye.
// Must be called between ovrHmd_BeginFrameTiming & ovrHmd_EndFrameTiming.
OVR_EXPORT ovrPosef ovrHmd_GetEyePose(ovrHmd hmd, ovrEyeType eye);

// Computes timewarp matrices used by distortion mesh shader, these are used to adjust
// for orientation change since the last call to ovrHmd_GetEyePose for this eye.
// The ovrDistortionVertex::TimeWarpFactor is used to blend between the matrices,
// usually representing two different sides of the screen.
// Must be called on the same thread as ovrHmd_BeginFrameTiming.
OVR_EXPORT void     ovrHmd_GetEyeTimewarpMatrices(ovrHmd hmd, ovrEyeType eye,
                                                  ovrPosef renderPose, ovrMatrix4f twmOut[2]);



//-------------------------------------------------------------------------------------
// ***** Stateless math setup functions

// Used to generate projection from ovrEyeDesc::Fov.
OVR_EXPORT ovrMatrix4f ovrMatrix4f_Projection( ovrFovPort fov,
                                               float znear, float zfar, ovrBool rightHanded );

// Used for 2D rendering, Y is down
// orthoScale = 1.0f / pixelsPerTanAngleAtCenter
// orthoDistance = distance from camera, such as 0.8m
OVR_EXPORT ovrMatrix4f ovrMatrix4f_OrthoSubProjection(ovrMatrix4f projection, ovrVector2f orthoScale,
                                                      float orthoDistance, float eyeViewAdjustX);

// Returns global, absolute high-resolution time in seconds. This is the same
// value as used in sensor messages.
OVR_EXPORT double   ovr_GetTimeInSeconds();

// Waits until the specified absolute time.
OVR_EXPORT double   ovr_WaitTillTime(double absTime);



// -----------------------------------------------------------------------------------
// ***** Latency Test interface

// Does latency test processing and returns 'TRUE' if specified rgb color should
// be used to clear the screen.
OVR_EXPORT ovrBool      ovrHmd_ProcessLatencyTest(ovrHmd hmd, unsigned char rgbColorOut[3]);

// Returns non-null string once with latency test result, when it is available.
// Buffer is valid until next call.
OVR_EXPORT const char*  ovrHmd_GetLatencyTestResult(ovrHmd hmd);

// Returns latency for HMDs that support internal latency testing via the
// pixel-read back method (-1 for invalid or N/A)
OVR_EXPORT double       ovrHmd_GetMeasuredLatencyTest2(ovrHmd hmd);


// -----------------------------------------------------------------------------------
// ***** Property Access

// NOTICE: This is experimental part of API that is likely to go away or change.

// These allow accessing different properties of the HMD and profile.
// Some of the properties may go away with profile/HMD versions, so software should
// use defaults and/or proper fallbacks.
// 

// For now, access profile entries; this will change.
#if !defined(OVR_KEY_USER)

    #define OVR_KEY_USER                        "User"
    #define OVR_KEY_NAME                        "Name"
    #define OVR_KEY_GENDER                      "Gender"
    #define OVR_KEY_PLAYER_HEIGHT               "PlayerHeight"
    #define OVR_KEY_EYE_HEIGHT                  "EyeHeight"
    #define OVR_KEY_IPD                         "IPD"
    #define OVR_KEY_NECK_TO_EYE_HORIZONTAL      "NeckEyeHori"
    #define OVR_KEY_NECK_TO_EYE_VERTICAL        "NeckEyeVert"

    #define OVR_DEFAULT_GENDER                  "Male"
    #define OVR_DEFAULT_PLAYER_HEIGHT           1.778f
    #define OVR_DEFAULT_EYE_HEIGHT              1.675f
    #define OVR_DEFAULT_IPD                     0.064f
    #define OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL  0.12f
    #define OVR_DEFAULT_NECK_TO_EYE_VERTICAL    0.12f
#endif


// Get float property. Returns first element if property is a float array.
// Returns defaultValue if property doesn't exist.
OVR_EXPORT float       ovrHmd_GetFloat(ovrHmd hmd, const char* propertyName, float defaultVal);

// Modify float property; false if property doesn't exist or is readonly.
OVR_EXPORT ovrBool      ovrHmd_SetFloat(ovrHmd hmd, const char* propertyName, float value);


// Get float[] property. Returns the number of elements filled in, 0 if property doesn't exist.
// Maximum of arraySize elements will be written.
OVR_EXPORT unsigned int ovrHmd_GetFloatArray(ovrHmd hmd, const char* propertyName,
                                            float values[], unsigned int arraySize);

// Modify float[] property; false if property doesn't exist or is readonly.
OVR_EXPORT ovrBool      ovrHmd_SetFloatArray(ovrHmd hmd, const char* propertyName,
                                             float values[], unsigned int arraySize);

// Get string property. Returns first element if property is a string array.
// Returns defaultValue if property doesn't exist.
// String memory is guaranteed to exist until next call to GetString or GetStringArray, or HMD is destroyed.
OVR_EXPORT const char* ovrHmd_GetString(ovrHmd hmd, const char* propertyName,
                                        const char* defaultVal);

// Returns array size of a property, 0 if property doesn't exist.
// Can be used to check existence of a property.
OVR_EXPORT unsigned int ovrHmd_GetArraySize(ovrHmd hmd, const char* propertyName);


#ifdef __cplusplus 
} // extern "C"
#endif


#endif	// OVR_CAPI_h
