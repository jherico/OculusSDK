/************************************************************************************

Filename    :   CAPI_HMDState.cpp
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

#include "CAPI_HMDState.h"
#include "CAPI_GlobalState.h"
#include "../OVR_Profile.h"

namespace OVR { namespace CAPI {

//-------------------------------------------------------------------------------------
// ***** HMDState


HMDState::HMDState(HMDDevice* device)
    : pHMD(device), HMDInfoW(device), HMDInfo(HMDInfoW.h),
      SensorStarted(0), SensorCreated(0), SensorCaps(0),
      AddSensorCount(0), AddLatencyTestCount(0), AddLatencyTestDisplayCount(0),
      RenderState(getThis(), pHMD->GetProfile(), HMDInfoW.h),
      LastFrameTimeSeconds(0.0f), LastGetFrameTimeSeconds(0.0),
      LatencyTestActive(false),
      LatencyTest2Active(false)
{
    pLastError = 0;
    GlobalState::pInstance->AddHMD(this);
    
    // Should be in renderer?
    TimeManager.Init(RenderState.RenderInfo);

    EyeRenderActive[0] = false;
    EyeRenderActive[1] = false;

    LatencyTestDrawColor[0] = 0;
    LatencyTestDrawColor[1] = 0;
    LatencyTestDrawColor[2] = 0;

    OVR_CAPI_VISION_CODE( pPoseTracker = 0; )

    RenderingConfigured = false;
    BeginFrameCalled    = false;
    BeginFrameThreadId  = 0;
    BeginFrameTimingCalled = false;
}

HMDState::HMDState(ovrHmdType hmdType)
  : pHMD(0), HMDInfoW(hmdType), HMDInfo(HMDInfoW.h),
    SensorStarted(0), SensorCreated(0), SensorCaps(0),
    AddSensorCount(0), AddLatencyTestCount(0), AddLatencyTestDisplayCount(0),
    RenderState(getThis(), 0, HMDInfoW.h), // No profile. 
    LastFrameTimeSeconds(0.0), LastGetFrameTimeSeconds(0.0)
{
    // TBD: We should probably be looking up the default profile for the given
    // device type + user.

    pLastError = 0;
    GlobalState::pInstance->AddHMD(this);

    // Should be in renderer?
    TimeManager.Init(RenderState.RenderInfo);

    EyeRenderActive[0] = false;
    EyeRenderActive[1] = false;

    OVR_CAPI_VISION_CODE( pPoseTracker = 0; )

    RenderingConfigured = false;
    BeginFrameCalled   = false;
    BeginFrameThreadId = 0;
    BeginFrameTimingCalled = false;
}


HMDState::~HMDState()
{
    OVR_ASSERT(GlobalState::pInstance);
   
    StopSensor();
    ConfigureRendering(0,0,0,0,0);

    OVR_CAPI_VISION_CODE( OVR_ASSERT(pPoseTracker == 0); )

    GlobalState::pInstance->RemoveHMD(this);
}


//-------------------------------------------------------------------------------------
// *** Sensor 

bool HMDState::StartSensor(unsigned supportedCaps, unsigned requiredCaps)
{
    Lock::Locker lockScope(&DevicesLock);

    // TBD: Implement an optimized path that allows you to change caps such as yaw.
    if (SensorStarted)
    {
        
        if ((SensorCaps ^ ovrHmdCap_LowPersistence) == supportedCaps)
        {
            // TBD: Fast persistance switching; redesign to make this better.
            if (HMDInfo.HmdType == HmdType_CrystalCoveProto || HMDInfo.HmdType == HmdType_DK2)
            {
                // switch to full persistence
                updateLowPersistenceMode((supportedCaps & ovrHmdCap_LowPersistence) != 0);
                SensorCaps = supportedCaps;
                return true;
            }
        }

        if ((SensorCaps ^ ovrHmdCap_DynamicPrediction) == supportedCaps)
        {
            // TBD: Fast persistance switching; redesign to make this better.
            if (HMDInfo.HmdType == HmdType_DK2)
            {
                // switch to full persistence
                TimeManager.ResetFrameTiming(TimeManager.GetFrameTiming().FrameIndex,
                                             (supportedCaps & ovrHmdCap_NoVSync) ? false : true,
                                             (supportedCaps & ovrHmdCap_DynamicPrediction) ? true : false,
                                             RenderingConfigured);
                SensorCaps = supportedCaps;
                return true;
            }
        }

        StopSensor();
    }

    supportedCaps |= requiredCaps;

    // TBD: In case of sensor not being immediately available, it would be good to check
    //      yaw config availability to match it with ovrHmdCap_YawCorrection requirement.
    // 

    if (requiredCaps & ovrHmdCap_Position)
    {
        if (HMDInfo.HmdType != HmdType_CrystalCoveProto && HMDInfo.HmdType != HmdType_DK2)
        {
            pLastError = "ovrHmdCap_Position not supported on this HMD.";
            return false;
        }
    }
	if (requiredCaps & ovrHmdCap_LowPersistence)
	{
		if (HMDInfo.HmdType != HmdType_CrystalCoveProto && HMDInfo.HmdType != HmdType_DK2)
		{
			pLastError = "ovrHmdCap_LowPersistence not supported on this HMD.";
			return false;
		}
	}


    SensorCreated = false;
    pSensor.Clear();
    if (pHMD)
    {
        // Zero AddSensorCount before creation, in case it fails (or succeeds but then
        // immediately gets disconnected) followed by another Add notification.        
        AddSensorCount = 0;
        pSensor        = *pHMD->GetSensor();
    }

    if (!pSensor)
    {        
        if (requiredCaps & ovrHmdCap_Orientation)
        {
            pLastError = "Failed to create sensor.";
            return false;
        }        
        // Succeed, waiting for sensor become available later.
        LogText("StartSensor succeeded - waiting for sensor.\n");
    }
    else
    {
        pSensor->SetReportRate(500);
        SFusion.AttachToSensor(pSensor);
        applyProfileToSensorFusion();

        if (requiredCaps & ovrHmdCap_YawCorrection)
        {
            if (!SFusion.HasMagCalibration())
            {
                pLastError = "ovrHmdCap_YawCorrection not available.";
                SFusion.AttachToSensor(0);
                SFusion.Reset();
                pSensor.Clear();
                return false;
            }
        }        

        SFusion.SetYawCorrectionEnabled((supportedCaps & ovrHmdCap_YawCorrection) != 0);
        LogText("Sensor created.\n");

		if (supportedCaps & ovrHmdCap_LowPersistence)
		{
			updateLowPersistenceMode(true);
		}
		else
		{
			if (HMDInfo.HmdType == HmdType_CrystalCoveProto || HMDInfo.HmdType == HmdType_DK2)
			{
				// switch to full persistence
				updateLowPersistenceMode(false);
			}
		}

        if (HMDInfo.HmdType == HmdType_DK2)
        {
            updateLatencyTestForHmd((supportedCaps & ovrHmdCap_LatencyTest) != 0);
        }        

#ifdef OVR_CAPI_VISIONSUPPORT
        if (supportedCaps & ovrHmdCap_Position)
        {
            pPoseTracker = new Vision::PoseTracker(SFusion);
            if (pPoseTracker)
            {
                pPoseTracker->AssociateHMD(pSensor);
            }
            LogText("Sensor Pose tracker created.\n");
        }
        // TBD: How do we verify that position tracking is actually available
        //      i.e. camera is plugged in?

#endif // OVR_CAPI_VISIONSUPPORT

        SensorCreated = true;
    }
    
    SensorCaps    = supportedCaps;
    SensorStarted = true;    

    return true;
}


// Stops sensor sampling, shutting down internal resources.
void HMDState::StopSensor()
{
    Lock::Locker lockScope(&DevicesLock);

    if (SensorStarted)
    {
#ifdef OVR_CAPI_VISIONSUPPORT
        if (pPoseTracker)
        {
            // TBD: Internals not thread safe - must fix!!
            delete pPoseTracker;
            pPoseTracker = 0;
            LogText("Sensor Pose tracker destroyed.\n");
        }        
#endif // OVR_CAPI_VISION_CODE

        SFusion.AttachToSensor(0);
        SFusion.Reset();
        pSensor.Clear();
        AddSensorCount = 0;
        SensorCaps     = 0;
        SensorCreated  = false;
        SensorStarted  = false;

        LogText("StopSensor succeeded.\n");
    }
}

// Resets sensor orientation.
void HMDState::ResetSensor()
{
    SFusion.Reset();
}


// Returns prediction for time.
ovrSensorState HMDState::PredictedSensorState(double absTime)
{    
    SensorState ss;

    // We are trying to keep this path lockless unless we are notified of new device
    // creation while not having a sensor yet. It's ok to check SensorCreated volatile
    // flag here, since GetSensorStateAtTime() is internally lockless and safe.

    if (SensorCreated || checkCreateSensor())
    {   
        ss = SFusion.GetSensorStateAtTime(absTime);

        if (!(ss.StatusFlags & ovrStatus_OrientationTracked))
        {
            Lock::Locker lockScope(&DevicesLock);

#ifdef OVR_CAPI_VISIONSUPPORT
            if (pPoseTracker)
            {
                // TBD: Internals not thread safe - must fix!!
                delete pPoseTracker;
                pPoseTracker = 0;
                LogText("Sensor Pose tracker destroyed.\n");
            }        
#endif // OVR_CAPI_VISION_CODE
            // Not needed yet; SFusion.AttachToSensor(0);
            // This seems to reset orientation anyway...
            pSensor.Clear();
            SensorCreated = false;
        }
    }
    else
    {
        // SensorState() defaults to 0s.
        // ss.Pose.Orientation       = Quatf();
        // ..

        // John:
        // We still want valid times so frames will get a delta-time
        // and allow operation with a joypad when the sensor isn't
        // connected.
        ss.Recorded.TimeInSeconds  = absTime;
        ss.Predicted.TimeInSeconds = absTime;
    }

    ss.StatusFlags |= ovrStatus_HmdConnected;
    return ss;
}


bool  HMDState::checkCreateSensor()
{
    if (!(SensorStarted && !SensorCreated && AddSensorCount))
        return false;

    Lock::Locker lockScope(&DevicesLock);

    // Re-check condition once in the lock, in case the state changed.
    if (SensorStarted && !SensorCreated && AddSensorCount)
    {        
        if (pHMD)
        {
            AddSensorCount = 0;
            pSensor        = *pHMD->GetSensor();
        }

        if (pSensor)
        {
            pSensor->SetReportRate(500);
            SFusion.AttachToSensor(pSensor);
            SFusion.SetYawCorrectionEnabled((SensorCaps & ovrHmdCap_YawCorrection) != 0);
            applyProfileToSensorFusion();

#ifdef OVR_CAPI_VISIONSUPPORT
            if (SensorCaps & ovrHmdCap_Position)
            {
                pPoseTracker = new Vision::PoseTracker(SFusion);
                if (pPoseTracker)
                {
                    pPoseTracker->AssociateHMD(pSensor);
                }
                LogText("Sensor Pose tracker created.\n");
            }
#endif // OVR_CAPI_VISION_CODE

            LogText("Sensor created.\n");

            SensorCreated = true;
            return true;
        }
    }

    return SensorCreated;
}

bool HMDState::GetSensorDesc(ovrSensorDesc* descOut)
{
    Lock::Locker lockScope(&DevicesLock);

    if (SensorCreated)
    {
        OVR_ASSERT(pSensor);
        OVR::SensorInfo si;
        pSensor->GetDeviceInfo(&si);
        descOut->VendorId  = si.VendorId;
        descOut->ProductId = si.ProductId;
        OVR_ASSERT(si.SerialNumber.GetSize() <= sizeof(descOut->SerialNumber));
        OVR_strcpy(descOut->SerialNumber, sizeof(descOut->SerialNumber), si.SerialNumber.ToCStr());
        return true;
    }
    return false;
}


void HMDState::applyProfileToSensorFusion()
{
    Profile* profile = pHMD ? pHMD->GetProfile() : 0;
    SFusion.SetUserHeadDimensions ( profile, RenderState.RenderInfo );
}

void HMDState::updateLowPersistenceMode(bool lowPersistence) const
{
	OVR_ASSERT(pSensor);
	DisplayReport dr;
	pSensor->GetDisplayReport(&dr);

	dr.Persistence = (UInt16) (dr.TotalRows * (lowPersistence ? 0.18f : 1.0f));
	dr.Brightness = lowPersistence ? 255 : 0;
    
	pSensor->SetDisplayReport(dr);
}

void HMDState::updateLatencyTestForHmd(bool latencyTesting)
{
    if (pSensor.GetPtr())
    {
        DisplayReport dr;
        pSensor->GetDisplayReport(&dr);

        dr.ReadPixel = latencyTesting;

        pSensor->SetDisplayReport(dr);
    }

    if (latencyTesting)
    {
        LatencyUtil2.SetSensorDevice(pSensor.GetPtr());
    }
    else
    {
        LatencyUtil2.SetSensorDevice(NULL);
    }
}

//-------------------------------------------------------------------------------------
// ***** Property Access

// TBD: This all needs to be cleaned up and organized into namespaces.

float HMDState::getFloatValue(const char* propertyName, float defaultVal)
{
	if (OVR_strcmp(propertyName, "LensSeparation") == 0)
	{
		return HMDInfo.LensSeparationInMeters;
	}
    else if (OVR_strcmp(propertyName, "CenterPupilDepth") == 0)
    {        
        return SFusion.GetCenterPupilDepth();
    }
	else if (pHMD)
	{
		Profile* p = pHMD->GetProfile();
		if (p)
		{
			return p->GetFloatValue(propertyName, defaultVal);
		}
	}
	return defaultVal;
}

bool HMDState::setFloatValue(const char* propertyName, float value)
{
    if (OVR_strcmp(propertyName, "CenterPupilDepth") == 0)
    {
        SFusion.SetCenterPupilDepth(value);
        return true;
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
            float data[2] = { HMDInfo.ScreenSizeInMeters.w, HMDInfo.ScreenSizeInMeters.h };

            return CopyFloatArrayWithLimit(values, arraySize, data, 2);
		}
        else if (OVR_strcmp(propertyName, "DistortionClearColor") == 0)
        {
            return CopyFloatArrayWithLimit(values, arraySize, RenderState.ClearColor, 4);
        }
        else if (OVR_strcmp(propertyName, "DK2Latency") == 0)
        {
            if (HMDInfo.HmdType != HmdType_DK2)
                return 0;

            float data[3];            
            TimeManager.GetLatencyTimings(data);
            
            return CopyFloatArrayWithLimit(values, arraySize, data, 3);
        }

        /*
        else if (OVR_strcmp(propertyName, "CenterPupilDepth") == 0)
        {
            if (arraySize >= 1)
            {
                values[0] = SFusion.GetCenterPupilDepth();
                return 1;
            }
            return 0;
        } */
		else if (pHMD)
		{        
			Profile* p = pHMD->GetProfile();

			// TBD: Not quite right. Should update profile interface, so that
			//      we can return 0 in all conditions if property doesn't exist.
			if (p)
			{
				unsigned count = p->GetFloatValues(propertyName, values, arraySize);
				return count;
			}
		}
	}

	return 0;
}

