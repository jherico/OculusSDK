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
/// Very similar to 'Near Stereo, Far Mono' sample, except now we create 
/// a third layer that is then at the vacant 'infinite-distance' slot.
/// Adjust switchpoint and IPD with keys '1' to '4', whereupon
/// the mid level mono will be purple and the far mono will be cyan.

#define   OVR_D3D_VERSION 11
#include "..\Common\Win32_DirectXAppUtil.h"  // DirectX
#include "..\Common\Win32_BasicVR.h"         // Basic VR

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    BasicVR basicVR(hinst);

    // Ensure symmetrical FOV for simplest monoscopic. 
    ovrFovPort newFov[2];
    newFov[0].UpTan    = max(basicVR.HMD->DefaultEyeFov[0].UpTan,    basicVR.HMD->DefaultEyeFov[1].UpTan);
    newFov[0].DownTan  = max(basicVR.HMD->DefaultEyeFov[0].DownTan,  basicVR.HMD->DefaultEyeFov[1].DownTan);
    newFov[0].LeftTan  = max(basicVR.HMD->DefaultEyeFov[0].LeftTan,  basicVR.HMD->DefaultEyeFov[1].LeftTan);
    newFov[0].RightTan = max(basicVR.HMD->DefaultEyeFov[0].RightTan, basicVR.HMD->DefaultEyeFov[1].RightTan);
    newFov[1] = newFov[0];
    basicVR.Layer[0] = new VRLayer(basicVR.HMD, newFov);

    // We create an extra eye buffers, and a means to render them
    Texture midMonoEyeTexture(true, basicVR.Layer[0]->pEyeRenderTexture[0]->Size);
    Model   renderMidMonoEyeTexture(&midMonoEyeTexture, -1, -1, 1, 1);
    Texture farMonoEyeTexture(true, basicVR.Layer[0]->pEyeRenderTexture[0]->Size);
    Model   renderFarMonoEyeTexture(&farMonoEyeTexture, -1, -1, 1, 1);

    // Main loop
    while (basicVR.HandleMessages())
    {
        basicVR.ActionFromInput();

        // Vary IPD and switchpoint
        bool visible = false;
        static float switchPoint = 4.0f;
        if (DIRECTX.Key['1']) { switchPoint -= 0.011f; visible = true; }
        if (DIRECTX.Key['2']) { switchPoint += 0.011f; visible = true; }
        static float newIPD = 0.064f;
        if (DIRECTX.Key['3']) { newIPD += 0.001f; visible = true; }
        if (DIRECTX.Key['4']) { newIPD -= 0.001f; visible = true; }
        Util.Output("IPD = %0.3f  Switch point = %0.2f\n", newIPD, switchPoint);

        // Get Eye poses, including central eye from ovrTrackingState.
        ovrTrackingState ots = basicVR.Layer[0]->GetEyePoses(0, 0, &newIPD);

        // Render the monoscopic far part into our buffer, with a tiny overlap to avoid a 'stitching line'.
        basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, 0, midMonoEyeTexture.TexRtv, &ots.HeadPose.ThePose, 1, 1, 1, 1, 1,
                                       switchPoint+(visible?0.1f:-0.1f), 1000.0f);

        // Lets render the scene again, but this time move it backwards, so it can represent
        // some other geometry in the distance.
        basicVR.MainCam->Pos.z -= 60;
        basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, 0, farMonoEyeTexture.TexRtv, &ots.HeadPose.ThePose, 1, 1, 1, 1, 1,
            switchPoint + (visible ? 0.1f : -0.1f), 1000.0f);
        basicVR.MainCam->Pos.z += 60;

        for (int eye = 0; eye < 2; eye++)
        {
            // Manually set and clear the render target
             int texIndex = basicVR.Layer[0]->pEyeRenderTexture[eye]->TextureSet->CurrentIndex;
            DIRECTX.SetAndClearRenderTarget(basicVR.Layer[0]->pEyeRenderTexture[eye]->TexRtv[texIndex],
                basicVR.Layer[0]->pEyeDepthBuffer[eye]);

            DIRECTX.SetViewport(Recti(basicVR.Layer[0]->EyeRenderViewport[eye]));

            // Render the infinite distance
            renderFarMonoEyeTexture.Render(Matrix4f(), visible ? 0.5f : 1, 1, 1, 1, true);

            // Zero the depth buffer, to ensure the infinite part is well and truly the far thing
            DIRECTX.Context->ClearDepthStencilView(basicVR.Layer[0]->pEyeDepthBuffer[eye]->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

            // Now render the mono part, but translated to ensure perfect matchup with 
            // the stereoscopic part.  If '5' pressed, then turn it off
            float translation = newIPD / ((newFov[0].LeftTan + newFov[0].RightTan)*switchPoint);
            if (DIRECTX.Key['5']) translation = 0;
            Matrix4f translateMatrix = Matrix4f::Translation(Vector3f(eye ? -translation : translation, 0, 0));
            renderMidMonoEyeTexture.Render(translateMatrix, 1, visible ? 0.5f : 1, 1, 1/*0.5f*/, true);

            // Zero the depth buffer, to ensure the stereo part is rendered in the foreground
            DIRECTX.Context->ClearDepthStencilView(basicVR.Layer[0]->pEyeDepthBuffer[eye]->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

            // Render the near stereoscopic part of the scene,
            // and making sure we don't clear the render target, as we would normally.
            basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene,eye, 0, 0,1,1, 1, 1, 1, 0.2f, switchPoint, false);
        }

        basicVR.Layer[0]->PrepareLayerHeader();
        basicVR.DistortAndPresent(1);
    }

    return (basicVR.Release(hinst));
}
