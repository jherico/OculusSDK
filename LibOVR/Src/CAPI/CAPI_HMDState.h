/************************************************************************************

Filename    :   CAPI_HMDState.h
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

#ifndef OVR_CAPI_HMDState_h
#define OVR_CAPI_HMDState_h

#include "Extras/OVR_Math.h"
#include "Kernel/OVR_List.h"
#include "Kernel/OVR_Log.h"
#include "OVR_CAPI.h"

// FIXME: Clean this up.  We don't need the whole HMD Render State anymore. -cat
#include "CAPI_HMDRenderState.h"

#include "Service/Service_NetClient.h"
#include "Net/OVR_NetworkTypes.h"
#include "CAPI_DistortionTiming.h"
#include "CAPI_CliCompositorClient.h"


struct ovrHmdStruct { };


namespace OVR { namespace CAPI {


using namespace OVR::Util::Render;
using namespace OVR::Service;
using namespace OVR::Net;


//-------------------------------------------------------------------------------------
// ***** ThreadChecker

// This helper class is used to verify that the API is used according to supported
// thread safety constraints (is not re-entrant for this and related functions).
class ThreadChecker
{
public:

#ifndef OVR_BUILD_DEBUG

    // In release build, thread checks are disabled.
    ThreadChecker() { }
    void Begin(const char* functionName)    { OVR_UNUSED1(functionName); }
    void End()                              {  }

    // Add thread-re-entrancy check for function scope
    struct Scope
    {
        Scope(ThreadChecker*, const char *) { }
        ~Scope() { }
    };


#else // OVR_BUILD_DEBUG
    ThreadChecker() : pFunctionName(0), FirstThread(0)
    { }

    void Begin(const char* functionName)
    {        
        if (!pFunctionName)
        {
            pFunctionName = functionName;
            FirstThread   = GetCurrentThreadId();
        }
        else
        {
            // pFunctionName may be not null here if function is called internally on the same thread.
            OVR_ASSERT_LOG((FirstThread == GetCurrentThreadId()),
                ("%s (threadId=%p) called at the same times as %s (threadId=%p)\n",
                functionName, GetCurrentThreadId(), pFunctionName, FirstThread) );
        }        
    }
    void End()
    {
        pFunctionName = 0;
        FirstThread   = 0;
    }

    // Add thread-reentrancy check for function scope.
    struct Scope
    {
        Scope(ThreadChecker* threadChecker, const char *functionName) : pChecker(threadChecker)
        { pChecker->Begin(functionName); }
        ~Scope()
        { pChecker->End(); }
    private:
        ThreadChecker* pChecker;
    };

private:
    // If not 0, contains the name of the function that first entered the scope.
    const char * pFunctionName;
    ThreadId     FirstThread;

#endif // OVR_BUILD_DEBUG
};


//-------------------------------------------------------------------------------------
// ***** HMDState


// Describes a single HMD.
class HMDState : public ListNode<HMDState>,
                 public ovrHmdStruct, public NewOverrideBase 
{
    void operator=(const HMDState&) { } // Quiet warning.

protected:   
    HMDState(HMDInfo const & hmdInfo,
             Profile* profile,
             Service::HMDNetworkInfo const * netInfo = nullptr,
             Service::NetClient* client = nullptr);

public:   
    virtual ~HMDState();

    static HMDState* CreateHMDState(Service::NetClient* client, const HMDNetworkInfo& netInfo);
    static HMDState* CreateDebugHMDState(Service::NetClient* client, ovrHmdType hmdType); // Used for debug mode

    // Call the optional provided callback for each open HMD, stopping when the callback returns false.
    // Return a count of the enumerated HMDStates. Note that this may deadlock if ovrHmd_Create/Destroy
    // are called from the callback.
    static unsigned EnumerateHMDStateList(bool (*callback)(const HMDState *state));

    ovrResult InitializeSharedState();

    // *** Sensor Setup

    ovrResult       ConfigureTracking(unsigned supportedCaps, unsigned requiredCaps);    
    void            ResetBackOfHeadTracking();
    void            ResetTracking();
    void            RecenterPose();
    ovrTrackingState PredictedTrackingState(double absTime, void* unused = nullptr);

    // Changes HMD Caps.
    // Capability bits that are not directly or logically tied to one system (such as sensor)
    // are grouped here. ovrHmdCap_VSync, for example, affects rendering and timing.
    void            SetEnabledHmdCaps(unsigned caps);
    unsigned        GetEnabledHmdCaps() const;

    bool            ProcessLatencyTest(unsigned char rgbColorOut[3]);

    void        UpdateRenderProfile(Profile* profile);

    ovrResult SubmitLayers(ovrLayerHeader const * const * layerPtrList, unsigned int layerCount);
    ovrResult SubmitFrame(uint32_t appFrameIndex, const ovrViewScaleDesc* viewScaleDesc, ovrLayerHeader const * const * layerPtrList, unsigned int layerCount);

    void applyProfileToSensorFusion();

    // Get properties by name.
    bool     getBoolValue(const char* propertyName, bool defaultVal) const;
    bool     setBoolValue(const char* propertyName, bool value);
    int      getIntValue(const char* propertyName, int defaultVal) const;
    bool     setIntValue(const char* propertyName, int value);
    float    getFloatValue(const char* propertyName, float defaultVal) const;
    bool     setFloatValue(const char* propertyName, float value);
    unsigned getFloatArray(const char* propertyName, float values[], unsigned arraySize);
    bool     setFloatArray(const char* propertyName, float values[], unsigned arraySize);
    const char* getString(const char* propertyName, const char* defaultVal);
    bool        setString(const char* propertyName, const char* value);

    VirtualHmdId GetNetId() const { return NetId; }

    const Ptr<CliCompositorClient>& GetCompClient() const { return pCompClient; }

public: // Intentionally public because this code is internal and usage is single-threaded.

    AppTiming        GetAppTiming(uint32_t frameIndex);
    ovrFrameTiming   GetFrameTiming(uint32_t frameIndex);
    ovrTrackingState GetMidpointPredictionTracking(uint32_t frameIndex);

    // Render timing
    void getTimewarpStartEnd(ovrEyeType eyeId, double timewarpStartEnd[2]);    

    mutable AppRenderTimer  RenderTimer;           // Timing for eye rendering
    AppTimingHistory        TimingHistory;         // History of predicted scanout times

public: // Intentionally public because this code is internal and usage is single-threaded.

    Ptr<Profile>            pProfile;
    // Descriptor that gets allocated and returned to the user as ovrHmd.
    ovrHmdDesc*             pHmdDesc;

    Ptr<CliCompositorClient> pCompClient;

    // Network
    Service::NetClient*     pClient;
    VirtualHmdId            NetId;
    HMDNetworkInfo          NetInfo;

    // HMDInfo shouldn't change, as its string pointers are passed out.    
    HMDInfo                 OurHMDInfo;

    const char*             pLastError;

    // Caps enabled for the HMD.
    unsigned                EnabledHmdCaps;

    // Caps actually sent to the Sensor Service
    unsigned                EnabledServiceHmdCaps;

    // *** Sensor
    SharedObjectReader<Vision::CombinedHmdUpdater> CombinedHmdReader;
    SharedObjectReader<Vision::CameraStateUpdater> CameraReader;


    Vision::TrackingStateReader TheTrackingStateReader;

    bool                    LatencyTestActive;
    unsigned char           LatencyTestDrawColor[3];

    // Rendering part    
    HMDRenderState          RenderState;

    // Last cached value returned by ovrHmd_GetString/ovrHmd_GetStringArray.
    char                    LastGetStringValue[256];
   
    uint32_t                AppFrameIndex;

    // Graphics functions are not re-entrant from other threads.
    ThreadChecker           RenderAPIThreadChecker;

    typedef ArrayPOD<DistortionRendererLayerDesc> LayerDescListType;
    LayerDescListType       LayerDescList;
    bool                    LayersOtherThan0MayBeEnabled;
};


}} // namespace OVR::CAPI

#endif // OVR_CAPI_HMDState_h
