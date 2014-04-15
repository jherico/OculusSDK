/************************************************************************************

Filename    :   OVR_CAPI.cpp
Content     :   Experimental simple C interface to the HMD - version 1.
Created     :   November 30, 2013
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

#include "OVR_CAPI.h"
#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_System.h"
#include "OVR_Stereo.h"
#include "OVR_Profile.h"

#include "CAPI/CAPI_GlobalState.h"
#include "CAPI/CAPI_HMDState.h"
#include "CAPI/CAPI_FrameTimeManager.h"


using namespace OVR;
using namespace OVR::Util::Render;

//-------------------------------------------------------------------------------------
// Math
namespace OVR {


// ***** FovPort

// C-interop support: FovPort <-> ovrFovPort
FovPort::FovPort(const ovrFovPort &src)
    : UpTan(src.UpTan), DownTan(src.DownTan), LeftTan(src.LeftTan), RightTan(src.RightTan)
{ }    

FovPort::operator const ovrFovPort () const
{
    ovrFovPort result;
    result.LeftTan  = LeftTan;
    result.RightTan = RightTan;
    result.UpTan    = UpTan;
    result.DownTan  = DownTan;
    return result;
}

// Converts Fov Tan angle units to [-1,1] render target NDC space
Vector2f FovPort::TanAngleToRendertargetNDC(Vector2f const &tanEyeAngle)
{  
    ScaleAndOffset2D eyeToSourceNDC = CreateNDCScaleAndOffsetFromFov(*this);
    return tanEyeAngle * eyeToSourceNDC.Scale + eyeToSourceNDC.Offset;
}


// ***** SensorState

SensorState::SensorState(const ovrSensorState& s)
{
    Predicted       = s.Predicted;
    Recorded        = s.Recorded;
    Temperature     = s.Temperature;
    StatusFlags     = s.StatusFlags;
}

SensorState::operator const ovrSensorState() const
{
    ovrSensorState result;
    result.Predicted    = Predicted;
    result.Recorded     = Recorded;
    result.Temperature  = Temperature;
    result.StatusFlags  = StatusFlags;
    return result;
}


} // namespace OVR

//-------------------------------------------------------------------------------------

using namespace OVR::CAPI;

#ifdef __cplusplus 
extern "C" {
#endif


// Used to generate projection from ovrEyeDesc::Fov
OVR_EXPORT ovrMatrix4f ovrMatrix4f_Projection(ovrFovPort fov, float znear, float zfar, ovrBool rightHanded)
{
    return CreateProjection(rightHanded ? true : false, fov, znear, zfar);
}


OVR_EXPORT ovrMatrix4f ovrMatrix4f_OrthoSubProjection(ovrMatrix4f projection, ovrVector2f orthoScale,
                                                      float orthoDistance, float eyeViewAdjustX)
{

    float orthoHorizontalOffset = eyeViewAdjustX / orthoDistance;

    // Current projection maps real-world vector (x,y,1) to the RT.
    // We want to find the projection that maps the range [-FovPixels/2,FovPixels/2] to
    // the physical [-orthoHalfFov,orthoHalfFov]
    // Note moving the offset from M[0][2]+M[1][2] to M[0][3]+M[1][3] - this means
    // we don't have to feed in Z=1 all the time.
    // The horizontal offset math is a little hinky because the destination is
    // actually [-orthoHalfFov+orthoHorizontalOffset,orthoHalfFov+orthoHorizontalOffset]
    // So we need to first map [-FovPixels/2,FovPixels/2] to
    //                         [-orthoHalfFov+orthoHorizontalOffset,orthoHalfFov+orthoHorizontalOffset]:
    // x1 = x0 * orthoHalfFov/(FovPixels/2) + orthoHorizontalOffset;
    //    = x0 * 2*orthoHalfFov/FovPixels + orthoHorizontalOffset;
    // But then we need the sam mapping as the existing projection matrix, i.e.
    // x2 = x1 * Projection.M[0][0] + Projection.M[0][2];
    //    = x0 * (2*orthoHalfFov/FovPixels + orthoHorizontalOffset) * Projection.M[0][0] + Projection.M[0][2];
    //    = x0 * Projection.M[0][0]*2*orthoHalfFov/FovPixels +
    //      orthoHorizontalOffset*Projection.M[0][0] + Projection.M[0][2];
    // So in the new projection matrix we need to scale by Projection.M[0][0]*2*orthoHalfFov/FovPixels and
    // offset by orthoHorizontalOffset*Projection.M[0][0] + Projection.M[0][2].

    Matrix4f ortho;
    ortho.M[0][0] = projection.M[0][0] * orthoScale.x;
    ortho.M[0][1] = 0.0f;
    ortho.M[0][2] = 0.0f;
    ortho.M[0][3] = -projection.M[0][2] + ( orthoHorizontalOffset * projection.M[0][0] );

    ortho.M[1][0] = 0.0f;
    ortho.M[1][1] = -projection.M[1][1] * orthoScale.y;       // Note sign flip (text rendering uses Y=down).
    ortho.M[1][2] = 0.0f;
    ortho.M[1][3] = -projection.M[1][2];

    /*
    if ( fabsf ( zNear - zFar ) < 0.001f )
    {
        ortho.M[2][0] = 0.0f;
        ortho.M[2][1] = 0.0f;
        ortho.M[2][2] = 0.0f;
        ortho.M[2][3] = zFar;
    }
    else
    {
        ortho.M[2][0] = 0.0f;
        ortho.M[2][1] = 0.0f;
        ortho.M[2][2] = zFar / (zNear - zFar);
        ortho.M[2][3] = (zFar * zNear) / (zNear - zFar);
    }
    */

    // MA: Undo effect of sign
    ortho.M[2][0] = 0.0f;
    ortho.M[2][1] = 0.0f;
    //ortho.M[2][2] = projection.M[2][2] * projection.M[3][2] * -1.0f; // reverse right-handedness
    ortho.M[2][2] = 0.0f;
    ortho.M[2][3] = 0.0f;
        //projection.M[2][3];

    // No perspective correction for ortho.
    ortho.M[3][0] = 0.0f;
    ortho.M[3][1] = 0.0f;
    ortho.M[3][2] = 0.0f;
    ortho.M[3][3] = 1.0f;

    return ortho;
}


