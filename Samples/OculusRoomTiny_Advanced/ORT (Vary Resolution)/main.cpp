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
/// This sample shows how the Oculus SDK readily allows realtime adjustment
/// of the resolution of the eye buffers.  Press '1' or '2' and the resolutions
/// cycle through low to high.  Having such dynamic resolution enables some 
/// applications to control their frame-rate, if lower resolution buffers significantly
/// improves performance.

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
        basicVR.ActionFromInput();
        basicVR.Layer[0]->GetEyePoses();

        // Incrememt a clock 
        static int clock = 0;
        clock++;

        for (int eye = 0; eye < 2; eye++)
        {
            // Realtime adjustment of eye buffer resolution,
            // vertically by pressing 1, horizontally by pressing '2'.
            if (DIRECTX.Key['1']) basicVR.Layer[0]->EyeRenderViewport[eye].Size.h
                = (int)(basicVR.Layer[0]->pEyeRenderTexture[eye]->Size.h
                *(2 + sin(0.05f*clock)) / 3.0f);
            if (DIRECTX.Key['2']) basicVR.Layer[0]->EyeRenderViewport[eye].Size.w
                = (int)(basicVR.Layer[0]->pEyeRenderTexture[eye]->Size.w
                                        *(1.25f+sin(0.1f*clock))/2.25f);
            basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, eye);
        }

        basicVR.Layer[0]->PrepareLayerHeader();
        basicVR.DistortAndPresent(1);
    }

    return (basicVR.Release(hinst));
}
