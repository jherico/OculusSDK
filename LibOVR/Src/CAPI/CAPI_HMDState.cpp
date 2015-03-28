/************************************************************************************

Filename    :   CAPI_HMDState.cpp
Content     :   State associated with a single HMD
Created     :   January 24, 2014
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

#include "CAPI_HMDState.h"
#include "../OVR_Profile.h"
#include "../Service/Service_NetClient.h"

#ifdef OVR_OS_WIN32
    #include "../Displays/OVR_Win32_ShimFunctions.h"

    // For auto-detection of window handle for direct mode:
    #include <OVR_CAPI_D3D.h>
    #include <GL/CAPI_GLE.h>
    #include <OVR_CAPI_GL.h>

#elif defined(OVR_OS_LINUX)

    #include "../Displays/OVR_Linux_SDKWindow.h" // For screen rotation

#endif

#include "Tracing/Tracing.h"


namespace OVR { namespace CAPI {


// Accessed via HMDState::EnumerateHMDStateList()
static OVR::Lock hmdStateListLock;
static OVR::List<HMDState> hmdStateList; // List of all created HMDStates.


//-------------------------------------------------------------------------------------
// ***** HMDState

HMDState::HMDState(HMDInfo const & hmdInfo,
                   Profile* profile,
                   Service::HMDNetworkInfo const * netInfo,
                   Service::NetClient* client) :
    TimewarpTimer(),
    RenderTimer(),
    RenderIMUTimeSeconds(0.),
    pProfile(profile),
    pHmdDesc(0),
    pWindow(0),
    pClient(client),
    NetId(InvalidVirtualHmdId),
    NetInfo(),
    OurHMDInfo(hmdInfo),
    pLastError(nullptr),
    EnabledHmdCaps(0),
    EnabledServiceHmdCaps(0),
    CombinedHmdReader(),
    TheTrackingStateReader(),
    TheLatencyTestStateReader(),
    LatencyTestActive(false),
  //LatencyTestDrawColor(),
    LatencyTest2Active(false),
  //LatencyTest2DrawColor(),
    ScreenLatencyTracker(),
    RenderState(),
    pRenderer(),
    pHSWDisplay(),
  //LastGetStringValue(),
    RenderingConfigured(false),
    BeginFrameCalled(false),
    BeginFrameThreadId(),
    BeginFrameIndex(0),
    RenderAPIThreadChecker(),
    BeginFrameTimingCalled(false)
{
    if (netInfo)
    {
        NetId = netInfo->NetId;
        NetInfo = *netInfo;
    }

    // Hook up the app timing lockless updater
    RenderTimer.SetUpdater(TimewarpTimer.GetUpdater());

    // TBD: We should probably be looking up the default profile for the given
    // device type + user if profile == 0.    
    pLastError = 0;

    RenderState.OurHMDInfo = OurHMDInfo;

    UpdateRenderProfile(profile);

    OVR_ASSERT(!pHmdDesc);
    pHmdDesc         = (ovrHmdDesc*)OVR_ALLOC(sizeof(ovrHmdDesc));
    *pHmdDesc        = RenderState.GetDesc();
    pHmdDesc->Handle = this;

    RenderState.ClearColor[0] = 0.0f;
    RenderState.ClearColor[1] = 0.0f;
    RenderState.ClearColor[2] = 0.0f;
    RenderState.ClearColor[3] = 0.0f;
    RenderState.EnabledHmdCaps = 0;

    if (!TimewarpTimer.Initialize(&RenderState, &ScreenLatencyTracker))
    {
        OVR_ASSERT(false);
    }

    /*
    LatencyTestDrawColor[0] = 0;
    LatencyTestDrawColor[1] = 0;
    LatencyTestDrawColor[2] = 0;
    */

    RenderingConfigured    = false;
    BeginFrameCalled       = false;
    BeginFrameThreadId     = 0;
    BeginFrameTimingCalled = false;

    // Construct the HSWDisplay. We will later reconstruct it with a specific ovrRenderAPI type if the application starts using SDK-based rendering.
    if(!pHSWDisplay)
    {
        pHSWDisplay = *OVR::CAPI::HSWDisplay::Factory(ovrRenderAPI_None, pHmdDesc, RenderState);
    }

    RenderIMUTimeSeconds = 0.;

    hmdStateListLock.DoLock();
    hmdStateList.PushBack(this);
    hmdStateListLock.Unlock();
}

HMDState::~HMDState()
{
    hmdStateListLock.DoLock();
    hmdStateList.Remove(this);
    hmdStateListLock.Unlock();

    if (pClient)
    {
        pClient->Hmd_Release(NetId);
        pClient = 0;
    }

    ConfigureRendering(0,0,0,0);

    if (pHmdDesc)
    {
        OVR_FREE(pHmdDesc);
        pHmdDesc = nullptr;
    }
}

