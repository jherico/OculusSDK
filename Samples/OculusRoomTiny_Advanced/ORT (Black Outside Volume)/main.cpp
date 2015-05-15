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
/// Another in the camera volume series of samples, where we show fading
/// of the VR world to black as you exit the camera volume, as a simple
/// example of gracefully handling the players exit of proximity to the 
/// position-tracking camera volume.
/// This demo is by no means a perfect demonstration of the visually 
/// correct thing - it is here to show functionally how to code such things.

#define   OVR_D3D_VERSION 11
#include "..\Common\Win32_DirectXAppUtil.h"   // DirectX
#include "..\Common\Win32_BasicVR.h"    // Basic VR
#include "..\Common\Win32_CameraCone.h" // Camera cone 

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    BasicVR basicVR(hinst);
    basicVR.Layer[0] = new VRLayer(basicVR.HMD);
    
    CameraCone cameraCone(&basicVR);

    // Main loop
    while (basicVR.HandleMessages())
    {
        basicVR.ActionFromInput();

        // As we get eye poses, we also get the tracking state, for use later
        ovrTrackingState trackingState = basicVR.Layer[0]->GetEyePoses();

        // Now lets see how far off the volume we are
        // But we don't want our game position, we only want our rift generated position,
        // which we'll take as average of two positions.
        Vector3f eye0 = (Vector3f) basicVR.Layer[0]->EyeRenderPose[0].Position;
        Vector3f eye1 = (Vector3f) basicVR.Layer[0]->EyeRenderPose[1].Position;
        float dist = cameraCone.DistToBoundary(((eye0+eye1)*0.5f),trackingState.CameraPose);

        // We want it to be full visible at dist of 0.2 and below, but not becoming completely invisible
        const float distFullVisible = 0.2f;
        const float rateOfDimming = 4.0f;
        const float minVisibility = 0.1f;
        float visible = 1.0f - rateOfDimming*(dist-distFullVisible);  
        visible = max(visible,minVisibility);
        visible = min(visible,1.0f);

        for (int eye = 0; eye < 2; eye++)
        {
            // Render the proper scene, but adjust alpha
             basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene,eye,0,0,1,1-visible); 

            // Lets clear the depth buffer, so we can see it clearly.
            // even if that means sorting over the top.
            // And also we have a different z buffer range, so would sort strangely
            DIRECTX.Context->ClearDepthStencilView(basicVR.Layer[0]->pEyeDepthBuffer[eye]->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

            // Note, we vary its visibility
            cameraCone.RenderToEyeBuffer(basicVR.Layer[0],eye, &trackingState, visible);
        }

        basicVR.Layer[0]->PrepareLayerHeader();
        basicVR.DistortAndPresent(1);
    }
    
    return (basicVR.Release(hinst));
}
