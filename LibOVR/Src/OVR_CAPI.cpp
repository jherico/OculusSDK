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
#include "OVR_Error.h"

#include "CAPI/CAPI_HMDState.h"

#include "Net/OVR_Session.h"
#include "Service/Service_NetClient.h"
#ifdef OVR_PRIVATE_FILE
#include "Service/Service_NetServer.h"
#include "OVR_PadCheck.h"
#endif

#if defined(OVR_OS_WIN32)
#include <VersionHelpers.h>
#include "CAPI/D3D1X/CAPI_D3D11_CliCompositorClient.h"

#elif defined(OVR_OS_LINUX)
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#endif

// New overrides to route std::vector and all other allocations through our allocator.
#if defined(OVR_BUILD_DEBUG) && !defined(OVR_STATIC_BUILD)
#include <Kernel/OVR_NewOverride.inl>
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
    nullTiming.FrameIntervalSeconds = 0.013f; // Provide nominal value
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
    return ovrTrue;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_InitializeRenderingShimVersion(int requestedMinorVersion)
{
    // We ignore the patch and build versions here, as they aren't relevant to compatibility.
    // And we don't store them away here, as we do that in ovr_Initialize() instead.
    
    if (requestedMinorVersion > OVR_MINOR_VERSION)
        return ovrFalse;

    return ovrTrue;
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
    0,                      // ConnectionTimeoutSeconds
    OVR_ON64("")            // pad0
};


#if !defined(OVR_OS_LINUX)
static ovrBool ConnectToService()
{
    ovrResult errorCode = CAPI_pNetClient->Connect(true);
    return (errorCode == ovrSuccess);
}

