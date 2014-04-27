/************************************************************************************

Filename    :   Win32_OculusRoomTiny2.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   October 4, 2012
Authors     :   Michael Antonov, Andrew Reisse

Copyright   :   Copyright 2012 Oculus, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/

//-------------------------------------------------------------------------------------
// This app renders a simple flat-shaded room allowing the user to move along the 
// floor and look around with an HMD and mouse/keyboard. 
// The following keys work:
//  'W', 'S', 'A', 'D', 'F' - Move forward, back; strafe left/right, toggle freeze in timewarp.
// The world right handed coordinate system is defined as  Y -> Up, Z -> Back, X -> Right

//Include the OculusVR SDK
#include "OVR_CAPI.h"

// ***** Choices and settings

// Whether the SDK performs rendering/distortion, or the app.
//#define          SDK_RENDER       1

const unsigned   DistortionCaps = ovrDistortion_Chromatic | ovrDistortion_TimeWarp;
const bool       VSyncEnabled  = true;
const bool       FullScreen    = true;

// Include Non-SDK supporting Utilities from other files
#include "RenderTiny_D3D11_Device.h"

RenderDevice   * Util_InitWindowAndGraphics    (Recti vp, int fullscreen, int multiSampleCount);
void             Util_ReleaseWindowAndGraphics (RenderDevice* pRender);
bool             Util_RespondToControls        (float & EyeYaw, Vector3f & EyePos,
                                                float deltaTime, Quatf PoseOrientation);
void             PopulateRoomScene             (Scene* scene, RenderDevice* render);

//Structures for the application
ovrHmd           HMD;
ovrHmdDesc       HMDDesc;
ovrEyeRenderDesc EyeRenderDesc[2];
RenderDevice*    pRender; 
Texture*         pRendertargetTexture;
Scene*           pRoomScene;

// Specifics for whether the SDK or the app is doing the distortion.
#if SDK_RENDER
    #define OVR_D3D_VERSION 11
    #include "OVR_CAPI_D3D.h"
    ovrD3D11Texture      EyeTexture[2];
#else
    void DistortionMeshInit  (unsigned distortionCaps, ovrHmd HMD,
                              ovrEyeRenderDesc eyeRenderDesc[2], RenderDevice * pRender);
    void DistortionMeshRender(unsigned distortionCaps, ovrHmd HMD,
                              double timwarpTimePoint, ovrPosef eyeRenderPoses[2],
                              RenderDevice * pRender, Texture* pRendertargetTexture);
#endif
   
//-------------------------------------------------------------------------------------

int Init()
{
    // Initializes LibOVR. 
    ovr_Initialize();

    HMD = ovrHmd_Create(0);
    if (!HMD)
    {
        MessageBoxA(NULL,"Oculus Rift not detected.","", MB_OK);
        return(1);
    }
    //Get more details about the HMD    
    ovrHmd_GetDesc(HMD, &HMDDesc);
    if (HMDDesc.DisplayDeviceName[0] == '\0')
        MessageBoxA(NULL,"Rift detected, display not enabled.","", MB_OK);

    //Setup Window and Graphics
    const int backBufferMultisample = 1;
    pRender = Util_InitWindowAndGraphics(Recti(HMDDesc.WindowsPos, HMDDesc.Resolution),
                                         FullScreen, backBufferMultisample);
    if (!pRender) return 1;
     
    //Configure Stereo settings.
    Sizei recommenedTex0Size = ovrHmd_GetFovTextureSize(HMD, ovrEye_Left,  HMDDesc.DefaultEyeFov[0], 1.0f);
    Sizei recommenedTex1Size = ovrHmd_GetFovTextureSize(HMD, ovrEye_Right, HMDDesc.DefaultEyeFov[1], 1.0f);
    Sizei RenderTargetSize;
    RenderTargetSize.w = recommenedTex0Size.w + recommenedTex1Size.w;
    RenderTargetSize.h = max ( recommenedTex0Size.h, recommenedTex1Size.h );

    const int eyeRenderMultisample = 1;
    pRendertargetTexture = pRender->CreateTexture(Texture_RGBA | Texture_RenderTarget |
                                                  eyeRenderMultisample,
                                                  RenderTargetSize.w, RenderTargetSize.h, NULL);
    // The actual RT size may be different due to HW limits.
    RenderTargetSize.w = pRendertargetTexture->GetWidth();
    RenderTargetSize.h = pRendertargetTexture->GetHeight();

    // Initialize eye rendering information for ovrHmd_Configure.
    // The viewport sizes are re-computed in case RenderTargetSize changed due to HW limitations.
    ovrEyeDesc eyes[2];
    eyes[0].Eye                 = ovrEye_Left;
    eyes[1].Eye                 = ovrEye_Right;
    eyes[0].Fov                 = HMDDesc.DefaultEyeFov[0];
    eyes[1].Fov                 = HMDDesc.DefaultEyeFov[1];
    eyes[0].TextureSize         = RenderTargetSize;
    eyes[1].TextureSize         = RenderTargetSize;
    eyes[0].RenderViewport.Pos  = Vector2i(0,0);
    eyes[0].RenderViewport.Size = Sizei(RenderTargetSize.w / 2, RenderTargetSize.h);
    eyes[1].RenderViewport.Pos  = Vector2i((RenderTargetSize.w + 1) / 2, 0);
    eyes[1].RenderViewport.Size = eyes[0].RenderViewport.Size;

#if SDK_RENDER
    // Query D3D texture data.
    Texture* rtt = (Texture*)pRendertargetTexture;
    EyeTexture[0].D3D11.Header.API            = ovrRenderAPI_D3D11;
    EyeTexture[0].D3D11.Header.TextureSize    = RenderTargetSize;
    EyeTexture[0].D3D11.Header.RenderViewport = eyes[0].RenderViewport;
    EyeTexture[0].D3D11.pTexture              = rtt->Tex.GetPtr();
    EyeTexture[0].D3D11.pSRView               = rtt->TexSv.GetPtr();

    // Right eye uses the same texture, but different rendering viewport.
    EyeTexture[1] = EyeTexture[0];
    EyeTexture[1].D3D11.Header.RenderViewport = eyes[1].RenderViewport;    

    // Configure d3d11.
    RenderDevice* render = (RenderDevice*)pRender;
    ovrD3D11Config d3d11cfg;
    d3d11cfg.D3D11.Header.API         = ovrRenderAPI_D3D11;
    d3d11cfg.D3D11.Header.RTSize      = Sizei(HMDDesc.Resolution.w, HMDDesc.Resolution.h);
    d3d11cfg.D3D11.Header.Multisample = backBufferMultisample;
    d3d11cfg.D3D11.pDevice            = render->Device;
    d3d11cfg.D3D11.pDeviceContext     = render->Context;
    d3d11cfg.D3D11.pBackBufferRT      = render->BackBufferRT;
    d3d11cfg.D3D11.pSwapChain         = render->SwapChain;

    if (!ovrHmd_ConfigureRendering(HMD, &d3d11cfg.Config,
                                   (VSyncEnabled ? 0 : ovrHmdCap_NoVSync), DistortionCaps,
                                   eyes, EyeRenderDesc)) return(1);
#else // !SDK_RENDER
    EyeRenderDesc[0] = ovrHmd_GetRenderDesc(HMD, eyes[0]);
    EyeRenderDesc[1] = ovrHmd_GetRenderDesc(HMD, eyes[1]);
    
     // Create our own distortion mesh and shaders
    DistortionMeshInit(DistortionCaps, HMD, EyeRenderDesc, pRender);
#endif

    // Start the sensor which informs of the Rift's pose and motion
    ovrHmd_StartSensor(HMD, ovrHmdCap_Orientation |
                            ovrHmdCap_YawCorrection |
                            ovrHmdCap_Position |
                            ovrHmdCap_LowPersistence |
                            ovrHmdCap_LatencyTest, 0);

    // This creates lights and models.
      pRoomScene = new Scene;
    PopulateRoomScene(pRoomScene, pRender);

    return 0;
}

//-------------------------------------------------------------------------------------

void ProcessAndRender()
{
#if SDK_RENDER
    ovrFrameTiming frameTiming = ovrHmd_BeginFrame(HMD, 0); 
#else
    ovrFrameTiming frameTiming = ovrHmd_BeginFrameTiming(HMD, 0); 
#endif

    //Adjust eye position and rotation from controls, maintaining y position from HMD.
    static Vector3f EyePos(0.0f, 1.6f, -5.0f);
    static float    EyeYaw(3.141592f);

    Posef     movePose = ovrHmd_GetSensorState(HMD, frameTiming.ScanoutMidpointSeconds).Predicted.Pose;
    ovrPosef  eyeRenderPose[2];

    EyePos.y = ovrHmd_GetFloat(HMD, OVR_KEY_EYE_HEIGHT, EyePos.y);
    bool freezeEyeRender = Util_RespondToControls(EyeYaw, EyePos,
                                frameTiming.DeltaSeconds, movePose.Orientation);

    pRender->BeginScene();
    
    //Render the two undistorted eye views into their render buffers.
    if (!freezeEyeRender) // freeze to debug, especially for time warp
    {
        pRender->SetRenderTarget ( pRendertargetTexture );
        pRender->SetViewport (Recti(0,0, pRendertargetTexture->GetWidth(),
                                         pRendertargetTexture->GetHeight() ));  
        pRender->Clear();
        for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
        {
            ovrEyeType eye = HMDDesc.EyeRenderOrder[eyeIndex];
#if SDK_RENDER
            eyeRenderPose[eye] = ovrHmd_BeginEyeRender(HMD, eye);
#else
            eyeRenderPose[eye] = ovrHmd_GetEyePose(HMD, eye);
#endif

            // Get view matrix
            Matrix4f rollPitchYaw       = Matrix4f::RotationY(EyeYaw);
            Matrix4f finalRollPitchYaw  = rollPitchYaw * Matrix4f(eyeRenderPose[eye].Orientation);
            Vector3f finalUp            = finalRollPitchYaw.Transform(Vector3f(0,1,0));
            Vector3f finalForward       = finalRollPitchYaw.Transform(Vector3f(0,0,-1));
            Vector3f shiftedEyePos      = EyePos + rollPitchYaw.Transform(eyeRenderPose[eye].Position);
            
            Matrix4f view = Matrix4f::LookAtRH(shiftedEyePos,
                                               shiftedEyePos + finalForward, finalUp); 

            Matrix4f proj = ovrMatrix4f_Projection(EyeRenderDesc[eye].Desc.Fov, 0.01f, 10000.0f, true);

            pRender->SetViewport(EyeRenderDesc[eye].Desc.RenderViewport.Pos.x,
                                 EyeRenderDesc[eye].Desc.RenderViewport.Pos.y,
                                 EyeRenderDesc[eye].Desc.RenderViewport.Size.w,
                                 EyeRenderDesc[eye].Desc.RenderViewport.Size.h);
            pRender->SetProjection(proj);
            pRender->SetDepthMode(true, true);
            pRoomScene->Render(pRender, Matrix4f::Translation(EyeRenderDesc[eye].ViewAdjust) * view);

        #if SDK_RENDER
            ovrHmd_EndEyeRender(HMD, eye, eyeRenderPose[eye], &EyeTexture[eye].Texture);
        #endif
        }
    }
    pRender->FinishScene();

    // Now render the distorted view and finish.
#if SDK_RENDER
    // Let OVR do distortion rendering, Present and flush/sync
    ovrHmd_EndFrame(HMD);

#else
    DistortionMeshRender(DistortionCaps, HMD, frameTiming.TimewarpPointSeconds,
                         eyeRenderPose, pRender, pRendertargetTexture);    
    pRender->Present( VSyncEnabled );
    pRender->WaitUntilGpuIdle();  //for lowest latency
    ovrHmd_EndFrameTiming(HMD);
#endif 
}

/*
void RenderFramePseudoCode()
{    
ovrFrame hmdFrameState = ovrHmd_BeginFrame(hmd); 

for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
{
    ovrEyeType eye           = HMDDesc.EyeRenderOrder[eyeIndex];
    ovrPosef   eyeRenderPose = ovrHmd_BeginEyeRender(hmd, eye);

    RenderGameView(RenderViewports[eye], eyeRenderPose);

    ovrHmd_EndEyeRender(hmd, eye, &EyeTexture[eye].Texture);
}

// Let OVR do distortion rendering, Present and Flush+Sync.
ovrHmd_EndFrame(hmd);
}
*/ 

//-------------------------------------------------------------------------------------
void Release(void)
{
    pRendertargetTexture->Release();
    pRendertargetTexture = 0;
    ovrHmd_Destroy(HMD);
    Util_ReleaseWindowAndGraphics(pRender);
    pRender = 0;
    if (pRoomScene)
    {
        delete pRoomScene;
        pRoomScene = 0;
    }    
    // No OVR functions involving memory are allowed after this.
    ovr_Shutdown(); 
}

