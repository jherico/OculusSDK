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
/// In this simple sample, we set the flag for protected content,
/// as our application is initialising its VR swapchain.
/// This then prevents other things like mirroring, capture buffers & APIs,
/// and DVR apps from getting the data (they should get black).
/// Specifically in this sample, the mirror window is thus black,
/// whilst the HMD continues to display the scene as normal.
/// Experiment by setting the value to false, and see the mirror
/// window return. 
/// This works as long as the HMD connected supports HDCP (DK2, CV1 are fine, CrescentBay does not).

#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct ProtectedContent : BasicVR
{
    ProtectedContent(HINSTANCE hinst) : BasicVR(hinst, L"Protected Content") {}

    void MainLoop()
    {
		//When we initialise our VR layer, we send through a parameter that
		//will add the extra flag ovrTextureMisc_ProtectedContent to our misc_flags
		//when we are creating the swapchain.
	    Layer[0] = new VRLayer(Session,0,1.0f,true); 

	    while (HandleMessages())
	    {
		    ActionFromInput();
		    Layer[0]->GetEyePoses();

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
	ProtectedContent app(hinst);
    return app.Run();
}
