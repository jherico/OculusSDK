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
/// This sample is a step on from the 'quantized yaw' sample, where we seek
/// to retain the anti-nausea benefits, but with a less jarring and 
/// immersion-breaking effect.  One of the means by which this appears to work,
/// is the short duration of the turn effect, before stopping it, and restarting.
/// Note, this would probably benefit from folding that yaw, into the 
/// timewarp calculation, as seen in other samples.

#define   OVR_D3D_VERSION 11
#include "..\Common\Win32_DirectXAppUtil.h" // DirectX
#include "..\Common\Win32_BasicVR.h"        // Basic VR

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    BasicVR basicVR(hinst);
    basicVR.Layer[0] = new VRLayer(basicVR.HMD);

    // Main loop
    while (basicVR.HandleMessages())
    {
        basicVR.ActionFromInput(1,false);
        basicVR.Layer[0]->GetEyePoses();

        // We're going to override the basic Yaw, note its disabled in the ActionFromInput call above
        // We only allow a certain duration of turning
        const int framesAtATime = 37; // About half a second for DK2
        
        // ...and we insist that following pause if observed, if holding the key down
        // this is ignored if you let go of the buttons, allowing, in theory, a faster turn
        // if you have a slow value below.  In practise, put it quite small.
        const int framesToRest = 10;

        // And we can vary the rotation speed if you wish
        const float rotSpeed = 0.02f;

        // Start conditions
        static int framesCanTurn = framesAtATime;
        static int framesToWait = 0;
        static float Yaw = 3.141f;

        if (DIRECTX.Key[VK_LEFT] || DIRECTX.Key[VK_RIGHT])
        {
            if (framesCanTurn) 
            {
                if (DIRECTX.Key[VK_LEFT])  basicVR.MainCam->Rot = Matrix4f::RotationY(Yaw+=rotSpeed);
                if (DIRECTX.Key[VK_RIGHT]) basicVR.MainCam->Rot = Matrix4f::RotationY(Yaw-=rotSpeed);
                framesCanTurn--;
                if (framesCanTurn==0) framesToWait = framesToRest;
            }
        }
        else // If let go, then it resets
        {
            framesCanTurn=framesAtATime;
            framesToWait = 0;
        }
        // If we have to wait, lets diminish them
        if (framesToRest)
        {
            framesToWait--;
            if (framesToWait==0) framesCanTurn = framesAtATime;
        }
 
        for (int eye = 0; eye < 2; eye++)
        {
            basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, eye);
        }

        basicVR.Layer[0]->PrepareLayerHeader();
        basicVR.DistortAndPresent(1);
    }

    return (basicVR.Release(hinst));
}
