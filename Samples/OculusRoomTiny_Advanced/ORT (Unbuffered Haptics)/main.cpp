/************************************************************************************
Filename    :   Win32_RoomTiny_Main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   20th July 2016
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
/// A sample to show vibration generation, with simple unbuffered 
/// and immediate commands.  
/// Press X on the Left Touch controller for a high frequency vibration
/// Press Y on the Left Touch controller for a low frequency vibration
/// Note - the Touch controller is not graphically displayed, 
/// to keep the sample minimal.

#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct UnbufferedHaptics : BasicVR
{
	UnbufferedHaptics(HINSTANCE hinst) : BasicVR(hinst, L"UnbufferedHaptics") {}

	void MainLoop()
	{
		Layer[0] = new VRLayer(Session);

		// Main loop
		while (HandleMessages())
		{
			Layer[0]->GetEyePoses();

			// Read the touch controller button presses, and 'play' the haptic buffer
			ovrInputState inputState;
			ovr_GetInputState(Session, ovrControllerType_Touch, &inputState);
			if      (inputState.Buttons & ovrTouch_X) ovr_SetControllerVibration(Session, ovrControllerType_LTouch, 1.0f, 1.0f); // High Freq
			else if (inputState.Buttons & ovrTouch_Y) ovr_SetControllerVibration(Session, ovrControllerType_LTouch, 0.0f, 1.0f); // Low Freq
			else                                      ovr_SetControllerVibration(Session, ovrControllerType_LTouch, 0.0f, 0.0f); // Off

			for (int eye = 0; eye < 2; ++eye)
			{
				XMMATRIX viewProj = Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
			}

			Layer[0]->PrepareLayerHeader();
			DistortAndPresent(1);
		}

	}
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	UnbufferedHaptics app(hinst);
	return app.Run();
}