bool HMDState::InitializeSharedState()
{
    if (!CombinedHmdReader.Open(NetInfo.SharedMemoryName.Hmd.ToCStr()) ||
        !CameraReader.Open(NetInfo.SharedMemoryName.Camera.ToCStr()))
    {
        return false;
    }

    TheTrackingStateReader.SetUpdaters(CombinedHmdReader.Get(), CameraReader.Get());
    TheLatencyTestStateReader.SetUpdater(CombinedHmdReader.Get());


    return true;
}

static Vector3f GetNeckModelFromProfile(Profile* profile)
{
    OVR_ASSERT(profile);

    float neckeye[2] = { OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL, OVR_DEFAULT_NECK_TO_EYE_VERTICAL };
    profile->GetFloatValues(OVR_KEY_NECK_TO_EYE_DISTANCE, neckeye, 2);

    // Make sure these are vaguely sensible values.
    //OVR_ASSERT((neckeye[0] > 0.05f) && (neckeye[0] < 0.5f));
    //OVR_ASSERT((neckeye[1] > 0.05f) && (neckeye[1] < 0.5f));

    // Named for clarity
    float NeckToEyeHorizontal = neckeye[0];
    float NeckToEyeVertical = neckeye[1];

    // Store the neck model
    return Vector3f(0.0, NeckToEyeVertical, -NeckToEyeHorizontal);
}

static float GetCenterPupilDepthFromRenderInfo(HmdRenderInfo* hmdRenderInfo)
{
    OVR_ASSERT(hmdRenderInfo);

    // Find the distance from the center of the screen to the "center eye"
    // This center eye is used by systems like rendering & audio to represent the player,
    // and they will handle the offsets needed from there to each actual eye.

    // HACK HACK HACK
    // We know for DK1 the screen->lens surface distance is roughly 0.049f, and that the faceplate->lens is 0.02357f.
    // We're going to assume(!!!!) that all HMDs have the same screen->faceplate distance.
    // Crystal Cove was measured to be roughly 0.025 screen->faceplate which agrees with this assumption.
    // TODO: do this properly!  Update:  Measured this at 0.02733 with a CC prototype, CES era (PT7), on 2/19/14 -Steve
    float screenCenterToMidplate = 0.02733f;
    float centerEyeRelief = hmdRenderInfo->GetEyeCenter().ReliefInMeters;
    float CenterPupilDepth = screenCenterToMidplate + hmdRenderInfo->LensSurfaceToMidplateInMeters + centerEyeRelief;

    return CenterPupilDepth;
}

void HMDState::UpdateRenderProfile(Profile* profile)
{
    // Apply the given profile to generate a render context
    RenderState.OurProfileRenderInfo = GenerateProfileRenderInfoFromProfile(RenderState.OurHMDInfo, profile);
    RenderState.RenderInfo = GenerateHmdRenderInfoFromHmdInfo(RenderState.OurHMDInfo, RenderState.OurProfileRenderInfo);

    RenderState.Distortion[0] = CalculateDistortionRenderDesc(StereoEye_Left, RenderState.RenderInfo, 0);
    RenderState.Distortion[1] = CalculateDistortionRenderDesc(StereoEye_Right, RenderState.RenderInfo, 0);

    if (pClient)
    {
        // Center pupil depth
        float centerPupilDepth = GetCenterPupilDepthFromRenderInfo(&RenderState.RenderInfo);
        pClient->SetNumberValue(GetNetId(), "CenterPupilDepth", centerPupilDepth);

        // Neck model
        Vector3f neckModel = GetNeckModelFromProfile(profile);
        double neckModelArray[3] = {
            neckModel.x,
            neckModel.y,
            neckModel.z
        };
        pClient->SetNumberValues(GetNetId(), "NeckModelVector3f", neckModelArray, 3);

        // Camera position

        // OVR_KEY_CAMERA_POSITION is actually the *inverse* of a camera position.
        Posed centeredFromWorld;

        double values[7];
        if (profile->GetDoubleValues(OVR_KEY_CAMERA_POSITION, values, 7) == 7)
        {
            centeredFromWorld = Posed::FromArray(values);
        }
        else
        {
            centeredFromWorld = TheTrackingStateReader.GetDefaultCenteredFromWorld();
        }

        // ComputeCenteredFromWorld wants a worldFromCpf pose, so invert it.
        // FIXME: The stored centeredFromWorld doesn't have a neck model offset applied, but probably should.
        TheTrackingStateReader.ComputeCenteredFromWorld(centeredFromWorld.Inverted(), Vector3d(0, 0, 0));
    }
}

HMDState* HMDState::CreateHMDState(NetClient* client, const HMDNetworkInfo& netInfo)
{
    // HMDState works through a handle to service HMD....
    HMDInfo hinfo;
    if (!client->Hmd_GetHmdInfo(netInfo.NetId, &hinfo))
    {
        OVR_DEBUG_LOG(("[HMDState] Unable to get HMD info"));
        return nullptr;
    }

#ifdef OVR_OS_WIN32
    OVR_DEBUG_LOG(("[HMDState] Setting up display shim"));

    // Initialize the display shim before reporting the display to the user code
    // so that this will happen before the D3D display object is created.
    Win32::DisplayShim::GetInstance().Update(&hinfo.ShimInfo);
#endif

    Ptr<Profile> pDefaultProfile = *ProfileManager::GetInstance()->GetDefaultUserProfile(&hinfo);
    OVR_DEBUG_LOG(("[HMDState] Using profile %s", pDefaultProfile->GetValue(OVR_KEY_USER)));

    HMDState* hmds = new HMDState(hinfo, pDefaultProfile, &netInfo, client);

    if (!hmds->InitializeSharedState())
    {
        delete hmds;
        return nullptr;
    }

    return hmds;
}

