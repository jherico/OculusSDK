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
/// This sample shows how to 'freeze' the images rendered for each eye,
/// and let timewarp fix up the image scene.  This is a recommended piece
/// of functionality to ensure that your timewarp functionality is operating 
/// correctly, being quite a subtle but fundamental effect.
/// Hold the '1' key to freeze timewarp, with no new pose data for the 
/// rendering of the texture - although texture is still updated.
/// Hold '2' to freeze even the update of the texture too.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct FreezeTimewarp : BasicVR
{
    FreezeTimewarp(HINSTANCE hinst) : BasicVR(hinst, L"Freeze Timewarp") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    ActionFromInput();

            // Don't update the basic eye poses if '1' or '2' are pressed.
            // These same eye poses are then fed into the SDK, which 
            // timewarps to the current view automatically.
            if ((DIRECTX.Key['1'] == false)
             && (DIRECTX.Key['2'] == false))
                Layer[0]->GetEyePoses();

            // If the eye poses aren't updated, you can opt t not render a new eye buffer
            // so the SDK will continue with the old one.
            if (DIRECTX.Key['2'] == false)
            {
		        for (int eye = 0; eye < 2; ++eye)
		        {
			        Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
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
	FreezeTimewarp app(hinst);
    return app.Run();
}