OVR_EXPORT double ovr_GetTimeInSeconds()
{
    return Timer::GetSeconds();
}

// Waits until the specified absolute time.
OVR_EXPORT double ovr_WaitTillTime(double absTime)
{
    volatile int i;
    double       initialTime = ovr_GetTimeInSeconds();
    double       newTime     = initialTime;
    
    while(newTime < absTime)
    {
        for (int j = 0; j < 50; j++)
            i = 0;
        newTime = ovr_GetTimeInSeconds();
    }

    // How long we waited
    return newTime - initialTime;
}

//-------------------------------------------------------------------------------------

// 1. Init/shutdown.

static ovrBool CAPI_SystemInitCalled = FALSE;

OVR_EXPORT ovrBool ovr_Initialize()
{
    if (OVR::CAPI::GlobalState::pInstance)
        return TRUE;

    // We must set up the system for the plugin to work
    if (!OVR::System::IsInitialized())
    {        
        OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));
        CAPI_SystemInitCalled = TRUE;
    }

    // Constructor detects devices
    GlobalState::pInstance = new GlobalState;
    return TRUE;
}

OVR_EXPORT void ovr_Shutdown()
{
    if (!GlobalState::pInstance)
       return;

    delete GlobalState::pInstance;
    GlobalState::pInstance = 0;

    // We should clean up the system to be complete
    if (CAPI_SystemInitCalled)
    {
        OVR::System::Destroy();
        CAPI_SystemInitCalled = FALSE;
    }    
    return;
}


// There is a thread safety issue with ovrHmd_Detect in that multiple calls from different
// threads can corrupt the global array state. This would lead to two problems:
//  a) Create(index) enumerator may miss or overshoot items. Probably not a big deal
//     as game logic can easily be written to only do Detect(s)/Creates in one place.
//     The alternative would be to return list handle.
//  b) TBD: Un-mutexed Detect access from two threads could lead to crash. We should
//         probably check this.
//

