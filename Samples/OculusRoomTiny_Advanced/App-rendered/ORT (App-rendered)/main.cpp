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
/// This samples shows the Room Tiny demo, but using the alternative 
/// programming path, known as 'App-rendered', or sometimes 'Client-rendered'.
/// Instead of the SDK performing the distortion rendering, and handling other
/// items internally, this path allows the developer much greater control in 
/// exposing these items and letting the developer do them manually. 
/// It is intended that the non-App-rendered path will absorb the desired 
/// functionality of this path, but in the meantime, here is the sample code.

#include <Windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return 0;
}


#if 0 // Disabled because the existing app rendering support is going away in the LibOVR 0.6 SDK.

#define   OVR_D3D_VERSION 11
#include "../../Common/Old/Win32_DirectXAppUtil.h" // DirectX
#include "../../Common/Old/Win32_BasicVR.h"        // Basic VR
#include "../../Common/Win32_AppRendered.h"    // App-rendered

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    AppRenderVR appRenderVR(hinst);
    appRenderVR.ConfigureRendering();

    // Main loop
    while (appRenderVR.HandleMessages())
    {
        appRenderVR.BeginFrame();
        appRenderVR.ActionFromInput();
        appRenderVR.GetEyePoses();

        for (int eye = 0; eye < 2; eye++)
        {
            appRenderVR.RenderSceneToEyeBuffer(eye);
        }

        appRenderVR.DistortAndPresent();
     }

    return (appRenderVR.Release(hinst));
}

#endif