HMDState* HMDState::CreateDebugHMDState(ovrHmdType hmdType)
{
    HmdTypeEnum t = HmdType_None;
    if (hmdType == ovrHmd_DK1)
        t = HmdType_DK1;    
    else if (hmdType == ovrHmd_DK2)
        t = HmdType_DK2;

    // FIXME: This does not actually grab the right user..
    Ptr<Profile> pDefaultProfile = *ProfileManager::GetInstance()->GetDefaultProfile(t);
    
    return new HMDState(CreateDebugHMDInfo(t), pDefaultProfile);
}

// Enumerate each open HMD
unsigned HMDState::EnumerateHMDStateList(bool (*callback)(const HMDState *state))
{
    unsigned count = 0;
    hmdStateListLock.DoLock();
    for (const HMDState *hmds = hmdStateList.GetFirst(); !hmdStateList.IsNull(hmds); hmds = hmdStateList.GetNext(hmds))
    {
        if (callback && !callback(hmds))
            break;
        ++count;
    }
    hmdStateListLock.Unlock();
    return count;
}

//-------------------------------------------------------------------------------------
// *** Sensor 

bool HMDState::ConfigureTracking(unsigned supportedCaps, unsigned requiredCaps)
{
    return pClient ? pClient->Hmd_ConfigureTracking(NetId, supportedCaps, requiredCaps) : true;
}

void HMDState::ResetTracking(bool visionReset)
{
    if (pClient) pClient->Hmd_ResetTracking(NetId, visionReset);
}        

// Re-center the orientation.
void HMDState::RecenterPose()
{
    float hnm[3];
    getFloatArray("NeckModelVector3f", hnm, 3);
    TheTrackingStateReader.RecenterPose(Vector3d(hnm[0], hnm[1], hnm[2]));
}

// Returns prediction for time.
ovrTrackingState HMDState::PredictedTrackingState(double absTime, void*)
{
    Vision::TrackingState ss;
    TheTrackingStateReader.GetTrackingStateAtTime(absTime, ss);

    // Zero out the status flags
    if (!pClient || !pClient->IsConnected(false, false))
    {
        ss.StatusFlags = 0;
    }

#ifdef OVR_OS_WIN32
    // Set up display code for Windows
    Win32::DisplayShim::GetInstance().Active = (ss.StatusFlags & ovrStatus_HmdConnected) != 0;
#endif


    return ss;
}

void HMDState::SetEnabledHmdCaps(unsigned hmdCaps)
{
    if (OurHMDInfo.HmdType < HmdType_DK2)
    {
        // disable low persistence and pentile.
        hmdCaps &= ~ovrHmdCap_LowPersistence;

        // disable dynamic prediction using the internal latency tester
        hmdCaps &= ~ovrHmdCap_DynamicPrediction;
    }

    if ((EnabledHmdCaps ^ hmdCaps) & ovrHmdCap_NoMirrorToWindow)
    {
#ifdef OVR_OS_WIN32
        Win32::DisplayShim::GetInstance().UseMirroring = (hmdCaps & ovrHmdCap_NoMirrorToWindow)  ?
                                                         false : true;
        if (pWindow)
        {   // Force window repaint so that stale mirrored image doesn't persist.
            ::InvalidateRect((HWND)pWindow, 0, true);
        }
#endif
    }

    // TBD: Should this include be only the rendering flags? Otherwise, bits that failed
    //      modification in Hmd_SetEnabledCaps may mis-match...
    EnabledHmdCaps             = hmdCaps & ovrHmdCap_Writable_Mask;
    RenderState.EnabledHmdCaps = EnabledHmdCaps;


    // If any of the modifiable service caps changed, call on the service.
    unsigned prevServiceCaps = EnabledServiceHmdCaps & ovrHmdCap_Writable_Mask;
    unsigned newServiceCaps  = hmdCaps & ovrHmdCap_Writable_Mask & ovrHmdCap_Service_Mask;

    if (prevServiceCaps ^ newServiceCaps)
    {
        EnabledServiceHmdCaps = pClient ? pClient->Hmd_SetEnabledCaps(NetId, newServiceCaps)
                                : newServiceCaps;
    }
}


unsigned HMDState::SetEnabledHmdCaps()
{
    unsigned serviceCaps = pClient ? pClient->Hmd_GetEnabledCaps(NetId) :
                                      EnabledServiceHmdCaps;
    
    return serviceCaps & ((~ovrHmdCap_Service_Mask) | EnabledHmdCaps);    
}


//-------------------------------------------------------------------------------------
// ***** Property Access

