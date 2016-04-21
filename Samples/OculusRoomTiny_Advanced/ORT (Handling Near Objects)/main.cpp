/************************************************************************************
Filename    :   Win32_RoomTiny_Main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   16th Mar 2016
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
/// This sample shows a method of handling near objects.  
/// In particular, the problem arises that we have positional tracking, and the possibility
/// exists in a lot of games, to move right up to the scenery, such that it appear centimeters
/// or millimetres away, when eye comfort would seem to dictate a minimum distance of 10-20cm.
/// Too close objects are the extreme of mismatch between accomodation of your lens, and 
/// and convergence of your eyes - its unnatural, and not the most comfortable.
/// One method is to simply set the near clip plane appropriately, as the other samples do,
/// but it can be 'counter-immersive' to see graphics clip out. 
/// This method shows an alternative,
/// where you set the clip plane very near, and then use a simple pixel shader change
/// to fade out the detail of the near object to a uniform colour, thus your eye is 
/// considerably less tempted to attempt viewing of the too-close graphics, and there
/// is now a built-in implicit warning that you are too close. 
/// Press '1' to fade to black
/// Press '2' to fade to white
/// Press '3' to fade to skyblue
/// To see the method in effect, go right up to one of the walls, or the various items
/// of furniture, and then loom in positionally for the last distance.


#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct HandlingNearObjects : BasicVR
{
	HandlingNearObjects(HINSTANCE hinst) : BasicVR(hinst, L"HandlingNearObjects") {}

    void MainLoop()
    {

		// We are going to make a custom pixel shader.
		// Make a buffer for pixel shader constants
		DataBuffer             * PSUniformBufferGen;
		PSUniformBufferGen = new DataBuffer(DIRECTX.Device, D3D11_BIND_CONSTANT_BUFFER, NULL, DIRECTX.UNIFORM_DATA_SIZE);
		DIRECTX.Context->PSSetConstantBuffers(0, 1, &PSUniformBufferGen->D3DBuffer);

		// Create pixel shader
		// Note that this is illustrative, rather than optimised for performance.
		char* pixelShaderText =
			"Texture2D Texture   : register(t0); SamplerState Linear : register(s0); "
			"float4 SolidNearColor;  "
			"float4 main(in float4 Position : SV_Position, in float4 Color: COLOR0, in float2 TexCoord : TEXCOORD0) : SV_Target"
			"{   float4 TexCol = Texture.Sample(Linear, TexCoord); "
			"    float4 Col = Color * TexCol;"
			//Get a proportion of 0 to 1, for Position.z from near to far
			"    float nearPosZ = 0.93f; "
			"    float farPosZ = 0.95f; "
			"    float prop01 = (farPosZ-Position.z)/(farPosZ-nearPosZ); "  
			"    prop01 = clamp(prop01,0.0f,1.0f);  "  
			//Fade colour to that solid colour, according to proportion
			"    Col = lerp(Col,SolidNearColor, prop01);"
			"    return(Col); }";
		ID3D11PixelShader       * newPixelShader; 
		ID3DBlob * blobData;
		D3DCompile(pixelShaderText, strlen(pixelShaderText), 0, 0, 0, "main", "ps_4_0", 0, 0, &blobData, 0);
		DIRECTX.Device->CreatePixelShader(blobData->GetBufferPointer(), blobData->GetBufferSize(), NULL, &newPixelShader);
		blobData->Release();



	    // We already have a RoomScene ready to go,
		// but we're doing to modify the models within it, 
		// to use our new pixel shader.
		for (int i = 0; i < RoomScene->numModels; i++)
		{
			RoomScene->Models[i]->Fill->PixelShader = newPixelShader;
		}


		Layer[0] = new VRLayer(Session);
	    while (HandleMessages())
	    {
		    ActionFromInput();
		    Layer[0]->GetEyePoses();

			//The shader will vary the near colour to whatever solid colour
			//we specify, so lets fill the shader constant memory with our choice of color
			//(default is black)
			static float black[] = { 0, 0, 0, 1 };
			static float white[] = { 1, 1, 1, 1 };
			static float skyblue[] = { 0, 0.5f, 1, 1 };
			static float * colChoice = black;
			if (DIRECTX.Key['1']) colChoice = black;
			if (DIRECTX.Key['2']) colChoice = white;
			if (DIRECTX.Key['3']) colChoice = skyblue;
			memcpy(DIRECTX.UniformData + 0, colChoice, 16); 
			D3D11_MAPPED_SUBRESOURCE map;
			DIRECTX.Context->Map(PSUniformBufferGen->D3DBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
			memcpy(map.pData, &DIRECTX.UniformData, DIRECTX.UNIFORM_DATA_SIZE);
			DIRECTX.Context->Unmap(PSUniformBufferGen->D3DBuffer, 0);


		    for (int eye = 0; eye < 2; ++eye)
		    {
				// We set the near clip plane to very close,
				// to allow our pixel shader to render, and operate on, these near graphics.
				// We are also changing the background colour, to clearly differentiate our 
				// shader's work, and when it is simply being clipped out, even for the case
				// when black.
			    Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye,0,0,1,1,1,1,1,0.01f/*very near clip plane*/,
					                             1000,true,0,0.5f/*a bit of red in the background*/);
		    }

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }

	//Free extra resources
	delete PSUniformBufferGen;
	newPixelShader->Release();

    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	HandlingNearObjects app(hinst);
    return app.Run();
}
