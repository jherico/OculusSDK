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
/// A simple sample to demonstrate the power and flexibility of the 'layers'
/// functionality now present in the SDK.  
/// This isn't intended as a sample of best practise or good use case, as such.  
/// Just showing how to set up layers with different characteristics.
/// Hold the '1' key to remove the smaller layer
/// Hold the '2' key to lower framerate on the outer layer - note as you translate, 
/// there are issues because of the lower framerate on that outer layer.
/// Hold the '3' key is freeze the 1st layer
/// Hold the '4' key is freeze the 2nd layer

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct Layers : BasicVR
{
    Layers(HINSTANCE hinst) : BasicVR(hinst, L"Layers") {}

    void MainLoop()
    {
        // Create a small FOV
        ovrFovPort newFOV[2];
        newFOV[0].DownTan = newFOV[0].UpTan = newFOV[0].LeftTan = newFOV[0].RightTan = 0.5f;
        newFOV[1] = newFOV[0];

        // Make layers, with 2nd one having smaller FOV, and the first having lower resolution
        Layer[0] = new VRLayer(Session, 0, 0.33f);
        Layer[1] = new VRLayer(Session, newFOV);

        // Main loop
        while (HandleMessages())
        {
            // Lets use a clock, and user input, to decide when each layer is updated
            static int clock = 0;
            ++clock;
            bool updateLayer0 = true;
            bool updateLayer1 = true;
            if ((DIRECTX.Key['2']) && ((clock % 4) != 0)) updateLayer0 = false;
            if (DIRECTX.Key['3']) updateLayer0 = false;
            if (DIRECTX.Key['4']) updateLayer1 = false;

            ActionFromInput();

            if (updateLayer0) Layer[0]->GetEyePoses();
            if (updateLayer1) Layer[1]->GetEyePoses();

            for (int eye = 0; eye < 2; ++eye)
            {
                if (updateLayer0) Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye, 0, 0, 1, 1, 1, 0.8f, 1);
                if (updateLayer1) Layer[1]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye, 0, 0, 1, 1, 1, 1, 1);
            }

            Layer[0]->PrepareLayerHeader();
            Layer[1]->PrepareLayerHeader();
        
            // Press 1 to show just one layer
            if (DIRECTX.Key['1']) DistortAndPresent(1);
            else                  DistortAndPresent(2);
        }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	Layers app(hinst);
    return app.Run();
}