OVR_EXPORT int ovrHmd_Detect()
{
    if (!GlobalState::pInstance)
        return 0;
    return GlobalState::pInstance->EnumerateDevices();
}


// ovrHmd_Create us explicitly separated from StartSensor and Configure to allow creation of 
// a relatively light-weight handle that would reference the device going forward and would 
// survive future ovrHmd_Detect calls. That is once ovrHMD is returned, index is no longer
// necessary and can be changed by a ovrHmd_Detect call.

OVR_EXPORT ovrHmd ovrHmd_Create(int index)
{
    if (!GlobalState::pInstance)
        return 0;
    Ptr<HMDDevice> device = *GlobalState::pInstance->CreateDevice(index);
    if (!device)
        return 0;

    HMDState* hmds = new HMDState(device);
    if (!hmds)
        return 0;

    return hmds;
}

OVR_EXPORT ovrHmd ovrHmd_CreateDebug(ovrHmdType type)
{
    if (!GlobalState::pInstance)
        return 0;    

    HMDState* hmds = new HMDState(type);    
    return hmds;
}

OVR_EXPORT void ovrHmd_Destroy(ovrHmd hmd)
{
    if (!hmd)
        return;
    // TBD: Any extra shutdown?
    HMDState* hmds = (HMDState*)hmd;
        
    {   // Thread checker in its own scope, to avoid access after 'delete'.
        // Essentially just checks that no other RenderAPI function is executing.
        ThreadChecker::Scope checkScope(&hmds->RenderAPIThreadChecker, "ovrHmd_Destroy");
    }    

    delete (HMDState*)hmd;
}