bool HMDState::setFloatArray(const char* propertyName, float values[], unsigned arraySize)
{
    if (!arraySize)
        return false;
    
    if (OVR_strcmp(propertyName, "DistortionClearColor") == 0)
    {
        CopyFloatArrayWithLimit(RenderState.ClearColor, 4, values, arraySize);
        return true;
    }
    return false;
}


const char* HMDState::getString(const char* propertyName, const char* defaultVal)
{
	if (pHMD)
	{
		// For now, just access the profile.
		Profile* p = pHMD->GetProfile();

		LastGetStringValue[0] = 0;
		if (p && p->GetValue(propertyName, LastGetStringValue, sizeof(LastGetStringValue)))
		{
			return LastGetStringValue;
		}
	}

	return defaultVal;
}

//-------------------------------------------------------------------------------------
// *** Latency Test

bool HMDState::ProcessLatencyTest(unsigned char rgbColorOut[3])
{
    bool result = false;

    // Check create.
    if (pLatencyTester)
    {
        if (pLatencyTester->IsConnected())
        {
            Color colorToDisplay;

            LatencyUtil.ProcessInputs();
            result = LatencyUtil.DisplayScreenColor(colorToDisplay);
            rgbColorOut[0] = colorToDisplay.R;
            rgbColorOut[1] = colorToDisplay.G;
            rgbColorOut[2] = colorToDisplay.B;
        }
        else
        {
            // Disconnect.
            LatencyUtil.SetDevice(NULL);
            pLatencyTester = 0;
            LogText("LATENCY SENSOR disconnected.\n");
        }
    }
    else if (AddLatencyTestCount > 0)
    {
        // This might have some unlikely race condition issue which could cause us to miss a device...
        AddLatencyTestCount = 0;

        pLatencyTester = *GlobalState::pInstance->GetManager()->
                            EnumerateDevices<LatencyTestDevice>().CreateDevice();
        if (pLatencyTester)
        {
            LatencyUtil.SetDevice(pLatencyTester);
            LogText("LATENCY TESTER connected\n");
        }        
    }
    
    return result;
}

