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
/// This sample shows how to interrogate and interpret the SDK to show
/// the camera cone of the position tracking camera.
/// Note how the wireframe box representing the camera in VR, should
/// appear to be in precisely the same location as the camera in the
/// real world.  If it isn't, then typically its a little forward or
/// back, which is caused by incorrect IPD for the user.
/// In particular, note the offset of the camera cone from the origin,
/// by the amount given by the SDK, and also the orientation of the 
/// camera that is a live, varying quantity, that should be accounted
/// for as displayed in this sample.
/// Also note the way the UVs of texture mapping are done on the cone - its 
/// tempting to do this differently, but beware optical illusions forcing
/// alternate interpretations on your brain, e.g with equally spaced lines.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h"   // DirectX
#include "../Common/Win32_BasicVR.h"    // Basic VR
#include "../Common/Win32_CameraCone.h" // Camera cone

struct CameraVolume : BasicVR
{
    CameraVolume(HINSTANCE hinst) : BasicVR(hinst, L"Camera Volume") {}

    void MainLoop()
    {
        Layer[0] = new VRLayer(Session);

        CameraCone cameraCone(this);

        while (HandleMessages())
        {
            ActionFromInput();

            // As we get eye poses, we also get the tracking state, for use later
            ovrTrackingState trackingState = Layer[0]->GetEyePoses();
            ovrTrackerPose   trackerPose  = ovr_GetTrackerPose(Session, 0);

            for (int eye = 0; eye < 2; ++eye)
            {
                Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);

                // Lets clear the depth buffer, so we can see it clearly.
                // even if that means sorting over the top.
                // And also we have a different z buffer range, so would sort strangely
                DIRECTX.Context->ClearDepthStencilView(Layer[0]->pEyeDepthBuffer[eye]->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

                // Note, we vary its visibility
                // and also note the constant update of the camera's
                // location and orientation from within the SDK
                cameraCone.RenderToEyeBuffer(Layer[0], eye, &trackingState, &trackerPose, 0.625f);
            }

            Layer[0]->PrepareLayerHeader();
            DistortAndPresent(1);
        }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	CameraVolume app(hinst);
    return app.Run();
}
