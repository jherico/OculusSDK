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
/// This is an initial sample to show a code example of utilising a 'static'
/// background to mitigate, or eliminate motion sickness.   Its not the best example
/// in terms of effectiveness, but shows the method of a general 'static' background,
/// even being able to mix itself at all depths with the moving foreground.
/// Static backgrounds (or foregrounds) work in general by convincing the brain that 
/// the user is not actually moving, and the static component provides the basis for that.
/// This leaves the moving part (if done well) seeming like it is moving around you, and
/// you yourself are reassuringly stationary.  As I say, much better examples to come!

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR

struct StaticBackground : BasicVR
{
    StaticBackground(HINSTANCE hinst) : BasicVR(hinst, L"Static Background") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

        // We create an extra eye buffer, a means to render it, and a static camera
        auto width = max(Layer[0]->pEyeRenderTexture[0]->SizeW, Layer[0]->pEyeRenderTexture[1]->SizeW);
        auto height = max(Layer[0]->pEyeRenderTexture[0]->SizeH, Layer[0]->pEyeRenderTexture[1]->SizeH);
        auto staticEyeTexture = new Texture(true, width, height);
        auto renderEyeTexture = new Model(new Material(staticEyeTexture), -1, -1, 1, 1);  
        Camera StaticMainCam = *MainCam;

	    while (HandleMessages())
	    {
		    ActionFromInput();
		    Layer[0]->GetEyePoses();

		    for (int eye = 0; eye < 2; ++eye)
		    {
                // Render the scene from an unmoving, static player - to the new buffer
                Layer[0]->RenderSceneToEyeBuffer(&StaticMainCam, RoomScene, eye, staticEyeTexture->TexRtv, 0, 1, 1, 1, 1, 1);

                // Render the scene as normal
                Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye, 0, 0, 1, 1, 1, 1, 1); 

                // Render static one over the top - different levels of transparency on buttons '1' and '2'. 
                float proportionOfStatic = 0.5f;
                if (DIRECTX.Key['1']) proportionOfStatic = 0;
                if (DIRECTX.Key['2']) proportionOfStatic = 1;
                renderEyeTexture->Render(&XMMatrixIdentity(), 1, 1, 1, proportionOfStatic, true); 
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
	StaticBackground app(hinst);
    return app.Run();
}
