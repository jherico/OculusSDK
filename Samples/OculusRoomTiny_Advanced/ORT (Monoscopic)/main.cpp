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
/// The sample shows a monoscopic view saving the double
/// rendering by only rendering a single central eye buffer
/// and distorting it for both eyes.

/// The sample allows you to press '1' and toggle the view back to stereoscopic,
/// as a primary use of this sample - to show the difference between the two
/// - which is readily perceived when the two are toggled, but easily missed
/// when the two aren't compared - especially with high quality graphics
/// providing a high degree of depth cues.  In fact, many applications have been
/// found to be accidentally monoscopic, so its useful to have such a 
/// debug toggle in your applications.

#define   OVR_D3D_VERSION 11
#include "..\Common\Win32_DirectXAppUtil.h" // DirectX
#include "..\Common\Win32_BasicVR.h"  // Basic VR

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    BasicVR basicVR(hinst);

    // Ensure symmetrical FOV in a simplistic way for now.
    // For DK2, this is more or less identical to the ideal FOV,
    // but for Hmd's where it isn't, then there will be performance
    // savings by drawing less of the eye texture for each eye,
    ovrFovPort newFov[2];
    newFov[0].UpTan    = max(basicVR.HMD->DefaultEyeFov[0].UpTan,    basicVR.HMD->DefaultEyeFov[1].UpTan);
    newFov[0].DownTan  = max(basicVR.HMD->DefaultEyeFov[0].DownTan,  basicVR.HMD->DefaultEyeFov[1].DownTan);
    newFov[0].LeftTan  = max(basicVR.HMD->DefaultEyeFov[0].LeftTan,  basicVR.HMD->DefaultEyeFov[1].LeftTan);
    newFov[0].RightTan = max(basicVR.HMD->DefaultEyeFov[0].RightTan, basicVR.HMD->DefaultEyeFov[1].RightTan);
    newFov[1] = newFov[0];
    basicVR.Layer[0] = new VRLayer(basicVR.HMD, newFov);
    
    // Main loop
    while (basicVR.HandleMessages())
    {
        basicVR.ActionFromInput();

        // Monoscopic
        if (!DIRECTX.Key['1'])
        {
            // Set IPD to zero, so getting 'middle eye'
            float scaleIPD = 0.0f;
            basicVR.Layer[0]->GetEyePoses(0, &scaleIPD);

            // Just do the one eye, the right one.
            basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, 1);

            // And now insist that the left texture used, is actually the right one.
            basicVR.Layer[0]->PrepareLayerHeader(basicVR.Layer[0]->pEyeRenderTexture[1]);
            basicVR.DistortAndPresent(1);
        }
        else // Regular stereoscopic for comparison
        {
            basicVR.Layer[0]->GetEyePoses();
            basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, 0);
            basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, 1);
            basicVR.Layer[0]->PrepareLayerHeader();
            basicVR.DistortAndPresent(1);
        }
    }
    return (basicVR.Release(hinst));
}
