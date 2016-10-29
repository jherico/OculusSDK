/************************************************************************************
Filename    :   main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   11th May 2015
Authors     :   Tom Heath, Simon Green
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

/*
    Instanced stereo sample

    This sample demonstrates how to use instancing to speed up stereo rendering, as described here:
    https://docs.google.com/presentation/d/19x9XDjUvkW_9gsfsMQzt3hZbRNziVsoCEHOn4AercAc/edit#slide=id.g5791d9ed1_015

    Rather than doing one draw call for each eye, a single draw call is made with 2 instances to draw both eyes.
    This can significantly reduce the CPU overhead for applications with a lot of draw calls.

    The Oculus SDK is set up to use a single texture containing both eye images side by side.
    The vertex shader is modified to offset the rendering to the left or right part of the viewport based on the instance ID.
    Clipping planes are used to prevent geometry from spilling from one side to the other.

    Press 'I' to enable instanced stereo (the results should look identical)
*/

// Include DirectX
#include "../../OculusRoomTiny_Advanced/Common/Win32_DirectXAppUtil.h"

// Include the Oculus SDK
#include "OVR_CAPI_D3D.h"


//------------------------------------------------------------
// ovrSwapTextureSet wrapper class that also maintains the render target views
// needed for D3D11 rendering.
struct OculusTexture
{
    ovrSession               Session;
    ovrTextureSwapChain      TextureChain;
    static const int         TextureCount = 3;
    ID3D11RenderTargetView * TexRtv[TextureCount];

    OculusTexture() :
        Session(nullptr),
        TextureChain(nullptr)
    {
        TexRtv[0] = TexRtv[1] = nullptr;
    }

    bool Init(ovrSession session, int sizeW, int sizeH)
    {
        Session = session;

        ovrTextureSwapChainDesc desc = {};
        desc.Type = ovrTexture_2D;
        desc.ArraySize = 1;
        desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.Width = sizeW;
        desc.Height = sizeH;
        desc.MipLevels = 1;
        desc.SampleCount = 1;
        desc.MiscFlags = ovrTextureMisc_DX_Typeless;
        desc.StaticImage = ovrFalse;
        desc.BindFlags = ovrTextureBind_DX_RenderTarget;

        ovrResult result = ovr_CreateTextureSwapChainDX(session, DIRECTX.Device, &desc, &TextureChain);
        if (!OVR_SUCCESS(result))
            return false;

        int textureCount = 0;
        ovr_GetTextureSwapChainLength(Session, TextureChain, &textureCount);
        VALIDATE(textureCount == TextureCount, "TextureCount mismatch.");

        for (int i = 0; i < TextureCount; ++i)
        {
            ID3D11Texture2D* tex = nullptr;
            ovr_GetTextureSwapChainBufferDX(Session, TextureChain, i, IID_PPV_ARGS(&tex));
            D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
            rtvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            DIRECTX.Device->CreateRenderTargetView(tex, &rtvd, &TexRtv[i]);
            tex->Release();
        }

        return true;
    }

    ~OculusTexture()
    {
        for (int i = 0; i < TextureCount; ++i)
        {
            Release(TexRtv[i]);
        }
        if (TextureChain)
        {
            ovr_DestroyTextureSwapChain(Session, TextureChain);
        }
    }

    ID3D11RenderTargetView* GetRTV()
    {
        int index = 0;
        ovr_GetTextureSwapChainCurrentIndex(Session, TextureChain, &index);
        return TexRtv[index];
    }

    // Commit changes
    void Commit()
    {
        ovr_CommitTextureSwapChain(Session, TextureChain);
    }
};