// FIXME: Remove the EGetBoolValue stuff and do it with a "Server:" prefix, so we do not
// need to keep a white-list of keys.  This is also way cool because it allows us to add
// new settings keys from outside CAPI that can modify internal server data.

bool HMDState::getBoolValue(const char* propertyName, bool defaultVal)
{
    if (NetSessionCommon::IsServiceProperty(NetSessionCommon::EGetBoolValue, propertyName))
    {
       return NetClient::GetInstance()->GetBoolValue(GetNetId(), propertyName, defaultVal);
    }
    else if (pProfile)
    {
        return pProfile->GetBoolValue(propertyName, defaultVal);
    }
    return defaultVal;
}

bool HMDState::setBoolValue(const char* propertyName, bool value)
{
    if (NetSessionCommon::IsServiceProperty(NetSessionCommon::ESetBoolValue, propertyName))
    {
        return NetClient::GetInstance()->SetBoolValue(GetNetId(), propertyName, value);
    }

    return false;
}

int HMDState::getIntValue(const char* propertyName, int defaultVal)
{
    if (NetSessionCommon::IsServiceProperty(NetSessionCommon::EGetIntValue, propertyName))
    {
        return NetClient::GetInstance()->GetIntValue(GetNetId(), propertyName, defaultVal);
    }
    else if (pProfile)
    {
        return pProfile->GetIntValue(propertyName, defaultVal);
    }
    return defaultVal;
}

bool HMDState::setIntValue(const char* propertyName, int value)
{
    if (NetSessionCommon::IsServiceProperty(NetSessionCommon::ESetIntValue, propertyName))
    {
        return NetClient::GetInstance()->SetIntValue(GetNetId(), propertyName, value);
    }

    return false;
}

float HMDState::getFloatValue(const char* propertyName, float defaultVal)
{
    if (OVR_strcmp(propertyName, "LensSeparation") == 0)
    {
        return OurHMDInfo.LensSeparationInMeters;
    }
    else if (OVR_strcmp(propertyName, "VsyncToNextVsync") == 0) 
    {
        return OurHMDInfo.Shutter.VsyncToNextVsync;
    }
    else if (OVR_strcmp(propertyName, "PixelPersistence") == 0) 
    {
        return OurHMDInfo.Shutter.PixelPersistence;
    }
    else if (NetSessionCommon::IsServiceProperty(NetSessionCommon::EGetNumberValue, propertyName))
    {
       return (float)NetClient::GetInstance()->GetNumberValue(GetNetId(), propertyName, defaultVal);
    }
    else if (pProfile)
    {
        return pProfile->GetFloatValue(propertyName, defaultVal);
    }

    return defaultVal;
}

bool HMDState::setFloatValue(const char* propertyName, float value)
{
    if (NetSessionCommon::IsServiceProperty(NetSessionCommon::ESetNumberValue, propertyName))
    {
        return NetClient::GetInstance()->SetNumberValue(GetNetId(), propertyName, value);
    }

    return false;
}

static unsigned CopyFloatArrayWithLimit(float dest[], unsigned destSize,
                                        float source[], unsigned sourceSize)
{
    unsigned count = Alg::Min(destSize, sourceSize);
    for (unsigned i = 0; i < count; i++)
        dest[i] = source[i];
    return count;
}

unsigned HMDState::getFloatArray(const char* propertyName, float values[], unsigned arraySize)
{
    if (arraySize)
    {
        if (OVR_strcmp(propertyName, "ScreenSize") == 0)
        {
            float data[2] = { OurHMDInfo.ScreenSizeInMeters.w, OurHMDInfo.ScreenSizeInMeters.h };

            return CopyFloatArrayWithLimit(values, arraySize, data, 2);
        }
        else if (OVR_strcmp(propertyName, "DistortionClearColor") == 0)
        {
            return CopyFloatArrayWithLimit(values, arraySize, RenderState.ClearColor, 4);
        }
        else if (OVR_strcmp(propertyName, "DK2Latency") == 0)
        {
            if (OurHMDInfo.HmdType < HmdType_DK2)
            {
                return 0;
            }

            OutputLatencyTimings timings;
            ScreenLatencyTracker.GetLatencyTimings(timings);

            if (arraySize > 0)
            {
                switch (arraySize)
                {
                default: values[4] = (float)timings.ErrorTimewarp;      // Fall-thru
                case 4:  values[3] = (float)timings.ErrorRender;        // Fall-thru
                case 3:  values[2] = (float)timings.LatencyPostPresent; // Fall-thru
                case 2:  values[1] = (float)timings.LatencyTimewarp;    // Fall-thru
                case 1:  values[0] = (float)timings.LatencyRender;
                }
            }

            return arraySize > 5 ? 5 : arraySize;
        }
        else if (OVR_strcmp(propertyName, "NeckModelVector3f") == 0)
        {
            // Query the service to grab the HNM.
            double hnm[3] = {};
            int count = NetClient::GetInstance()->GetNumberValues(GetNetId(), propertyName, hnm, (int)arraySize);

            // If the service is unavailable or returns zero data,
            if (count < 3 ||
                (hnm[0] == 0.0 && hnm[1] == 0.0 && hnm[2] == 0.0))
            {
                // These are the default values used if the server does not return any data, due to not
                // being reachable or other errors.
                OVR_ASSERT(pProfile.GetPtr());
                if (pProfile.GetPtr())
                {
                    Vector3f neckModel = GetNeckModelFromProfile(pProfile);
                    hnm[0] = neckModel.x;
                    hnm[1] = neckModel.y;
                    hnm[2] = neckModel.z;
                }
            }

            for (unsigned i = 0; i < 3 && i < arraySize; ++i)
            {
                values[i] = (float)hnm[i];
            }

            return arraySize > 3 ? 3 : arraySize;
        }
        else if (NetSessionCommon::IsServiceProperty(NetSessionCommon::EGetNumberValues, propertyName))
        {
            // Convert floats to doubles
            double* da = new double[arraySize];
            for (int i = 0; i < (int)arraySize; ++i)
            {
                da[i] = values[i];
            }

            int count = NetClient::GetInstance()->GetNumberValues(GetNetId(), propertyName, da, (int)arraySize);

            for (int i = 0; i < count; ++i)
            {
                values[i] = (float)da[i];
            }

            delete[] da;

            return count;
        }
        else if (pProfile)
        {        
            // TBD: Not quite right. Should update profile interface, so that
            //      we can return 0 in all conditions if property doesn't exist.
        
            return pProfile->GetFloatValues(propertyName, values, arraySize);
        }
    }

    return 0;
}

