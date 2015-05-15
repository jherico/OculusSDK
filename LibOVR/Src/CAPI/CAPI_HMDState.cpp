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
    // For auto-detection of window handle for direct mode:
    #include <OVR_CAPI_D3D.h>
    #include <GL/CAPI_GLE.h>
    #include <OVR_CAPI_GL.h>

    #include "D3D1X/CAPI_D3D11_CliCompositorClient.h"

#elif defined(OVR_OS_MAC)

    #include "GL/CAPI_GL_CliCompositorClient_OSX.h"

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
    RenderTimer(),
    pProfile(profile),
    pHmdDesc(0),
    pClient(client),
    pCompClient(),
    NetId(InvalidVirtualHmdId),
    NetInfo(),
    OurHMDInfo(hmdInfo),
    pLastError(nullptr),
    EnabledHmdCaps(0),
    EnabledServiceHmdCaps(0),
    CombinedHmdReader(),
    TheTrackingStateReader(),
    LatencyTestActive(false),
  //LatencyTestDrawColor(),
    RenderState(),
  //LastGetStringValue(),
    AppFrameIndex(0),
    RenderAPIThreadChecker(),    
    LayerDescList(),
    LayersOtherThan0MayBeEnabled(true)
{
    if (netInfo)
    {
        NetId = netInfo->NetId;
        NetInfo = *netInfo;
    }

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


    /*
    LatencyTestDrawColor[0] = 0;
    LatencyTestDrawColor[1] = 0;
    LatencyTestDrawColor[2] = 0;
    */

    hmdStateListLock.DoLock();
    hmdStateList.PushBack(this);
    hmdStateListLock.Unlock();
}

HMDState::~HMDState()
{
    hmdStateListLock.DoLock();
    hmdStateList.Remove(this);
    hmdStateListLock.Unlock();

    pCompClient.Clear();

    if (pClient)
    {
        pClient->Hmd_Release(NetId);
        pClient = 0;
    }

    if (pHmdDesc)
    {
        OVR_FREE(pHmdDesc);
        pHmdDesc = nullptr;
    }
}

ovrResult HMDState::InitializeSharedState()
{
    // Open up the camera and HMD shared memory sections
    if (!CombinedHmdReader.Open(NetInfo.SharedMemoryName.Hmd.ToCStr()) ||
        !CameraReader.Open(NetInfo.SharedMemoryName.Camera.ToCStr()))
    {
        return ovrError_Initialize;
    }

    TheTrackingStateReader.SetUpdaters(CombinedHmdReader.Get(), CameraReader.Get());


    // Connect to the compositor. Note that this doesn't fully initialize the connection
    // with graphics information. That is delay initialized on demand on first texture set creation
#if defined(OVR_OS_MS)
    pCompClient = *new CliD3D11CompositorClient(this);
#else
    //pCompClient = *new CliCompositorClient(this);
#endif
    if (!pCompClient)
    {
        OVR_ASSERT(false);
        return ovrError_Initialize;
    }

    return ovrSuccess;
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
    if (!client->Hmd_GetHmdInfo(netInfo.NetId, hinfo))
    {
        OVR_DEBUG_LOG(("[HMDState] Unable to get HMD info.\n"));
        return nullptr;
    }

    Ptr<Profile> pDefaultProfile = *ProfileManager::GetInstance()->GetDefaultUserProfile(&hinfo);
    OVR_DEBUG_LOG(("[HMDState] Using profile %s\n", pDefaultProfile->GetValue(OVR_KEY_USER)));

    HMDState* hmds = new HMDState(hinfo, pDefaultProfile, &netInfo, client);

    ovrResult result = hmds->InitializeSharedState();
    if (result != ovrSuccess)
    {
        delete hmds;
        return nullptr;
    }

    return hmds;
}

