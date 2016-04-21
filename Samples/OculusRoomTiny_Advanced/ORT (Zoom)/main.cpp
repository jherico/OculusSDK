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
/// This sample shows one method of employing a 'zoomed-in' view
/// in an application.  Press '1' and '2' to vary the zoom.
/// To have the entire view zoomed, will be
/// very uncomfortable, but we let you try this to see,
/// by pressing '3' and '4' to make the scope fill more of the
/// screen - once it gets big enough, the effect is very nauseous.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR

struct ZoomForIO : BasicVR
{
    ZoomForIO(HINSTANCE hinst) : BasicVR(hinst, L"Zoom For IO") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

        // Make a texture to render the zoomed image into.  Make it same size as left eye buffer, for simplicity.
        auto zoomedTexture = new Texture(true, max(Layer[0]->pEyeRenderTexture[0]->SizeW, Layer[0]->pEyeRenderTexture[1]->SizeW),
                                               max(Layer[0]->pEyeRenderTexture[0]->SizeH, Layer[0]->pEyeRenderTexture[1]->SizeH));

        // Make a scope model - its small and close to us
        float scopeScale = 0.25f;
        auto cube = new TriangleSet();
        cube->AddQuad(Vertex(XMFLOAT3( scopeScale,  scopeScale, 0), 0xffffffff, 0, 0),
                     Vertex(XMFLOAT3(-scopeScale,  scopeScale, 0), 0xffffffff, 1, 0),
                     Vertex(XMFLOAT3( scopeScale, -scopeScale, 0), 0xffffffff, 0, 1),
                     Vertex(XMFLOAT3(-scopeScale, -scopeScale, 0), 0xffffffff, 1, 1));
        auto sniperModel = new Model(cube, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1), new Material(zoomedTexture));

	    while (HandleMessages())
	    {
		    ActionFromInput();
		    Layer[0]->GetEyePoses();

            // Render the zoomed scene, making sure we clear the back screen with solid alpha
            DIRECTX.SetAndClearRenderTarget(zoomedTexture->TexRtv, Layer[0]->pEyeDepthBuffer[0], 0, 0, 0, 1);

            // Lets set a slightly small viewport, so we get a black border
            int blackBorder = 16;
            DIRECTX.SetViewport((float)Layer[0]->EyeRenderViewport[0].Pos.x + blackBorder,
                                (float)Layer[0]->EyeRenderViewport[0].Pos.y + blackBorder,
                                (float)Layer[0]->EyeRenderViewport[0].Size.w - 2*blackBorder,
                                (float)Layer[0]->EyeRenderViewport[0].Size.h - 2*blackBorder);
    
		    //Get the pose information in XM format
		    XMVECTOR eyeQuat = ConvertToXM(Layer[0]->EyeRenderPose[0].Orientation);
		    XMVECTOR eyePos  = ConvertToXM(Layer[0]->EyeRenderPose[0].Position);

            // Get view and projection matrices for the Rift camera
		    XMVECTOR CombinedPos = XMVectorAdd(MainCam->Pos, XMVector3Rotate(eyePos, MainCam->Rot));
		    Camera finalCam(&CombinedPos, &(XMQuaternionMultiply(eyeQuat, MainCam->Rot)));
            XMMATRIX view = finalCam.GetViewMatrix();

            // Vary amount of zoom with '1' and '2'Lets pick a zoomed in FOV
            static float amountOfZoom = 0.1f;
            if (DIRECTX.Key['1']) amountOfZoom = max(amountOfZoom - 0.002f, 0.050f);
            if (DIRECTX.Key['2']) amountOfZoom = min(amountOfZoom + 0.002f, 0.500f);
            ovrFovPort zoomedFOV;
            zoomedFOV.DownTan = zoomedFOV.UpTan = zoomedFOV.LeftTan = zoomedFOV.RightTan = amountOfZoom;

            // Finally, render zoomed scene onto the texture
            XMMATRIX proj = ConvertToXM(ovrMatrix4f_Projection(zoomedFOV, 0.2f, 1000.0f, ovrProjection_None));
            XMMATRIX projView = XMMatrixMultiply(view,proj);
            RoomScene->Render(&projView, 1, 1, 1, 1, true);

		    for (int eye = 0; eye < 2; ++eye)
		    {
                // Render main, outer world
                Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);

                // Render scope with special static camera, always in front of us
                static float howFarAway = 0.75f;
                if (DIRECTX.Key['3']) howFarAway = max(howFarAway - 0.002f, 0.25f);
                if (DIRECTX.Key['4']) howFarAway = min(howFarAway + 0.002f, 1.00f);
                Camera  StaticMainCam(&XMVectorSet(0, 0, -howFarAway,0), &XMQuaternionRotationRollPitchYaw(0, 3.14f, 0));
                XMMATRIX view = StaticMainCam.GetViewMatrix();
                XMMATRIX proj = ConvertToXM(ovrMatrix4f_Projection(Layer[0]->EyeRenderDesc[eye].Fov, 0.2f, 1000.0f, ovrProjection_None));
                XMMATRIX projView = XMMatrixMultiply(view, proj);
                sniperModel->Render(&projView, 1, 1, 1, 1, true);
		    }

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }

        delete sniperModel;
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	ZoomForIO app(hinst);
    return app.Run();
}
