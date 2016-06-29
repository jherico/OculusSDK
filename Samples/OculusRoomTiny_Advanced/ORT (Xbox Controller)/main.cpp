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
/// This is a simple sample to show how to read in and use
/// the button presses and features of the Xbox controller, using the
/// Oculus SDK.  
/// The background turns red if there is no Xbox controller detected, and green if one is.
/// At present the rumble functionality is not present - but the code is here to illustrate
/// how to do it, once it does become functional.

#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct XboxController : BasicVR
{
	XboxController(HINSTANCE hinst) : BasicVR(hinst, L"XboxController") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    ActionFromInput();
		    Layer[0]->GetEyePoses();

			// Read the remote state 
			ovrInputState inputState;
			ovr_GetInputState(Session, ovrControllerType_XBox, &inputState);
			unsigned int result = ovr_GetConnectedControllerTypes(Session);
			bool isXboxConnected = (result & ovrControllerType_XBox) ? true : false;

			// Some auxiliary controls we're going to read from the remote. 
			XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, -0.05f, 0), MainCam->Rot);
			XMVECTOR right = XMVector3Rotate(XMVectorSet(0.05f, 0, 0, 0), MainCam->Rot);
			if (inputState.Buttons & ovrButton_Up)	  MainCam->Pos = XMVectorAdd(MainCam->Pos, forward);
			if (inputState.Buttons & ovrButton_Down)  MainCam->Pos = XMVectorSubtract(MainCam->Pos, forward);
			if (inputState.Buttons & ovrButton_Left)  MainCam->Pos = XMVectorSubtract(MainCam->Pos, right);
			if (inputState.Buttons & ovrButton_Right)  MainCam->Pos = XMVectorAdd(MainCam->Pos, right);

			//Set the rumble - currently not functional
			ovrResult result2 = ovr_SetControllerVibration(Session, ovrControllerType_XBox, 100.0f/*frequency*/, 1000.0f/*amplitude*/);

			for (int eye = 0; eye < 2; ++eye)
		    {
				//Tint the world, green for it the controller is attached, otherwise red
				if (isXboxConnected)
					Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye, 0, 0, 1,/**/ 1, 0.5f, 1, 0.5f /*green*/);
				else 
					Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye, 0, 0, 1,/**/ 1, 1, 0, 0 /*red*/);
			}

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	XboxController app(hinst);
    return app.Run();
}
