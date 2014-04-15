/************************************************************************************

Filename    :   CAPI_HMDState.h
Content     :   State associated with a single HMD
Created     :   January 24, 2014
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

#ifndef OVR_CAPI_HMDState_h
#define OVR_CAPI_HMDState_h

#include "../Kernel/OVR_Math.h"
#include "../Kernel/OVR_List.h"
#include "../Kernel/OVR_Log.h"
#include "../OVR_CAPI.h"
#include "../OVR_SensorFusion.h"
#include "../Util/Util_LatencyTest.h"
#include "../Util/Util_LatencyTest2.h"

#include "CAPI_FrameTimeManager.h"
#include "CAPI_HMDRenderState.h"
#include "CAPI_DistortionRenderer.h"

// Define OVR_CAPI_VISIONSUPPORT to compile in vision support 
#ifdef OVR_CAPI_VISIONSUPPORT
    #define OVR_CAPI_VISION_CODE(c) c
    #include "../Vision/Vision_PoseTracker.h"
#else
    #define OVR_CAPI_VISION_CODE(c)
#endif


struct ovrHmdStruct { };

namespace OVR { namespace CAPI {

using namespace OVR::Util::Render;


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
                ("%s (threadId=%d) called at the same times as %s (threadId=%d)\n",
                functionName, GetCurrentThreadId(), pFunctionName, FirstThread) );
        }        
    }
    void End()
    {
        pFunctionName = 0;
        FirstThread   = 0;
    }

    // Add thread-re-entrancy check for function scope.
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
public:   

    HMDState(HMDDevice* device);
    HMDState(ovrHmdType hmdType);
    virtual ~HMDState();


    // *** Sensor Setup

    bool            StartSensor(unsigned supportedCaps, unsigned requiredCaps);
    void            StopSensor();
    void            ResetSensor();
    ovrSensorState  PredictedSensorState(double absTime);
    bool            GetSensorDesc(ovrSensorDesc* descOut);

    bool            ProcessLatencyTest(unsigned char rgbColorOut[3]);
    void            ProcessLatencyTest2(unsigned char rgbColorOut[3], double startTime);
    

    // *** Rendering Setup

    bool       ConfigureRendering(ovrEyeRenderDesc eyeRenderDescOut[2],
                                  const ovrEyeDesc eyeDescIn[2],
                                  const ovrRenderAPIConfig* apiConfig,
                                  unsigned hmdCaps,
                                  unsigned distortionCaps);  
    
    ovrPosef    BeginEyeRender(ovrEyeType eye);
    void        EndEyeRender(ovrEyeType eye, ovrPosef renderPose, ovrTexture* eyeTexture);


    const char* GetLastError()
    {
        const char* p = pLastError;
        pLastError = 0;
        return p;
    }    

    void NotifyAddDevice(DeviceType deviceType)
    {
        if (deviceType == Device_Sensor)
            AddSensorCount++;
        else if (deviceType == Device_LatencyTester)
        {
            AddLatencyTestCount++;
            AddLatencyTestDisplayCount++;
        }
    }

    bool checkCreateSensor();

    void applyProfileToSensorFusion();

    // INlines so that they can be easily compiled out.    
    // Does debug ASSERT checks for functions that require BeginFrame.
    // Also verifies that we are on the right thread.
    void checkBeginFrameScope(const char* functionName)
    {
        OVR_UNUSED1(functionName); // for Release build.
        OVR_ASSERT_LOG(BeginFrameCalled == true,
                       ("%s called outside ovrHmd_BeginFrame."));
        OVR_ASSERT_LOG(BeginFrameThreadId == OVR::GetCurrentThreadId(),
                       ("%s called on a different thread then ovrHmd_BeginFrame."));
    }

    void checkRenderingConfigured(const char* functionName)
    {
        OVR_UNUSED1(functionName); // for Release build.
        OVR_ASSERT_LOG(RenderingConfigured == true,
                       ("%s called without ovrHmd_ConfigureRendering."));
    }

    void checkBeginFrameTimingScope(const char* functionName)
    {
        OVR_UNUSED1(functionName); // for Release build.
        OVR_ASSERT_LOG(BeginFrameTimingCalled == true,
                       ("%s called outside ovrHmd_BeginFrameTiming."));
    }


    HMDState* getThis() { return this; }

    void updateLowPersistenceMode(bool lowPersistence) const;
	void updateLatencyTestForHmd(bool latencyTesting);
    
	// Get properties by name.
	float    getFloatValue(const char* propertyName, float defaultVal);
    bool     setFloatValue(const char* propertyName, float value);
	unsigned getFloatArray(const char* propertyName, float values[], unsigned arraySize);
    bool     setFloatArray(const char* propertyName, float values[], unsigned arraySize);
	const char* getString(const char* propertyName, const char* defaultVal);
public:
    
    // Wrapper to support 'const'
    struct HMDInfoWrapper
    {
        HMDInfoWrapper(ovrHmdType hmdType)
        {
            HmdTypeEnum t = HmdType_None;
            if (hmdType == ovrHmd_DK1)
                t = HmdType_DK1;
            else if (hmdType == ovrHmd_CrystalCoveProto)
                t = HmdType_CrystalCoveProto;
            else if (hmdType == ovrHmd_DK2)
                t = HmdType_DK2;
            h = CreateDebugHMDInfo(t);
        }
        HMDInfoWrapper(HMDDevice* device) { if (device) device->GetDeviceInfo(&h); }
        OVR::HMDInfo h;
    };

    // Note: pHMD can be null if we are representing a virtualized debug HMD.
    Ptr<HMDDevice>          pHMD;

    // HMDInfo shouldn't change, as its string pointers are passed out.
    const HMDInfoWrapper    HMDInfoW;
    const OVR::HMDInfo&     HMDInfo;

    const char*             pLastError;

    
    // *** Sensor

    // Lock used to support thread-safe lifetime access to sensor.
    Lock                    DevicesLock;

    // Atomic integer used as a flag that we should check the sensor device.
    AtomicInt<int>          AddSensorCount;    

    // All of Sensor variables may be modified/used with DevicesLock, with exception that
    // the {SensorStarted, SensorCreated} can be read outside the lock to see
    // if device creation check is necessary.
    // Whether we called StartSensor() and requested sensor caps.    
    volatile bool           SensorStarted;
    volatile bool           SensorCreated;
    // pSensor may still be null or non-running after start if it wasn't yet available
    Ptr<SensorDevice>       pSensor;	// Head
    unsigned                SensorCaps;    

    // SensorFusion state may be accessible without a lock.
    SensorFusion            SFusion;

    
    // Vision pose tracker is currently new-allocated
    OVR_CAPI_VISION_CODE(
    Vision::PoseTracker*    pPoseTracker;
    )
    
    // Latency tester
    Ptr<LatencyTestDevice>  pLatencyTester;
    Util::LatencyTest	    LatencyUtil;
    AtomicInt<int>          AddLatencyTestCount;

    bool                    LatencyTestActive;
    unsigned char           LatencyTestDrawColor[3];

    // Using latency tester as debug display
    Ptr<LatencyTestDevice>  pLatencyTesterDisplay;
    AtomicInt<int>          AddLatencyTestDisplayCount;
    Util::LatencyTest2	    LatencyUtil2;

    bool                    LatencyTest2Active;
    unsigned char           LatencyTest2DrawColor[3];
    //bool                    ReadbackColor;

    // Rendering part
    FrameTimeManager        TimeManager;
    HMDRenderState          RenderState;
    Ptr<DistortionRenderer> pRenderer;
       
    // Last timing value reported by BeginFrame.
    double                  LastFrameTimeSeconds;    
    // Last timing value reported by GetFrameTime. These are separate since the intended
    // use is from different threads. TBD: Move to FrameTimeManager? Make atomic?
    double                  LastGetFrameTimeSeconds;

    // Last cached value returned by ovrHmd_GetString/ovrHmd_GetStringArray.
    char                    LastGetStringValue[256];
   

    // Debug flag set after ovrHmd_ConfigureRendering succeeds.
    bool                    RenderingConfigured;
    // Set after BeginFrame succeeds, and its corresponding thread id for debug checks.
    bool                    BeginFrameCalled;
    ThreadId                BeginFrameThreadId;    
    // Graphics functions are not re-entrant from other threads.
    ThreadChecker           RenderAPIThreadChecker;
    // 
    bool                    BeginFrameTimingCalled;
    
    // Flags set when we've called BeginEyeRender on a given eye.
    bool                    EyeRenderActive[2];
};


}} // namespace OVR::CAPI


#endif // OVR_CAPI_HMDState_h