HMDState* HMDState::CreateDebugHMDState(NetClient* client, ovrHmdType hmdType)
{
    HmdTypeEnum t = HmdType_None;
    if (hmdType == ovrHmd_DK1)
        t = HmdType_DK1;    
    else if (hmdType == ovrHmd_DK2)
        t = HmdType_DK2;

    // FIXME: This does not actually grab the right user..
    Ptr<Profile> pDefaultProfile = *ProfileManager::GetInstance()->GetDefaultProfile(t);
    
    HMDState* hmds = new HMDState(CreateDebugHMDInfo(t), pDefaultProfile, nullptr, client);

    // Connect to the compositor. Note that this doesn't fully initialize the connection
    // with graphics information. That is delay initialized on demand on first texture set creation
#if defined(OVR_OS_MS)
    hmds->pCompClient = *new CliD3D11CompositorClient(hmds);
#else
    //pCompClient = *new CliCompositorClient(this);
#endif
    if (!hmds->pCompClient)
    {
        OVR_ASSERT(false);
        delete hmds;
        return nullptr;
    }

    return hmds;
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

ovrResult HMDState::ConfigureTracking(unsigned supportedCaps, unsigned requiredCaps)
{
    return pClient ? pClient->Hmd_ConfigureTracking(NetId, supportedCaps, requiredCaps) : ovrError_NotInitialized;
}

void HMDState::ResetBackOfHeadTracking()
{
    if (pClient) pClient->Hmd_ResetTracking(NetId, true);
}

void HMDState::ResetTracking()
{
    if (pClient) pClient->Hmd_ResetTracking(NetId, false);
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

    // Record the render IMU time in seconds from the raw sensor data.
    TimingHistory.SetRenderIMUTime(absTime, ss.RawSensorData.AbsoluteTimeSeconds);

    // Zero out the status flags
    if (!pClient || !pClient->IsConnected(false, false))
    {
        ss.StatusFlags = 0;
    }


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


unsigned HMDState::GetEnabledHmdCaps() const
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

bool HMDState::getBoolValue(const char* propertyName, bool defaultVal) const
{
    if (OVR_strcmp(propertyName, "QueueAheadEnabled") == 0)
    {
        OVR_ASSERT(pCompClient);
        if (pCompClient)
        {
            return pCompClient->GetQueueAheadSeconds() > 0.f;
        }
    }
    else if (NetSessionCommon::IsServiceProperty(NetSessionCommon::EGetBoolValue, propertyName))
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
    if (OVR_strcmp(propertyName, "QueueAheadEnabled") == 0)
    {
        OVR_ASSERT(pCompClient);
        if (pCompClient)
        {
            // 2.8ms queue ahead by default
            static const float DefaultQueueAheadSeconds = 0.0028f;
            return pCompClient->SetQueueAheadSeconds(value ? DefaultQueueAheadSeconds : 0.f).Succeeded();
        }
    }
    else if (NetSessionCommon::IsServiceProperty(NetSessionCommon::ESetBoolValue, propertyName))
    {
        return NetClient::GetInstance()->SetBoolValue(GetNetId(), propertyName, value);
    }

    return false;
}

int HMDState::getIntValue(const char* propertyName, int defaultVal) const
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

float HMDState::getFloatValue(const char* propertyName, float defaultVal) const
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
            if (OurHMDInfo.HmdType < HmdType_DK2 || !pCompClient)
            {
                return 0;
            }

            CAPI::OutputLatencyTimings timings = pCompClient->GetLatencyTimings();

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

    // Update the timing for this frame index
    TimingHistory.SetTiming(frameIndex, timing);

    return timing;
}

ovrFrameTiming HMDState::GetFrameTiming(uint32_t frameIndex)
{
    AppTiming timing = GetAppTiming(frameIndex);

    // Construct a ovrFrameTiming object from the base app timing information
    ovrFrameTiming result;
    result.FrameIntervalSeconds   = timing.FrameInterval;
    result.DisplayMidpointSeconds = timing.VisibleMidpointTime;
    result.AppFrameIndex          = frameIndex;
    result.DisplayFrameIndex      = timing.DisplayFrameIndex;
    return result;
}

ovrTrackingState HMDState::GetMidpointPredictionTracking(uint32_t frameIndex)
{
    AppTiming timing = GetAppTiming(frameIndex);

    return PredictedTrackingState(timing.VisibleMidpointTime);
}



static const ovrTexture &GetCurrentTexture ( const ovrSwapTextureSet* texSet )
{
    // This is belt-and-braces, but it seems worryingly easy for apps to feed us a bad CurrentIndex and blow up everything.
    OVR_ASSERT ( texSet != nullptr );
    if ( ( texSet->CurrentIndex >= 0 ) && ( texSet->CurrentIndex < texSet->TextureCount ) )
    {
        return texSet->Textures[texSet->CurrentIndex];
    }
    else
    {
        OVR_DEBUG_LOG(("[HMDState] Invalid ovrSwapTextureSet::CurrentIndex %i", texSet->CurrentIndex));
        return texSet->Textures[0];
    }
}


// We convert the public ovrLayerEye_Union and friends to our internal DistortionRendererLayerDesc.
// Requires a valid pLayerHeader pointer.
static void ConvertLayerHeaderToLayerDesc(const ovrLayerHeader* pLayerHeader, DistortionRendererLayerDesc& layerDesc)
{
    OVR_ASSERT(pLayerHeader);

    layerDesc.Desc.Type                       = (LayerDesc::LayerType)pLayerHeader->Type;
    layerDesc.Desc.bTextureOriginAtBottomLeft = (pLayerHeader->Flags & ovrLayerFlag_TextureOriginAtBottomLeft) != 0;

    layerDesc.Desc.bAnisoFiltering = false;
    layerDesc.Desc.Quality = LayerDesc::QualityType_Normal;
    if ( (pLayerHeader->Flags & ovrLayerFlag_HighQuality) != 0 )
    {
        // TODO: for sRGB, don't use aniso - it's not energy-conserving.
        // TODO: different "high quality" for eye buffers vs quads, since quads are more frequently at an angle.
        // Note - currently for the EWA types, aniso doesn't do anything because they always sample level 0.
        layerDesc.Desc.bAnisoFiltering = true;
        layerDesc.Desc.Quality = LayerDesc::QualityType_Normal;
    }

    switch(pLayerHeader->Type)
    {
        case ovrLayerType_EyeFov:
        {
            const ovrLayerEyeFov* pLayerEyeFov = reinterpret_cast<const ovrLayerEyeFov*>(pLayerHeader);
            OVR_ASSERT(pLayerEyeFov);

            const ovrSwapTextureSet* texSet[2];
            texSet[0] = pLayerEyeFov->ColorTexture[0];
            texSet[1] = pLayerEyeFov->ColorTexture[1];
            if ( texSet[1] == nullptr )
            {
                // Only one texture supplied, so use it for both eyes.
                texSet[1] = pLayerEyeFov->ColorTexture[0];
            }
            if ( texSet[0] == nullptr )
            {
                OVR_DEBUG_LOG(("[HMDState] NULL texture set pointer in layer %i - disabling", layerDesc.LayerNum));
                layerDesc.SetToDisabled();
                return;
            }

            for (int eyeId = 0; eyeId < 2; eyeId++)
            {
                layerDesc.Desc.EyeRenderPose[eyeId]     = pLayerEyeFov->RenderPose[eyeId];
                layerDesc.Desc.EyeTextureSize[eyeId]    = GetCurrentTexture(texSet[eyeId]).Header.TextureSize;
                layerDesc.Desc.EyeRenderViewport[eyeId] = pLayerEyeFov->Viewport[eyeId];
                layerDesc.Desc.EyeRenderFovPort[eyeId]  = pLayerEyeFov->Fov[eyeId];
                layerDesc.Desc.pEyeTextureSets[eyeId]   = texSet[eyeId];

                // Unused for this layer type:
                layerDesc.Desc.QuadSize[eyeId]             = ovrVector2f();
                layerDesc.Desc.pEyeDepthTextureSets[eyeId] = nullptr;
            }

            break;
        }

        case ovrLayerType_EyeFovDepth:
        {
            const ovrLayerEyeFovDepth* pLayerEyeFovDepth = reinterpret_cast<const ovrLayerEyeFovDepth*>(pLayerHeader);
            OVR_ASSERT(pLayerEyeFovDepth);

            const ovrSwapTextureSet* colorTexSet[2];
            colorTexSet[0] = pLayerEyeFovDepth->ColorTexture[0];
            colorTexSet[1] = pLayerEyeFovDepth->ColorTexture[1];
            if ( colorTexSet[1] == nullptr )
            {
                // Only one texture supplied, so use it for both eyes.
                colorTexSet[1] = pLayerEyeFovDepth->ColorTexture[0];
            }
            if ( colorTexSet[0] == nullptr )
            {
                OVR_DEBUG_LOG(("[HMDState] NULL texture set pointer in layer %i - disabling", layerDesc.LayerNum));
                layerDesc.SetToDisabled();
                return;
            }

            const ovrSwapTextureSet* depthTexSet[2];
            depthTexSet[0] = pLayerEyeFovDepth->DepthTexture[0];
            depthTexSet[1] = pLayerEyeFovDepth->DepthTexture[1];
            if ( depthTexSet[1] == nullptr )
            {
                // Only one texture supplied, so use it for both eyes.
                depthTexSet[1] = pLayerEyeFovDepth->DepthTexture[0];
            }
            if ( depthTexSet[0] == nullptr )
            {
                OVR_DEBUG_LOG(("[HMDState] NULL texture set pointer in layer %i - disabling", layerDesc.LayerNum));
                layerDesc.SetToDisabled();
                return;
            }

            for (int eyeId = 0; eyeId < 2; eyeId++)
            {
                // Force the sanity-checking that GetCurrentTexture does.
                ovrTexture const &ignored = GetCurrentTexture(depthTexSet[eyeId]);
                OVR_UNUSED ( ignored );

                layerDesc.Desc.EyeRenderPose[eyeId]        = pLayerEyeFovDepth->RenderPose[eyeId];
                layerDesc.Desc.EyeTextureSize[eyeId]       = GetCurrentTexture(colorTexSet[eyeId]).Header.TextureSize;
                layerDesc.Desc.EyeRenderViewport[eyeId]    = pLayerEyeFovDepth->Viewport[eyeId];
                layerDesc.Desc.EyeRenderFovPort[eyeId]     = pLayerEyeFovDepth->Fov[eyeId];
                layerDesc.Desc.pEyeTextureSets[eyeId]      = colorTexSet[eyeId];
                layerDesc.Desc.pEyeDepthTextureSets[eyeId] = depthTexSet[eyeId];
                layerDesc.Desc.ProjectionDesc = pLayerEyeFovDepth->ProjectionDesc;

                // Unused for this layer type:
                layerDesc.Desc.QuadSize[eyeId] = ovrVector2f();
            }

            break;
        }

        case ovrLayerType_QuadInWorld:
        case ovrLayerType_QuadHeadLocked:
        {
            const ovrLayerQuad* pLayerQuad = reinterpret_cast<const ovrLayerQuad*>(pLayerHeader);
            OVR_ASSERT(pLayerQuad);

            if ( pLayerQuad->ColorTexture == nullptr )
            {
                OVR_DEBUG_LOG(("[HMDState] NULL texture set pointer in layer %i - disabling", layerDesc.LayerNum));
                layerDesc.SetToDisabled();
                return;
            }

            for (int eyeId = 0; eyeId < 2; eyeId++)
            {
                // TODO: write a stereo-pair-capable version of this call.
                layerDesc.Desc.EyeRenderPose[eyeId]     = pLayerQuad->QuadPoseCenter;
                layerDesc.Desc.QuadSize[eyeId]          = pLayerQuad->QuadSize;
                layerDesc.Desc.EyeTextureSize[eyeId]    = GetCurrentTexture(pLayerQuad->ColorTexture).Header.TextureSize;
                layerDesc.Desc.EyeRenderViewport[eyeId] = pLayerQuad->Viewport;
                layerDesc.Desc.pEyeTextureSets[eyeId]   = pLayerQuad->ColorTexture;

                // Unused for this layer type:
                layerDesc.Desc.pEyeDepthTextureSets[eyeId] = nullptr;
                layerDesc.Desc.EyeRenderFovPort[eyeId]     = FovPort();
            }

            break;
        }

        case ovrLayerType_Direct:
        {
            const ovrLayerDirect* pLayerDirect = reinterpret_cast<const ovrLayerDirect*>(pLayerHeader);
            OVR_ASSERT(pLayerDirect);

            const ovrSwapTextureSet* texSet[2];
            texSet[0] = pLayerDirect->ColorTexture[0];
            texSet[1] = pLayerDirect->ColorTexture[1];
            if ( texSet[1] == nullptr )
            {
                // Only one texture supplied, so use it for both eyes.
                texSet[1] = pLayerDirect->ColorTexture[0];
            }
            if ( texSet[0] == nullptr )
            {
                OVR_DEBUG_LOG(("[HMDState] NULL texture set pointer in layer %i - disabling", layerDesc.LayerNum));
                layerDesc.SetToDisabled();
                return;
            }

            for (int eyeId = 0; eyeId < 2; eyeId++)
            {
                layerDesc.Desc.EyeTextureSize[eyeId]       = GetCurrentTexture(texSet[eyeId]).Header.TextureSize;
                layerDesc.Desc.EyeRenderViewport[eyeId]    = pLayerDirect->Viewport[eyeId];
                layerDesc.Desc.pEyeTextureSets[eyeId]      = texSet[eyeId];

                // Unused for this layer type:
                layerDesc.Desc.QuadSize[eyeId]             = ovrVector2f();
                layerDesc.Desc.pEyeDepthTextureSets[eyeId] = nullptr;
                layerDesc.Desc.EyeRenderPose[eyeId]        = Posef();
                layerDesc.Desc.EyeRenderFovPort[eyeId]     = FovPort();
            }

            break;
        }
    }
}


ovrResult HMDState::SubmitLayers(ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
    OVR_ASSERT(pCompClient);

    // Ignore layers that are beyond the supported count.
    if(layerCount > MaxNumLayersTotal)
       layerCount = MaxNumLayersTotal;

    // Make it so our LayerDescList can have an entry for every user-supplied ovrLayerHeader.
    if(LayerDescList.GetSize() < layerCount)
        LayerDescList.Resize(layerCount);            

    for (unsigned int i = 0; i < layerCount; ++i)
    {
        DistortionRendererLayerDesc& layerDesc = LayerDescList[i];

        layerDesc.LayerNum = i; // To do: LayerNum is always the same for this layer index, so we could assign this value externally.

        // Should we return an error code or log an error if the user passes an invalid Type?
        if (layerPtrList[i] && (layerPtrList[i]->Type >= ovrLayerType_EyeFov) && 
                               (layerPtrList[i]->Type <= ovrLayerType_Direct))
        {
            ConvertLayerHeaderToLayerDesc(layerPtrList[i], layerDesc);
            OVRError err;
            if ( layerDesc.Desc.Type == LayerDesc::LayerType_Disabled )
            {
                // ConvertLayerHeaderToLayerDesc found something scary and disabled the layer.
                err = pCompClient->DisableLayer(i);
            }
            else
            {
                err = pCompClient->SubmitLayer(i, &layerDesc.Desc);
            }

            if (!err.Succeeded())
            {
                OVR_SET_ERROR(err);
                return err.GetCode();
            }

            if (i > 0)
                LayersOtherThan0MayBeEnabled = true;
        }
        else
        {
            OVRError err = pCompClient->DisableLayer(i);
            if (!err.Succeeded())
            {
                OVR_SET_ERROR(err);
                return err.GetCode();
            }
        }
    }

    for(unsigned int i = layerCount; i < MaxNumLayersPublic; ++i)
    {
        OVRError err = pCompClient->DisableLayer(i);
        if (!err.Succeeded())
        {
            OVR_SET_ERROR(err);
            return err.GetCode();
        }
    }

    return ovrSuccess;
}

ovrResult HMDState::SubmitFrame(uint32_t appFrameIndex, const ovrViewScaleDesc* viewScaleDesc, ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
    OVR_ASSERT(layerCount >= 0);
    OVR_ASSERT((layerCount == 0) || (layerPtrList && (*layerPtrList != nullptr)));
    OVR_ASSERT(pCompClient);
    OVR_ASSERT(viewScaleDesc != NULL);

    ovrResult result = SubmitLayers(layerPtrList, layerCount);

    if (result != ovrSuccess)
    {
        // To do: We need to call OVR_MAKE_ERROR if it hasn't been done yet, in order to record the error for posterity.
        OVR_ASSERT(false);
        return result;
    }

    #if defined (OVR_OS_MS) || defined(OVR_OS_MAC)
        OVRError err = pCompClient->EndFrame(appFrameIndex, viewScaleDesc);

        if (!err.Succeeded())
        {
            OVR_SET_ERROR(err);
            return err.GetCode();
        }

        result = err.GetCode();
    #else
        OVR_UNUSED(appFrameIndex);
        OVR_UNUSED(viewScaleDesc);
    #endif

    // Next App Frame Index
    AppFrameIndex++;

    return result;
}





}} // namespace OVR::CAPI
