/*****************************************************************************

Filename    :   Win32_OculusRoomTiny(D3D9).cpp
Content     :   Simple minimal VR demo
Created     :   December 1, 2014
Author      :   Tom Heath
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

/*****************************************************************************/

#define USE_OPENGL     0

#if USE_OPENGL
#include "Win32_GLAppUtil.h"
#include <OVR_CAPI_GL.h>
#else
#include "Win32_DX11AppUtil.h"
#include <OVR_CAPI_D3D.h>
#endif

#include <Kernel/OVR_System.h>

using namespace OVR;

//----------------------------------------------------------------------------
int WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));

	//Initialise rift
    if (!ovr_Initialize()) { MessageBoxA(NULL, "Unable to initialize libOVR.", "", MB_OK); return 0; }
	ovrHmd HMD = ovrHmd_Create(0);
    if (HMD == NULL)
    {
        HMD = ovrHmd_CreateDebug(ovrHmd_DK2);
    }
 
    if (!HMD) {	MessageBoxA(NULL,"Oculus Rift not detected.","", MB_OK); ovr_Shutdown(); return 0; }
	if (HMD->ProductName[0] == '\0') MessageBoxA(NULL,"Rift detected, display not enabled.","", MB_OK);

    bool windowed = (HMD->HmdCaps & ovrHmdCap_ExtendDesktop) ? false : true;    
    if (!Platform.InitWindowAndDevice(hInst, Recti(HMD->WindowsPos, HMD->Resolution), windowed, (char *)HMD->DisplayDeviceName))
        return 0;

  	// Make eye render buffers
	TextureBuffer * eyeRenderTexture[2];
	DepthBuffer   * eyeDepthBuffer[2];
	for (int i=0; i<2; i++)
    {
		ovrSizei idealTextureSize = ovrHmd_GetFovTextureSize(HMD, (ovrEyeType)i, HMD->DefaultEyeFov[i], 1);
		eyeRenderTexture[i] = new TextureBuffer(true, idealTextureSize, 1, NULL, 1);
		eyeDepthBuffer[i]   = new DepthBuffer(eyeRenderTexture[i]->GetSize(), 0);
	}

#if USE_OPENGL
    ovrGLConfig config;
    config.OGL.Header.API			   = ovrRenderAPI_OpenGL;
    config.OGL.Header.BackBufferSize   = HMD->Resolution;
    config.OGL.Header.Multisample      = 0;
    config.OGL.Window                  = Platform.Window;
    config.OGL.DC					   = Platform.hDC;
#else
    ovrD3D11Config config;
    config.D3D11.Header.API            = ovrRenderAPI_D3D11;
    config.D3D11.Header.BackBufferSize = HMD->Resolution;
    config.D3D11.Header.Multisample    = 1;
    config.D3D11.pDevice               = Platform.Device;
    config.D3D11.pDeviceContext        = Platform.Context;
    config.D3D11.pBackBufferRT         = Platform.BackBufferRT;
    config.D3D11.pSwapChain            = Platform.SwapChain;
