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
/// In this sample, we use the layer system to show 
/// how to render a quad directly into the distorted image, 
/// thus bypassing the eye textures and retaining the resolution 
/// and precision of the original image.
/// The sample shows a simple textured quad, fixed in the scene 
/// in front of you.  By varying the input parameters, it
/// is simple to fix this into the scene if required, rather than
/// move and rotate with the player.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct DirectQuad : BasicVR
{
    DirectQuad(HINSTANCE hinst) : BasicVR(hinst, L"DirectQuad") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);


	    // Make a duplicate of the left eye texture, and render a static image into it
	    OculusTexture extraRenderTexture;
        if (!extraRenderTexture.Init(Session, 1024, 1024))
            return;

	    Camera zeroCam(XMVectorSet(-9, 2.25f, 0, 0), XMQuaternionRotationRollPitchYaw(0, 0.5f * 3.141f, 0));
	    ovrPosef zeroPose;
	    zeroPose.Position.x = 0;
	    zeroPose.Position.y = 0;
	    zeroPose.Position.z = 0;
	    zeroPose.Orientation.w = 1;
	    zeroPose.Orientation.x = 0;
	    zeroPose.Orientation.y = 0;
	    zeroPose.Orientation.z = 0;
	    Layer[0]->RenderSceneToEyeBuffer(&zeroCam, RoomScene, 0, extraRenderTexture.TexRtv[0], &zeroPose, 1, 1, 0.5f);
		// Mustn't forget to tell the SDK about it
		extraRenderTexture.Commit();



	    while (HandleMessages())
	    {
		    ActionFromInput();
		    Layer[0]->GetEyePoses();

		    for (int eye = 0; eye < 2; ++eye)
		    {
			    Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
		    }

		    Layer[0]->PrepareLayerHeader();


		    // Expanded distort and present from the basic sample, to allow for direct quad
		    ovrLayerHeader* layerHeaders[2];

		    // The standard one
			layerHeaders[0] = &Layer[0]->ovrLayer.Header;

		    // ...and now the new quad
		    static ovrLayerQuad myQuad;
            myQuad.Header.Type = ovrLayerType_Quad;
		    myQuad.Header.Flags = 0;
			myQuad.ColorTexture = extraRenderTexture.TextureChain;
			myQuad.Viewport.Pos.x = 0;
		    myQuad.Viewport.Pos.y = 0;
			myQuad.Viewport.Size.w = extraRenderTexture.SizeW;
			myQuad.Viewport.Size.h = extraRenderTexture.SizeH;
		    myQuad.QuadPoseCenter = zeroPose;
		    myQuad.QuadPoseCenter.Position.z = -1.0f;
		    myQuad.QuadSize.x = 1.0f;
		    myQuad.QuadSize.y = 2.0f;
		    layerHeaders[1] = &myQuad.Header;
			
		    // Submit them
		    presentResult = ovr_SubmitFrame(Session, 0, nullptr, layerHeaders, 2);
            if (!OVR_SUCCESS(presentResult))
                return;

		    // Render mirror
            ID3D11Resource* resource = nullptr;
            ovr_GetMirrorTextureBufferDX(Session, mirrorTexture, IID_PPV_ARGS(&resource));
            DIRECTX.Context->CopyResource(DIRECTX.BackBuffer, resource);
            resource->Release();
		    DIRECTX.SwapChain->Present(0, 0);
	    }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	DirectQuad app(hinst);
    return app.Run();
}