void HMDState::ProcessLatencyTest2(unsigned char rgbColorOut[3], double startTime)
{
    // Check create.
    if (!(SensorCaps & ovrHmdCap_LatencyTest))
        return;

    if (pLatencyTesterDisplay && !LatencyUtil2.HasDisplayDevice())
    {
        if (!pLatencyTesterDisplay->IsConnected())
        {
            LatencyUtil2.SetDisplayDevice(NULL);
        }
    }
    else if (AddLatencyTestDisplayCount > 0)
    {
        // This might have some unlikely race condition issue
        // which could cause us to miss a device...
        AddLatencyTestDisplayCount = 0;

        pLatencyTesterDisplay = *GlobalState::pInstance->GetManager()->
                                 EnumerateDevices<LatencyTestDevice>().CreateDevice();
        if (pLatencyTesterDisplay)
        {
            LatencyUtil2.SetDisplayDevice(pLatencyTesterDisplay);
        }
    }

    if (LatencyUtil2.HasDevice() && pSensor && pSensor->IsConnected())
    {
        LatencyUtil2.BeginTest(startTime);

        Color colorToDisplay;
        LatencyTest2Active = LatencyUtil2.DisplayScreenColor(colorToDisplay);
        rgbColorOut[0] = colorToDisplay.R;
        rgbColorOut[1] = colorToDisplay.G;
        rgbColorOut[2] = colorToDisplay.B;
    }
    else
    {
        LatencyTest2Active = false;
    }
}

