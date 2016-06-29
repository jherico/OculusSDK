/************************************************************************************
Filename    :   Win32_RoomTiny_Main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   18th Dec 2014
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
/// This sample shows a possible control method, whereby your positional
/// velocity in the world is proportional to your deviation from a 'centre-point',
/// located 1m in front of the camera.  Its interesting, perhaps the basis for 
/// future research, but also to provide a quick demonstration of such a thing,
/// to save everyone repeating it.
/// Try picking points in the room, and moving to them, to see how intuitive it feels.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR

struct AutoLinearVelocity : BasicVR
{
    AutoLinearVelocity(HINSTANCE hinst) : BasicVR(hinst, L"Auto Linear Velocity") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
			//Need to check we're visible, before proceeding with velocity changes, 
			//otherwise it does this a lot of times, and we
			//end up miles away from our start point from the sheer number of iterations.
			ovrSessionStatus sessionStatus;
			ovr_GetSessionStatus(Session, &sessionStatus);
			if (sessionStatus.IsVisible)
			{
				// We turn off yaw to keep the case simple
				ActionFromInput(1, false);
				Layer[0]->GetEyePoses();

				// Find perturbation of position from point 1m in front of camera
				XMVECTOR eye0 = ConvertToXM(Layer[0]->EyeRenderPose[0].Position);
				XMVECTOR eye1 = ConvertToXM(Layer[0]->EyeRenderPose[1].Position);
				XMVECTOR perturb = XMVectorScale(XMVectorAdd(eye0, eye1), 0.5f);

				// Calculate velocity from this
				const float sensitivity = 0.2f;
				XMVECTOR vel = XMVectorScale(XMVectorSet(XMVectorGetX(perturb), 0, XMVectorGetZ(perturb), 0), sensitivity);

				// Add velocity to camera
				MainCam->Pos = XMVectorAdd(MainCam->Pos, vel);

				for (int eye = 0; eye < 2; ++eye)
				{
					Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
				}

				Layer[0]->PrepareLayerHeader();
				DistortAndPresent(1);
			}
	    }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	AutoLinearVelocity app(hinst);
    return app.Run();
}