bool HMDState::setFloatArray(const char* propertyName, float values[], unsigned arraySize)
{
    if (!arraySize)
    {
        return false;
    }
    
    if (OVR_strcmp(propertyName, "DistortionClearColor") == 0)
    {
        CopyFloatArrayWithLimit(RenderState.ClearColor, 4, values, arraySize);
        return true;
    }

    if (NetSessionCommon::IsServiceProperty(NetSessionCommon::ESetNumberValues, propertyName))
    {
        double* da = new double[arraySize];
        for (int i = 0; i < (int)arraySize; ++i)
        {
            da[i] = values[i];
        }

        bool result = NetClient::GetInstance()->SetNumberValues(GetNetId(), propertyName, da, arraySize);

        delete[] da;

        return result;
    }

    return false;
}

const char* HMDState::getString(const char* propertyName, const char* defaultVal)
{
    if (NetSessionCommon::IsServiceProperty(NetSessionCommon::EGetStringValue, propertyName))
    {
        return NetClient::GetInstance()->GetStringValue(GetNetId(), propertyName, defaultVal);
    }

    if (pProfile)
    {
        LastGetStringValue[0] = 0;
        if (pProfile->GetValue(propertyName, LastGetStringValue, sizeof(LastGetStringValue)))
        {
            return LastGetStringValue;
        }
    }

    return defaultVal;
}

bool HMDState::setString(const char* propertyName, const char* value)
{
    if (NetSessionCommon::IsServiceProperty(NetSessionCommon::ESetStringValue, propertyName))
    {
        return NetClient::GetInstance()->SetStringValue(GetNetId(), propertyName, value);
    }

    return false;
}


//-------------------------------------------------------------------------------------
// *** Latency Test

bool HMDState::ProcessLatencyTest(unsigned char rgbColorOut[3])
{    
    return NetClient::GetInstance()->LatencyUtil_ProcessInputs(Timer::GetSeconds(), rgbColorOut);
}


//-------------------------------------------------------------------------------------
// *** Timewarp

AppTiming HMDState::GetAppTiming(uint32_t frameIndex)
{
    // Get prediction time for the requested frame index
    AppTiming timing;
    const bool VsyncOn = ((RenderState.EnabledHmdCaps & ovrHmdCap_NoVSync) == 0);
    RenderTimer.GetAppTimingForIndex(timing, VsyncOn, frameIndex);

    // Update the predicted scanout time for this frame index
    TimingHistory.SetScanoutTimeForFrame(frameIndex, timing.ScanoutStartTime);

    return timing;
}

ovrFrameTiming HMDState::GetFrameTiming(uint32_t frameIndex)
{
    AppTiming timing = GetAppTiming(frameIndex);

    // Calculate eye render times based on shutter type
    double eyePhotonsTimes[2];
    CalculateEyeRenderTimes(timing.VisibleMidpointTime, timing.FrameInterval,
                            RenderState.RenderInfo.Shutter.Type,
                            eyePhotonsTimes[0], eyePhotonsTimes[1]);

    RenderIMUTimeSeconds = Timer::GetSeconds(); // RenderPrediction.RawSensorData.TimeInSeconds;

    // Construct a ovrFrameTiming object from the base app timing information
    ovrFrameTiming result;
    result.DeltaSeconds           = (float)timing.FrameInterval;
    result.EyeScanoutSeconds[0]   = eyePhotonsTimes[0];
    result.EyeScanoutSeconds[1]   = eyePhotonsTimes[1];
    result.ScanoutMidpointSeconds = timing.VisibleMidpointTime;
    result.ThisFrameSeconds       = timing.ScanoutStartTime - timing.FrameInterval;
    result.NextFrameSeconds       = timing.ScanoutStartTime;
    // Deprecated: This should be queried after render work completes.  Please delete me from CAPI.
    result.TimewarpPointSeconds   = 0.;
    return result;
}

