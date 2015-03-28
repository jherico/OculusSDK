/************************************************************************************

Filename    :   OVR_CAPI.cpp
Content     :   Experimental simple C interface to the HMD - version 1.
Created     :   November 30, 2013
Authors     :   Michael Antonov

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

#include "OVR_CAPI.h"
#include "OVR_Version.h"


#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_System.h"
#include "Kernel/OVR_DebugHelp.h"
#include "Extras/OVR_Math.h"
#include "OVR_Stereo.h"
#include "OVR_Profile.h"

#include "CAPI/CAPI_HMDState.h"

#include "Net/OVR_Session.h"
#include "Service/Service_NetClient.h"
#ifdef OVR_PRIVATE_FILE
#include "Service/Service_NetServer.h"
#endif

#include "Displays/OVR_Display.h"

#if defined(OVR_OS_WIN32)
#include "Displays/OVR_Win32_ShimFunctions.h"
#include <VersionHelpers.h>
#endif

// Forward decl to keep the callback static

// Note: Removed CaptureHmdDescTrace from non-Win32 due to build warning.
#if !defined(OVR_OS_MAC) && !defined(OVR_OS_LINUX)
static bool CaptureHmdDescTrace(const OVR::CAPI::HMDState *state);
#endif
#define TRACE_STATE_CAPTURE_FUNC OVR::CAPI::HMDState::EnumerateHMDStateList(CaptureHmdDescTrace)
#include "Tracing/Tracing.h"

#if !defined(OVR_OS_MAC) && !defined(OVR_OS_LINUX)
// EnumerateHMDStateList callback for tracing state capture
static bool CaptureHmdDescTrace(const OVR::CAPI::HMDState* state)
{
    TraceHmdDesc(*state->pHmdDesc);
    OVR_UNUSED(state); // Avoid potential compiler warnings.
    return true;
}
#endif

// Produce an invalid tracking state that will not mess up the application too badly.
static ovrTrackingState GetNullTrackingState()
{
    ovrTrackingState nullState = ovrTrackingState();
    nullState.HeadPose.ThePose.Orientation.w = 1.f; // Provide valid quaternions for head pose.
    return nullState;
}

// Produce a null frame timing structure that will not break the calling application.
static ovrFrameTiming GetNullFrameTiming()
{
    ovrFrameTiming nullTiming = ovrFrameTiming();
    nullTiming.DeltaSeconds = 0.013f; // Provide nominal value
    return nullTiming;
}


using namespace OVR;
using namespace OVR::Util::Render;
using namespace OVR::Vision;

//-------------------------------------------------------------------------------------
// Math
namespace OVR {


// ***** SensorDataType

SensorDataType::SensorDataType(const ovrSensorData& s)
{
    Acceleration        = s.Accelerometer;
    RotationRate        = s.Gyro;
    MagneticField       = s.Magnetometer;
    Temperature         = s.Temperature;
    AbsoluteTimeSeconds = s.TimeInSeconds;
}

SensorDataType::operator ovrSensorData () const
{
    ovrSensorData result;
    result.Accelerometer = Acceleration;
    result.Gyro          = RotationRate;
    result.Magnetometer  = MagneticField;
    result.Temperature   = Temperature;
    result.TimeInSeconds = (float)AbsoluteTimeSeconds;
    return result;
}




// ***** SensorState

TrackingState::TrackingState(const ovrTrackingState& s)
{
    HeadPose          = s.HeadPose;
    CameraPose        = s.CameraPose;
    LeveledCameraPose = s.LeveledCameraPose;
    RawSensorData     = s.RawSensorData;
    StatusFlags       = s.StatusFlags;
}

TrackingState::operator ovrTrackingState() const
{
    ovrTrackingState result;
    result.HeadPose          = HeadPose;
    result.CameraPose        = CameraPose;
    result.LeveledCameraPose = LeveledCameraPose;
    result.RawSensorData     = RawSensorData;
    result.StatusFlags       = StatusFlags;
    return result;
}


} // namespace OVR

//-------------------------------------------------------------------------------------

using namespace OVR::CAPI;


// Helper function to validate the HMD object provided by the API user.
static HMDState* GetHMDStateFromOvrHmd(ovrHmd hmddesc)
{
    if (!hmddesc || !hmddesc->Handle)
        return nullptr;

    return (HMDState*)hmddesc->Handle;
}


OVR_PUBLIC_FUNCTION(double) ovr_GetTimeInSeconds()
{
    return Timer::GetSeconds();
}


//-------------------------------------------------------------------------------------

// 1. Init/shutdown.

static ovrBool CAPI_ovrInitializeCalled = ovrFalse;
static OVR::Service::NetClient* CAPI_pNetClient = nullptr;


ovrBool ovr_InitializeRenderingShim()
{
    OVR::Display::Initialize();

    return OVR::Display::GetDirectDisplayInitialized();
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_InitializeRenderingShimVersion(int requestedMinorVersion)
{
    // We ignore the patch and build versions here, as they aren't relevant to compatibility.
    // And we don't store them away here, as we do that in ovr_Initialize() instead.
    
    if (requestedMinorVersion > OVR_MINOR_VERSION)
        return ovrFalse;

    return ovr_InitializeRenderingShim();
}

// Write out to the log where the current running module is located on disk.
static void LogLocationOfThisModule()
{
#if defined (OVR_OS_WIN32)
    // Log out the DLL file path on startup.
    {
        bool success = false;

        HMODULE hModule = nullptr;
        GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            (LPCTSTR)&ovr_Initialize,
            &hModule);
        if (hModule)
        {
            wchar_t filename[_MAX_PATH];
            DWORD len = GetModuleFileNameW(hModule, filename, OVR_ARRAY_COUNT(filename));
            if (len > 0 && filename[0])
            {
                success = true;
                LogText("[CAPI] LibOVR module is located at %ws\n", filename);
            }
        }

        if (!success)
        {
            LogError("[CAPI] WARNING: Unable to find LibOVR module.");
        }
    }
#endif // OVR_OS_WIN32
}

// These defaults are also in OVR_CAPIShim.c
static const ovrInitParams DefaultParams = {
    ovrInit_RequestVersion, // Flags
    OVR_MINOR_VERSION,      // RequestedMinorVersion
    0,                      // LogCallback
    0                       // ConnectionTimeoutSeconds
};

// Precondition: params is not null
OVR_PUBLIC_FUNCTION(ovrBool) ovr_Initialize(ovrInitParams const* params)
{
    // TBD: Should we check if the version requested changed and fail here?
    if (CAPI_ovrInitializeCalled) // If already initialized...
        return ovrTrue;

    TraceInit();
    TraceCall(0);

    if (!params)
    {
        params = &DefaultParams;
    }

    bool DebugMode = (params->Flags & ovrInit_Debug) != 0;

#if defined(OVR_BUILD_DEBUG)
    // If no debug setting is provided,
    if (!(params->Flags & (ovrInit_Debug | ovrInit_ForceNoDebug)))
    {
        DebugMode = true;
    }
#endif

    // We ignore the requested patch version and build version, as they are not currently relevant to
    // the library compatibility. Our test for minor version compatibility is currently simple: we support
    // only older or equal minor versions, and don't change our behavior if the requested minor version
    // is older than than OVR_MINOR_VERSION.

    if ((params->Flags & ovrInit_RequestVersion) != 0 &&
        (params->RequestedMinorVersion > OVR_MINOR_VERSION))
    {
        goto Abort;
    }

#if defined(OVR_OS_WIN32)
    // Older than Windows 7 SP1?
    if (!IsWindows7SP1OrGreater())
    {
        MessageBoxA(nullptr, "This software depends on features available starting with \nWindows 7 Service Pack 1, and it cannot start.", "LibOVR: Cannot start", 0);
        OVR_ASSERT(false);
        goto Abort;
    }
#endif // OVR_OS_WIN32

    OVR::Net::RuntimeSDKVersion.SetCurrent();  // Fill in the constant parts of this struct.
    OVR::Net::RuntimeSDKVersion.RequestedMinorVersion = (uint16_t)params->RequestedMinorVersion;

    // Initialize display subsystem regardless of Allocator initialization.
    OVR::Display::Initialize();

    // We must set up the system for the plugin to work
    if (!OVR::System::IsInitialized())
    {
        // TBD: Base this on registry setting?
        Allocator::SetLeakTracking(DebugMode);

        OVR::Log* logger = OVR::Log::ConfigureDefaultLog(OVR::LogMask_All);

        // Set the CAPI logger callback
        logger->SetCAPICallback(params->LogCallback);

        OVR::System::Init(logger);
    }

    if (!OVR::Display::GetDirectDisplayInitialized() && !OVR::Display::InCompatibilityMode(true))
    {
        OVR_ASSERT(false);
        goto Abort;
    }

    CAPI_pNetClient = NetClient::GetInstance();

    // Store off the initialization parameters from ovr_Initialize()
    CAPI_pNetClient->ApplyParameters(params);

    // Mark as initialized regardless of whether or not we can connect to the server.
    CAPI_ovrInitializeCalled = 1;

    // Log the location of the module after most of the bring-up, as the game
    // could do almost anything in response to a log message callback.
    LogLocationOfThisModule();

    // If unable to connect to server and we are not in a debug mode,
    if (!CAPI_pNetClient->Connect(true) && !DebugMode)
    {
        // Then it's a failure when the server is unreachable.
        // This means that a DebugHMD cannot be created unless the ovrInit_Debug flag is set.
        goto Abort;
    }

    // everything is okay
    TraceReturn(0);
    return ovrTrue;

Abort:
    // clean up and return failure
    TraceReturn(0);
    TraceFini();
    return ovrFalse;
}


OVR_PUBLIC_FUNCTION(void) ovr_Shutdown()
{  
    if (CAPI_ovrInitializeCalled)
    {
        OVR::Display::Shutdown();
        TraceCall(0);
        TraceFini();
        CAPI_ovrInitializeCalled = 0;
    }

    if (OVR::System::IsInitialized())
    {
        OVR::System::Destroy();
    }

    OVR::Net::RuntimeSDKVersion.Reset();
    CAPI_pNetClient = nullptr;  // Not strictly necessary, but useful for debugging and cleanliness.
}


// There is a thread safety issue with ovrHmd_Detect in that multiple calls from different
// threads can corrupt the global array state. This would lead to two problems:
//  a) Create(index) enumerator may miss or overshoot items. Probably not a big deal
//     as game logic can easily be written to only do Detect(s)/Creates in one place.
//     The alternative would be to return list handle.
//  b) TBD: Un-mutexed Detect access from two threads could lead to crash. We should
//         probably check this.
//

OVR_PUBLIC_FUNCTION(int) ovrHmd_Detect()
{
    if (!CAPI_ovrInitializeCalled)
        return -2;

    return CAPI_pNetClient->Hmd_Detect();
}


// ovrHmd_Create us explicitly separated from ConfigureTracking and ConfigureRendering to allow creation of 
// a relatively light-weight handle that would reference the device going forward and would 
// survive future ovrHmd_Detect calls. That is once ovrHMD is returned, index is no longer
// necessary and can be changed by a ovrHmd_Detect call.
OVR_PUBLIC_FUNCTION(ovrHmd) ovrHmd_Create(int index)
{
    if (!CAPI_ovrInitializeCalled)
        return 0;

    double t0 = Timer::GetSeconds();
    HMDNetworkInfo netInfo;

    // There may be some delay before the HMD is fully detected.
    // Since we are also trying to create the HMD immediately it may lose this race and
    // get "NO HMD DETECTED."  Wait a bit longer to avoid this.
    while (!CAPI_pNetClient->Hmd_Create(index, &netInfo) ||
           netInfo.NetId == InvalidVirtualHmdId)
    {
        double waitTime = 2.0; // Default wait time

        if (NetClient::GetInstance()->IsConnected(false, false))
        {
            NetClient::GetInstance()->SetLastError("No HMD Detected");

            // If in single process mode,
            if (Net::Session::IsSingleProcess())
            {
                // Wait 8 seconds for HMD to be detected, as this is a single process
                // build and we expect that the operator has the system set up properly.
                waitTime = 8.0;
            }
            else
            {
                // Wait 1/2 second for HMD to be detected.
                waitTime = 0.5;
            }
        }
        else
        {
            NetClient::GetInstance()->SetLastError("Not connected to service");

            // Wait the default amount of time for the service to start up.
        }

        // If two seconds elapse and still no HMD detected,
        if (Timer::GetSeconds() - t0 > waitTime)
        {
            return 0;
        }
    }

    // Create HMD State object
    HMDState* hmds = HMDState::CreateHMDState(CAPI_pNetClient, netInfo);
    if (!hmds)
    {
        CAPI_pNetClient->Hmd_Release(netInfo.NetId);

        NetClient::GetInstance()->SetLastError("Unable to create HMD state");
        return 0;
    }

    // Reset frame timing so that FrameTimeManager values are properly initialized in AppRendered mode.
    ovrHmd_ResetFrameTiming(hmds->pHmdDesc, 0);

    TraceHmdDesc(*hmds->pHmdDesc);

    return hmds->pHmdDesc;
}


OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_AttachToWindow(ovrHmd hmddesc, void* window,
                                                   const ovrRecti* destMirrorRect,
                                                   const ovrRecti* sourceRenderTargetRect)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return ovrFalse;

    OVR_UNUSED3(destMirrorRect, sourceRenderTargetRect, hmds);

    if (!CAPI_ovrInitializeCalled)
        return ovrFalse;

#ifndef OVR_OS_MAC
    CAPI_pNetClient->Hmd_AttachToWindow(hmds->GetNetId(), window);
    hmds->pWindow = window;
#endif
#ifdef OVR_OS_WIN32
    Win32::DisplayShim::GetInstance().hWindow = (HWND)window;
#endif
#ifdef OVR_OS_MAC
    OVR_UNUSED(window);
#endif

    return ovrTrue;
}

OVR_PUBLIC_FUNCTION(ovrHmd) ovrHmd_CreateDebug(ovrHmdType type)
{
    if (!CAPI_ovrInitializeCalled)
        return 0;

    HMDState* hmds = HMDState::CreateDebugHMDState(type);
    if (!hmds)
        return nullptr;

    return hmds->pHmdDesc;
}

OVR_PUBLIC_FUNCTION(void) ovrHmd_Destroy(ovrHmd hmddesc)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return;

    {   // Thread checker in its own scope, to avoid access after 'delete'.
        // Essentially just checks that no other RenderAPI function is executing.
        ThreadChecker::Scope checkScope(&hmds->RenderAPIThreadChecker, "ovrHmd_Destroy");
    }    

#ifdef OVR_OS_WIN32
    if (hmds->pWindow)
    {
        // ? ok to call
        //CAPI_pNetClient->Hmd_AttachToWindow(hmds->GetNetId(), 0);
        hmds->pWindow = 0;
        Win32::DisplayShim::GetInstance().hWindow = (HWND)0;
    }    
#endif

    delete (HMDState*)hmddesc->Handle;
}


OVR_PUBLIC_FUNCTION(const char*) ovrHmd_GetLastError(ovrHmd hmddesc)
{
    if (!CAPI_ovrInitializeCalled)
        return "System initialize not called";

    VirtualHmdId netId = InvalidVirtualHmdId;

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (hmds)
        netId = hmds->GetNetId();

    return CAPI_pNetClient->Hmd_GetLastError(netId);
}

#define OVR_VERSION_LIBOVR_PFX "libOVR:"

// Returns version string representing libOVR version. Static, so
// string remains valid for app lifespan
OVR_PUBLIC_FUNCTION(const char*) ovr_GetVersionString()
{
	static const char* version = OVR_VERSION_LIBOVR_PFX OVR_VERSION_STRING;
    return version + sizeof(OVR_VERSION_LIBOVR_PFX) - 1;
}



//-------------------------------------------------------------------------------------

// Returns capability bits that are enabled at this time; described by ovrHmdCapBits.
// Note that this value is different font ovrHmdDesc::Caps, which describes what
// capabilities are available.
OVR_PUBLIC_FUNCTION(unsigned int) ovrHmd_GetEnabledCaps(ovrHmd hmddesc)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return 0;

    return hmds->EnabledHmdCaps;
}

// Modifies capability bits described by ovrHmdCapBits that can be modified,
// such as ovrHmdCap_LowPersistance.
OVR_PUBLIC_FUNCTION(void) ovrHmd_SetEnabledCaps(ovrHmd hmddesc, unsigned int capsBits)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return;

    hmds->SetEnabledHmdCaps(capsBits);
}


//-------------------------------------------------------------------------------------
// *** Sensor

// Sensor APIs are separated from Create & Configure for several reasons:
//  - They need custom parameters that control allocation of heavy resources
//    such as Vision tracking, which you don't want to create unless necessary.
//  - A game may want to switch some sensor settings based on user input, 
//    or at lease enable/disable features such as Vision for debugging.
//  - The same or syntactically similar sensor interface is likely to be used if we 
//    introduce controllers.
//
//  - Sensor interface functions are all Thread-safe, unlike the frame/render API
//    functions that have different rules (all frame access functions
//    must be on render thread)

OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_ConfigureTracking(ovrHmd hmddesc, unsigned int supportedCaps,
                                                      unsigned int requiredCaps)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return ovrFalse;

    return hmds->ConfigureTracking(supportedCaps, requiredCaps) ? ovrTrue : ovrFalse;
}


OVR_PUBLIC_FUNCTION(void) ovrHmd_RecenterPose(ovrHmd hmddesc)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return;

    hmds->RecenterPose();
}

OVR_PUBLIC_FUNCTION(ovrTrackingState) ovrHmd_GetTrackingState(ovrHmd hmddesc, double absTime)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return GetNullTrackingState();

    return hmds->PredictedTrackingState(absTime);
}





//-------------------------------------------------------------------------------------
// *** General Setup

// Per HMD -> calculateIdealPixelSize
OVR_PUBLIC_FUNCTION(ovrSizei) ovrHmd_GetFovTextureSize(ovrHmd hmddesc, ovrEyeType eye,
                                                       ovrFovPort fov, float pixelsPerDisplayPixel)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return Sizei(0);

    return hmds->RenderState.GetFOVTextureSize(eye, fov, pixelsPerDisplayPixel);
}


//-------------------------------------------------------------------------------------


OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_ConfigureRendering(ovrHmd hmddesc,
                                                       const ovrRenderAPIConfig* apiConfig,
                                                       unsigned int distortionCaps,
                                                       const ovrFovPort eyeFovIn[2],
                                                       ovrEyeRenderDesc eyeRenderDescOut[2])
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return ovrFalse;

    return hmds->ConfigureRendering(eyeRenderDescOut, eyeFovIn,
                                    apiConfig, distortionCaps);
}

OVR_PUBLIC_FUNCTION(ovrFrameTiming) ovrHmd_BeginFrame(ovrHmd hmddesc, unsigned int frameIndex)
{
    // NOTE: frameIndex == 0 is handled inside ovrHmd_BeginFrameTiming()

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return GetNullFrameTiming();

    TraceCall(frameIndex);

    // Check: Proper configure and threading state for the call.
    hmds->checkRenderingConfigured("ovrHmd_BeginFrame");
    OVR_DEBUG_LOG_COND(hmds->BeginFrameCalled, ("ovrHmd_BeginFrame called multiple times."));
    ThreadChecker::Scope checkScope(&hmds->RenderAPIThreadChecker, "ovrHmd_BeginFrame");

    hmds->BeginFrameCalled   = true;
    hmds->BeginFrameThreadId = OVR::GetCurrentThreadId();

    ovrFrameTiming timing = ovrHmd_BeginFrameTiming(hmddesc, frameIndex);

    TraceReturn(frameIndex);

    return timing;
}


// Renders textures to frame buffer
OVR_PUBLIC_FUNCTION(void) ovrHmd_EndFrame(ovrHmd hmddesc,
                                          const ovrPosef renderPose[2],
                                          const ovrTexture eyeTexture[2])
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return;

    const ovrTexture*        eyeDepthTexture = nullptr;
    ovrPositionTimewarpDesc* posTimewarpDesc = nullptr;

    TraceCall(hmds->BeginFrameIndex);

    hmds->SubmitEyeTextures(renderPose, eyeTexture, eyeDepthTexture);

    // Debug state checks: Must be in BeginFrame, on the same thread.
    hmds->checkBeginFrameScope("ovrHmd_EndFrame");
    ThreadChecker::Scope checkScope(&hmds->RenderAPIThreadChecker, "ovrHmd_EndFrame");  
    
    hmds->pRenderer->SetLatencyTestColor(hmds->LatencyTestActive ? hmds->LatencyTestDrawColor : nullptr);

    ovrHmd_GetLatencyTest2DrawColor(hmddesc, nullptr); // We don't actually need to draw color, so send nullptr
    
    if (hmds->pRenderer)
    {
        hmds->pRenderer->SaveGraphicsState();

        // See if we need to show the HSWDisplay.
        if (hmds->pHSWDisplay) // Until we know that these are valid, assume any of them can't be.
        {
            ovrHSWDisplayState hswDisplayState;
            hmds->pHSWDisplay->TickState(&hswDisplayState, true);  // This may internally call HASWarning::Display.

            if (hswDisplayState.Displayed)
            {
                hmds->pHSWDisplay->Render(ovrEye_Left, &eyeTexture[ovrEye_Left]);
                hmds->pHSWDisplay->Render(ovrEye_Right, &eyeTexture[ovrEye_Right]);
            }
        }

        if (posTimewarpDesc)
            hmds->pRenderer->SetPositionTimewarpDesc(*posTimewarpDesc);

        hmds->pRenderer->EndFrame(hmds->BeginFrameIndex, true);
        hmds->pRenderer->RestoreGraphicsState();
    }

    // Call after present
    ovrHmd_EndFrameTiming(hmddesc);

    TraceReturn(hmds->BeginFrameIndex);

    // Out of BeginFrame
    hmds->BeginFrameThreadId = 0;
    hmds->BeginFrameCalled   = false;
    hmds->BeginFrameIndex++; // Set frame index to the next value in case 0 is passed.
}

// Not exposed as part of public API
OVR_PUBLIC_FUNCTION(void) ovrHmd_RegisterPostDistortionCallback(ovrHmd hmddesc, PostDistortionCallback callback)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds || !hmds->pRenderer)
        return;

    hmds->pRenderer->RegisterPostDistortionCallback(callback);
}



//-------------------------------------------------------------------------------------
// ***** Frame Timing logic

OVR_PUBLIC_FUNCTION(ovrFrameTiming) ovrHmd_GetFrameTiming(ovrHmd hmddesc, unsigned int frameIndex)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return GetNullFrameTiming();

    // If no frame index is provided,
    if (frameIndex == 0)
    {
        // Use the next one in the series.
        frameIndex = hmds->BeginFrameIndex;
    }

    return hmds->GetFrameTiming(frameIndex);
}

OVR_PUBLIC_FUNCTION(ovrFrameTiming) ovrHmd_BeginFrameTiming(ovrHmd hmddesc, unsigned int frameIndex)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return GetNullFrameTiming();

    // Check: Proper state for the call.    
    OVR_DEBUG_LOG_COND(hmds->BeginFrameTimingCalled,
                      ("ovrHmd_BeginFrameTiming called multiple times."));    
    hmds->BeginFrameTimingCalled = true;

    // If a frame index is specified,
    if (frameIndex != 0)
    {
        // Use the next one after the last BeginFrame() index.
        hmds->BeginFrameIndex = (uint32_t)frameIndex;
    }
    else
    {
        frameIndex = hmds->BeginFrameIndex;
    }

    // Update latency tester once per frame
    hmds->LatencyTestActive = hmds->ProcessLatencyTest(hmds->LatencyTestDrawColor);

    if (!hmds->BeginFrameCalled)
    {
        hmds->TimewarpTimer.CalculateTimewarpTiming(frameIndex);
    }

    return hmds->GetFrameTiming(frameIndex);
}

OVR_PUBLIC_FUNCTION(void) ovrHmd_EndFrameTiming(ovrHmd hmddesc)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return;

    // Debug state checks: Must be in BeginFrameTiming, on the same thread.
    hmds->checkBeginFrameTimingScope("ovrHmd_EndTiming");
   // MA TBD: Correct check or not?
   // ThreadChecker::Scope checkScope(&hmds->RenderAPIThreadChecker, "ovrHmd_EndFrame");

    hmds->BeginFrameTimingCalled = false;

    hmds->endFrameRenderTiming();
}

OVR_PUBLIC_FUNCTION(void) ovrHmd_ResetFrameTiming(ovrHmd hmddesc, unsigned int frameIndex)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return;

    OVR_UNUSED(frameIndex);

    // Clear timing-related state.
    hmds->BeginFrameIndex = frameIndex;
    hmds->TimewarpTimer.Reset();
}

OVR_PUBLIC_FUNCTION(void) ovrHmd_GetEyePoses(ovrHmd hmddesc, unsigned int frameIndex, const ovrVector3f hmdToEyeViewOffset[2],
                                             ovrPosef outEyePoses[2], ovrTrackingState* outHmdTrackingState)
{
    if (!hmdToEyeViewOffset || !outEyePoses)
    {
        OVR_ASSERT(false);
        return;
    }

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
    {
        outEyePoses[0] = ovrPosef(); outEyePoses[0].Orientation.w = 1;
        outEyePoses[1] = ovrPosef(); outEyePoses[1].Orientation.w = 1;
        if (outHmdTrackingState)
            *outHmdTrackingState = GetNullTrackingState();
        return;
    }

    // If no frame index is provided,
    if (frameIndex == 0)
    {
        // Use the next one in the series.
        frameIndex = hmds->BeginFrameIndex;
    }

    ovrTrackingState hmdTrackingState = hmds->GetMidpointPredictionTracking(frameIndex);
    TraceTrackingState(hmdTrackingState);
    ovrPosef hmdPose = hmdTrackingState.HeadPose.ThePose;

    // caller passed in a valid pointer, so copy to output
    if (outHmdTrackingState)
       *outHmdTrackingState = hmdTrackingState;

    // Currently HmdToEyeViewOffset is only a 3D vector
    // (Negate HmdToEyeViewOffset because offset is a view matrix offset and not a camera offset)
    outEyePoses[0] = Posef(hmdPose.Orientation, ((Posef)hmdPose).Apply(-((Vector3f)hmdToEyeViewOffset[0])));
    outEyePoses[1] = Posef(hmdPose.Orientation, ((Posef)hmdPose).Apply(-((Vector3f)hmdToEyeViewOffset[1])));
}

OVR_PUBLIC_FUNCTION(ovrPosef) ovrHmd_GetHmdPosePerEye(ovrHmd hmddesc, ovrEyeType eye)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
    {
        ovrPosef nullPose = ovrPosef();
        nullPose.Orientation.w = 1.0f; // Return a proper quaternion.
        return nullPose;
    }

    hmds->checkBeginFrameTimingScope("ovrHmd_GetEyePose");

    return hmds->GetEyePredictionPose(eye);
}

OVR_PUBLIC_FUNCTION(void) ovrHmd_AddDistortionTimeMeasurement(ovrHmd hmddesc, double distortionTimeSeconds)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return;

    hmds->checkBeginFrameTimingScope("ovrHmd_GetTimewarpEyeMatrices");   

    hmds->TimewarpTimer.AddDistortionTimeMeasurement(distortionTimeSeconds);
}

OVR_PUBLIC_FUNCTION(void) ovrHmd_GetEyeTimewarpMatricesDebug(ovrHmd hmddesc, ovrEyeType eye, ovrPosef renderPose,
                                                             ovrQuatf playerTorsoMotion, ovrMatrix4f twmOut[2],
                                                             double debugTimingOffsetInSeconds)
{
    if (!twmOut)
    {
        OVR_ASSERT(false);
        return;
    }

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return;

    // Debug checks: BeginFrame was called, on the same thread.
    hmds->checkBeginFrameTimingScope("ovrHmd_GetTimewarpEyeMatrices");   

    // TODO: Position input disabled for now
    bool calcPosition = false;
    ovrVector3f* hmdToEyeViewOffset = nullptr;

    // playerTorsoMotion can be fed in by the app to indicate player rotation,
    // i.e. renderPose is in torso space, and playerTorsoMotion says that torso space changed.
    Quatf playerTorsoMotionInv = Quatf(playerTorsoMotion).Inverted();
    Posef renderPoseTorso = (Posef)renderPose * Posef(playerTorsoMotionInv, Vector3f::Zero());
    hmds->GetTimewarpMatricesEx(eye, renderPoseTorso, calcPosition, hmdToEyeViewOffset, twmOut,
                                debugTimingOffsetInSeconds);
}


OVR_PUBLIC_FUNCTION(void) ovrHmd_GetEyeTimewarpMatrices(ovrHmd hmddesc, ovrEyeType eye, ovrPosef renderPose, ovrMatrix4f twmOut[2])
{
    if (!twmOut)
    {
        OVR_ASSERT(false);
        return;
    }

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return;

    // Shortcut to doing orientation-only timewarp.

    hmds->GetTimewarpMatrices(eye, renderPose, twmOut);
}


OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc) ovrHmd_GetRenderDesc(ovrHmd hmddesc,
                                                           ovrEyeType eyeType, ovrFovPort fov)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return ovrEyeRenderDesc();

    return hmds->RenderState.CalcRenderDesc(eyeType, fov);
}


#define OVR_OFFSET_OF(s, field) ((size_t)&((s*)0)->field)


OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_CreateDistortionMeshDebug(ovrHmd hmddesc,
                                                              ovrEyeType eyeType, ovrFovPort fov,
                                                              unsigned int distortionCaps,
                                                              ovrDistortionMesh *meshData,
												              float debugEyeReliefOverrideInMetres)
{
    if (!meshData)
    {
        OVR_ASSERT(false);
        return ovrFalse;
    }

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return ovrFalse;

    return hmds->CreateDistortionMesh(eyeType, fov,
                                      distortionCaps,
                                      meshData,
                                      debugEyeReliefOverrideInMetres)
           ? ovrTrue : ovrFalse;
}


OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_CreateDistortionMesh(ovrHmd hmddesc,
                                                         ovrEyeType eyeType, ovrFovPort fov,
                                                         unsigned int distortionCaps,
                                                         ovrDistortionMesh *meshData)
{
    if (!meshData)
    {
        OVR_ASSERT(false);
        return ovrFalse;
    }

    return ovrHmd_CreateDistortionMeshDebug(hmddesc, eyeType, fov, distortionCaps, meshData, 0);
}



// Frees distortion mesh allocated by ovrHmd_GenerateDistortionMesh. meshData elements
// are set to null and 0s after the call.
OVR_PUBLIC_FUNCTION(void) ovrHmd_DestroyDistortionMesh(ovrDistortionMesh* meshData)
{
    if (!meshData)
    {
        OVR_ASSERT(false);
        return;
    }

    if (meshData->pVertexData)
        DistortionMeshDestroy((DistortionMeshVertexData*)meshData->pVertexData,
                              meshData->pIndexData);
    meshData->pVertexData = 0;
    meshData->pIndexData  = 0;
    meshData->VertexCount = 0;
    meshData->IndexCount  = 0;
}



// Computes updated 'uvScaleOffsetOut' to be used with a distortion if render target size or
// viewport changes after the fact. This can be used to adjust render size every frame, if desired.
OVR_PUBLIC_FUNCTION(void) ovrHmd_GetRenderScaleAndOffset(ovrFovPort fov,
                                                         ovrSizei textureSize, ovrRecti renderViewport,
                                                         ovrVector2f uvScaleOffsetOut[2] )
{        
    if (!uvScaleOffsetOut)
    {
        OVR_ASSERT(false);
        return;
    }

    // Find the mapping from TanAngle space to target NDC space.
    ScaleAndOffset2D  eyeToSourceNDC = CreateNDCScaleAndOffsetFromFov(fov);
    // Find the mapping from TanAngle space to textureUV space.
    ScaleAndOffset2D  eyeToSourceUV  = CreateUVScaleAndOffsetfromNDCScaleandOffset(
                                         eyeToSourceNDC,
                                         renderViewport, textureSize );

    if (uvScaleOffsetOut)
    {
        uvScaleOffsetOut[0] = eyeToSourceUV.Scale;
        uvScaleOffsetOut[1] = eyeToSourceUV.Offset;
    }
}


//-------------------------------------------------------------------------------------
// ***** Latency Test interface

OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_GetLatencyTestDrawColor(ovrHmd hmddesc, unsigned char rgbColorOut[3])
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return ovrFalse;

    if (rgbColorOut)
    {
        rgbColorOut[0] = hmds->LatencyTestDrawColor[0];
        rgbColorOut[1] = hmds->LatencyTestDrawColor[1];
        rgbColorOut[2] = hmds->LatencyTestDrawColor[2];
    }

    return hmds->LatencyTestActive;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_ProcessLatencyTest(ovrHmd hmddesc, unsigned char rgbColorOut[3])
{
    OVR_UNUSED(hmddesc);

    if (!rgbColorOut)
    {
        OVR_ASSERT(false);
        return ovrFalse;
    }

    return NetClient::GetInstance()->LatencyUtil_ProcessInputs(Timer::GetSeconds(), rgbColorOut);
}

OVR_PUBLIC_FUNCTION(const char*) ovrHmd_GetLatencyTestResult(ovrHmd hmddesc)
{
    OVR_UNUSED(hmddesc);

    return NetClient::GetInstance()->LatencyUtil_GetResultsString();
}

OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_GetLatencyTest2DrawColor(ovrHmd hmddesc, unsigned char rgbColorOut[3])
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return ovrFalse;

    // TBD: Move directly into renderer
    bool dk2LatencyTest = (hmds->EnabledHmdCaps & ovrHmdCap_DynamicPrediction) != 0;
    if (dk2LatencyTest)
    {
        hmds->LatencyTest2DrawColor[0] = hmds->ScreenLatencyTracker.GetNextDrawColor();
        hmds->LatencyTest2DrawColor[1] = hmds->ScreenLatencyTracker.IsLatencyTimingAvailable() ? 255 : 0;
        hmds->LatencyTest2DrawColor[2] = hmds->ScreenLatencyTracker.IsLatencyTimingAvailable() ? 0 : 255;

        if (rgbColorOut)
        {
            rgbColorOut[0] = hmds->LatencyTest2DrawColor[0];
            rgbColorOut[1] = hmds->LatencyTest2DrawColor[1];
            rgbColorOut[2] = hmds->LatencyTest2DrawColor[2];
        }

        if (hmds->pRenderer)
            hmds->pRenderer->SetLatencyTest2Color(hmds->LatencyTest2DrawColor);
    }
    else
    {
        if (hmds->pRenderer)
            hmds->pRenderer->SetLatencyTest2Color(nullptr);
    }

    return dk2LatencyTest ? ovrTrue : ovrFalse;
}


OVR_PUBLIC_FUNCTION(double) ovrHmd_GetMeasuredLatencyTest2(ovrHmd hmddesc)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return 0.;

    double latencyPostPresent = 0.;
    hmds->ScreenLatencyTracker.GetVsyncToScanout(latencyPostPresent);
    return latencyPostPresent;
}


//-------------------------------------------------------------------------------------
// ***** Health and Safety Warning Display interface
//

OVR_PUBLIC_FUNCTION(void) ovrHmd_GetHSWDisplayState(ovrHmd hmddesc, ovrHSWDisplayState *hswDisplayState)
{
    if (!hswDisplayState)
    {
        OVR_ASSERT(false);
        return;
    }

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return;

    OVR::CAPI::HSWDisplay* pHSWDisplay = hmds->pHSWDisplay;

    if (pHSWDisplay)
        pHSWDisplay->TickState(hswDisplayState); // This may internally call HSWDisplay::Display.
}

OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_DismissHSWDisplay(ovrHmd hmddesc)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return ovrFalse;

    OVR::CAPI::HSWDisplay* pHSWDisplay = hmds->pHSWDisplay;

    if (pHSWDisplay && pHSWDisplay->Dismiss())
        return ovrTrue;

    return ovrFalse;
}


// -----------------------------------------------------------------------------------
// ***** Property Access

OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_GetBool(ovrHmd hmddesc,
                                            const char* propertyName,
                                            ovrBool defaultVal)
{
    OVR_ASSERT(propertyName != nullptr);
    if (!propertyName)
        return ovrFalse;

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    bool success = false;
    if (hmds)
    {
        success = hmds->getBoolValue(propertyName, (defaultVal != ovrFalse));
    }
    else
    {
        success = NetClient::GetInstance()->GetBoolValue(InvalidVirtualHmdId, propertyName, (defaultVal != ovrFalse));
    }
    return success ? ovrTrue : ovrFalse;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetBool(ovrHmd hmddesc,
                                            const char* propertyName,
                                            ovrBool value)
{
    OVR_ASSERT(propertyName != nullptr);
    if (!propertyName)
        return ovrFalse;

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    bool retval = false;
    if (hmds)
    {
        retval = hmds->setBoolValue(propertyName, value != ovrFalse);
    }
    else
    {
        retval = NetClient::GetInstance()->SetBoolValue(InvalidVirtualHmdId, propertyName, (value != ovrFalse));
    }
    return retval ? ovrTrue : ovrFalse;
}

OVR_PUBLIC_FUNCTION(int) ovrHmd_GetInt(ovrHmd hmddesc,
                                       const char* propertyName,
                                       int defaultVal)
{
    OVR_ASSERT(propertyName != nullptr);
    if (!propertyName)
        return ovrFalse;

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (hmds)
    {
        return hmds->getIntValue(propertyName, defaultVal);
    }
    else
    {
        return NetClient::GetInstance()->GetIntValue(InvalidVirtualHmdId, propertyName, defaultVal);
    }
}

OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetInt(ovrHmd hmddesc,
                                           const char* propertyName,
                                           int value)
{
    OVR_ASSERT(propertyName != nullptr);
    if (!propertyName)
        return ovrFalse;

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    bool success = false;
    if (hmds)
    {
        success = hmds->setIntValue(propertyName, value);
    }
    else
    {
        success = NetClient::GetInstance()->SetIntValue(InvalidVirtualHmdId, propertyName, value);
    }
    return success ? ovrTrue : ovrFalse;
}

OVR_PUBLIC_FUNCTION(float) ovrHmd_GetFloat(ovrHmd hmddesc,
                                           const char* propertyName,
                                           float defaultVal)
{
    OVR_ASSERT(propertyName != nullptr);
    if (!propertyName)
        return ovrFalse;

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (hmds)
    {
        return hmds->getFloatValue(propertyName, defaultVal);
    }
    else
    {
        return (float)NetClient::GetInstance()->GetNumberValue(InvalidVirtualHmdId, propertyName, defaultVal);
    }
}

OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetFloat(ovrHmd hmddesc,
                                             const char* propertyName,
                                             float value)
{
    OVR_ASSERT(propertyName != nullptr);
    if (!propertyName)
        return ovrFalse;

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    bool success = false;
    if (hmds)
    {
        success = hmds->setFloatValue(propertyName, value);
    }
    else
    {
        success = NetClient::GetInstance()->SetNumberValue(InvalidVirtualHmdId, propertyName, value);
    }
    return success ? ovrTrue : ovrFalse;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovrHmd_GetFloatArray(ovrHmd hmddesc,
                                                       const char* propertyName,
                                                       float values[],
                                                       unsigned int arraySize)
{
    OVR_ASSERT(propertyName != nullptr && values != nullptr);
    if (!propertyName || !values)
        return 0;

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds) return 0;

    return hmds->getFloatArray(propertyName, values, arraySize);
}

// Modify float[] property; false if property doesn't exist or is readonly.
OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetFloatArray(ovrHmd hmddesc,
                                                  const char* propertyName,
                                                  float values[],
                                                  unsigned int arraySize)
{
    OVR_ASSERT(propertyName != nullptr && values != nullptr);
    if (!propertyName || !values)
        return ovrFalse;

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds) return ovrFalse;

    return hmds->setFloatArray(propertyName, values, arraySize) ? ovrTrue : ovrFalse;
}

OVR_PUBLIC_FUNCTION(const char*) ovrHmd_GetString(ovrHmd hmddesc,
                                                  const char* propertyName,
                                                  const char* defaultVal)
{
    OVR_ASSERT(propertyName != nullptr);
    if (!propertyName)
        return "";

    // Replace null default with empty string
    if (!defaultVal)
        defaultVal = "";

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (hmds)
    {
        return hmds->getString(propertyName, defaultVal);
    }
    else
    {
        return NetClient::GetInstance()->GetStringValue(InvalidVirtualHmdId, propertyName, defaultVal);
    }
}

OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_SetString(ovrHmd hmddesc,
                                              const char* propertyName,
                                              const char* value)
{
    OVR_ASSERT(propertyName != nullptr);
    if (!propertyName)
        return ovrFalse;

    // Replace null value with empty string
    if (!value)
        value = "";

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    bool success = false;
    if (hmds)
    {
        success = hmds->setString(propertyName, value);
    }
    else
    {
        success = NetClient::GetInstance()->SetStringValue(InvalidVirtualHmdId, propertyName, value);
    }
    return success ? ovrTrue : ovrFalse;
}


// -----------------------------------------------------------------------------------
// ***** Logging

// make sure OVR_Log.h's enum matches CAPI's
static_assert(
    ((ovrLogLevel_Debug == ovrLogLevel(LogLevel_Debug)) &&
    (ovrLogLevel_Info   == ovrLogLevel(LogLevel_Info)) &&
    (ovrLogLevel_Error  == ovrLogLevel(LogLevel_Error))),
    "mismatched LogLevel enums"
);

#define OVR_TRACEMSG_MAX_LEN 1024 /* in chars */

OVR_PUBLIC_FUNCTION(int) ovr_TraceMessage(int level, const char* message)
{
    OVR_ASSERT(message != nullptr);
    if (!message)
        return -1;

    // Keep traced messages to some reasonable maximum length
    int len = (int)strnlen(message, OVR_TRACEMSG_MAX_LEN);
    if (len >= OVR_TRACEMSG_MAX_LEN)
        return -1;

    switch (level)
    {
    case ovrLogLevel_Debug:
        TraceLogDebug(message);
        break;
    case ovrLogLevel_Info:
    default:
        TraceLogInfo(message);
        break;
    case ovrLogLevel_Error:
        TraceLogError(message);
        break;
    }

    return len;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_StartPerfLog(ovrHmd hmddesc, const char* fileName, const char* userData1)
{
    // DEPRECATED
    OVR_UNUSED3(hmddesc, fileName, userData1);
    return ovrFalse;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovrHmd_StopPerfLog(ovrHmd hmddesc)
{
    // DEPRECATED
    OVR_UNUSED(hmddesc);
    return ovrFalse;
}
