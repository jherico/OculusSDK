/************************************************************************************
Filename    :   Win32_RoomTiny_Main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   3rd June 2016
Authors     :   Tom Heath
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
/// This sample shows a variation on the theme of zooming,
/// whereby the application uses a button press - in this 
/// case the SPACEBAR - to zoom in using a scope, with that
/// scope then having more fine control than 1:1 with the 
/// users head movement.  

/// This shows a glimpse of a
/// method to have the zoomed ratio depart from 1:1, 
/// and still allow the user to unzoom and rezoom without
/// excessive disturbance to their play, or artifacts.

/// You can vary the ratio of the zoomed scope movement versus
/// 1:1 by adjusting the 'masterRatio' in the code below.

/// Note the helpful components of the implementation, such as the 
/// scaling of the scope to full size, and the reversion to 1:1
/// movement when the scope reaches the edge of view.

/// Note because its experimental, its not catering fully for the situations where you are
/// tilting your head sideway, as yet.


#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR


//-----------------------------------------------------------
XMFLOAT3 GetEulerFromQuat(float x, float y, float z, float w)
{
	XMFLOAT3 pitchYawRoll;
	pitchYawRoll.z = atan2(2 * y*w - 2 * x*z, 1 - 2 * y*y - 2 * z*z);
	pitchYawRoll.x = atan2(2 * x*w - 2 * y*z, 1 - 2 * x*x - 2 * z*z);
	pitchYawRoll.y = asin(2 * x*y + 2 * z*w);
	return(pitchYawRoll);
}

//------------------------------------------------------------
struct Zoom2 : BasicVR
{
	Zoom2(HINSTANCE hinst) : BasicVR(hinst, L"Zoom2") {}

	void MainLoop()
	{
		Layer[0] = new VRLayer(Session);

		// Make a texture to render the zoomed image into.  Make it same size as left eye buffer, for simplicity.
		auto zoomedTexture = new Texture(true, max(Layer[0]->pEyeRenderTexture[0]->SizeW, Layer[0]->pEyeRenderTexture[1]->SizeW),
			max(Layer[0]->pEyeRenderTexture[0]->SizeH, Layer[0]->pEyeRenderTexture[1]->SizeH));

		// Make a scope model - its small and close to us
		float scopeScale = 0.25f;
		auto cube = new TriangleSet();
		cube->AddQuad(Vertex(XMFLOAT3(scopeScale, scopeScale, 0), 0xffffffff, 0, 0),
			Vertex(XMFLOAT3(-scopeScale, scopeScale, 0), 0xffffffff, 1, 0),
			Vertex(XMFLOAT3(scopeScale, -scopeScale, 0), 0xffffffff, 0, 1),
			Vertex(XMFLOAT3(-scopeScale, -scopeScale, 0), 0xffffffff, 1, 1));
		auto sniperModel = new Model(cube, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1), new Material(zoomedTexture));

		while (HandleMessages())
		{
			ActionFromInput();
			Layer[0]->GetEyePoses();

			// Render the zoomed scene, making sure we clear the back screen with solid alpha
			DIRECTX.SetAndClearRenderTarget(zoomedTexture->TexRtv, Layer[0]->pEyeDepthBuffer[0], 0.2f, 0.2f, 0.2f, 1);

			// Lets set a slightly small viewport, so we get a black border
			int blackBorder = 16;
			DIRECTX.SetViewport((float)Layer[0]->EyeRenderViewport[0].Pos.x + blackBorder,
				(float)Layer[0]->EyeRenderViewport[0].Pos.y + blackBorder,
				(float)Layer[0]->EyeRenderViewport[0].Size.w - 2 * blackBorder,
				(float)Layer[0]->EyeRenderViewport[0].Size.h - 2 * blackBorder);

			// Get the pose information in XM format
			XMVECTOR eyeQuat = ConvertToXM(Layer[0]->EyeRenderPose[0].Orientation);

			// A little boost up
			Layer[0]->EyeRenderPose[0].Position.y += 0.2f;
			Layer[0]->EyeRenderPose[1].Position.y += 0.2f;

			XMVECTOR eyePos = ConvertToXM(Layer[0]->EyeRenderPose[0].Position);


			// Set to origin
			MainCam->Pos = XMVectorSet(0, 0, 0, 0);
			MainCam->Rot = XMVectorSet(0, 0, 0, 1);

			// Get yaw from head rotation - note z is horiz
			XMFLOAT3 e = GetEulerFromQuat(Layer[0]->EyeRenderPose[0].Orientation.x, Layer[0]->EyeRenderPose[0].Orientation.y,
				Layer[0]->EyeRenderPose[0].Orientation.z, Layer[0]->EyeRenderPose[0].Orientation.w);

			static float baseYaw = 0;
			static float basePitch = 0;
			static float count = 0;
			if (DIRECTX.Key[' '])
			{
				count++;
			}
			else
			{
				baseYaw = e.z; //set when off
				basePitch = e.x;
				count = 0;
			}

			e.z -= baseYaw;
			e.x -= basePitch;

			// Master ratio - adjust this if you wish
			float masterRatio = 0.66f;

			float horizOffset = masterRatio*e.z;
			float vertiOffset = masterRatio*e.x;
			if (horizOffset > 0.4) { count = 0;  horizOffset = 0.4f; }
			if (horizOffset < -0.4) { count = 0; horizOffset = -0.4f; }
			if (vertiOffset > 0.4) { count = 0; vertiOffset = 0.4f; }
			if (vertiOffset < -0.4) { count = 0; vertiOffset = -0.4f; }
			Util.Output("horizOffset = %f  verti = %f\n", horizOffset, vertiOffset);

			// Get view and projection matrices for the Rift camera
			Camera finalCam(&eyePos, &(XMQuaternionMultiply(eyeQuat, XMQuaternionRotationRollPitchYaw(-vertiOffset, -horizOffset, 0)))); //This scale is correct for motion
			XMMATRIX view = finalCam.GetViewMatrix();

			// Vary amount of zoom with '1' and '2'Lets pick a zoomed in FOV
			static float amountOfZoom = 0.1f;
			if (DIRECTX.Key['1']) amountOfZoom = max(amountOfZoom - 0.002f, 0.050f);
			if (DIRECTX.Key['2']) amountOfZoom = min(amountOfZoom + 0.002f, 0.500f);
			ovrFovPort zoomedFOV;
			zoomedFOV.DownTan = zoomedFOV.UpTan = zoomedFOV.LeftTan = zoomedFOV.RightTan = amountOfZoom;

			// Finally, render zoomed scene onto the texture
			XMMATRIX proj = ConvertToXM(ovrMatrix4f_Projection(zoomedFOV, 0.2f, 1000.0f, ovrProjection_None));
			XMMATRIX projView = XMMatrixMultiply(view, proj);
			RoomScene->Render(&projView, 1, 1, 1, 1, true);

			for (int eye = 0; eye < 2; ++eye)
			{
				// Render main, outer world
				Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);

				// Render scope with special static camera, always in front of us
				static float howFarAway = 0.75f;
				if (DIRECTX.Key['3']) howFarAway = max(howFarAway - 0.002f, 0.25f);
				if (DIRECTX.Key['4']) howFarAway = min(howFarAway + 0.002f, 1.00f);

				//Zero z buffer
				DIRECTX.Context->ClearDepthStencilView(Layer[0]->pEyeDepthBuffer[eye]->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

				Camera  StaticMainCam(&XMVectorSet(0, 0, -howFarAway, 0), &XMQuaternionRotationRollPitchYaw(vertiOffset, horizOffset + 3.14f, 0));
				XMMATRIX view = StaticMainCam.GetViewMatrix();
				XMMATRIX proj = ConvertToXM(ovrMatrix4f_Projection(Layer[0]->EyeRenderDesc[eye].Fov, 0.2f, 1000.0f, ovrProjection_None));
				XMMATRIX projView = XMMatrixMultiply(view, proj);
				if (DIRECTX.Key[' '])  howFarAway = 0.95f*howFarAway + 0.05f * 0.75f;
				else                   howFarAway = 0.95f*howFarAway + 0.05f * 10.75f;
				sniperModel->Render(&projView, 0, 1, 0, 1, true);
			}

			Layer[0]->PrepareLayerHeader();
			DistortAndPresent(1);
		}

		delete sniperModel;
	}
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	Zoom2 app(hinst);
	return app.Run();
}