ovrTrackingState HMDState::GetMidpointPredictionTracking(uint32_t frameIndex)
{
    AppTiming timing = GetAppTiming(frameIndex);
    RenderIMUTimeSeconds = Timer::GetSeconds(); // RenderPrediction.RawSensorData.TimeInSeconds;
    return PredictedTrackingState(timing.VisibleMidpointTime);
}

Posef HMDState::GetEyePredictionPose(ovrEyeType eye)
{
    // Note that this function does not get the frame index parameter and depends
    // on whichever value is passed into the BeginFrame() function.
    ovrTrackingState ts = GetMidpointPredictionTracking(BeginFrameIndex);
    TraceTrackingState(ts);
    Posef const & hmdPose = ts.HeadPose.ThePose;

    // Currently HmdToEyeViewOffset is only a 3D vector
    // (Negate HmdToEyeViewOffset because offset is a view matrix offset and not a camera offset)
    if (eye == ovrEye_Left)
    {
        return Posef(hmdPose.Rotation, ((Posef)hmdPose).Apply(-((Vector3f)RenderState.EyeRenderDesc[0].HmdToEyeViewOffset)));
    }
    else
    {
        return Posef(hmdPose.Rotation, ((Posef)hmdPose).Apply(-((Vector3f)RenderState.EyeRenderDesc[1].HmdToEyeViewOffset)));
    }
}

void HMDState::endFrameRenderTiming()
{
    TimewarpTimer.SetLastPresentTime(); // Record approximate vsync time

    bool dk2LatencyTest = (EnabledHmdCaps & ovrHmdCap_DynamicPrediction) != 0;
    if (dk2LatencyTest)
    {
        Util::FrameTimeRecordSet recordSet;
        TheLatencyTestStateReader.GetRecordSet(recordSet);

        FrameLatencyData data;
        data.DrawColor                    = LatencyTest2DrawColor[0];
        data.RenderIMUTime                = RenderIMUTimeSeconds;
        data.RenderPredictedScanoutTime   = TimingHistory.LookupScanoutTime(BeginFrameIndex);
        data.PresentTime                  = TimewarpTimer.GetLatencyTesterPresentTime();
        data.TimewarpPredictedScanoutTime = TimewarpTimer.GetTimewarpTiming()->ScanoutTime;
        data.TimewarpIMUTime              = TimewarpTimer.GetTimewarpIMUTime();

        //OVR_ASSERT(data.TimewarpIMUTime == 0. || data.TimewarpIMUTime >= data.RenderIMUTime);

        ScreenLatencyTracker.SaveDrawColor(data);
        ScreenLatencyTracker.MatchRecord(recordSet);
    }
}

void HMDState::getTimewarpStartEnd(ovrEyeType eyeId, double timewarpStartEnd[2])
{
    // Get eye start/end scanout times
    TimewarpTiming const* timewarpTiming = TimewarpTimer.GetTimewarpTiming();

    for (int i = 0; i < 2; ++i)
    {
        timewarpStartEnd[i] = timewarpTiming->EyeStartEndTimes[eyeId][i];
    }
}

void HMDState::GetTimewarpMatricesEx(ovrEyeType eyeId,
                                     ovrPosef renderPose, 
                                     bool calcPosition, const ovrVector3f hmdToEyeViewOffset[2], 
                                     ovrMatrix4f twmOut[2], double debugTimingOffsetInSeconds)
{
    // Get timewarp start/end timing
    double timewarpStartEnd[2];
    getTimewarpStartEnd(eyeId, timewarpStartEnd);

    //TPH, to vary timing, to allow developers to debug, to shunt the predicted time forward 
    //and back, and see if the SDK is truly delivering the correct time.  Also to allow
    //illustration of the detrimental effects when this is not done right. 
    timewarpStartEnd[0] += debugTimingOffsetInSeconds;
    timewarpStartEnd[1] += debugTimingOffsetInSeconds;

    ovrTrackingState startState = PredictedTrackingState(timewarpStartEnd[0]);
    ovrTrackingState endState   = PredictedTrackingState(timewarpStartEnd[1]);

    ovrPosef startHmdPose = startState.HeadPose.ThePose;
    ovrPosef endHmdPose   = endState.HeadPose.ThePose;
    Vector3f eyeOffset    = Vector3f(0.0f, 0.0f, 0.0f);
    Matrix4f timewarpStart, timewarpEnd;
    if (calcPosition)
    {
        if (!hmdToEyeViewOffset)
        {
            OVR_ASSERT(false);
            LogError("{ERR-102} [FrameTime] No hmdToEyeViewOffset provided even though calcPosition is true.");

            // disable position to avoid positional issues
            renderPose.Position = Vector3f::Zero();
            startHmdPose.Position = Vector3f::Zero();
            endHmdPose.Position = Vector3f::Zero();
        }
        else if (hmdToEyeViewOffset[eyeId].x >= MATH_FLOAT_MAXVALUE)
        {
            OVR_ASSERT(false);
            LogError("{ERR-103} [FrameTime] Invalid hmdToEyeViewOffset provided by client.");

            // disable position to avoid positional issues
            renderPose.Position = Vector3f::Zero();
            startHmdPose.Position = Vector3f::Zero();
            endHmdPose.Position = Vector3f::Zero();
        }
        else
        {
            // Currently HmdToEyeViewOffset is only a 3D vector
            // (Negate HmdToEyeViewOffset because offset is a view matrix offset and not a camera offset)
            eyeOffset = ((Posef)startHmdPose).Apply(-((Vector3f)hmdToEyeViewOffset[eyeId]));
        }

        Posef fromEye = Posef(renderPose).Inverted();   // because we need the view matrix, not the camera matrix
        CalculatePositionalTimewarpMatrix(fromEye, startHmdPose, eyeOffset, timewarpStart);
        CalculatePositionalTimewarpMatrix(fromEye,   endHmdPose, eyeOffset, timewarpEnd);
    }
    else
    {
        Quatf fromEye = Quatf(renderPose.Orientation).Inverted();   // because we need the view matrix, not the camera matrix
        CalculateOrientationTimewarpMatrix(fromEye, startHmdPose.Orientation, timewarpStart);
        CalculateOrientationTimewarpMatrix(fromEye,   endHmdPose.Orientation, timewarpEnd);
    }
    twmOut[0] = timewarpStart;
    twmOut[1] = timewarpEnd;
}