#else // OVR_OS_LINUX
static ovrBool ConnectToService()
{
    ovrResult errorCode = CAPI_pNetClient->Connect(true);
    if (errorCode != ovrSuccess)
    {
        // If that fails, maybe the daemon is just not running.
        // So, try to start the daemon ourselves.
        const char *fifo_name = "/var/tmp/ovrd_start";
        const char *cmd_fmt = "ovrd --daemonize --fifo=%s";
        char command[strlen(fifo_name) + strlen(cmd_fmt)];

        if (mkfifo(fifo_name, 0600) < 0)
        {
            // Another ovrd is running, abort with original error
            return ovrFalse;
        }

        sprintf(command, cmd_fmt, fifo_name);
        int ret = system(command);
        if (ret == -1)
        {
            // Failure in system() itself
            return ovrFalse;
        }

        int fifo_fd = open(fifo_name, O_RDONLY | O_NONBLOCK);
        if (fifo_fd < 0)
        {
            // Another ovrd is running, abort with original error
            return ovrFalse;
        }

        // Wait for the service to signal it's ready
        pollfd wait_on = pollfd();
        wait_on.fd = fifo_fd;
        wait_on.events = POLLIN;
        poll(&wait_on, 1, 10000);

        close(fifo_fd);
        unlink(fifo_name);

        // Now try connecting to the service again
        errorCode = CAPI_pNetClient->Connect(true);
        return (errorCode == ovrSuccess);
    }
    return ovrTrue;
}
#endif // OVR_OS_LINUX

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Initialize(ovrInitParams const* params)
{
    if (CAPI_ovrInitializeCalled) // If already initialized...
    {
        if(params && (OVR::Net::RuntimeSDKVersion.RequestedMinorVersion != params->RequestedMinorVersion)) // If the caller is requesting a different version...
        {
            OVR_MAKE_ERROR_F(ovrError_Reinitialization, "Cannot reinitialize LibOVRRT with a different version. Newly requested major.minor version: %d.%d; Current version: %d.%d", 
                             OVR_MAJOR_VERSION, (int)params->RequestedMinorVersion, OVR_MAJOR_VERSION, OVR::Net::RuntimeSDKVersion.RequestedMinorVersion);
            return ovrError_Reinitialization;
        }

        return ovrSuccess;
    }

    OVRError  error;
    ovrResult errorCode;

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

    // We must set up the system for the plugin to work
    if (!OVR::System::IsInitialized())
    {
        // TBD: Base this on registry setting?
        Allocator::SetLeakTracking(DebugMode);

        OVR::Log* logger = OVR::Log::ConfigureDefaultLog(OVR::LogMask_All);
        OVR_ASSERT(logger != nullptr);

        // Set the CAPI logger callback
        logger->SetCAPICallback(params->LogCallback);

#if defined(OVR_BUILD_DEBUG)
        DebugPageAllocator* debugAlloc = DebugPageAllocator::InitSystemSingleton();
        debugAlloc->EnableOverrunDetection(true, true);
        OVR::System::Init(logger, debugAlloc);
#else
        OVR::System::Init(logger);
#endif
    }

    // We ignore the requested patch version and build version, as they are not currently relevant to
    // the library compatibility. Our test for minor version compatibility is currently simple: we support
    // only older or equal minor versions, and don't change our behavior if the requested minor version
    // is older than than OVR_MINOR_VERSION.

    if ((params->Flags & ovrInit_RequestVersion) != 0 &&
        (params->RequestedMinorVersion > OVR_MINOR_VERSION))
    {
        error = OVR_MAKE_ERROR_F(ovrError_LibVersion, "Insufficient LibOVRRT version. Requested major.minor version: %d.%d; LibOVRRT version: %d.%d", 
                                    OVR_MAJOR_VERSION, (int)params->RequestedMinorVersion, OVR_MAJOR_VERSION, OVR_MINOR_VERSION);
        goto Abort;
    }

#if defined(OVR_OS_WIN32)
    // Older than Windows 7 SP1? Question: Why is SP1 required? Can we document that here?
    if (!IsWindows7SP1OrGreater())
    {
        error = OVR_MAKE_ERROR(ovrError_IncompatibleOS, "Windows 7 Service Pack 1 or a later operating system version is required.");
        goto Abort;
    }
#endif // OVR_OS_WIN32

    OVR::Net::RuntimeSDKVersion.SetCurrent();  // Fill in the constant parts of this struct.
    OVR::Net::RuntimeSDKVersion.RequestedMinorVersion = (uint16_t)params->RequestedMinorVersion;

    CAPI_pNetClient = NetClient::GetInstance();

    // Store off the initialization parameters from ovr_Initialize()
    CAPI_pNetClient->ApplyParameters(params);

    // Log the location of the module after most of the bring-up, as the game
    // could do almost anything in response to a log message callback.
    LogLocationOfThisModule();

    // If unable to connect to server and we are not in a debug mode or the server is optional then fail.
    if (!ConnectToService() && !DebugMode && ((params->Flags & ovrInit_ServerOptional) == 0))
    {
        // Then it's a failure when the server is unreachable.
        // This means that a DebugHMD cannot be created unless the ovrInit_Debug flag is set.
        // No need to call OVR_MAKE_ERROR because Connect would already have done that before returning the error code.
        goto Abort;
    }

    CAPI_ovrInitializeCalled = ovrTrue;


    // everything is okay
    TraceReturn(0);
    return ovrSuccess;

Abort:
    OVR_ASSERT(error.GetCode() != ovrSuccess);
    errorCode = error.GetCode();

    // We undo anything we may have done above.
    OVR::Log* logger = OVR::Log::GetDefaultLog();
    if(logger)
        logger->SetCAPICallback(nullptr);

    // clean up and return failure
    if (OVR::System::IsInitialized())
    {
        error.Reset();  // Normally we don't need to explicitly call OVRError::Reset, but we are about to shut down the heap.
        OVR::System::Destroy();
    }

    TraceReturn(0);
    TraceFini();
    return errorCode;
}


OVR_PUBLIC_FUNCTION(void) ovr_Shutdown()
{  
    if (CAPI_ovrInitializeCalled)
    {
        TraceCall(0);
        TraceFini();
        CAPI_ovrInitializeCalled = ovrFalse;
    }

    if (OVR::System::IsInitialized())
    {
        OVR::System::Destroy();
    }

    OVR::Net::RuntimeSDKVersion.Reset();
    CAPI_pNetClient = nullptr;  // Not strictly necessary, but useful for debugging and cleanliness.
}


