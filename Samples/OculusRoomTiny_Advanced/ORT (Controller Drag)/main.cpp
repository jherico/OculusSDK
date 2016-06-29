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
/// WORK IN PROGRESS !!!!
/// WORK IN PROGRESS !!!!
/// WORK IN PROGRESS !!!!
/// An initial simple sample to show how to interrogate the touch
/// controllers, and how the presence of a stabilising cockpit, coupled
/// with direct control of the game world by the Touch controllers might
/// allow comfortable navigation 

#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR
#include "../Common/Win32_CameraCone.h" // Camera cone



struct ControllerDrag : BasicVR
{
	ControllerDrag(HINSTANCE hinst) : BasicVR(hinst, L"Controller Drag") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

		CameraCone cameraCone(this);


	    // Create a trivial model to represent the left controller
	    TriangleSet cube;
	    cube.AddSolidColorBox(0.05f, -0.05f, 0.05f, -0.05f, 0.05f, -0.05f, 0xff404040);
		Model * controllerL = new Model(&cube, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1), new Material(new Texture(false, 256, 256, Texture::AUTO_CEILING)));
		Model * controllerR = new Model(&cube, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1), new Material(new Texture(false, 256, 256, Texture::AUTO_CEILING)));

	    // Main loop
	    while (HandleMessages())
	    {
		    // We don't allow yaw change for now, as this sample is too simple to cater for it.
		    ActionFromInput(0.0f,false, true);
		    ovrTrackingState hmdState = Layer[0]->GetEyePoses();
			ovrTrackerPose   trackerPose = ovr_GetTrackerPose(Session, 0);


		    //Write position and orientation into controller models.
		    controllerL->Pos = XMFLOAT3(XMVectorGetX(MainCam->Pos) + hmdState.HandPoses[ovrHand_Left].ThePose.Position.x,
			                           XMVectorGetY(MainCam->Pos) + hmdState.HandPoses[ovrHand_Left].ThePose.Position.y,
			                           XMVectorGetZ(MainCam->Pos) + hmdState.HandPoses[ovrHand_Left].ThePose.Position.z);
		    controllerL->Rot = XMFLOAT4(hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.x, 
                                       hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.y,
			                           hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.z, 
                                       hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.w);
			controllerR->Pos = XMFLOAT3(XMVectorGetX(MainCam->Pos) + hmdState.HandPoses[ovrHand_Right].ThePose.Position.x,
				                        XMVectorGetY(MainCam->Pos) + hmdState.HandPoses[ovrHand_Right].ThePose.Position.y,
				                        XMVectorGetZ(MainCam->Pos) + hmdState.HandPoses[ovrHand_Right].ThePose.Position.z);
			controllerR->Rot = XMFLOAT4(hmdState.HandPoses[ovrHand_Right].ThePose.Orientation.x,
				                        hmdState.HandPoses[ovrHand_Right].ThePose.Orientation.y,
				                        hmdState.HandPoses[ovrHand_Right].ThePose.Orientation.z,
				                        hmdState.HandPoses[ovrHand_Right].ThePose.Orientation.w);


			//Stuff for position, reasonable straightforward
			ovrVector3f conPos = hmdState.HandPoses[ovrHand_Left].ThePose.Position;
			static ovrVector3f lastConPos;
			ovrVector3f diff;
			diff.x = conPos.x - lastConPos.x;
			diff.y = conPos.y - lastConPos.y;
			diff.z = conPos.z - lastConPos.z;

			//Stuff for rotation
			XMFLOAT4 conQuat = controllerL->Rot;
			static XMFLOAT4 lastConQuat;



		    //Button presses are modifying the colour of the controller model below
		    ovrInputState inputState;
		    ovr_GetInputState(Session, ovrControllerType_Touch, &inputState);

			//static XMFLOAT3 lastControllerPos;
			static bool buttonDown;
			if (inputState.Buttons & ovrTouch_X)
				buttonDown = true;
			else
				buttonDown = false;

		///		this->MainCam->Pos = initialPos = controllerL->Pos;


		    for (int eye = 0; eye < 2; ++eye)
		    {
			    XMMATRIX viewProj = Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye,0,0,1,1,1,1,1,0.01f);

			    // Render the controller models
				controllerL->Render(&viewProj, 1, buttonDown ? 1.0f : 0.0f,0.0f, 1, true);
				controllerR->Render(&viewProj, 1, inputState.Buttons & ovrTouch_A ? 1.0f : 0.0f,
					                              inputState.Buttons & ovrTouch_B ? 1.0f : 0.0f, 1, true);


				// Lets clear the depth buffer, so we can see it clearly.
				// even if that means sorting over the top.
				// And also we have a different z buffer range, so would sort strangely
				DIRECTX.Context->ClearDepthStencilView(Layer[0]->pEyeDepthBuffer[eye]->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

				// Note, we vary its visibility
				// and also note the constant update of the camera's
				// location and orientation from within the SDK
				cameraCone.RenderToEyeBuffer(Layer[0], eye, &hmdState, &trackerPose, 0.625f);

			}

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);


			float scale = -1.0f;
			if (buttonDown == true)
			{
				//Ok, thats the position.
				MainCam->Pos = XMVectorAdd(MainCam->Pos, XMVectorSet(scale*diff.x, scale*diff.y, scale*diff.z, 0));
				//For the rotation, I'm going to apply the inverse of the old one, then apply the new one - what could go wrong!

				XMVECTOR lastQuat = XMLoadFloat4(&lastConQuat);
				XMVECTOR invLastQuat = XMQuaternionInverse(lastQuat);
				MainCam->Rot = XMQuaternionMultiply(MainCam->Rot,lastQuat);
				XMVECTOR currQuat = XMLoadFloat4(&conQuat);
				XMVECTOR invCurrQuat = XMQuaternionInverse(currQuat);
				MainCam->Rot = XMQuaternionMultiply(MainCam->Rot, invCurrQuat);
			}


			lastConPos = conPos;
			lastConQuat = conQuat;
	    }

		delete controllerL;
		delete controllerR;
	}
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	ControllerDrag app(hinst);
    return app.Run();
}