#endif

	ovrEyeRenderDesc EyeRenderDesc[2]; 
    ovrHmd_ConfigureRendering( HMD, &config.Config,
		                       ovrDistortionCap_Vignette | ovrDistortionCap_TimeWarp |
							   ovrDistortionCap_Overdrive, HMD->DefaultEyeFov, EyeRenderDesc );

    ovrHmd_SetEnabledCaps(HMD, ovrHmdCap_LowPersistence|ovrHmdCap_DynamicPrediction);
	ovrHmd_AttachToWindow(HMD, Platform.Window, NULL, NULL);

	// Start the sensor
    ovrHmd_ConfigureTracking(HMD, ovrTrackingCap_Orientation|ovrTrackingCap_MagYawCorrection|
		                          ovrTrackingCap_Position, 0);

	// Make scene - can simplify further if needed
	Scene roomScene(false); 

    ovrHmd_DismissHSWDisplay(HMD);

	// Main loop
	while (!(Platform.Key['Q'] && Platform.Key[VK_CONTROL]) && !Platform.Key[VK_ESCAPE])
    {
		ovrHmd_BeginFrame(HMD, 0); 

		// Keyboard inputs to adjust player orientation
		static float Yaw(3.141592f);  
		Platform.HandleMessages();
		if (Platform.Key[VK_LEFT])  Yaw += 0.02f;
		if (Platform.Key[VK_RIGHT]) Yaw -= 0.02f;

		// Keyboard inputs to adjust player position
		static Vector3f Pos2(0.0f,1.6f,-5.0f);
		if (Platform.Key['W']||Platform.Key[VK_UP])   Pos2+=Matrix4f::RotationY(Yaw).Transform(Vector3f(0,0,-0.05f));
		if (Platform.Key['S']||Platform.Key[VK_DOWN]) Pos2+=Matrix4f::RotationY(Yaw).Transform(Vector3f(0,0,+0.05f));
		if (Platform.Key['D'])                   Pos2+=Matrix4f::RotationY(Yaw).Transform(Vector3f(+0.05f,0,0));
		if (Platform.Key['A'])                   Pos2+=Matrix4f::RotationY(Yaw).Transform(Vector3f(-0.05f,0,0));
		Pos2.y = ovrHmd_GetFloat(HMD, OVR_KEY_EYE_HEIGHT, Pos2.y);

		// Animate the cube
		roomScene.Models[0]->Pos = Vector3f(9*sin((float)ovr_GetTimeInSeconds()),3,
			                                9*cos((float)ovr_GetTimeInSeconds()));

		//Get eye poses, feeding in correct IPD offset
		ovrVector3f ViewOffset[2] = {EyeRenderDesc[0].HmdToEyeViewOffset,EyeRenderDesc[1].HmdToEyeViewOffset};
		ovrPosef EyeRenderPose[2];
        ovrHmd_GetEyePoses(HMD, 0, ViewOffset, EyeRenderPose, NULL);

		for (int eye=0;eye<2;eye++) 
		{
			//Switch to eye render target
			eyeRenderTexture[eye]->SetAndClearRenderSurface(eyeDepthBuffer[eye]);

			// Get view and projection matrices
			Matrix4f rollPitchYaw       = Matrix4f::RotationY(Yaw);
			Matrix4f finalRollPitchYaw  = rollPitchYaw * Matrix4f(EyeRenderPose[eye].Orientation);
			Vector3f finalUp            = finalRollPitchYaw.Transform(Vector3f(0, 1, 0));
			Vector3f finalForward       = finalRollPitchYaw.Transform(Vector3f(0, 0, -1));
			Vector3f shiftedEyePos      = Pos2 + rollPitchYaw.Transform(EyeRenderPose[eye].Position);

            Matrix4f view = Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp);
            Matrix4f proj = ovrMatrix4f_Projection(HMD->DefaultEyeFov[eye], 0.2f, 1000.0f, ovrProjection_RightHanded); 

			//Render world
			roomScene.Render(view,proj);
		}

        // Do distortion rendering, Present and flush/sync
#if USE_OPENGL
        ovrGLTexture eyeTex[2];
        for (int i = 0; i<2; i++)
        {
            eyeTex[i].OGL.Header.API = ovrRenderAPI_OpenGL;
            eyeTex[i].OGL.Header.TextureSize = eyeRenderTexture[i]->GetSize();
            eyeTex[i].OGL.Header.RenderViewport = Recti(Vector2i(0, 0), eyeRenderTexture[i]->GetSize());
            eyeTex[i].OGL.TexId = eyeRenderTexture[i]->texId;
        }
#else
        ovrD3D11Texture eyeTex[2];
        for (int i = 0; i<2; i++)
        {
            eyeTex[i].D3D11.Header.API            = ovrRenderAPI_D3D11;
            eyeTex[i].D3D11.Header.TextureSize    = eyeRenderTexture[i]->GetSize();
			eyeTex[i].D3D11.Header.RenderViewport = Recti(Vector2i(0,0),eyeRenderTexture[i]->GetSize());
            eyeTex[i].D3D11.pTexture              = eyeRenderTexture[i]->Tex;
			eyeTex[i].D3D11.pSRView               = eyeRenderTexture[i]->TexSv;
        }
#endif
		ovrHmd_EndFrame(HMD, EyeRenderPose, &eyeTex[0].Texture);
	}

	//Release
	ovrHmd_Destroy(HMD);
	ovr_Shutdown();
	Platform.ReleaseWindow(hInst);
    return 0;
}