//-------------------------------------------------------------------------------------
// *** Rendering

bool HMDState::ConfigureRendering(ovrEyeRenderDesc eyeRenderDescOut[2],
                                  const ovrEyeDesc eyeDescIn[2],
                                  const ovrRenderAPIConfig* apiConfig,
                                  unsigned hmdCaps,
                                  unsigned distortionCaps)
{
    ThreadChecker::Scope checkScope(&RenderAPIThreadChecker, "ovrHmd_ConfigureRendering");

    // null -> shut down.
    if (!apiConfig)
    {
        if (pRenderer)
            pRenderer.Clear();        
        RenderingConfigured = false; 
        return true;
    }

    if (pRenderer &&
        (apiConfig->Header.API != pRenderer->GetRenderAPI()))
    {
        // Shutdown old renderer.
        if (pRenderer)
            pRenderer.Clear();
    }


    // Step 1: do basic setup configuration
    RenderState.setupRenderDesc(eyeRenderDescOut, eyeDescIn);
    RenderState.HMDCaps        = hmdCaps;         // Any cleaner way?
    RenderState.DistortionCaps = distortionCaps;

    TimeManager.ResetFrameTiming(0,
                                 (hmdCaps & ovrHmdCap_NoVSync) ? false : true,
                                 (hmdCaps & ovrHmdCap_DynamicPrediction) ? true : false,
                                 true);

    LastFrameTimeSeconds = 0.0f;

    // Set RenderingConfigured early to avoid ASSERTs in renderer initialization.
    RenderingConfigured = true;

    if (!pRenderer)
    {
        pRenderer = *DistortionRenderer::APICreateRegistry
                        [apiConfig->Header.API](this, TimeManager, RenderState);
    }

    if (!pRenderer ||
        !pRenderer->Initialize(apiConfig, hmdCaps, distortionCaps))
    {
        RenderingConfigured = false;
        return false;
    }    

    return true;
}



