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
/// This sample shows how to an application can save drawing the exterior
/// of a cockpit twice, by rendering it once and viewing monoscopically
/// at infinity.  
/// Hold the '1' key to toggle back to full stereoscopic 3D of the exterior
/// to compare the effect. 
/// The cockpit remains full stereoscopically 3D throughout.

#define   OVR_D3D_VERSION 11
#include "..\Common\Win32_DirectXAppUtil.h"   // DirectX
#include "..\Common\Win32_BasicVR.h"    // Basic VR
#include "..\Common\Win32_CameraCone.h" // Camera cone 

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    BasicVR basicVR(hinst);

    // Ensure symmetrical FOV for simplest monoscopic. 
    ovrFovPort newFov[2];
    newFov[0].UpTan = max(basicVR.HMD->DefaultEyeFov[0].UpTan, basicVR.HMD->DefaultEyeFov[1].UpTan);
    newFov[0].DownTan = max(basicVR.HMD->DefaultEyeFov[0].DownTan, basicVR.HMD->DefaultEyeFov[1].DownTan);
    newFov[0].LeftTan = max(basicVR.HMD->DefaultEyeFov[0].LeftTan, basicVR.HMD->DefaultEyeFov[1].LeftTan);
    newFov[0].RightTan = max(basicVR.HMD->DefaultEyeFov[0].RightTan, basicVR.HMD->DefaultEyeFov[1].RightTan);
    newFov[1] = newFov[0];
    basicVR.Layer[0] = new VRLayer(basicVR.HMD,newFov);

    // We'll use the camera cone as a convenient cockpit.
    CameraCone cameraCone(&basicVR);

    // We create an extra eye buffer, a means to render it
    Texture monoEyeTexture(true, basicVR.Layer[0]->pEyeRenderTexture[0]->Size);
    Model   renderEyeTexture(&monoEyeTexture, -1, -1, 1, 1);

    // Main loop
    while (basicVR.HandleMessages())
    {
        basicVR.ActionFromInput();

        // As we get eye poses, we also get the tracking state, for use later
        ovrTrackingState trackingState = basicVR.Layer[0]->GetEyePoses();

        // Render the monoscopic far part into our buffer, with a tiny overlap to avoid a 'stitching line'.
        basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, 0, monoEyeTexture.TexRtv, &trackingState.HeadPose.ThePose);

        for (int eye = 0; eye < 2; eye++)
        {
            if (DIRECTX.Key['1']) // For comparison, can render exterior as non-mono
            {
                basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, eye);
            }
            else        
            {
                // Manually set and clear the render target
                int texIndex = basicVR.Layer[0]->pEyeRenderTexture[eye]->TextureSet->CurrentIndex;
                DIRECTX.SetAndClearRenderTarget(basicVR.Layer[0]->pEyeRenderTexture[eye]->TexRtv[texIndex], 
                                                basicVR.Layer[0]->pEyeDepthBuffer[eye]);

                DIRECTX.SetViewport(Recti(basicVR.Layer[0]->EyeRenderViewport[eye]));
                 
                // Render the mono part, at infinity.
                renderEyeTexture.Render(Matrix4f(), 1, 1, 1, 1, true);
            }

            // Zero the depth buffer, to ensure the cockpit is rendered in the foreground
            DIRECTX.Context->ClearDepthStencilView(basicVR.Layer[0]->pEyeDepthBuffer[eye]->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

            // Render cockpit
            cameraCone.RenderToEyeBuffer(basicVR.Layer[0],eye, &trackingState, 0.625f);
        }

        basicVR.Layer[0]->PrepareLayerHeader();
        basicVR.DistortAndPresent(1);
    }

    return (basicVR.Release(hinst));
}
