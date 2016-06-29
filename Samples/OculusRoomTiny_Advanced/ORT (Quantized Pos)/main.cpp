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
/// This sample extends the quantised yaw technique for comfort
/// with automated quantized position as well.
/// This is a potential mitigator of discomfort, and is here
/// as a starter sample to give a glimpse of the potential.
/// Controls for movement, as always, are cursor and 'WASD' keys.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR

struct QuantizedYaw : BasicVR
{
	QuantizedYaw(HINSTANCE hinst) : BasicVR(hinst, L"Quantized Yaw") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    ActionFromInput();
		    Layer[0]->GetEyePoses();
 
			//Now lets get out current position and store it.
			XMVECTOR storeCamPos = MainCam->Pos;
			XMVECTOR storeCamRot = MainCam->Rot;

			//Now write in a quantised version - do it over time, only update every so often
			//You can modify the parameter below to vary this - a lower value to make it
			//update more frequently - and a higher value to update less often.
			int framesBetweenUpdates = 20;
			static XMVECTOR quantCamPos = MainCam->Pos;
			static XMVECTOR quantCamRot = MainCam->Rot;

			static int count = 0; count++;
			if ((count % framesBetweenUpdates) == 0)
			{
				quantCamPos = MainCam->Pos;
				quantCamRot = MainCam->Rot;
			}
			MainCam->Pos = quantCamPos;
			MainCam->Rot = quantCamRot;


		    for (int eye = 0; eye < 2; ++eye)
		    {
			    Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
		    }

			//Now write the camera pos back in to allow movement logic to flow as if not quantised
			MainCam->Pos = storeCamPos;
			MainCam->Rot = storeCamRot;

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	QuantizedYaw app(hinst);
    return app.Run();
}