OVR_EXPORT const char* ovrHmd_GetLastError(ovrHmd hmd)
{
    using namespace OVR;
    if (!hmd)
    {
        if (!GlobalState::pInstance)  
            return "LibOVR not initialized.";
        return GlobalState::pInstance->GetLastError();
    }
    HMDState* p = (HMDState*)hmd;
    return p->GetLastError();
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

OVR_EXPORT ovrBool ovrHmd_StartSensor(ovrHmd hmd, unsigned int supportedCaps, unsigned int requiredCaps)
{
    HMDState* p = (HMDState*)hmd;
    // TBD: Decide if we null-check arguments.
    return p->StartSensor(supportedCaps, requiredCaps);
}

OVR_EXPORT void ovrHmd_StopSensor(ovrHmd hmd)
{
    HMDState* p = (HMDState*)hmd;
    p->StopSensor();
}

OVR_EXPORT void ovrHmd_ResetSensor(ovrHmd hmd)
{
    HMDState* p = (HMDState*)hmd;
    p->ResetSensor();
}

OVR_EXPORT ovrSensorState ovrHmd_GetSensorState(ovrHmd hmd, double absTime)
{
    HMDState* p = (HMDState*)hmd;
    return p->PredictedSensorState(absTime);
}

// Returns information about a sensor. Only valid after SensorStart.
OVR_EXPORT ovrBool ovrHmd_GetSensorDesc(ovrHmd hmd, ovrSensorDesc* descOut)
{
    HMDState* p = (HMDState*)hmd;
    return p->GetSensorDesc(descOut) ? TRUE : FALSE;
}



//-------------------------------------------------------------------------------------
// *** General Setup


OVR_EXPORT void ovrHmd_GetDesc(ovrHmd hmd, ovrHmdDesc* desc)
{
    HMDState* hmds = (HMDState*)hmd;    
    *desc = hmds->RenderState.GetDesc();
    desc->Handle = hmd;
}

// Per HMD -> calculateIdealPixelSize
OVR_EXPORT ovrSizei ovrHmd_GetFovTextureSize(ovrHmd hmd, ovrEyeType eye, ovrFovPort fov,
                                             float pixelsPerDisplayPixel)
{
    if (!hmd) return Sizei(0);
    
    HMDState* hmds = (HMDState*)hmd;
    return hmds->RenderState.GetFOVTextureSize(eye, fov, pixelsPerDisplayPixel);
}


//-------------------------------------------------------------------------------------


OVR_EXPORT 
ovrBool ovrHmd_ConfigureRendering( ovrHmd hmd,
                                   const ovrRenderAPIConfig* apiConfig,
                                   unsigned int hmdCaps,
                                   unsigned int distortionCaps,
                                   const ovrEyeDesc eyeDescIn[2],
                                   ovrEyeRenderDesc eyeRenderDescOut[2] )
{
    if (!hmd) return FALSE;
    return ((HMDState*)hmd)->ConfigureRendering(eyeRenderDescOut, eyeDescIn,
                                                apiConfig, hmdCaps, distortionCaps);
}



// TBD: MA - Deprecated, need alternative
void ovrHmd_SetVsync(ovrHmd hmd, ovrBool vsync)
{
    if (!hmd) return;

    return ((HMDState*)hmd)->TimeManager.SetVsync(vsync? true : false);
}


OVR_EXPORT ovrFrameTiming ovrHmd_BeginFrame(ovrHmd hmd, unsigned int frameIndex)
{           
    HMDState* hmds = (HMDState*)hmd;
    if (!hmds)
    {
        ovrFrameTiming f;
        memset(&f, 0, sizeof(f));
        return f;
    }

    // Check: Proper configure and threading state for the call.
    hmds->checkRenderingConfigured("ovrHmd_BeginFrame");
    OVR_ASSERT_LOG(hmds->BeginFrameCalled == false, ("ovrHmd_BeginFrame called multiple times."));
    ThreadChecker::Scope checkScope(&hmds->RenderAPIThreadChecker, "ovrHmd_BeginFrame");
    
    hmds->BeginFrameCalled   = true;
    hmds->BeginFrameThreadId = OVR::GetCurrentThreadId();

    return ovrHmd_BeginFrameTiming(hmd, frameIndex);
}


// Renders textures to frame buffer
OVR_EXPORT void ovrHmd_EndFrame(ovrHmd hmd)
{
    HMDState* hmds = (HMDState*)hmd;
    if (!hmds) return;

    // Debug state checks: Must be in BeginFrame, on the same thread.
    hmds->checkBeginFrameScope("ovrHmd_EndFrame");
    ThreadChecker::Scope checkScope(&hmds->RenderAPIThreadChecker, "ovrHmd_EndFrame");  

    // TBD: Move directly into renderer
    bool dk2LatencyTest = (hmds->HMDInfo.HmdType == HmdType_DK2) &&
                           (hmds->SensorCaps & ovrHmdCap_LatencyTest);
    if (dk2LatencyTest)
    {
        hmds->LatencyTest2DrawColor[0] = hmds->TimeManager.GetFrameLatencyTestDrawColor();
        hmds->LatencyTest2DrawColor[1] = hmds->LatencyTest2DrawColor[0];
        hmds->LatencyTest2DrawColor[2] = hmds->LatencyTest2DrawColor[0];
    }

    if (hmds->pRenderer)
    {
        hmds->pRenderer->EndFrame(true,
                                  hmds->LatencyTestActive ? hmds->LatencyTestDrawColor : NULL,
                            
                                  // MA: Use this color since we are running DK2 test all the time.
                                  dk2LatencyTest ? hmds->LatencyTest2DrawColor : 0
                                  //hmds->LatencyTest2Active ? hmds->LatencyTest2DrawColor : NULL
                                  );
    }
    // Call after present
    ovrHmd_EndFrameTiming(hmd);

    if (dk2LatencyTest)
    {
        hmds->TimeManager.UpdateFrameLatencyTrackingAfterEndFrame(
            hmds->LatencyTest2DrawColor[0], hmds->LatencyUtil2.GetLocklessState());
    }

    // Out of BeginFrame
    hmds->BeginFrameThreadId = 0;
    hmds->BeginFrameCalled   = false;
}


OVR_EXPORT ovrPosef ovrHmd_BeginEyeRender(ovrHmd hmd, ovrEyeType eye)
{
    HMDState* hmds = (HMDState*)hmd;
    if (!hmds) return ovrPosef();
    return hmds->BeginEyeRender(eye);
}

OVR_EXPORT void ovrHmd_EndEyeRender(ovrHmd hmd, ovrEyeType eye,
                                    ovrPosef renderPose, ovrTexture* eyeTexture)
{
    HMDState* hmds = (HMDState*)hmd;
    if (!hmds) return;
    hmds->EndEyeRender(eye, renderPose, eyeTexture);
}


//-------------------------------------------------------------------------------------
// ***** Frame Timing logic


OVR_EXPORT ovrFrameTiming ovrHmd_GetFrameTiming(ovrHmd hmd, unsigned int frameIndex)
{
    ovrFrameTiming f;
    memset(&f, 0, sizeof(f));

    HMDState* hmds = (HMDState*)hmd;
    if (hmds)
    {
        FrameTimeManager::Timing frameTiming = hmds->TimeManager.GetFrameTiming(frameIndex);

        f.ThisFrameSeconds     = frameTiming.ThisFrameTime;
        f.NextFrameSeconds       = frameTiming.NextFrameTime;
        f.TimewarpPointSeconds  = frameTiming.TimewarpPointTime;
        f.ScanoutMidpointSeconds= frameTiming.MidpointTime;
        f.EyeScanoutSeconds[0]  = frameTiming.EyeRenderTimes[0];
        f.EyeScanoutSeconds[1]  = frameTiming.EyeRenderTimes[1];

         // Compute DeltaSeconds.
        f.DeltaSeconds = (hmds->LastGetFrameTimeSeconds == 0.0f) ? 0.0f :
                         (float) (f.ThisFrameSeconds - hmds->LastFrameTimeSeconds);    
        hmds->LastGetFrameTimeSeconds = f.ThisFrameSeconds;
        if (f.DeltaSeconds > 1.0f)
            f.DeltaSeconds = 1.0f;
    }
        
    return f;
}

OVR_EXPORT ovrFrameTiming ovrHmd_BeginFrameTiming(ovrHmd hmd, unsigned int frameIndex)
{
    ovrFrameTiming f;
    memset(&f, 0, sizeof(f));

    HMDState* hmds = (HMDState*)hmd;
    if (!hmds) return f;

    // Check: Proper state for the call.    
    OVR_ASSERT_LOG(hmds->BeginFrameTimingCalled == false,
                    ("ovrHmd_BeginFrameTiming called multiple times."));    
    hmds->BeginFrameTimingCalled = true;

    double thisFrameTime = hmds->TimeManager.BeginFrame(frameIndex);        

    const FrameTimeManager::Timing &frameTiming = hmds->TimeManager.GetFrameTiming();

    f.ThisFrameSeconds     = thisFrameTime;
    f.NextFrameSeconds       = frameTiming.NextFrameTime;
    f.TimewarpPointSeconds  = frameTiming.TimewarpPointTime;
    f.ScanoutMidpointSeconds= frameTiming.MidpointTime;
    f.EyeScanoutSeconds[0]  = frameTiming.EyeRenderTimes[0];
    f.EyeScanoutSeconds[1]  = frameTiming.EyeRenderTimes[1];

    // Compute DeltaSeconds.
    f.DeltaSeconds = (hmds->LastFrameTimeSeconds == 0.0f) ? 0.0f :
                     (float) (thisFrameTime - hmds->LastFrameTimeSeconds);    
    hmds->LastFrameTimeSeconds = thisFrameTime;
    if (f.DeltaSeconds > 1.0f)
        f.DeltaSeconds = 1.0f;

    return f;
}


OVR_EXPORT void ovrHmd_EndFrameTiming(ovrHmd hmd)
{
    HMDState* hmds = (HMDState*)hmd;
    if (!hmds) return;

    // Debug state checks: Must be in BeginFrameTiming, on the same thread.
    hmds->checkBeginFrameTimingScope("ovrHmd_EndTiming");
   // MA TBD: Correct chek or not?
   // ThreadChecker::Scope checkScope(&hmds->RenderAPIThreadChecker, "ovrHmd_EndFrame");

    hmds->TimeManager.EndFrame();   
    hmds->BeginFrameTimingCalled = false;
}


OVR_EXPORT void ovrHmd_ResetFrameTiming(ovrHmd hmd,  unsigned int frameIndex, bool vsync) 
{
    HMDState* hmds = (HMDState*)hmd;
    if (!hmds) return;
    
    hmds->TimeManager.ResetFrameTiming(frameIndex, vsync, false,
                                       hmds->RenderingConfigured);
    hmds->LastFrameTimeSeconds    = 0.0;
    hmds->LastGetFrameTimeSeconds = 0.0;
}



OVR_EXPORT ovrPosef ovrHmd_GetEyePose(ovrHmd hmd, ovrEyeType eye)
{
    HMDState* hmds = (HMDState*)hmd;
    if (!hmds) return ovrPosef();    

    hmds->checkBeginFrameTimingScope("ovrHmd_GetEyePose");
    return hmds->TimeManager.GetEyePredictionPose(hmd, eye);
}


OVR_EXPORT void ovrHmd_GetEyeTimewarpMatrices(ovrHmd hmd, ovrEyeType eye,
                                              ovrPosef renderPose, ovrMatrix4f twmOut[2])
{
    HMDState* hmds = (HMDState*)hmd;
    if (!hmd)
        return;

    // Debug checks: BeginFrame was called, on the same thread.
    hmds->checkBeginFrameTimingScope("ovrHmd_GetTimewarpEyeMatrices");   

    hmds->TimeManager.GetTimewarpMatrices(hmd, eye, renderPose, twmOut);

    /*
    // MA: Took this out because new latency test approach just sames
    //     the sample times in FrameTimeManager.
    // TODO: if no timewarp, then test latency in begin eye render
    if (eye == 0)
    {        
        hmds->ProcessLatencyTest2(hmds->LatencyTest2DrawColor, -1.0f);
    }
    */
}



OVR_EXPORT ovrEyeRenderDesc ovrHmd_GetRenderDesc(ovrHmd hmd, const ovrEyeDesc eyeDesc)
{
    ovrEyeRenderDesc erd;
   
    HMDState* hmds = (HMDState*)hmd;
    if (!hmds)
    {
        memset(&erd, 0, sizeof(erd));
        return erd;
    }

    return hmds->RenderState.calcRenderDesc(eyeDesc);
}



#define OVR_OFFSET_OF(s, field) ((size_t)&((s*)0)->field)



// Generate distortion mesh per eye.
// scaleAndOffsetOut - this will be needed for shader
OVR_EXPORT ovrBool ovrHmd_CreateDistortionMesh( ovrHmd hmd, ovrEyeDesc eyeDesc,
                                                unsigned int distortionCaps,
                                                ovrVector2f uvScaleOffsetOut[2], 
                                                ovrDistortionMesh *meshData )
{
    if (!meshData)
        return FALSE;
    HMDState* hmds = (HMDState*)hmd;

    // Not used now, but Chromatic flag or others could possibly be checked for in the future.
    OVR_UNUSED1(distortionCaps); 
    
    // TBD: We should probably be sharing some C API structures with C++ to avoid this mess...
    OVR_COMPILER_ASSERT(sizeof(DistortionMeshVertexData)                       == sizeof(ovrDistortionVertex));
    OVR_COMPILER_ASSERT(OVR_OFFSET_OF(DistortionMeshVertexData, ScreenPosNDC)  == OVR_OFFSET_OF(ovrDistortionVertex, Pos));
    OVR_COMPILER_ASSERT(OVR_OFFSET_OF(DistortionMeshVertexData, TimewarpLerp)  == OVR_OFFSET_OF(ovrDistortionVertex, TimeWarpFactor));
    OVR_COMPILER_ASSERT(OVR_OFFSET_OF(DistortionMeshVertexData, Shade)         == OVR_OFFSET_OF(ovrDistortionVertex, VignetteFactor));
    OVR_COMPILER_ASSERT(OVR_OFFSET_OF(DistortionMeshVertexData, TanEyeAnglesR) == OVR_OFFSET_OF(ovrDistortionVertex, TexR));
    OVR_COMPILER_ASSERT(OVR_OFFSET_OF(DistortionMeshVertexData, TanEyeAnglesG) == OVR_OFFSET_OF(ovrDistortionVertex, TexG));
    OVR_COMPILER_ASSERT(OVR_OFFSET_OF(DistortionMeshVertexData, TanEyeAnglesB) == OVR_OFFSET_OF(ovrDistortionVertex, TexB));


    // *** Calculate a part of "StereoParams" needed for mesh generation

    // Note that mesh distortion generation is invariant of RenderTarget UVs, allowing
    // render target size and location to be changed after the fact dynamically. 
    // eyeToSourceUV is computed here for convenience, so that users don't need
    // to call ovrHmd_GetRenderScaleAndOffset unless changing RT dynamically.

    
    const HmdRenderInfo&  hmdri          = hmds->RenderState.RenderInfo;    
    StereoEye             stereoEye      = (eyeDesc.Eye == ovrEye_Left) ? StereoEye_Left : StereoEye_Right;

    const DistortionRenderDesc& distortion = hmds->RenderState.Distortion[eyeDesc.Eye];

    // Find the mapping from TanAngle space to target NDC space.
    ScaleAndOffset2D      eyeToSourceNDC = CreateNDCScaleAndOffsetFromFov(eyeDesc.Fov);
    // Find the mapping from TanAngle space to textureUV space.
    ScaleAndOffset2D      eyeToSourceUV  = CreateUVScaleAndOffsetfromNDCScaleandOffset(
                                            eyeToSourceNDC,
                                            Recti(eyeDesc.RenderViewport), eyeDesc.TextureSize );

    uvScaleOffsetOut[0] = eyeToSourceUV.Scale;
    uvScaleOffsetOut[1] = eyeToSourceUV.Offset;

    int triangleCount = 0;
    int vertexCount = 0;

    DistortionMeshCreate((DistortionMeshVertexData**)&meshData->pVertexData, (UInt16**)&meshData->pIndexData,
                          &vertexCount, &triangleCount,
                          (stereoEye == StereoEye_Right),
                          hmdri, distortion, eyeToSourceNDC);

    if (meshData->pVertexData)
    {
        // Convert to index
        meshData->IndexCount = triangleCount * 3;
        meshData->VertexCount = vertexCount;
        return TRUE;
    }

    return FALSE;
}


// Frees distortion mesh allocated by ovrHmd_GenerateDistortionMesh. meshData elements
// are set to null and 0s after the call.
OVR_EXPORT void ovrHmd_DestroyDistortionMesh(ovrDistortionMesh* meshData)
{
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
OVR_EXPORT void ovrHmd_GetRenderScaleAndOffset( ovrHmd hmd, ovrEyeDesc eyeDesc,
                                                unsigned int distortionCaps,
                                                ovrVector2f uvScaleOffsetOut[2] )
{        
    OVR_UNUSED2(hmd, distortionCaps);
    // TBD: We could remove dependency on HMD here, but what if we need it in the future?
    //HMDState*        hmds = (HMDState*)hmd;

    // Find the mapping from TanAngle space to target NDC space.
    ScaleAndOffset2D  eyeToSourceNDC = CreateNDCScaleAndOffsetFromFov(eyeDesc.Fov);
    // Find the mapping from TanAngle space to textureUV space.
    ScaleAndOffset2D  eyeToSourceUV  = CreateUVScaleAndOffsetfromNDCScaleandOffset(
                                         eyeToSourceNDC,
                                         eyeDesc.RenderViewport, eyeDesc.TextureSize );

    uvScaleOffsetOut[0] = eyeToSourceUV.Scale;
    uvScaleOffsetOut[1] = eyeToSourceUV.Offset;
}


//-------------------------------------------------------------------------------------
// ***** Latency Test interface

OVR_EXPORT ovrBool ovrHmd_GetLatencyTestDrawColor(ovrHmd hmd, unsigned char rgbColorOut[3])
{
    HMDState* p = (HMDState*)hmd;
    rgbColorOut[0] = p->LatencyTestDrawColor[0];
    rgbColorOut[1] = p->LatencyTestDrawColor[1];
    rgbColorOut[2] = p->LatencyTestDrawColor[2];
    return p->LatencyTestActive;
}

OVR_EXPORT const char*  ovrHmd_GetLatencyTestResult(ovrHmd hmd)
{
    HMDState* p = (HMDState*)hmd;
    return p->LatencyUtil.GetResultsString();
}

OVR_EXPORT double ovrHmd_GetMeasuredLatencyTest2(ovrHmd hmd)
{
    HMDState* p = (HMDState*)hmd;

    // MA Test
    float latencies[3];
    p->TimeManager.GetLatencyTimings(latencies);
    return latencies[2];
  //  return p->LatencyUtil2.GetMeasuredLatency();
}


// -----------------------------------------------------------------------------------
// ***** Property Access

OVR_EXPORT float ovrHmd_GetFloat(ovrHmd hmd, const char* propertyName, float defaultVal)
{
    HMDState* hmds = (HMDState*)hmd;
    if (hmds)
	{
		return hmds->getFloatValue(propertyName, defaultVal);
	}

    return defaultVal;
}

OVR_EXPORT ovrBool ovrHmd_SetFloat(ovrHmd hmd, const char* propertyName, float value)
{
    HMDState* hmds = (HMDState*)hmd;
    if (hmds)
    {
        return hmds->setFloatValue(propertyName, value);
    }
    return false;
}



OVR_EXPORT unsigned int ovrHmd_GetFloatArray(ovrHmd hmd, const char* propertyName,
                              float values[], unsigned int arraySize)
{
    HMDState* hmds = (HMDState*)hmd;
    if (hmds)
    {       
		return hmds->getFloatArray(propertyName, values, arraySize);
    }

    return 0;
}


// Modify float[] property; false if property doesn't exist or is readonly.
OVR_EXPORT ovrBool ovrHmd_SetFloatArray(ovrHmd hmd, const char* propertyName,
                                        float values[], unsigned int arraySize)
{
    HMDState* hmds = (HMDState*)hmd;
    if (hmds)
    {       
        return hmds->setFloatArray(propertyName, values, arraySize);
    }

    return 0;
}

OVR_EXPORT const char* ovrHmd_GetString(ovrHmd hmd, const char* propertyName,
                                        const char* defaultVal)
{
    HMDState* hmds = (HMDState*)hmd;
    if (hmds)
    {
		return hmds->getString(propertyName, defaultVal);
    }

    return defaultVal;
}
 
/* Not needed yet.

// Get array of strings, i.e. const char* [] property.
// Returns the number of elements filled in, 0 if property doesn't exist.
// Maximum of arraySize elements will be written.
// String memory is guaranteed to exist until next call to GetString or GetStringArray, or HMD is destroyed.
OVR_EXPORT 
unsigned int ovrHmd_GetStringArray(ovrHmd hmd, const char* propertyName,
                               const char* values[], unsigned int arraySize)
{
    HMDState* hmds = (HMDState*)hmd;
    if (hmds && hmds->pHMD && arraySize)
    {        
        Profile* p = hmds->pHMD->GetProfile();

        hmds->LastGetStringValue[0] = 0;
        if (p && p->GetValue(propertyName, hmds->LastGetStringValue, sizeof(hmds->LastGetStringValue)))
        {
            values[0] = hmds->LastGetStringValue;
            return 1;
        }
    }

    return 0;
}
*/

// Returns array size of a property, 0 if property doesn't exist.
// Can be used to check existence of a property.
OVR_EXPORT unsigned int ovrHmd_GetArraySize(ovrHmd hmd, const char* propertyName)
{
    HMDState* hmds = (HMDState*)hmd;
    if (hmds && hmds->pHMD)
    {
        // For now, just access the profile.
        Profile* p = hmds->pHMD->GetProfile();
        
        if (p)
            return p->GetNumValues(propertyName);
    }
    return 0;
}


#ifdef __cplusplus 
} // extern "C"
#endif


//-------------------------------------------------------------------------------------
// ****** Special access for VRConfig

// Return the sensor fusion object for the purposes of magnetometer calibration.  The
// function is private and is only exposed through VRConfig header declarations
OVR::SensorFusion* ovrHmd_GetSensorFusion(ovrHmd hmd)
{
    HMDState* p = (HMDState*)hmd;
    return &p->SFusion;
}


