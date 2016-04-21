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
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR
#include "../Common/Win32_CameraCone.h" // Camera cone

struct CockpitMonoExterior : BasicVR
{
    CockpitMonoExterior(HINSTANCE hinst) : BasicVR(hinst, L"Cockpit Mono Exterior") {}

    void MainLoop()
    {
        // Ensure symmetrical FOV for simplest monoscopic. 
        ovrFovPort newFov[2];
        newFov[0].UpTan = max(HmdDesc.DefaultEyeFov[0].UpTan, HmdDesc.DefaultEyeFov[1].UpTan);
        newFov[0].DownTan = max(HmdDesc.DefaultEyeFov[0].DownTan, HmdDesc.DefaultEyeFov[1].DownTan);
        newFov[0].LeftTan = max(HmdDesc.DefaultEyeFov[0].LeftTan, HmdDesc.DefaultEyeFov[1].LeftTan);
        newFov[0].RightTan = max(HmdDesc.DefaultEyeFov[0].RightTan, HmdDesc.DefaultEyeFov[1].RightTan);
        newFov[1] = newFov[0];
        Layer[0] = new VRLayer(Session, newFov);

        // We'll use the camera cone as a convenient cockpit.
        CameraCone cameraCone(this);

        // We create an extra eye buffer, a means to render it
	    auto monoEyeTexture = new Texture(true, Layer[0]->pEyeRenderTexture[0]->SizeW, Layer[0]->pEyeRenderTexture[0]->SizeH);
        auto renderEyeTexture = new Model(new Material(monoEyeTexture), -1, -1, 1, 1);

	    while (HandleMessages())
	    {
		    ActionFromInput();

            // As we get eye poses, we also get the tracking state, for use later
            ovrTrackingState trackingState = Layer[0]->GetEyePoses();
            ovrTrackerPose   trackerPose   = ovr_GetTrackerPose(Session, 0);

            // Render the monoscopic far part into our buffer, with a tiny overlap to avoid a 'stitching line'.
            Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, 0, monoEyeTexture->TexRtv, &trackingState.HeadPose.ThePose);

            for (int eye = 0; eye < 2; ++eye)
            {
                if (DIRECTX.Key['1']) // For comparison, can render exterior as non-mono
                {
                    Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
                }
                else        
                {
                    // Manually set and clear the render target
                    DIRECTX.SetAndClearRenderTarget(Layer[0]->pEyeRenderTexture[eye]->GetRTV(),
                                                    Layer[0]->pEyeDepthBuffer[eye]);
		            DIRECTX.SetViewport((float) Layer[0]->EyeRenderViewport[eye].Pos.x,
					                    (float) Layer[0]->EyeRenderViewport[eye].Pos.y,
					                    (float) Layer[0]->EyeRenderViewport[eye].Size.w,
					                    (float) Layer[0]->EyeRenderViewport[eye].Size.h);
             
                    // Render the mono part, at infinity.
                    renderEyeTexture->Render(&XMMatrixIdentity(), 1, 1, 1, 1, true);
					Layer[0]->pEyeRenderTexture[eye]->Commit();
                }

                // Zero the depth buffer, to ensure the cockpit is rendered in the foreground
                DIRECTX.Context->ClearDepthStencilView(Layer[0]->pEyeDepthBuffer[eye]->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

                // Render cockpit
                cameraCone.RenderToEyeBuffer(Layer[0], eye, &trackingState, &trackerPose, 0.625f);
            }

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }

        delete renderEyeTexture;
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	CockpitMonoExterior app(hinst);
    return app.Run();
}
