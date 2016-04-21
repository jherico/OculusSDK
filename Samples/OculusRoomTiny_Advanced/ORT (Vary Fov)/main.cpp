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
/// In this sample, pressing '1' varies the FOV.  This is not intended as a realtime 
/// adjustment, since the distortion meshes need to be recalculated at non-trivial
/// cost.  However, this sample illustrates both how to adjust FOV in the SDK,
/// and also underlines the concept of FOV in VR, which is really all about how 
/// much of the screen is visible, rather than zooming or wideangle as its 
/// traditionally thought of in non-VR applications.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h"  // DirectX
#include "../Common/Win32_BasicVR.h"         // Basic VR

struct VaryFOV : BasicVR
{
    VaryFOV(HINSTANCE hinst) : BasicVR(hinst, L"Vary FOV") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    ActionFromInput();
		    Layer[0]->GetEyePoses();

            // Modify fov and reconfigure VR - at present, not realtime as
            // new distortion meshes are created internally
            if (DIRECTX.Key['1'])
            {
                ovrFovPort newFov[2] = { HmdDesc.DefaultEyeFov[0], HmdDesc.DefaultEyeFov[1] };
                static int clock=0;
                ++clock;

                newFov[0].UpTan   += 0.2f * sin(0.20f * clock);
                newFov[0].DownTan += 0.2f * sin(0.16f * clock);
                newFov[1].UpTan   += 0.2f * sin(0.20f * clock);
                newFov[1].DownTan += 0.2f * sin(0.16f * clock);

                Layer[0]->ConfigureRendering(newFov); // Includes repreparing layer header
            }

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
	VaryFOV app(hinst);
    return app.Run();
}
