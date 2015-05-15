/************************************************************************************
Filename    :   Win32_BasicVR.h
Content     :   Core components for achieving basic VR, shared amongst samples
Created     :   October 20th, 2014
Author      :   Tom Heath
Copyright   :   Copyright 2014 Oculus, Inc. All Rights reserved.
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

#ifndef OVR_Win32_BasicVR_h
#define OVR_Win32_BasicVR_h

// Include the OculusVR SDK
#include "OVR_CAPI_D3D.h"                  
using namespace OVR;

//------------------------------------------------------------
struct OculusTexture
{
	ovrSwapTextureSet          * TextureSet;
	ID3D11RenderTargetView     * TexRtv[3];
	Sizei                        Size; 

	OculusTexture(ovrHmd hmd, Sizei size) : Size(size)
	{
		D3D11_TEXTURE2D_DESC dsDesc;
		dsDesc.Width = size.w;
		dsDesc.Height = size.h;
		dsDesc.MipLevels = 1;
		dsDesc.ArraySize = 1;
		dsDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		dsDesc.SampleDesc.Count = 1;   //Must be 1, no multisampling allowed
		dsDesc.SampleDesc.Quality = 0;
		dsDesc.Usage = D3D11_USAGE_DEFAULT;
		dsDesc.CPUAccessFlags = 0;
		dsDesc.MiscFlags = 0;
		dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		ovrHmd_CreateSwapTextureSetD3D11(hmd, DIRECTX.Device, &dsDesc, &TextureSet);
		for (int i = 0; i < TextureSet->TextureCount; ++i)
		{
			ovrD3D11Texture* tex = (ovrD3D11Texture*)&TextureSet->Textures[i];
			DIRECTX.Device->CreateRenderTargetView(tex->D3D11.pTexture, NULL, &TexRtv[i]);
		}
	}
	void Increment()
	{
		TextureSet->CurrentIndex = (TextureSet->CurrentIndex + 1) % TextureSet->TextureCount;
	}
	void Release(ovrHmd hmd)
	{
		ovrHmd_DestroySwapTextureSet(hmd, TextureSet);
	}
};

//----------------------------------------------------------------------
struct VRLayer
{
	ovrHmd                      HMD;
	ovrEyeRenderDesc            EyeRenderDesc[2];        // Description of the VR.
	ovrRecti                    EyeRenderViewport[2];    // Useful to remember when varying resolution
	OculusTexture             * pEyeRenderTexture[2];    // Where the eye buffers will be rendered
	DepthBuffer               * pEyeDepthBuffer[2];      // For the eye buffers to use when rendered
	ovrPosef                    EyeRenderPose[2];
	ovrLayerEyeFov              ovrLayer;

	//---------------------------------------------------------------
	VRLayer(ovrHmd argHMD, const ovrFovPort * fov = 0, float pixelsPerDisplayPixel = 1.0f)
	{
		HMD = argHMD;
		MakeEyeBuffers(pixelsPerDisplayPixel);
		ConfigureRendering(fov); 
	}

	//-----------------------------------------------------------------------
	void MakeEyeBuffers(float pixelsPerDisplayPixel = 1.0f)
	{
		for (int eye = 0; eye<2; eye++)
		{
			Sizei idealSize = ovrHmd_GetFovTextureSize(HMD, (ovrEyeType)eye, HMD->DefaultEyeFov[eye], pixelsPerDisplayPixel);
			pEyeRenderTexture[eye] = new OculusTexture(HMD, idealSize);
			pEyeDepthBuffer[eye] = new DepthBuffer(DIRECTX.Device, idealSize);
			EyeRenderViewport[eye].Pos = Vector2i(0, 0);
			EyeRenderViewport[eye].Size = idealSize;
		}
	}

	//--------------------------------------------------------
	void ConfigureRendering(const ovrFovPort * fov = 0)
	{
		// If any values are passed as NULL, then we use the default basic case
		if (!fov) fov = HMD->DefaultEyeFov;
		EyeRenderDesc[0] = ovrHmd_GetRenderDesc(HMD, ovrEye_Left, fov[0]);
		EyeRenderDesc[1] = ovrHmd_GetRenderDesc(HMD, ovrEye_Right, fov[1]);
	}

	//------------------------------------------------------------
	ovrTrackingState GetEyePoses(ovrPosef * useEyeRenderPose = 0, float * scaleIPD = 0, float * newIPD = 0)
	{
		// Get both eye poses simultaneously, with IPD offset already included. 
		ovrVector3f useHmdToEyeViewOffset[2] = { EyeRenderDesc[0].HmdToEyeViewOffset,
		EyeRenderDesc[1].HmdToEyeViewOffset };

		// If any values are passed as NULL, then we use the default basic case
		if (!useEyeRenderPose) useEyeRenderPose = EyeRenderPose;
		if (scaleIPD)
		{
			useHmdToEyeViewOffset[0].x *= *scaleIPD;
			useHmdToEyeViewOffset[1].x *= *scaleIPD;
		}
		if (newIPD)
		{
			useHmdToEyeViewOffset[0].x = +(*newIPD * 0.5f);
			useHmdToEyeViewOffset[1].x = -(*newIPD * 0.5f);
		}

		ovrFrameTiming   ftiming = ovrHmd_GetFrameTiming(HMD, 0);
		ovrTrackingState trackingState = ovrHmd_GetTrackingState(HMD, ftiming.DisplayMidpointSeconds);

		ovr_CalcEyePoses(trackingState.HeadPose.ThePose, useHmdToEyeViewOffset, useEyeRenderPose);

		return(trackingState);
	}

	//-----------------------------------------------------------
	Matrix4f RenderSceneToEyeBuffer(Camera * player, Scene * sceneToRender, int eye, ID3D11RenderTargetView * rtv = 0,
		ovrPosef * eyeRenderPose = 0, int timesToRenderRoom = 1,
		float alpha = 1, float red = 1, float green = 1, float blue = 1, float nearZ = 0.2f, float farZ = 1000.0f,
		bool doWeSetupRender = true, DepthBuffer * depthBuffer = 0)
	{
		// If any values are passed as NULL, then we use the default basic case
		if (!depthBuffer)    depthBuffer = pEyeDepthBuffer[eye];
		if (!eyeRenderPose)  eyeRenderPose = &EyeRenderPose[eye];

		if (doWeSetupRender)
		{
			// If none specified, then using special, and default, Oculus eye buffer render target
			if (rtv)
				DIRECTX.SetAndClearRenderTarget(rtv, depthBuffer);
			else
			{
				// We increment which texture we are using, to the next one, just before writing
				pEyeRenderTexture[eye]->Increment();
				int texIndex = pEyeRenderTexture[eye]->TextureSet->CurrentIndex;
			    DIRECTX.SetAndClearRenderTarget(pEyeRenderTexture[eye]->TexRtv[texIndex], depthBuffer);
			}

			DIRECTX.SetViewport(Recti(EyeRenderViewport[eye]));
		}

		// Get view and projection matrices for the Rift camera
		Camera finalCam(player->Pos + player->Rot.Transform(eyeRenderPose->Position),
			player->Rot * Matrix4f(eyeRenderPose->Orientation));
		Matrix4f view = finalCam.GetViewMatrix();
		Matrix4f proj = ovrMatrix4f_Projection(EyeRenderDesc[eye].Fov, nearZ, farZ, ovrProjection_RightHanded);
		Matrix4f projView = proj*view;

		// Render the scene
		for (int n = 0; n< timesToRenderRoom; n++)
			sceneToRender->Render(proj*view, red, green, blue, alpha, true);

		return(projView);
	}

	//------------------------------------------------------------
	void PrepareLayerHeader(OculusTexture * leftEyeTexture = 0, ovrPosef * leftEyePose = 0, Quatf * extraQuat = 0)
	{
		// Use defaults where none specified
		OculusTexture *   useEyeTexture[2] = { pEyeRenderTexture[0], pEyeRenderTexture[1] };
		ovrPosef    useEyeRenderPose[2] = { EyeRenderPose[0], EyeRenderPose[1] };
		if (leftEyeTexture) useEyeTexture[0] = leftEyeTexture;
		if (leftEyePose)    useEyeRenderPose[0] = *leftEyePose;

		// If we need to fold in extra rotations to the timewarp, per eye
		// We make the changes to the temporary copy, rather than 
		// the global one.
		if (extraQuat)
		{
			Quatf localExtraQuat[2] = { extraQuat[0], extraQuat[1] };
			useEyeRenderPose[0].Orientation = (Quatf)useEyeRenderPose[0].Orientation * localExtraQuat[0].Inverted(); // The order of multiplication could be the reversed - insufficient use cases to confirm at this stage.
			useEyeRenderPose[1].Orientation = (Quatf)useEyeRenderPose[1].Orientation * localExtraQuat[1].Inverted(); // The order of multiplication could be the reversed - insufficient use cases to confirm at this stage.
		}

		ovrLayer.Header.Type = ovrLayerType_EyeFov;
		ovrLayer.Header.Flags = 0;
		ovrLayer.ColorTexture[0] = useEyeTexture[0]->TextureSet;
		ovrLayer.ColorTexture[1] = useEyeTexture[1]->TextureSet;
		ovrLayer.RenderPose[0] = useEyeRenderPose[0];
		ovrLayer.RenderPose[1] = useEyeRenderPose[1];
		ovrLayer.Fov[0] = EyeRenderDesc[0].Fov;
		ovrLayer.Fov[1] = EyeRenderDesc[1].Fov;
		ovrLayer.Viewport[0] = EyeRenderViewport[0];
		ovrLayer.Viewport[1] = EyeRenderViewport[1];
	}
};

//----------------------------------------------------------------------------------------
struct BasicVR
{
	#define MAX_LAYERS 32
    ovrHmd       HMD;
	VRLayer    * Layer[MAX_LAYERS]; 
	Camera     * MainCam;
    Scene      * pRoomScene;
    ovrTexture * mirrorTexture;

    //------------------------------------------------------
    BasicVR(HINSTANCE hinst)
    {
        // Initializes LibOVR, and the Rift
        ovrResult result = ovr_Initialize(nullptr);
        VALIDATE(result == ovrSuccess, "Failed to initialize libOVR.");
		result = ovrHmd_Create(0, &HMD);
		if (result != ovrSuccess) ovrHmd_CreateDebug(ovrHmd_DK2, &HMD); // Use debug one, if no genuine Rift available
		VALIDATE(result == ovrSuccess, "Oculus Rift not detected.");
		VALIDATE(HMD->ProductName[0] != '\0', "Rift detected, display not enabled.");

        // Setup Window and Graphics - use window frame if relying on Oculus driver
        // The size of 1280x720 was arbitrarily chosen. The window size can be anything you like.
        bool initialized = DIRECTX.InitWindowAndDevice(hinst, Recti(Vector2i(0), Sizei(1280, 720)), L"OculusRoomTiny (Using BasicVR)");
        VALIDATE(initialized, "Unable to initialize window and D3D11 device.");

        ovrHmd_SetEnabledCaps(HMD, ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction);

        // Start the sensor which informs of the Rift's pose and motion
        result = ovrHmd_ConfigureTracking(HMD, ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection |
                                      ovrTrackingCap_Position, 0);
		VALIDATE(result == ovrSuccess, "Failed to configure tracking.");

		// Create a mirror, to see Rift output on a monitor
		mirrorTexture = nullptr;
		D3D11_TEXTURE2D_DESC td = {};
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.Width = DIRECTX.WinSize.w;
		td.Height = DIRECTX.WinSize.h;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.SampleDesc.Count = 1;
		td.MipLevels = 1;
		ovrHmd_CreateMirrorTextureD3D11(HMD, DIRECTX.Device, &td, &mirrorTexture);

		// Create the room model
		pRoomScene = new Scene;

		// Create camera
		MainCam = new Camera(Vector3f(0.0f, 1.6f, -5.0f), Matrix4f::RotationY(3.141f));

		// Set all layers to zero
		for (int i = 0; i < MAX_LAYERS; i++) Layer[i] = nullptr;
    }

    bool HandleMessages()
    {
        return DIRECTX.HandleMessages();
    }

    //-------------------------------------------------------
    bool ActionFromInput(float speed = 1.0f, bool updateYaw = true)
    {
        // Keyboard inputs to adjust player orientation, unaffected by speed
        if (updateYaw)
        {
            static float Yaw = 3.141f;
            if (DIRECTX.Key[VK_LEFT])  MainCam->Rot = Matrix4f::RotationY(Yaw+=0.02f); 
            if (DIRECTX.Key[VK_RIGHT]) MainCam->Rot = Matrix4f::RotationY(Yaw-=0.02f);
        }
        // Keyboard inputs to adjust player position
        if (DIRECTX.Key['W']||DIRECTX.Key[VK_UP])   MainCam->Pos+=MainCam->Rot.Transform(Vector3f(0,0,-speed*0.05f));
        if (DIRECTX.Key['S']||DIRECTX.Key[VK_DOWN]) MainCam->Pos+=MainCam->Rot.Transform(Vector3f(0,0,+speed*0.05f));
        if (DIRECTX.Key['D'])                       MainCam->Pos+=MainCam->Rot.Transform(Vector3f(+speed*0.05f,0,0));
        if (DIRECTX.Key['A'])                       MainCam->Pos+=MainCam->Rot.Transform(Vector3f(-speed*0.05f,0,0));
        MainCam->Pos.y = ovrHmd_GetFloat(HMD, OVR_KEY_EYE_HEIGHT, 0);
  
        // Animate the cube
        static float cubeClock =0; cubeClock+=speed;
        pRoomScene->Models[0]->Pos = Vector3f(9*sin(0.015f*cubeClock),3,
                                              9*cos(0.015f*cubeClock));
        return(false);
    }

	//------------------------------------------------------------
	void DistortAndPresent(int numLayersToRender)
	{
		ovrLayerHeader* layerHeaders[MAX_LAYERS];
		for (int i = 0; i < MAX_LAYERS; i++)
		{
			if (Layer[i]) layerHeaders[i] = &Layer[i]->ovrLayer.Header;
		}

		ovrHmd_SubmitFrame(HMD, 0, nullptr, layerHeaders, numLayersToRender);

		// Render mirror
		ovrD3D11Texture* tex = (ovrD3D11Texture*)mirrorTexture;
		DIRECTX.Context->CopyResource(DIRECTX.BackBuffer, tex->D3D11.pTexture);
		DIRECTX.SwapChain->Present(0, 0);
    }

    //------------------------------------------------------------
    int Release(HINSTANCE hinst)
    {
		ovrHmd_DestroyMirrorTexture(HMD, mirrorTexture);
		for (int i = 0; i < MAX_LAYERS; i++)
		{
			if (Layer[i])
			{
				Layer[i]->pEyeRenderTexture[0]->Release(HMD);
				Layer[i]->pEyeRenderTexture[1]->Release(HMD);
			}
		}
        ovrHmd_Destroy(HMD);
        ovr_Shutdown();
        DIRECTX.ReleaseWindow(hinst);
        if (DIRECTX.Key['Q'] && DIRECTX.Key[VK_CONTROL]) return(99);  // Special return code for quitting sample 99 
        return(0);
    }
};

#endif // OVR_Win32_BasicVR_h