ovrPosef HMDState::BeginEyeRender(ovrEyeType eye)
{
    // Debug checks.
    checkBeginFrameScope("ovrHmd_BeginEyeRender");
    ThreadChecker::Scope checkScope(&RenderAPIThreadChecker, "ovrHmd_BeginEyeRender");

    // Unknown eyeId provided in ovrHmd_BeginEyeRender
    OVR_ASSERT_LOG(eye == ovrEye_Left || eye == ovrEye_Right,
                   ("ovrHmd_BeginEyeRender eyeId out of range."));     
    OVR_ASSERT_LOG(EyeRenderActive[eye] == false,
                   ("Multiple calls to ovrHmd_BeginEyeRender for the same eye."));

    EyeRenderActive[eye] = true;
    
    // Only process latency tester for drawing the left eye (assumes left eye is drawn first)
    if (pRenderer && eye == 0)
    {
        LatencyTestActive = ProcessLatencyTest(LatencyTestDrawColor);
    }

    return ovrHmd_GetEyePose(this, eye);
}


void HMDState::EndEyeRender(ovrEyeType eye, ovrPosef renderPose, ovrTexture* eyeTexture)
{
    // Debug checks.
    checkBeginFrameScope("ovrHmd_EndEyeRender");
    ThreadChecker::Scope checkScope(&RenderAPIThreadChecker, "ovrHmd_EndEyeRender");

    if (!EyeRenderActive[eye])
    {
        OVR_ASSERT_LOG(false,
                       ("ovrHmd_EndEyeRender called without ovrHmd_BeginEyeRender."));
        return;
    }

    RenderState.EyeRenderPoses[eye] = renderPose;

    if (pRenderer)
        pRenderer->SubmitEye(eye, eyeTexture);

    EyeRenderActive[eye] = false;
}

}} // namespace OVR::CAPI

