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
/// A sample showing a combination of some of the other sample control
/// methods, to show how they can be combined, and to present an
/// interesting effect of them all together.  
/// This sample combines auto-yaw, jump from accelerometers, and 
/// tilt controlled movement.  Plus tap on the controller to fire a 
/// trivial bullet in the look direction.
/// Note, you can hold down SPACEBAR to temporarily disable tilt movement.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR
#include "../Common/Win32_ControlMethods.h"  // Control code

struct ControlCombination : BasicVR
{
    ControlCombination(HINSTANCE hinst) : BasicVR(hinst, L"Control Combination") {}

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
				// Take out manual yaw rotation (leaving button move for now)
				ActionFromInput(1, false);
				ovrTrackingState trackingState = Layer[0]->GetEyePoses();

				// Set various control methods into camera
				MainCam->Pos = XMVectorAdd(MainCam->Pos, FindVelocityFromTilt(this, Layer[0], &trackingState));

				MainCam->Pos = XMVectorSet(XMVectorGetX(MainCam->Pos),
					GetAccelJumpPosY(this, &trackingState),
					XMVectorGetZ(MainCam->Pos), 0);

				MainCam->Rot = GetAutoYawRotation(Layer[0]);

				// If tap side of Rift, then fire a bullet
				bool singleTap = WasItTapped(trackingState.HeadPose.LinearAcceleration);

				static XMVECTOR bulletPos = XMVectorZero();
				static XMVECTOR bulletVel = XMVectorZero();
				if (singleTap)
				{
					XMVECTOR eye0 = ConvertToXM(Layer[0]->EyeRenderPose[0].Position);
					XMVECTOR eye1 = ConvertToXM(Layer[0]->EyeRenderPose[1].Position);
					XMVECTOR midEyePos = XMVectorScale(XMVectorAdd(eye0, eye1), 0.5f);

					XMVECTOR totalRot = XMQuaternionMultiply(ConvertToXM(Layer[0]->EyeRenderPose[0].Orientation), MainCam->Rot);
					XMVECTOR posOfOrigin = XMVectorAdd(MainCam->Pos, XMVector3Rotate(midEyePos, MainCam->Rot));

					XMVECTOR unitDirOfMainCamera = XMVector3Rotate(XMVectorSet(0, 0, -1, 0), totalRot);

					bulletPos = XMVectorAdd(posOfOrigin, XMVectorScale(unitDirOfMainCamera, 2.0f));
					bulletVel = XMVectorScale(unitDirOfMainCamera, 0.3f);
				}

				// Move missile on, and set its position
				bulletPos = XMVectorAdd(bulletPos, bulletVel);
				XMStoreFloat3(&RoomScene->Models[1]->Pos, bulletPos);

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
	ControlCombination app(hinst);
    return app.Run();
}
