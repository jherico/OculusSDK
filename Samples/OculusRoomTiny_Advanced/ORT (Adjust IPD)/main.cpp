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
/// This sample shows how to vary IPD (interpupillary distance).  Again, this
/// should have been set correctly behind the scenes, from the Oculus config tool,
/// but if objects are perceived at different sizes than expected, then wrong IPD
/// can be the cause, so this sample shows how you can vary it manually,
/// ideally for debug purposes.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct AdjustIPD : BasicVR
{
    AdjustIPD(HINSTANCE hinst) : BasicVR(hinst, L"Adjust IPD") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    ActionFromInput();

            float newIPD = 0.064f;
            if (DIRECTX.Key['1']) newIPD = 0.05f;
            if (DIRECTX.Key['2']) newIPD = 0.06f;
            if (DIRECTX.Key['3']) newIPD = 0.07f;
            if (DIRECTX.Key['4']) newIPD = 0.08f;
            Layer[0]->GetEyePoses(0, 0, &newIPD);

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
	AdjustIPD app(hinst);
    return app.Run();
}