OVR_PUBLIC_FUNCTION(void) ovr_GetLastErrorInfo(ovrErrorInfo* errorInfo)
{
    if (errorInfo)
    {
        const OVRError& error = LastErrorTLS::GetInstance()->LastError();

        errorInfo->Result = error.GetCode();

        static_assert(OVR_ARRAY_COUNT(errorInfo->ErrorString) == 512, "If this value changes between public releases then we need to dynamically handle older versions.");
        OVR_strlcpy(errorInfo->ErrorString, error.GetDescription().ToCStr(), OVR_ARRAY_COUNT(errorInfo->ErrorString));
    }
}


// There is a thread safety issue with ovrHmd_Detect in that multiple calls from different
// threads can corrupt the global array state. This would lead to two problems:
//  a) Create(index) enumerator may miss or overshoot items. Probably not a big deal
//     as game logic can easily be written to only do Detect(s)/Creates in one place.
//     The alternative would be to return list handle.
//  b) TBD: Un-mutexed Detect access from two threads could lead to crash. We should
//         probably check this.
//

OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_Detect()
{
    if (!CAPI_ovrInitializeCalled)
        return ovrError_ServiceConnection;

    return CAPI_pNetClient->Hmd_Detect();
}


// ovrHmd_Create us explicitly separated from ConfigureTracking and ConfigureRendering to allow creation of 
// a relatively light-weight handle that would reference the device going forward and would 
// survive future ovrHmd_Detect calls. That is once ovrHMD is returned, index is no longer
// necessary and can be changed by a ovrHmd_Detect call.
OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_Create(int index, ovrHmd* pHmd)
{
    OVRError ovrError;

    OVR_ASSERT(pHmd);
    *pHmd = nullptr;

    if (!CAPI_ovrInitializeCalled)
    {
        ovrError = OVR_MAKE_ERROR(ovrError_NotInitialized, "ovrHmd_Create: CAPI is not initialized.");
        return ovrError.GetCode();
    }

    double t0 = Timer::GetSeconds();
    HMDNetworkInfo netInfo;

    // There may be some delay before the HMD is fully detected.
    // Since we are also trying to create the HMD immediately it may lose this race and
    // get "NO HMD DETECTED."  Wait a bit longer to avoid this.
    while (!CAPI_pNetClient->Hmd_Create(index, &netInfo, &ovrError))
    {
        double waitTime = 2.0; // Default wait time

        if (NetClient::GetInstance()->IsConnected(false, false))
        {
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
            // Wait the default amount of time for the service to start up.
        }

        // If still no HMD detected,
        if (Timer::GetSeconds() - t0 > waitTime)
        {
            LogError("ovrHmd_Create failed to complete within the timeout period.");
            // This should not set error because this asserts, and ovrHmd_Create(0) should not assert in debug mode in this common case.
            //OVR_SET_ERROR(ovrError); // We are not making a new error, but simply using the one returned by Hmd_Create. We may want to consider appending a ovrError_Timeout error to this underlying OVRError.
            return ovrError.GetCode();
        }
    }

    OVR_ASSERT(netInfo.NetId != InvalidVirtualHmdId);

    // Create HMD State object
    HMDState* hmds = HMDState::CreateHMDState(CAPI_pNetClient, netInfo);
    if (!hmds)
    {
        CAPI_pNetClient->Hmd_Release(netInfo.NetId);

        ovrError = OVR_MAKE_ERROR(ovrError_InvalidHmd, "ovrHmd_Create: CreateHMDState failed."); // We may need a better error code for this.
        return ovrError.GetCode();
    }

    // Reset frame timing so that FrameTimeManager values are properly initialized in AppRendered mode.
    // TBD: No longer needed? -cat
    ovrHmd_ResetFrameTiming(hmds->pHmdDesc, 0);

    TraceHmdDesc(*hmds->pHmdDesc);
    *pHmd = hmds->pHmdDesc;

    return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_CreateDebug(ovrHmdType type, ovrHmd* pHmd)
{
    OVRError ovrError;
    OVR_ASSERT(pHmd);

    if (!pHmd)
    {
        ovrError = OVR_MAKE_ERROR(ovrError_InvalidParameter, "ovrHmd_CreateDebug: Null ovrHmd parameter.");
        return ovrError.GetCode();
    }

    *pHmd = nullptr;

    if (!CAPI_ovrInitializeCalled)
    {
        ovrError = OVR_MAKE_ERROR(ovrError_NotInitialized, "ovrHmd_CreateDebug: CAPI is not initialized.");
        return ovrError.GetCode();
    }

    HmdTypeEnum t = HmdType_None;
    switch (type)
    {
    case ovrHmd_DK1:    t = HmdType_DK1;    break;
    case ovrHmd_DK2:    t = HmdType_DK2;    break;
    default: OVR_ASSERT(false); return ovrError_InvalidParameter;
    }

    if (!CAPI_pNetClient->Hmd_CreateDebug(t, &ovrError))
    {
        LogError("ovrHmd_CreateDebug failed.");
        OVR_SET_ERROR(ovrError); // We are not making a new error, but simply using the one returned by Hmd_CreateDebug. We may want to consider appending a ovrError_Timeout error to this underlying OVRError.
        return ovrError.GetCode();
    }

    HMDState* hmds = HMDState::CreateDebugHMDState(CAPI_pNetClient, type);
    if (!hmds)
    {
        ovrError = OVR_MAKE_ERROR(ovrError_InvalidHmd, "ovrHmd_CreateDebug: CreateDebugHMDState failed."); // We may need a better error code for this.
        return ovrError.GetCode();
    }

    *pHmd = hmds->pHmdDesc;

    return ovrSuccess;
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

    delete (HMDState*)hmddesc->Handle;
}


// Returns version string representing libOVR version. 
// Valid for the lifetime of LibOVRRT within the process.
OVR_PUBLIC_FUNCTION(const char*) ovr_GetVersionString()
{
    return OVR_VERSION_STRING;
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
//
//  - Sensor interface functions are all Thread-safe, unlike the frame/render API
//    functions that have different rules (all frame access functions
//    must be on render thread)

OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_ConfigureTracking(ovrHmd hmddesc, unsigned int supportedCaps,
                                                      unsigned int requiredCaps)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return ovrError_InvalidHmd;

    return hmds->ConfigureTracking(supportedCaps, requiredCaps);
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
// *** SwapTextureSets

#if defined (OVR_OS_MS)

OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_CreateSwapTextureSetD3D11(ovrHmd hmd,
                                                                ID3D11Device* device,
                                                                const D3D11_TEXTURE2D_DESC* desc,
                                                                ovrSwapTextureSet** outTextureSet)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmd);
    if (!hmds)
    {
        OVR_MAKE_ERROR(ovrError_InvalidHmd, L"Invalid HMD object provided");
        return ovrError_InvalidHmd;
    }

    if (outTextureSet == nullptr)
    {
        OVR_MAKE_ERROR(ovrError_InvalidParameter, L"Null textureSet pointer provided.");
        return ovrError_InvalidParameter;
    }

    *outTextureSet = nullptr;

    if (!hmds->pCompClient)
    {
        OVR_MAKE_ERROR(ovrError_ServiceConnection, L"Incomplete service connection.");
        return ovrError_ServiceConnection;
    }

    /// On Windows, we always use CliD3D11CompositorClient
    CliD3D11CompositorClient* d3d11Client = static_cast<CliD3D11CompositorClient*>(hmds->pCompClient.GetPtr());
    if (!d3d11Client)
    {
        return ovrError_NotInitialized;
    }
    return d3d11Client->CreateTextureSetD3D(device, desc, outTextureSet).GetCode();
}

OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_CreateMirrorTextureD3D11(ovrHmd hmd,
                                                               ID3D11Device* device,
                                                               const D3D11_TEXTURE2D_DESC* desc,
                                                               ovrTexture** outMirrorTexture)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmd);
    if (!hmds)
    {
        OVR_MAKE_ERROR(ovrError_InvalidHmd, L"Invalid HMD object provided");
        return ovrError_InvalidHmd;
    }

    if (outMirrorTexture == nullptr)
    {
        OVR_MAKE_ERROR(ovrError_InvalidParameter, L"Null mirror texture pointer provided.");
        return ovrError_InvalidParameter;
    }

    *outMirrorTexture = nullptr;

    if (!hmds->pCompClient)
    {
        OVR_MAKE_ERROR(ovrError_ServiceConnection, L"Incomplete service connection.");
        return ovrError_ServiceConnection;
    }

    /// On Windows, we always use CliD3D11CompositorClient
    CliD3D11CompositorClient* d3d11Client = static_cast<CliD3D11CompositorClient*>(hmds->pCompClient.GetPtr());
    if (!d3d11Client)
    {
        return ovrError_NotInitialized;
    }
    return d3d11Client->CreateMirrorTextureD3D(device, desc, outMirrorTexture).GetCode();
}

#endif // OVR_OS_MS

OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_CreateSwapTextureSetGL(ovrHmd hmd, GLuint format,
                                                             int width, int height,
                                                             ovrSwapTextureSet** outTextureSet)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmd);
    if (!hmds)
    {
        OVR_MAKE_ERROR(ovrError_InvalidHmd, L"Invalid HMD object provided");
        return ovrError_InvalidHmd;
    }

    if (outTextureSet == nullptr)
    {
        OVR_MAKE_ERROR(ovrError_InvalidParameter, L"Null textureSet pointer provided.");
        return ovrError_InvalidParameter;
    }

    *outTextureSet = nullptr;

    if (!hmds->pCompClient)
    {
        OVR_MAKE_ERROR(ovrError_ServiceConnection, L"Incomplete service connection.");
        return ovrError_ServiceConnection;
    }

#if defined (OVR_OS_MS)
    /// On Windows, we always use CliD3D11CompositorClient
    CliD3D11CompositorClient* d3d11Client = static_cast<CliD3D11CompositorClient*>(hmds->pCompClient.GetPtr());
    if (!d3d11Client)
    {
        return ovrError_NotInitialized;
    }
    return d3d11Client->CreateTextureSetGL(format, width, height, outTextureSet).GetCode();

#else

    return ovrError_IncompatibleOS;

#endif
}

OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_CreateMirrorTextureGL(ovrHmd hmd, GLuint format,
                                                            int width, int height,
                                                            ovrTexture** outMirrorTexture)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmd);
    if (!hmds)
    {
        OVR_MAKE_ERROR(ovrError_InvalidHmd, L"Invalid HMD object provided");
        return ovrError_InvalidHmd;
    }

    if (outMirrorTexture == nullptr)
    {
        OVR_MAKE_ERROR(ovrError_InvalidParameter, L"Null textureSet pointer provided.");
        return ovrError_InvalidParameter;
    }

    *outMirrorTexture = nullptr;

    if (!hmds->pCompClient)
    {
        OVR_MAKE_ERROR(ovrError_ServiceConnection, L"Incomplete service connection.");
        return ovrError_ServiceConnection;
    }

#if defined (OVR_OS_MS)
    /// On Windows, we always use CliD3D11CompositorClient
    CliD3D11CompositorClient* d3d11Client = static_cast<CliD3D11CompositorClient*>(hmds->pCompClient.GetPtr());
    if (!d3d11Client)
    {
        return ovrError_NotInitialized;
    }
    return d3d11Client->CreateMirrorTextureGL(format, width, height, outMirrorTexture).GetCode();

#else

    return ovrError_IncompatibleOS;

#endif
}

