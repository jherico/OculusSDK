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
/// Another sample showing an interesting control method, this time positionally
/// accelerating your avatar via tilts on the Rift.  Hold SPACEBAR to suspend
/// motion to look around.  This is interesting, because the gravity component
/// of your head tilts suggests to your brain that this is a genuine motion, 
/// and thus mitigates motion sickness.
/// Try loosely following the animating cube with your positional movements.

#define   OVR_D3D_VERSION 11
#include "..\Common\Win32_DirectXAppUtil.h"  // DirectX
#include "..\Common\Win32_BasicVR.h"         // Basic VR
#include "..\Common\Win32_ControlMethods.h"  // Control code

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

        // Add velocity to camera
        basicVR.MainCam->Pos += FindVelocityFromTilt(&basicVR,basicVR.Layer[0],&trackingState);

        for (int eye = 0; eye < 2; eye++)
        {
            basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, eye);
        }

        basicVR.Layer[0]->PrepareLayerHeader();
        basicVR.DistortAndPresent(1);
    }

    return (basicVR.Release(hinst));
}
