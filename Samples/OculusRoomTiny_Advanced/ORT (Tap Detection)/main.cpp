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
/// A simple piece of sample code to show how to detect a user tapping
/// on the Rift. 

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR
#include "../Common/Win32_ControlMethods.h" // Control code

struct TapDetection : BasicVR
{
    TapDetection(HINSTANCE hinst) : BasicVR(hinst, L"Tap Detection") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    ActionFromInput();
		    ovrTrackingState trackingState = Layer[0]->GetEyePoses();

            // Change color mode if single tapped
            bool singleTap = WasItTapped(trackingState.HeadPose.LinearAcceleration);
            static int color_mode = 0;
            if (singleTap)
                ++color_mode;

            for (int eye = 0; eye < 2; ++eye)
		    {
                // Render world according to color mode
                switch (color_mode % 4)
                {
                case 0: Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye); break;
                case 1: Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye, 0, 0, 1, 1, 1, 0, 0); break;
                case 2: Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye, 0, 0, 1, 1, 0, 1, 0); break;
                case 3: Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye, 0, 0, 1, 1, 0, 0, 1); break;
                }
		    }

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	TapDetection app(hinst);
    return app.Run();
}
