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
/// A simple sample to show how to scale your world by adjusting 
/// IPD and intereye distances.   This sample lets you press '1' or '2'
/// to scale the world.  World scaling is useful where your art assets
/// are not 1 unit = 1 metre, which the Oculus SDK would otherwise assume.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h"  // DirectX
#include "../Common/Win32_BasicVR.h"         // Basic VR

struct WorldScaling : BasicVR
{
    WorldScaling(HINSTANCE hinst) : BasicVR(hinst, L"World Scaling") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    ActionFromInput();
		    Layer[0]->GetEyePoses();

            // Decide our scale factor
            float scaleFactor = 1.0f;
            if (DIRECTX.Key['1']) scaleFactor = 0.5f;
            if (DIRECTX.Key['2']) scaleFactor = 4.0f;

            // Modify player height to fit with new scale
		    MainCam->Pos = XMVectorMultiply(MainCam->Pos, XMVectorSet(1, 1.0f / scaleFactor, 1, 1));

            for (int eye = 0; eye < 2; ++eye)
            {
                // Modify eye render pose used, since it incorporated
                // the eye offsets, which need to be scaled, and
                // the IPD.  Simply adjusting the output position
                // achieves the required result.
			    Layer[0]->EyeRenderPose[eye].Position.x /= scaleFactor;
                Layer[0]->EyeRenderPose[eye].Position.y /= scaleFactor;
                Layer[0]->EyeRenderPose[eye].Position.z /= scaleFactor;

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
	WorldScaling app(hinst);
    return app.Run();
}