void HMDState::GetTimewarpMatrices(ovrEyeType eyeId, ovrPosef renderPose,
                                   ovrMatrix4f twmOut[2])
{
    // Get timewarp start/end timing
    double timewarpStartEnd[2];
    getTimewarpStartEnd(eyeId, timewarpStartEnd);

    ovrTrackingState startState = PredictedTrackingState(timewarpStartEnd[0]);
    ovrTrackingState endState   = PredictedTrackingState(timewarpStartEnd[1]);

    Quatf quatFromEye = Quatf(renderPose.Orientation);
    quatFromEye.Invert();   // because we need the view matrix, not the camera matrix

    Matrix4f timewarpStart, timewarpEnd;
    CalculateOrientationTimewarpMatrix(
        quatFromEye, startState.HeadPose.ThePose.Orientation, timewarpStart);
    CalculateOrientationTimewarpMatrix(
        quatFromEye, endState.HeadPose.ThePose.Orientation, timewarpEnd);

    twmOut[0] = timewarpStart;
    twmOut[1] = timewarpEnd;
}


//-------------------------------------------------------------------------------------
// *** Rendering

bool HMDState::ConfigureRendering(ovrEyeRenderDesc eyeRenderDescOut[2],
                                  const ovrFovPort eyeFovIn[2],
                                  const ovrRenderAPIConfig* apiConfig,                                  
                                  unsigned distortionCaps)
{
    ThreadChecker::Scope checkScope(&RenderAPIThreadChecker, "ovrHmd_ConfigureRendering");

    // null -> shut down.
    if (!apiConfig)
    {
        if (pHSWDisplay)
        {
            pHSWDisplay->Shutdown();
            pHSWDisplay.Clear();
        }

        if (pRenderer)
            pRenderer.Clear();        
        RenderingConfigured = false; 
        return true;
    }

    if (pRenderer &&
        (apiConfig->Header.API != pRenderer->GetRenderAPI()))
    {
        // Shutdown old renderer.
        if (pHSWDisplay)
        {
            pHSWDisplay->Shutdown();
            pHSWDisplay.Clear();
        }

        if (pRenderer)
            pRenderer.Clear();
    }

    distortionCaps = distortionCaps & pHmdDesc->DistortionCaps;

    // Step 1: do basic setup configuration
    RenderState.EnabledHmdCaps = EnabledHmdCaps;     // This is a copy... Any cleaner way?
    RenderState.DistortionCaps = distortionCaps;
    RenderState.EyeRenderDesc[0] = RenderState.CalcRenderDesc(ovrEye_Left,  eyeFovIn[0]);
    RenderState.EyeRenderDesc[1] = RenderState.CalcRenderDesc(ovrEye_Right, eyeFovIn[1]);
    eyeRenderDescOut[0] = RenderState.EyeRenderDesc[0];
    eyeRenderDescOut[1] = RenderState.EyeRenderDesc[1];

    // Set RenderingConfigured early to avoid ASSERTs in renderer initialization.
    RenderingConfigured = true;

    if (!pRenderer)
    {
        pRenderer = *DistortionRenderer::APICreateRegistry
                        [apiConfig->Header.API]();
    }

    if (!pRenderer ||
        !pRenderer->Initialize(apiConfig, &TheTrackingStateReader,
                               &TimewarpTimer, &RenderState))
    {
        RenderingConfigured = false;
        return false;
    }

    // Setup the Health and Safety Warning display system.
    if(pHSWDisplay && (pHSWDisplay->GetRenderAPIType() != apiConfig->Header.API)) // If we need to reconstruct the HSWDisplay for a different graphics API type, delete the existing display.
    {
        pHSWDisplay->Shutdown();
        pHSWDisplay.Clear();
    }

    if(!pHSWDisplay) // Use * below because that for of operator= causes it to inherit the refcount the factory gave the object.
    {
        pHSWDisplay = *OVR::CAPI::HSWDisplay::Factory(apiConfig->Header.API, pHmdDesc, RenderState);
    }

    if (pHSWDisplay)
    {
        pHSWDisplay->Initialize(apiConfig); // This is potentially re-initializing it with a new config.
    }

#ifdef OVR_OS_WIN32
    if (!pWindow)
    {
        // We can automatically populate the window to attach to by
        // pulling that information off the swap chain that the
        // application provides.  If the application later calls the
        // ovrHmd_AttachToWindow() function these will get harmlessly
        // overwritten.  The check above verifies that the window is
        // not set yet, and it insures that this default doesn't
        // overwrite the application setting.

        if (apiConfig->Header.API == ovrRenderAPI_D3D11)
        {
            ovrD3D11Config* d3d11Config = (ovrD3D11Config*)apiConfig;
            if (d3d11Config->D3D11.pSwapChain)
            {
                DXGI_SWAP_CHAIN_DESC desc = {};
                HRESULT hr = d3d11Config->D3D11.pSwapChain->GetDesc(&desc);
                if (SUCCEEDED(hr))
                {
                    pWindow = (void*)desc.OutputWindow;
                }
            }
        }
        else if (apiConfig->Header.API == ovrRenderAPI_OpenGL)
        {
            ovrGLConfig* glConfig = (ovrGLConfig*)apiConfig;
            pWindow = (void*)glConfig->OGL.Window;
        }
OVR_DISABLE_MSVC_WARNING(4996) // Disable deprecation warning
        else if (apiConfig->Header.API == ovrRenderAPI_D3D9)
        {
            ovrD3D9Config* dx9Config = (ovrD3D9Config*)apiConfig;
            if (dx9Config->D3D9.pDevice)
            {
                D3DDEVICE_CREATION_PARAMETERS  params = {};
                HRESULT hr = dx9Config->D3D9.pDevice->GetCreationParameters(&params);
                if (SUCCEEDED(hr))
                {
                    pWindow = (void*)params.hFocusWindow;
                }
            }
        }
OVR_RESTORE_MSVC_WARNING()

        // If a window handle was implied by render configuration,
        if (pWindow)
        {
            // This is the same logic as ovrHmd_AttachToWindow() on Windows:
            if (pClient)
                pClient->Hmd_AttachToWindow(GetNetId(), pWindow);
            Win32::DisplayShim::GetInstance().hWindow = (HWND)pWindow;
            // On the server side it is updating the association of connection
            // to window handle.  This is perfectly safe to update later to
            // a new window handle (verified).  Also verified that if this
            // handle is garbage that it doesn't crash anything.
        }
    }
#endif

    return true;
}


