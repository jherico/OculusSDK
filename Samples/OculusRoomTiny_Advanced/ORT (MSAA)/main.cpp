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
/// This sample demonstrates multi-sample anti-aliasing,
/// which can be compared to its absence by holding the '1' key.

#define   OVR_D3D_VERSION 11
#include "..\Common\Win32_DirectXAppUtil.h" // DirectX
#include "..\Common\Win32_BasicVR.h"  // Basic VR

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    BasicVR basicVR(hinst);
    basicVR.Layer[0] = new VRLayer(basicVR.HMD);

    // Make MSAA textures and depth buffers
    int sampleCount = 4;
    Texture  * MSAATexture[2];
    MSAATexture[0] = new Texture(true, basicVR.Layer[0]->pEyeRenderTexture[0]->Size, 0, sampleCount);
    MSAATexture[1] = new Texture(true, basicVR.Layer[0]->pEyeRenderTexture[1]->Size, 0, sampleCount);
    DepthBuffer * MSAADepthBuffer[2];
    MSAADepthBuffer[0] = new DepthBuffer(DIRECTX.Device, basicVR.Layer[0]->pEyeRenderTexture[0]->Size, sampleCount);
    MSAADepthBuffer[1] = new DepthBuffer(DIRECTX.Device, basicVR.Layer[0]->pEyeRenderTexture[1]->Size, sampleCount);

    // Main loop
    while (basicVR.HandleMessages())
    {
        basicVR.ActionFromInput();
        basicVR.Layer[0]->GetEyePoses();

        for (int eye = 0; eye < 2; eye++)
        {
            if (DIRECTX.Key['1'])
            {
                // Without MSAA, for comparison
                basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, eye);
            }
            else
            {
                // Render to higher resolution texture
                basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, eye, MSAATexture[eye]->TexRtv, 0, 1, 1, 1, 1, 1, 0.2f, 1000.0f, true,
                                                    MSAADepthBuffer[eye]);
                // Then resolve down into smaller buffer
                int destIndex = basicVR.Layer[0]->pEyeRenderTexture[eye]->TextureSet->CurrentIndex;
                ID3D11Texture2D* dstTex = ((ovrD3D11Texture*)&(basicVR.Layer[0]->pEyeRenderTexture[eye]->TextureSet->Textures[destIndex]))->D3D11.pTexture;
                DIRECTX.Context->ResolveSubresource(dstTex, 0, MSAATexture[eye]->Tex, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
            }
        }

        basicVR.Layer[0]->PrepareLayerHeader();
        basicVR.DistortAndPresent(1);
    }

    return (basicVR.Release(hinst));
}
