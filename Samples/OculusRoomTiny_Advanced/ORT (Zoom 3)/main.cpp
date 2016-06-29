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
/// Another variation on the theme of a scoped/zoomed view, whereby the user 
/// has a finer control of a cross-hairs (in this case a small box), a finer control tham the 1:1
/// motion of their head would otherwise imply.
/// In this sample, the zoomed scope moves 1:1, yet the box illustrating the crosshairs
/// is moving slower and thus 'lags' behind in the scope - whilst still affording the user
/// a fine control over where to aim.  When it reaches the edge of the scope, it 
/// again starts tracking at 1:1 in order to remain in the scope.
/// Use SPACEBAR to zoom in and out. 

/// A 'masterRatio' is edittable in the code below, to allow you to 
/// experiment with different departure of targetting, from 1:1, when 
/// you zoom.

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

//---------------------------------------------------------------
struct Zoom3 : BasicVR
{
	Zoom3(HINSTANCE hinst) : BasicVR(hinst, L"Zoom3") {}

	void MainLoop()
	{
		Layer[0] = new VRLayer(Session);

		// Make targetting cube.
		TriangleSet tCube;
		float sizeLittle = 0.5f;
		tCube.AddSolidColorBox(sizeLittle, -sizeLittle, sizeLittle, -sizeLittle, sizeLittle, -sizeLittle, 0xff009000);
		Model * tModel = new Model(&tCube, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1),
			new Material(
			new Texture(false, 256, 256, Texture::AUTO_CEILING)
			)
			);


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
			ActionFromInput(1,false);
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
			XMVECTOR eyePos = ConvertToXM(Layer[0]->EyeRenderPose[0].Position);


			// Get yaw from head rotation - note z is horiz
			XMFLOAT3 e = GetEulerFromQuat(Layer[0]->EyeRenderPose[0].Orientation.x, Layer[0]->EyeRenderPose[0].Orientation.y,
				Layer[0]->EyeRenderPose[0].Orientation.z, Layer[0]->EyeRenderPose[0].Orientation.w);

			static float baseYaw = 0;
			static float basePitch = 0;
			static float count = 0;
			if (DIRECTX.Key[' '])
			{
			}
			else
			{
				baseYaw = e.z; //set when off
				basePitch = e.x;
			}

			e.z -= baseYaw;
			e.x -= basePitch;

			// Adjust this ratio if you wish.
			float masterRatio = 0.66f;

			float horizOffset = masterRatio*e.z;
			float vertiOffset = masterRatio*e.x;
			float thres = 0.08f; // 0.1 is to very edge just coincidence its like 2/3
			if (horizOffset > thres)  baseYaw += (horizOffset - thres); 
			if (horizOffset < -thres) baseYaw -= (-thres - horizOffset);
			if (vertiOffset > thres)  basePitch += (vertiOffset - thres); 
			if (vertiOffset < -thres) basePitch -= (-thres - vertiOffset);


			XMVECTOR lookQuat = XMQuaternionMultiply(eyeQuat, XMQuaternionRotationRollPitchYaw(-vertiOffset, -horizOffset, 0));


			#define DISTANCE 100
			XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, -DISTANCE, 0), lookQuat);
			XMVECTOR location = XMVectorAdd(forward, MainCam->Pos);

			XMFLOAT3 camPos3; XMStoreFloat3(&camPos3, MainCam->Pos);


			XMFLOAT3 location3; XMStoreFloat3(&location3, location);
			tModel->Pos = location3;


			// Get view and projection matrices for the Rift camera
			XMVECTOR CombinedPos = XMVectorAdd(MainCam->Pos, XMVector3Rotate(eyePos, MainCam->Rot));
			Camera finalCam(&CombinedPos, &(XMQuaternionMultiply(eyeQuat, MainCam->Rot)));
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

			//Zero z buffer
			DIRECTX.Context->ClearDepthStencilView(Layer[0]->pEyeDepthBuffer[0]->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

			tModel->Render(&projView, 1, 1, 1, 1, true);

			for (int eye = 0; eye < 2; ++eye)
			{
				// Render main, outer world
				XMMATRIX projView2 = Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);

				//Zero z buffer
				DIRECTX.Context->ClearDepthStencilView(Layer[0]->pEyeDepthBuffer[eye]->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

				tModel->Render(&projView2, 1, 1, 1, 1, true);

				// Render scope with special static camera, always in front of us
				static float howFarAway = 0.75f;
				if (DIRECTX.Key['3']) howFarAway = max(howFarAway - 0.002f, 0.25f);
				if (DIRECTX.Key['4']) howFarAway = min(howFarAway + 0.002f, 1.00f);
				Camera  StaticMainCam(&XMVectorSet(0, 0, -howFarAway, 0), &XMQuaternionRotationRollPitchYaw(0, 3.14f, 0));
				XMMATRIX view = StaticMainCam.GetViewMatrix();
				XMMATRIX proj = ConvertToXM(ovrMatrix4f_Projection(Layer[0]->EyeRenderDesc[eye].Fov, 0.2f, 1000.0f, ovrProjection_None));
				XMMATRIX projView = XMMatrixMultiply(view, proj);
				if (DIRECTX.Key[' '])  howFarAway = 0.95f*howFarAway + 0.05f * 0.75f;
				else                   howFarAway = 0.95f*howFarAway + 0.05f * 10.75f;
				if (howFarAway < 8) sniperModel->Render(&projView, 0, 1, 0, 1, true);
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
	Zoom3 app(hinst);
	return app.Run();
}
