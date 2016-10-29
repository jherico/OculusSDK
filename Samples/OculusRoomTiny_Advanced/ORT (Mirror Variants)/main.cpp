/************************************************************************************
Filename    :   Win32_RoomTiny_Main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   25th June 2015
Authors     :   Tom Heath
Copyright   :   Copyright 2015 Oculus, Inc. All Rights reserved.

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
/// This sample shows variations on the theme of the mirror window.
/// Press '0' for normal
/// Press '1' for one distorted screen
/// Press '2' for full screen version of 1
/// Press '3' for one distorted screen, cut down to appear as if undistorted.
/// Press '4' for a scaled undistorted single buffer
/// Press '5' for a scaled undistorted single buffer, but stretched to full screen sized

#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct MirrorVariants : BasicVR
{
    int mirrorMode;

    MirrorVariants(HINSTANCE hinst) :
        BasicVR(hinst, L"Mirror Variants")
    {
        SetMirrorMode(0);
    }

    void SetMirrorMode(int mode)
    {
		scaleWindowW = 0.50f;
		scaleWindowH = 0.50f;
		scaleMirrorW = 1.0f;
		scaleMirrorH = 1.0f;
        windowed = true;

        switch (mirrorMode = mode)
        {
        case(0) :
            break;
        case(1) :
            scaleMirrorW = 2.0f;
			scaleWindowW = 0.375f;
			scaleWindowH = 0.75f;
			break;
        case(2) :
            windowed = false;
            scaleMirrorW = 2.0f;
            break;
        case(3) :
			scaleWindowW = 0.375f;
			scaleWindowH = 0.75f;
			scaleMirrorW = (2.0f * 5.0f) / 4.0f;
            scaleMirrorH = (1.0f * 5.0f) / 4.0f;
            break;
        case(4) :
			scaleWindowW = 0.375f;
			scaleWindowH = 0.75f;
			break;
        case(5) :
            scaleWindowW = 1.0f;
            scaleWindowH = 1.0f;
            windowed = false;
            break;
        }
    }
	
    void MainLoop()
    {
        Layer[0] = new VRLayer(Session);

        // Special treatment for mode 4 and 5
        Model *renderEyeTexture = nullptr;
        if ((mirrorMode == 4) || (mirrorMode == 5))
        {
            // For mirror window, make a texture with the TexSv from the left eye buffer
            // Note that we are going to just use the 0th of the texture set, 
            // so the mirror window will be a lower framerate than in the HUD which
            // cycles through all textures in the set.
			// We do it this way for simplicity and brevity.
            auto mirrorEyeBufferTexture = new Texture();
            ID3D11Texture2D* texture = nullptr;
            ovr_GetTextureSwapChainBufferDX(Session, Layer[0]->pEyeRenderTexture[0]->TextureChain, 0, IID_PPV_ARGS(&texture));
            D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
            srvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvd.Texture2D.MipLevels = 1;
            srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            DIRECTX.Device->CreateShaderResourceView(texture, &srvd, &mirrorEyeBufferTexture->TexSv);
            texture->Release();

            //Make a model that will render this full screen
            renderEyeTexture = new Model(new Material(mirrorEyeBufferTexture), -1, -1, 1, 1);
        }

        while (HandleMessages())
        {
            ActionFromInput();
            Layer[0]->GetEyePoses();

            for (int eye = 0; eye < 2; ++eye)
            {
                Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
            }

            Layer[0]->PrepareLayerHeader();

            // Render the selected mirror mode
            switch (mirrorMode)
            {
            case(0) :
            case(1) :
            case(2) :
                DistortAndPresent(1);
                break;
            case(3) :
                D3D11_BOX box;
                box.left = DIRECTX.WinSizeW / 8;
                box.right = (DIRECTX.WinSizeW * 9) / 8;
                box.top = DIRECTX.WinSizeH / 8;
                box.bottom = (DIRECTX.WinSizeH * 9) / 8;
                box.front = 0;
                box.back = 1;
                DistortAndPresent(1, &box);
                break;
            case(4) :
            case(5) :
                DistortAndPresent(1, 0, false);

                // Now we render the eye texture into the full window
                DIRECTX.SetAndClearRenderTarget(DIRECTX.BackBufferRT, DIRECTX.MainDepthBuffer);
                DIRECTX.SetViewport(0, 0, float(DIRECTX.WinSizeW), float(DIRECTX.WinSizeH));
                renderEyeTexture->Render(XMMatrixIdentity(), 1, 1, 1, 1, true);
                DIRECTX.SwapChain->Present(0, 0);
                break;
            }

            // See if we want to try another mode [0..5]
            for (int mode = 0; mode < 6; ++mode)
            {
                if (DIRECTX.Key['0' + mode])
                {
                    SetMirrorMode(mode);
                    Restart();
                    break;
                }
            }
        }

        delete renderEyeTexture;
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    MirrorVariants app(hinst);
    return app.Run();
}
