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
/// This sample shows two important debug facilities, that work using the 
/// 'App-rendered' rendering path.

/// This sample illustrates the importance of correct eye-relief, but allowing you
/// to modify it by pressing keys '1' and '2'.  Eye-relief is the distance of your
/// eye from the Rift lens, and controls the distortion correction which is vital 
/// to correct VR.  Ideally, you will have set the correct eye-relief via the 
/// Oculus config tool, and that value should automatically come through.  However, to 
/// debug, this functionality to adjust this, is available in 'App-rendered' mode. 
/// Note that chromatic aberration correction is tied to distortion, and thus this 
/// parameter, and is an easily recognisable symptom if you have the wrong 
/// eye relief.

/// This sample also shows how to manually adjust the timing of the SDK.  Timing is 
/// very important as the SDK needs to predict exactly when the eye images are on
/// the screen, in order to correctly predict the Rift pose to therefore render 
/// the correct image at that time.  This is only available in the 'App-rendered'
/// version, and the user can press keys '3' to '6' for preset adjustments to the timing.
/// In particular, not the detrimental effects of under- or over- prediction and 
/// learn to recognise such things in your application.  In particular, you may 
/// wish to implement such timing variation in your app, to confirm for yourself
/// that the timing delivered by the SDK is correct (it may not be for all hardware
/// yet, but ultimately will be).

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

		// ADJUST EYE RELIEF
		// Shows the range of eye relief possible from the config tool, and how to 
		// live adjust them in an application.  Use keys '1' and '2'.
		// Note that the distortion meshes need to be recreated when this is adjusted,
		// hence not currently a realtime switch.
		// Non SDK-rendered only.
		// TBD - an example of reverting the eye relief back to the profile value 
		if (DIRECTX.Key['1']) appRenderVR.MakeNewDistortionMeshes(0.001f); // Min eye relief
		if (DIRECTX.Key['2']) appRenderVR.MakeNewDistortionMeshes(1.000f); // Max eye relief

        for (int eye = 0; eye < 2; eye++)
        {
            appRenderVR.RenderSceneToEyeBuffer(eye);
        }

		// ADJUST TIMING
        // Adjustng the timing in order to display and recognise the detrimental effects of incorrect timing,
        // and for perhaps correcting timing temporarily on less supported hardware.
        // Non SDK-rendered only.
        double TimeAdjuster = 0;
        if (DIRECTX.Key['3']) TimeAdjuster = -0.026; // Greatly underpredicting
        if (DIRECTX.Key['4']) TimeAdjuster = -0.006; // Slightly underpredicting
        if (DIRECTX.Key['5']) TimeAdjuster = +0.006; // Slightly overpredicting
        if (DIRECTX.Key['6']) TimeAdjuster = +0.026; // Greatly overpredicting
        appRenderVR.DistortAndPresent(0,0,TimeAdjuster);
     }

    return (appRenderVR.Release(hinst));
}

#endif