OVR_PUBLIC_FUNCTION(void) ovrHmd_DestroySwapTextureSet(ovrHmd hmd, ovrSwapTextureSet* textureSet)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmd);
    if (!hmds)
    {
        return;
    }

    if (hmds->pCompClient)
    {
        hmds->pCompClient->DestroyTextureSet(textureSet);
    }
}

OVR_PUBLIC_FUNCTION(void) ovrHmd_DestroyMirrorTexture(ovrHmd hmd, ovrTexture* mirrorTexture)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmd);
    if (!hmds)
    {
        return;
    }

    if (hmds->pCompClient)
    {
        hmds->pCompClient->DestroyMirrorTexture(mirrorTexture);
    }
}

//-------------------------------------------------------------------------------------
// *** Layers

/* We may re-enable this in the future:
OVR_PUBLIC_FUNCTION(int) ovrHmd_GetMaxNumLayers(ovrHmd hmddesc)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return -1;

    return hmds->GetMaxNumLayers();
}
*/

OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_SubmitFrame(ovrHmd hmddesc, unsigned int frameIndex, const ovrViewScaleDesc* viewScaleDesc,
                                                  ovrLayerHeader const * const * layers, unsigned int layerCount)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return ovrError_InvalidHmd;

    ThreadChecker::Scope checkScope(&hmds->RenderAPIThreadChecker, "ovrHmd_EndFrame");

    if (frameIndex == 0)
        frameIndex = hmds->AppFrameIndex;

    ovrViewScaleDesc viewScaleDescDefault;
    if (!viewScaleDesc)
    {
        // If the caller supplies a NULL viewScaleDesc then use defaults.
        viewScaleDescDefault.HmdToEyeViewOffset[0] = hmds->RenderState.EyeRenderDesc[0].HmdToEyeViewOffset;
        viewScaleDescDefault.HmdToEyeViewOffset[1] = hmds->RenderState.EyeRenderDesc[1].HmdToEyeViewOffset;
        viewScaleDescDefault.HmdSpaceToWorldScaleInMeters = 1.f;
        viewScaleDesc = &viewScaleDescDefault;
    }

    return hmds->SubmitFrame(frameIndex, viewScaleDesc, layers, layerCount);
}


//-------------------------------------------------------------------------------------
// ***** Frame Timing logic

OVR_PUBLIC_FUNCTION(ovrFrameTiming) ovrHmd_GetFrameTiming(ovrHmd hmddesc, unsigned int frameIndex)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return GetNullFrameTiming();

    // MA: Moved assignment of AppFrameIndex here from BeginFrameTiming to allow removal
    //     of that function. However, supporting index value of 0 is not good for threading
    //     as it introduces a race condition (assignment vs. use later in SubmitFrame)
    //     TBD: Remove FrameIndex 0 special case and add debug checks?
    
    if (frameIndex != 0)
    {
        // If a frame index is specified,
        // Use the next one after the last BeginFrame() index.
        hmds->AppFrameIndex = (uint32_t)frameIndex;
    }
    else
    {
        frameIndex = hmds->AppFrameIndex;
    }

    return hmds->GetFrameTiming(frameIndex);
}


OVR_PUBLIC_FUNCTION(void) ovrHmd_ResetFrameTiming(ovrHmd hmddesc, unsigned int frameIndex)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return;

    // Clear timing-related state.
    hmds->AppFrameIndex = frameIndex;    
}


OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc) ovrHmd_GetRenderDesc(ovrHmd hmddesc,
                                                           ovrEyeType eyeType, ovrFovPort fov)
{
    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds)
        return ovrEyeRenderDesc();

    return hmds->RenderState.CalcRenderDesc(eyeType, fov);
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
                                                  const float values[],
                                                  unsigned int arraySize)
{
    OVR_ASSERT(propertyName != nullptr && values != nullptr);
    if (!propertyName || !values)
        return ovrFalse;

    HMDState* hmds = GetHMDStateFromOvrHmd(hmddesc);
    if (!hmds) return ovrFalse;

    return hmds->setFloatArray(propertyName, const_cast<float*>(values), arraySize) ? ovrTrue : ovrFalse;
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