void  HMDState::SubmitEyeTextures(const ovrPosef renderPose[2],
                                  const ovrTexture eyeTexture[2],
                                  const ovrTexture eyeDepthTexture[2])
{
    RenderState.EyeRenderPoses[0] = renderPose[0];
    RenderState.EyeRenderPoses[1] = renderPose[1];

    if (pRenderer)
    {
        if(eyeDepthTexture)
        {
            pRenderer->SubmitEyeWithDepth(0, &eyeTexture[0], &eyeDepthTexture[0]);
            pRenderer->SubmitEyeWithDepth(1, &eyeTexture[1], &eyeDepthTexture[1]);
        }
        else
        {
            //OVR_ASSERT(!(RenderState.DistortionCaps & ovrDistortionCap_DepthProjectedTimeWarp));
            //LogError("{ERR-104} [HMDState] Even though ovrDistortionCap_DepthProjectedTimeWarp is enabled, no depth buffer was provided.");

        pRenderer->SubmitEye(0, &eyeTexture[0]);
        pRenderer->SubmitEye(1, &eyeTexture[1]);
    }
}
}

bool  HMDState::CreateDistortionMesh(ovrEyeType eyeType, ovrFovPort fov,
                                     unsigned int distortionCaps,
                                     ovrDistortionMesh *meshData,
                                     float overrideEyeReliefIfNonZero)
{
    const HmdRenderInfo& hmdri = RenderState.RenderInfo;

    DistortionRenderDesc& distortion = RenderState.Distortion[eyeType];
    if (overrideEyeReliefIfNonZero)
    {
        distortion.Lens = GenerateLensConfigFromEyeRelief(overrideEyeReliefIfNonZero, hmdri);
    }

    if (CalculateDistortionMeshFromFOV(
            hmdri, distortion,
            (eyeType == ovrEye_Left ? StereoEye_Left : StereoEye_Right),
            fov, distortionCaps, meshData))
    {
        return 1;
    }

    return 0;
}


}} // namespace OVR::CAPI