// return true to retry later (e.g. after display lost)
static bool MainLoop(bool retryCreate)
{
    // Initialize these to nullptr here to handle device lost failures cleanly
    ovrMirrorTexture mirrorTexture = nullptr;
    OculusTexture  * pEyeRenderTexture = nullptr;
    DepthBuffer    * pEyeDepthBuffer = nullptr;
    Scene          * roomScene = nullptr; 
    Camera         * mainCam = nullptr;
    ovrMirrorTextureDesc desc = {};

    bool isVisible          = true;
    long long frameIndex    = 0;
    bool useInstancing      = false;
    const int repeatDrawing = 1;

    ovrSession session;
    ovrGraphicsLuid luid;
    ovrResult result = ovr_Create(&session, &luid);
    if (!OVR_SUCCESS(result))
        return retryCreate;

    ovrHmdDesc hmdDesc = ovr_GetHmdDesc(session);

    // Setup Device and Graphics
    // Note: the mirror window can be any size, for this sample we use 1/2 the HMD resolution
    if (!DIRECTX.InitDevice(hmdDesc.Resolution.w / 2, hmdDesc.Resolution.h / 2, reinterpret_cast<LUID*>(&luid)))
        goto Done;

    ovrRecti eyeRenderViewport[2];

    // Make a single eye texture
    {
        ovrSizei eyeTexSizeL = ovr_GetFovTextureSize(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0], 1.0f);
        ovrSizei eyeTexSizeR = ovr_GetFovTextureSize(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1], 1.0f);
        ovrSizei textureSize;
        textureSize.w = eyeTexSizeL.w + eyeTexSizeR.w;
        textureSize.h = max(eyeTexSizeL.h, eyeTexSizeR.h);

        pEyeRenderTexture = new OculusTexture();
        if (!pEyeRenderTexture->Init(session, textureSize.w, textureSize.h))
        {
            if (retryCreate) goto Done;
            VALIDATE(OVR_SUCCESS(result), "Failed to create eye texture.");
        }

        pEyeDepthBuffer = new DepthBuffer(DIRECTX.Device, textureSize.w, textureSize.h);

        // set viewports
        eyeRenderViewport[0].Pos.x = 0;
        eyeRenderViewport[0].Pos.y = 0;
        eyeRenderViewport[0].Size = eyeTexSizeL;

        eyeRenderViewport[1].Pos.x = eyeTexSizeL.w;
        eyeRenderViewport[1].Pos.y = 0;
        eyeRenderViewport[1].Size = eyeTexSizeR;
    }

    if (!pEyeRenderTexture->TextureChain)
    {
        if (retryCreate) goto Done;
        FATALERROR("Failed to create texture.");
    }

    // Create a mirror to see on the monitor.
    desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.Width = DIRECTX.WinSizeW;
    desc.Height = DIRECTX.WinSizeH;
    result = ovr_CreateMirrorTextureDX(session, DIRECTX.Device, &desc, &mirrorTexture);
    if (!OVR_SUCCESS(result))
    {
        if (retryCreate) goto Done;
        FATALERROR("Failed to create mirror texture.");
    }

    // Create the room model
    roomScene = new Scene(false);

    // Create camera
    mainCam = new Camera(XMVectorSet(0.0f, 1.6f, 5.0f, 0), XMQuaternionIdentity());

    // Setup VR components, filling out description
    ovrEyeRenderDesc eyeRenderDesc[2];
    eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
    eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);

    // Main loop
    while (DIRECTX.HandleMessages())
    {
        XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, -0.05f, 0), mainCam->Rot);
        XMVECTOR right   = XMVector3Rotate(XMVectorSet(0.05f, 0, 0, 0),  mainCam->Rot);
        XMVECTOR up      = XMVector3Rotate(XMVectorSet(0, 0.05f, 0, 0), mainCam->Rot);
        if (DIRECTX.Key['W'] || DIRECTX.Key[VK_UP])	  mainCam->Pos = XMVectorAdd(mainCam->Pos, forward);
        if (DIRECTX.Key['S'] || DIRECTX.Key[VK_DOWN]) mainCam->Pos = XMVectorSubtract(mainCam->Pos, forward);
        if (DIRECTX.Key['D'])                         mainCam->Pos = XMVectorAdd(mainCam->Pos, right);
        if (DIRECTX.Key['A'])                         mainCam->Pos = XMVectorSubtract(mainCam->Pos, right);
        if (DIRECTX.Key['Q'])                         mainCam->Pos = XMVectorAdd(mainCam->Pos, up);
        if (DIRECTX.Key['E'])                         mainCam->Pos = XMVectorSubtract(mainCam->Pos, up);

        static float Yaw = 0;
        if (DIRECTX.Key[VK_LEFT])  mainCam->Rot = XMQuaternionRotationRollPitchYaw(0, Yaw += 0.02f, 0);
        if (DIRECTX.Key[VK_RIGHT]) mainCam->Rot = XMQuaternionRotationRollPitchYaw(0, Yaw -= 0.02f, 0);

        if (DIRECTX.Key['P'])
            ovr_SetInt(session, OVR_PERF_HUD_MODE, int(ovrPerfHud_AppRenderTiming));
        else
            ovr_SetInt(session, OVR_PERF_HUD_MODE, int(ovrPerfHud_Off));

        useInstancing = DIRECTX.Key['I'];

        // Animate the cube
        static float cubeClock = 0;
        roomScene->Models[0]->Pos = XMFLOAT3(9 * sin(cubeClock), 3, 9 * cos(cubeClock += 0.015f));

        // Get both eye poses simultaneously, with IPD offset already included. 
        ovrPosef         EyeRenderPose[2];
        ovrVector3f      HmdToEyeOffset[2] = { eyeRenderDesc[0].HmdToEyeOffset,
                                               eyeRenderDesc[1].HmdToEyeOffset };

        double sensorSampleTime;    // sensorSampleTime is fed into the layer later
        ovr_GetEyePoses(session, frameIndex, ovrTrue, HmdToEyeOffset, EyeRenderPose, &sensorSampleTime);

        // Render scene to eye texture
        if (isVisible)
        {
            DIRECTX.SetAndClearRenderTarget(pEyeRenderTexture->GetRTV(), pEyeDepthBuffer);

            // calculate eye transforms
            XMMATRIX viewProjMatrix[2];
            for (int eye = 0; eye < 2; ++eye)
            {
                //Get the pose information in XM format
                XMVECTOR eyeQuat = XMLoadFloat4((XMFLOAT4 *)&EyeRenderPose[eye].Orientation.x);
                XMVECTOR eyePos = XMVectorSet(EyeRenderPose[eye].Position.x, EyeRenderPose[eye].Position.y, EyeRenderPose[eye].Position.z, 0);

                // Get view and projection matrices for the Rift camera
                XMVECTOR CombinedPos = XMVectorAdd(mainCam->Pos, XMVector3Rotate(eyePos, mainCam->Rot));
                Camera finalCam(CombinedPos, XMQuaternionMultiply(eyeQuat, mainCam->Rot));
                XMMATRIX view = finalCam.GetViewMatrix();
                ovrMatrix4f p = ovrMatrix4f_Projection(eyeRenderDesc[eye].Fov, 0.1f, 100.0f, ovrProjection_None);
                XMMATRIX proj = XMMatrixSet(p.M[0][0], p.M[1][0], p.M[2][0], p.M[3][0],
                    p.M[0][1], p.M[1][1], p.M[2][1], p.M[3][1],
                    p.M[0][2], p.M[1][2], p.M[2][2], p.M[3][2],
                    p.M[0][3], p.M[1][3], p.M[2][3], p.M[3][3]);

                if (useInstancing)
                {
                    // scale and offset projection matrix to shift image to correct part of texture for each eye
                    XMMATRIX scale = XMMatrixScaling(0.5f, 1.0f, 1.0f);
                    XMMATRIX translate = XMMatrixTranslation((eye==0) ? -0.5f : 0.5f, 0.0f, 0.0f);
                    proj = XMMatrixMultiply(proj, scale);
                    proj = XMMatrixMultiply(proj, translate);
                }

                viewProjMatrix[eye] = XMMatrixMultiply(view, proj);
            }

            if (useInstancing)
            {
                // use instancing for stereo
                DIRECTX.SetViewport(0.0f, 0.0f, (float)eyeRenderViewport[0].Size.w + eyeRenderViewport[1].Size.w, (float)eyeRenderViewport[0].Size.h);

                // render scene
                for (int i = 0; i < repeatDrawing; i++)
                    roomScene->RenderInstanced(&viewProjMatrix[0], 1, 1, 1, 1, true);
            }
            else
            {
                // non-instanced path
                for (int eye = 0; eye < 2; ++eye)
                {
                    // set viewport
                    DIRECTX.SetViewport((float)eyeRenderViewport[eye].Pos.x, (float)eyeRenderViewport[eye].Pos.y,
                        (float)eyeRenderViewport[eye].Size.w, (float)eyeRenderViewport[eye].Size.h);

                    // render scene
                    for (int i = 0; i < repeatDrawing; i++)
                        roomScene->Render(&viewProjMatrix[eye], 1, 1, 1, 1, true);
                }
            }

            // Commit rendering to the swap chain
            pEyeRenderTexture->Commit();
        }

        // Initialize our single full screen Fov layer.
        ovrLayerEyeFov ld = {};
        ld.Header.Type = ovrLayerType_EyeFov;
        ld.Header.Flags = 0;
        ld.SensorSampleTime = sensorSampleTime;

        for (int eye = 0; eye < 2; ++eye)
        {
            ld.ColorTexture[eye] = pEyeRenderTexture->TextureChain;
            ld.Viewport[eye] = eyeRenderViewport[eye];
            ld.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
            ld.RenderPose[eye] = EyeRenderPose[eye];
        }

        ovrLayerHeader* layers = &ld.Header;
        result = ovr_SubmitFrame(session, frameIndex, nullptr, &layers, 1);
        // exit the rendering loop if submit returns an error, will retry on ovrError_DisplayLost
        if (!OVR_SUCCESS(result))
            goto Done;

        isVisible = (result == ovrSuccess);

        // Render mirror
        ID3D11Texture2D* tex = nullptr;
        ovr_GetMirrorTextureBufferDX(session, mirrorTexture, IID_PPV_ARGS(&tex));
        DIRECTX.Context->CopyResource(DIRECTX.BackBuffer, tex);
        tex->Release();
        DIRECTX.SwapChain->Present(0, 0);

        frameIndex++;
    }

    // Release resources
Done:
    delete mainCam;
    delete roomScene;
    if (mirrorTexture) ovr_DestroyMirrorTexture(session, mirrorTexture);
    delete pEyeRenderTexture;
    delete pEyeDepthBuffer;

    DIRECTX.ReleaseDevice();
    ovr_Destroy(session);

    // Retry on ovrError_DisplayLost
    return retryCreate || OVR_SUCCESS(result) || (result == ovrError_DisplayLost);
}

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    // Initializes LibOVR, and the Rift
    ovrResult result = ovr_Initialize(nullptr);
    VALIDATE(OVR_SUCCESS(result), "Failed to initialize libOVR.");

    VALIDATE(DIRECTX.InitWindow(hinst, L"Oculus Room Tiny (DX11)"), "Failed to open window.");

    DIRECTX.Run(MainLoop);

    ovr_Shutdown();
    return(0);
}
