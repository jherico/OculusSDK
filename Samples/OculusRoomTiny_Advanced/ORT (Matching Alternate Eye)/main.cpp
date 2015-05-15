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
/// This sample is of the rather more complex version of alternate eye rendering.
/// It still renders only one eye per frame, thus saving the processing, but 
/// now it uses a third eye buffer to hold onto frames, and only present them
/// in both eyes when they are a stereoscopically-matching pair.  This gives
/// the impression of running at half frame-rate, even though rotationally the
/// timewarp fixes things up to rotationally be at full frame rate.   
/// Activate by holding the '1' key.
/// Additionally the user manual yaws are incorporated in the timewarp.  Thus the only artifact
/// remaining is that of the double-image animating object. 
/// Hold the '2' to temporarily disable incorporating user yaw into timewarp.

/// Some of the logic is little unusual, so these notes explain in a little
/// more depth what is happening in the 4 frame cycle.
/// Clock%4=0  Move then Render:eye0, into basic0,   Show extra,  basic1
/// Clock%4=1  (same pos)Render:eye1, into basic1,   Show basic0, basic1,
/// Clock%4=2  Move then Render:eye0, into extra     Show basic0, basic1.
/// Clock%4=3  (same pos)Render:eye1, into basic1    Show extra,  basic1

#define   OVR_D3D_VERSION 11
#include "..\Common\Win32_DirectXAppUtil.h" // DirectX
#include "..\Common\Win32_BasicVR.h"        // Basic VR

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    BasicVR basicVR(hinst);
    basicVR.Layer[0] = new VRLayer(basicVR.HMD);

    // Make a duplicate of the left eye texture, and a place to save renderpose
    OculusTexture  extraRenderTexture(basicVR.HMD, basicVR.Layer[0]->pEyeRenderTexture[0]->Size);
    ovrPosef extraRenderPose;

    // Main loop
    while (basicVR.HandleMessages())
    {
        // Keep a clock of what's happening
        static int clock = 0;
        clock++;

        // Adjust speed, because we only want movement at certain junctures
        float speed = 1;
        if (DIRECTX.Key['1'])
        {
            if ((clock % 2) != 0) speed = 0;
            else                  speed *= 2;
        }
        basicVR.ActionFromInput(speed);

        // Get Eye poses, but into a temporary buffer,
        ovrPosef tempEyeRenderPose[2];
        basicVR.Layer[0]->GetEyePoses(tempEyeRenderPose);

        // Now find out player yaw at this time
        Quatf playerOrientation(basicVR.MainCam->Rot);

        // And, we're going to store the player orientations from when we render
        static Quatf playerOrientationAtRender[2];
        static Quatf extraOrientationAtRender;

        for (int eye = 0; eye < 2; eye++)
        {
            if (DIRECTX.Key['1'])
            {
                // Don't do this eye
                if ((clock & 1) != eye) continue;

                // This situation, use the extra buffer, and we're done
                if (((clock % 4) == 2) && (eye == 0))
                {
                    extraRenderPose = tempEyeRenderPose[eye];
                    extraOrientationAtRender = playerOrientation;
                    basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, eye,
                                                             extraRenderTexture.TexRtv[extraRenderTexture.TextureSet->CurrentIndex],
                                                             &extraRenderPose);
                    continue;
                }
            }

            // Otherwise, operate as usual
            basicVR.Layer[0]->EyeRenderPose[eye] = tempEyeRenderPose[eye];
            playerOrientationAtRender[eye] = playerOrientation;
            basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, eye);
        }

        // If this situation is true, then want to use left texture and pose
        Quatf diff[2] = { Quatf(), Quatf() };
        if ((DIRECTX.Key['1']) && (((clock % 4) == 0) || ((clock % 4) == 3)))
        {
            if (!DIRECTX.Key['2']) diff[0] = extraOrientationAtRender.Inverted()     * playerOrientation;
            if (!DIRECTX.Key['2']) diff[1] = playerOrientationAtRender[1].Inverted() * playerOrientation;
            basicVR.Layer[0]->PrepareLayerHeader(&extraRenderTexture, &extraRenderPose, diff);
            basicVR.DistortAndPresent(1);
        }
        else
        {
            if (!DIRECTX.Key['2']) diff[0] = playerOrientationAtRender[0].Inverted() * playerOrientation;
            if (!DIRECTX.Key['2']) diff[1] = playerOrientationAtRender[1].Inverted() * playerOrientation;
            basicVR.Layer[0]->PrepareLayerHeader(0, 0, diff);
            basicVR.DistortAndPresent(1);

        }
    }

    ovrHmd_DestroySwapTextureSet(basicVR.HMD, extraRenderTexture.TextureSet);
    return (basicVR.Release(hinst));
}
