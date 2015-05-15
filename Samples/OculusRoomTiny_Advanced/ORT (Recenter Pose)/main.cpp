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
/// A sample to show the use of the SDK function, to recenter the pose, by pressing '1'. 
/// In practise it is fairly straightforward to implement your own 
/// recentering functions by intercepting Rift poses and modifying them 
/// accordingly. In fact, that may be advisable as this function does one
/// specific implementation of recentering that may not be exactly 
/// what you require. 

#define   OVR_D3D_VERSION 11
#include "..\Common\Win32_DirectXAppUtil.h" // DirectX
#include "..\Common\Win32_BasicVR.h"  // Basic VR

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    BasicVR basicVR(hinst);
    basicVR.Layer[0] = new VRLayer(basicVR.HMD);

    // Main loop
    while (basicVR.HandleMessages())
    {
        basicVR.ActionFromInput();

        // Recenter the Rift by pressing '1'
        if (DIRECTX.Key['1'])
            ovrHmd_RecenterPose(basicVR.HMD);


        basicVR.Layer[0]->GetEyePoses();

        for (int eye = 0; eye < 2; eye++)
        {
            basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, eye);
        }

        basicVR.Layer[0]->PrepareLayerHeader();
        basicVR.DistortAndPresent(1);
    }

    return (basicVR.Release(hinst));
}
