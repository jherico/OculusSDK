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
/// A sample to show a very intuitive jump mechanism by moving the Rift in an 
/// upward motion on your head.  The jump is proportional to the magnitude of 
/// your movement. 

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR
#include "../Common/Win32_ControlMethods.h"  // Control code

struct JumpFromAccelerometers : BasicVR
{
    JumpFromAccelerometers(HINSTANCE hinst) : BasicVR(hinst, L"Jump From Accelerometers") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    ActionFromInput();

            ovrTrackingState trackingState = Layer[0]->GetEyePoses();

            // Set jump from accelerometers y pos value into camera
		    MainCam->Pos = XMVectorSet(XMVectorGetX(MainCam->Pos),
			                           GetAccelJumpPosY(this, &trackingState),
									   XMVectorGetZ(MainCam->Pos), 0);

		    for (int eye = 0; eye < 2; ++eye)
		    {
			    Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
		    }

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	JumpFromAccelerometers app(hinst);
    return app.Run();
}
