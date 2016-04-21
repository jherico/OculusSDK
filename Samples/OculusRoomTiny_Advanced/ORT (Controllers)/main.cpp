/************************************************************************************
Filename    :   Win32_RoomTiny_Main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   30th July 2015
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
/// An initial simple sample to show how to interrogate the touch
/// controllers.  In this sample, just the left controller is dealt with.
/// Move and rotate the controller, and also change its colour
/// by holding the X and Y buttons. 

#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct Controllers : BasicVR
{
    Controllers(HINSTANCE hinst) : BasicVR(hinst, L"Controllers") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    // Create a trivial model to represent the left controller
	    TriangleSet cube;
	    cube.AddSolidColorBox(0.05f, -0.05f, 0.05f, -0.05f, 0.05f, -0.05f, 0xff404040);
	    Model * controller = new Model(&cube, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1), new Material(new Texture(false, 256, 256, Texture::AUTO_CEILING)));
	
	    // Main loop
	    while (HandleMessages())
	    {
		    // We don't allow yaw change for now, as this sample is too simple to cater for it.
		    ActionFromInput(1.0f,false);
		    ovrTrackingState hmdState = Layer[0]->GetEyePoses();

		    //Write position and orientation into controller model.
		    controller->Pos = XMFLOAT3(XMVectorGetX(MainCam->Pos) + hmdState.HandPoses[ovrHand_Left].ThePose.Position.x,
			                           XMVectorGetY(MainCam->Pos) + hmdState.HandPoses[ovrHand_Left].ThePose.Position.y,
			                           XMVectorGetZ(MainCam->Pos) + hmdState.HandPoses[ovrHand_Left].ThePose.Position.z);
		    controller->Rot = XMFLOAT4(hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.x, 
                                       hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.y,
			                           hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.z, 
                                       hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.w);

		    //Button presses are modifying the colour of the controller model below
		    ovrInputState inputState;
		    ovr_GetInputState(Session, ovrControllerType_Touch, &inputState);

		    for (int eye = 0; eye < 2; ++eye)
		    {
			    XMMATRIX viewProj = Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);

			    // Render the controller model
			    controller->Render(&viewProj, 1, inputState.Buttons & ovrTouch_X ? 1.0f : 0.0f,
				                                 inputState.Buttons & ovrTouch_Y ? 1.0f : 0.0f, 1, true);
		    }

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }

        delete controller;
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	Controllers app(hinst);
    return app.Run();
}
