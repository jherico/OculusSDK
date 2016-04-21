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
/// This sample is a simple testbed for trying out some 3rd person 
/// techniques. At present, this is probably one of those samples that
/// shows what NOT to do.   In theory, linear motion segments
/// should make for comfortable movement.  However in practice
/// this doesn't work quite as well as expected.  

#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR
#include "../Common/Win32_ControlMethods.h"  // Control code


//--------------------------------------------
XMVECTOR GetSpecialYawRotation(VRLayer * vrLayer, float degree)
{
	// Increments in yaw are proportional to Rift yaw
	static float Yaw = 3.141f;
	XMVECTOR orientQuat = ConvertToXM(vrLayer->EyeRenderPose[0].Orientation);
	float yaw = GetEulerAngles(orientQuat).y;
	Yaw = degree*yaw;
	return (XMQuaternionRotationRollPitchYaw(0, Yaw, 0));
}




struct ThirdPerson : BasicVR
{
	ThirdPerson(HINSTANCE hinst) : BasicVR(hinst, L"ThirdPerson") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {

			//Lets move and control a vehicle
			static XMFLOAT3 carPos = XMFLOAT3(0, 0, 0);
			static float yaw = 0;
			static float speed = 0.0f;
			if (DIRECTX.Key[VK_LEFT])  yaw += 0.02f;
			if (DIRECTX.Key[VK_RIGHT]) yaw -= 0.02f;
			if (DIRECTX.Key[VK_UP])  speed += 0.002f;
			if (DIRECTX.Key[VK_DOWN]) speed -= 0.002f;
			///if (speed < 0) speed = 0;

			//Move pos on because of speed
			carPos.x += -speed * sin(yaw);
			carPos.z += -speed * cos(yaw);


			//Set positions in the model
			RoomScene->Models[0]->Pos = carPos;
			XMStoreFloat4(&RoomScene->Models[0]->Rot, XMQuaternionRotationRollPitchYaw(0, yaw, 0));


			//Now move the camera in response
			XMFLOAT3 idealCamPos;
			float distBehind = 4.0f;
			float heightOffGround = 2.0f;
			idealCamPos.x = carPos.x + distBehind * sin(0.0f/*yaw*/);
			idealCamPos.z = carPos.z + distBehind * cos(0.0f/*yaw*/);
			idealCamPos.y = heightOffGround;


			static int framesBetween = 90;// 90;

			static XMFLOAT3 cameraVel = XMFLOAT3(0, 0, 0);


			static int frameCount = 0;

			frameCount++;

			static XMFLOAT3 actualCamPos = XMFLOAT3(0,0,0);

			if (frameCount == framesBetween)
			{
				frameCount = 0;
				cameraVel.x = (idealCamPos.x - actualCamPos.x) / ((float)framesBetween);
				cameraVel.y = (idealCamPos.y - actualCamPos.y) / ((float)framesBetween);
				cameraVel.z = (idealCamPos.z - actualCamPos.z) / ((float)framesBetween);
			}



			actualCamPos.x += cameraVel.x;
			actualCamPos.y += cameraVel.y;
			actualCamPos.z += cameraVel.z;
			//	camPos.x = pos.x;
		//	camPos.y = pos.y + 2.0f;
		//	camPos.z = pos.z + 4.0f;

			MainCam->Pos = XMVectorSet(actualCamPos.x, actualCamPos.y, actualCamPos.z, 0);
			///MainCam->Rot = XMQuaternionRotationRollPitchYaw(0, yaw, 0);



		//    ActionFromInput();
		    Layer[0]->GetEyePoses();


			// Set auto yaw into camera
			///MainCam->Rot = GetSpecialYawRotation(Layer[0], 5.0f);




		    for (int eye = 0; eye < 2; ++eye)
		    {
			    Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye,0,0,frameCount==0? 1 : 1);
		    }

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	ThirdPerson app(hinst);
    return app.Run();
}
