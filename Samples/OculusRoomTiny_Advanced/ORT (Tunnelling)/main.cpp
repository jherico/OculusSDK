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
/// The is a sample based on some excellent work by Brant Lewis.
/// Brant's work is on a variation of OWD, which looks considerably prettier than 
/// this however many of the concepts are reproduced and explored.
/// The term 'tunnelling' is Brant's!!!
/// In this early sample, the central region of the screen moves according 
/// to your realtime controls, notably cursor movement and rotation
/// whilst the outer regions of the screen will
/// remain fixed relative to the user's real world frame of reference.
/// Hence the outer parts serve to ground the player and relief any discomfort
/// generated from motion.
/// Periodically the two regions are synched - thus if you aren't adding
/// additional movements or yaws, then the scene is unaffected.

/// Press 1 and 2 to vary the transparency of the outer margin
/// Press 3,4,5 and 6 to vary the x and y widths of the outer margin.
/// For now, the outer part is synched with the moving frame of reference
/// once every 60 game loops

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR



struct Tunnelling : BasicVR
{
    Tunnelling(HINSTANCE hinst) : BasicVR(hinst, L"Tunnelling") {}

    void MainLoop()
    {
		//Ensure symmetric frustom to make simple sample work
		ovrFovPort newFov[2];
		newFov[0].UpTan = max(HmdDesc.DefaultEyeFov[0].UpTan, HmdDesc.DefaultEyeFov[1].UpTan);
		newFov[0].DownTan = max(HmdDesc.DefaultEyeFov[0].DownTan, HmdDesc.DefaultEyeFov[1].DownTan);
		newFov[0].LeftTan = max(HmdDesc.DefaultEyeFov[0].LeftTan, HmdDesc.DefaultEyeFov[1].LeftTan);
		newFov[0].RightTan = max(HmdDesc.DefaultEyeFov[0].RightTan, HmdDesc.DefaultEyeFov[1].RightTan);
		newFov[1] = newFov[0];
		Layer[0] = new VRLayer(Session, newFov);

        // We create an extra eye buffer, a means to render it, and a static camera
        auto width = max(Layer[0]->pEyeRenderTexture[0]->SizeW, Layer[0]->pEyeRenderTexture[1]->SizeW);
        auto height = max(Layer[0]->pEyeRenderTexture[0]->SizeH, Layer[0]->pEyeRenderTexture[1]->SizeH);
        auto staticEyeTexture = new Texture(true, width, height);
 		TriangleSet quad;
		Material * staticMat = new Material(staticEyeTexture);
		Model * renderEyeTexture = 0;
		float marginx = 0.35f;
		float marginy = 0.35f;



		//Start the static camera to match
        Camera StaticMainCam = *MainCam;

	    while (HandleMessages())
	    {
		    ActionFromInput();
		    Layer[0]->GetEyePoses();

			static int clock = 0; clock++;
		    //Sync them periodically
            if ((clock % 60)==0) StaticMainCam = *MainCam;

			//Vary margin
            if (DIRECTX.Key['3']) marginx -= 0.001f;
            if (DIRECTX.Key['4']) marginx += 0.001f;
            if (DIRECTX.Key['5']) marginy -= 0.001f;
            if (DIRECTX.Key['6']) marginy += 0.001f;
			marginx = min(1.0f,marginx);
			marginx = max(0.0f,marginx);
			marginy = min(1.0f,marginy);
			marginy = max(0.0f,marginy);

			//Lets make a fresh model------------------------
			//We do it here in the game loop so we can vary its size
			quad.numIndices = 0;
			quad.numVertices = 0;
			float zDepth = 0;
			float minx,miny,maxx,maxy; 

			//Left side
			minx = -1; miny = -1;  maxx = -1 + 2*marginx; maxy = 1; 
			quad.AddQuad(Vertex(XMFLOAT3(minx, miny, zDepth), 0xffffffff, 0, 1.0f),
				Vertex(XMFLOAT3(minx, maxy, zDepth), 0xffffffff, 0, 0),
				Vertex(XMFLOAT3(maxx, miny, zDepth), 0xffffffff, marginx, 1.0f),
				Vertex(XMFLOAT3(maxx, maxy, zDepth), 0xffffffff, marginx, 0));
        
			//Right side
			minx = 1- 2*marginx; miny = -1;  maxx = 1.0f; maxy = 1; 
			quad.AddQuad(Vertex(XMFLOAT3(minx, miny, zDepth), 0xffffffff, 1-marginx, 1.0f),
						 Vertex(XMFLOAT3(minx, maxy, zDepth), 0xffffffff, 1-marginx, 0),
						 Vertex(XMFLOAT3(maxx, miny, zDepth), 0xffffffff, 1.0f, 1.0f),
						 Vertex(XMFLOAT3(maxx, maxy, zDepth), 0xffffffff, 1.0f, 0));
        
			//Top middle
			minx = -1 + 2*marginx; miny = 1-2*marginy;  maxx = 1-2*marginx; maxy = 1; 
			quad.AddQuad(Vertex(XMFLOAT3(minx, miny, zDepth), 0xffffffff, marginx, marginy),
						 Vertex(XMFLOAT3(minx, maxy, zDepth), 0xffffffff, marginx, 0.0f),
						 Vertex(XMFLOAT3(maxx, miny, zDepth), 0xffffffff, 1-marginx, marginy),
						 Vertex(XMFLOAT3(maxx, maxy, zDepth), 0xffffffff, 1-marginx, 0.0f));
        
			//Bot middle
			minx = -1 + 2*marginx; miny = -1.0f;  maxx = 1-2*marginx; maxy = -1+2*marginy; 
			quad.AddQuad(Vertex(XMFLOAT3(minx, miny, zDepth), 0xffffffff, marginx, 1.00f),
						 Vertex(XMFLOAT3(minx, maxy, zDepth), 0xffffffff, marginx, 1-marginy),
						 Vertex(XMFLOAT3(maxx, miny, zDepth), 0xffffffff, 1-marginx, 1.0f),
						 Vertex(XMFLOAT3(maxx, maxy, zDepth), 0xffffffff, 1-marginx, 1-marginy));
 			//Delete the vertices, leave the material intact.
			//A little wasteful of memory, but lets not worry
			if (renderEyeTexture)
			{
		        delete renderEyeTexture->VertexBuffer; renderEyeTexture->VertexBuffer = nullptr;
			    delete renderEyeTexture->IndexBuffer; renderEyeTexture->IndexBuffer = nullptr;
			}
			renderEyeTexture = new Model(&quad,XMFLOAT3(0,0,0),XMFLOAT4(0,0,0,1),staticMat);
			//----------------

			

		    for (int eye = 0; eye < 2; ++eye)
		    {
                // Render the scene from an unmoving, static player - to the new buffer
                Layer[0]->RenderSceneToEyeBuffer(&StaticMainCam, RoomScene, eye, staticEyeTexture->TexRtv, 0, 1, 1, 1, 1, 1);

                // Render the scene as normal
                Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye, 0, 0, 1, 1, 1, 1, 1); 

                // Render static one over the top - different levels of transparency on buttons '1' and '2'. 
                static float proportionOfStatic = 1.0f;
                if (DIRECTX.Key['1']) proportionOfStatic += 0.001f;
                if (DIRECTX.Key['2']) proportionOfStatic -= 0.001f;
				proportionOfStatic = min(1.0f,proportionOfStatic);
				proportionOfStatic = max(0.0f,proportionOfStatic);

				//Just change colour on a button press
				if ((DIRECTX.Key['1'])
				 || (DIRECTX.Key['2']) 
				 || (DIRECTX.Key['3']) 
				 || (DIRECTX.Key['4']) 
				 || (DIRECTX.Key['5']) 
				 || (DIRECTX.Key['6'])) 
				{
				    renderEyeTexture->Render(&XMMatrixIdentity(), 1, 0, 1, proportionOfStatic, true); 
				}
				else
				{
				    renderEyeTexture->Render(&XMMatrixIdentity(), 1, 1, 1, proportionOfStatic, true); 
				}
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
	Tunnelling app(hinst);
    return app.Run();
}

