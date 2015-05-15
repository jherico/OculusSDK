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
#include "..\Common\Win32_DirectXAppUtil.h" // DirectX
#include "..\Common\Win32_BasicVR.h"        // Basic VR
#include "..\Common\Win32_ControlMethods.h" // Control code

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    BasicVR basicVR(hinst);
    basicVR.Layer[0] = new VRLayer(basicVR.HMD);

    // Main loop
    while (basicVR.HandleMessages())
    {
        basicVR.ActionFromInput();
        ovrTrackingState trackingState = basicVR.Layer[0]->GetEyePoses();

        // Change colour mode if single tapped
        bool singleTap = WasItTapped(trackingState.HeadPose.LinearAcceleration);
        static int color_mode = 0;
        if (singleTap) color_mode++;

        for (int eye = 0; eye < 2; eye++)
        {
            // Render world according to colour mode
            switch(color_mode % 4)
            {
                case(0) : basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, eye); break;
                case(1) : basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene,eye,0,0,1,1,1,0,0); break;
                case(2) : basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene,eye,0,0,1,1,0,1,0); break;
                case(3) : basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene,eye,0,0,1,1,0,0,1); break;
            }
        }

        basicVR.Layer[0]->PrepareLayerHeader();
        basicVR.DistortAndPresent(1);
    }

    return (basicVR.Release(hinst));
}
