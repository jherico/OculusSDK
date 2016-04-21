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
///!!!!!!!!!!!!!!Not sure this is working quite right, timing seems to be off
///!!!!!!!!!!!!!! - might be someone checked in a bug in OVRLib

/// This sample demonstrates how you can burden your application by rendering the room many times, 
/// such that it starts juddering ordinarily, and then remove that juddering and restore
/// the framerate by pressing '1' and allowing the CPU and GPU to run in parallel. 
/// In practise, for your hardware, first increase the burden until framerate is missed.
/// Then press '1' and see frame-rate restored.  This is because we are relaxing the requirement
/// for the GPU to finish before we go onto the next frame, and instead allow it to 
/// complete in parallel on the next frame.
/// However, this often results in an extra 1 frame of latency, so there is a tradeoff here
/// between allowing a higher quality of graphics to run in a frame, and having the lowest
/// latency possible for the best VR.

#include <Windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	return 0;
}


#if 0 // Disabled because the existing app rendering support is going away in the LibOVR 0.6 SDK.


#define   OVR_D3D_VERSION 11
#include "../../Common/Old/Win32_DirectXAppUtil.h" // DirectX
#include "../../Common/Old/Win32_BasicVR.h"        // Basic VR
#include "../../Common/Win32_AppRendered.h" // App-rendered

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
            // Load the application, to ensure it overruns without
            // the '1' key pressed
            int timesToDrawRoom = 300;
            appRenderVR.RenderSceneToEyeBuffer(eye,0,0,0,timesToDrawRoom);
        }

        Util.OutputFrameTime(ovr_GetTimeInSeconds());

        // Decide if allowing parallelism, with subsequent 
        // addition of extra frame of latency
        if (DIRECTX.Key['1']) appRenderVR.DistortAndPresent(0,0,0,0,false);
        else                  appRenderVR.DistortAndPresent(0,0,0,0,true);
    }

    return (appRenderVR.Release(hinst));
}


#endif