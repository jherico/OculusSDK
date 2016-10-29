/************************************************************************************
Filename    :   main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   10/28/2015

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
/// This is an entry-level sample, showing a minimal VR sample, 
/// in a simple environment.  Use WASD keys to move around, and cursor keys.
/// It runs with DirectX12.

// Include Windows.h
#include <sdkddkver.h>

#if !defined(_WIN32_WINNT_WIN10) || (_WIN32_WINNT < _WIN32_WINNT_WIN10)
    #error Need to be building with the Windows 10 SDK with Windows 10 functionality enabled (_WIN32_WINNT >= _WIN32_WINNT_WIN10).\
 The Oculus Room Tiny (DX12) sample projects for each of VS2010 through VS2015 requires that an appropriate Windows 10 SDK be\
 set for the build. By default these projects are set to use Windows SDK v10.0.10240.0 and they should compile\
 as-is is you have that Windows 10 SDK version installed. If you have a different Windows 10 SDK then you will likely get compile\
 errors about missing dx12.h or similar. To fix this for VS2010 - VS2013, you need to edit the\
 Samples\OculusRoomTiny\OculusRoomTiny (DX12)\Projects\Windows\Windows10SDKPaths.props file with a text editor and change the\
 numbers to refer to your Windows 10 SDK, typically installed with headers at C:\Program Files (x86)\Windows Kits\10\Include.\
 To fix this for VS2015 you need to edit the Project Properties -> General -> Target PlatformVersion drop down box.
#endif

// Include DirectX
#include "../../OculusRoomTiny_Advanced/Common/Win32_DirectX12AppUtil.h"

// Include the Oculus SDK
#include "OVR_CAPI_D3D.h"

//------------------------------------------------------------
// ovrSwapTextureSet wrapper class that also maintains the render target views
// needed for D3D12 rendering.
struct OculusEyeTexture
{
    ovrSession               Session;
    ovrTextureSwapChain      TextureChain;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> TexRtv;
    std::vector<ID3D12Resource*> TexResource;

    std::vector<DepthBuffer*>                DepthTex;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> DepthTexDsv;

    OculusEyeTexture() :
        Session(nullptr),
        TextureChain(nullptr)
    {
    }

    bool Init(ovrSession session, int sizeW, int sizeH, bool createDepth)
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

        ovrResult result = ovr_CreateTextureSwapChainDX(session, DIRECTX.CommandQueue, &desc, &TextureChain);
        if (!OVR_SUCCESS(result))
            return false;

        int textureCount = 0;
        ovr_GetTextureSwapChainLength(Session, TextureChain, &textureCount);
        TexRtv.resize(textureCount);
        TexResource.resize(textureCount);
        for (int i = 0; i < textureCount; ++i)
        {
            result = ovr_GetTextureSwapChainBufferDX(Session, TextureChain, i, IID_PPV_ARGS(&TexResource[i]));
            if (!OVR_SUCCESS(result))
                return false;

            D3D12_RENDER_TARGET_VIEW_DESC rtvd = {};
            rtvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            rtvd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            TexRtv[i] = DIRECTX.RtvHandleProvider.AllocCpuHandle();
            DIRECTX.Device->CreateRenderTargetView(TexResource[i], &rtvd, TexRtv[i]);
        }

        if (createDepth)
        {
            DepthTex.resize(textureCount);
            DepthTexDsv.resize(textureCount);
            for (int i = 0; i < textureCount; i++)
            {
                DepthTexDsv[i] = DIRECTX.DsvHandleProvider.AllocCpuHandle();
                DepthTex[i] = new DepthBuffer(DIRECTX.Device, DepthTexDsv[i], sizeW, sizeH);
            }
        }

        return true;
    }

    ~OculusEyeTexture()
    {
        if (TextureChain)
        {
            for (size_t i = 0; i < TexResource.size(); i++)
            {
                Release(TexResource[i]);
            }

            ovr_DestroyTextureSwapChain(Session, TextureChain);
        }

        for (size_t i = 0; i < DepthTex.size(); i++)
        {
            delete DepthTex[i];
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetRtv()
    {
        int index = 0;
        ovr_GetTextureSwapChainCurrentIndex(Session, TextureChain, &index);
        return TexRtv[index];
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDsv()
    {
        int index = 0;
        ovr_GetTextureSwapChainCurrentIndex(Session, TextureChain, &index);
        return DepthTexDsv[index];
    }

    ID3D12Resource* GetD3DResource()
    {
        int index = 0;
        ovr_GetTextureSwapChainCurrentIndex(Session, TextureChain, &index);
        return TexResource[index];
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
    ovrMirrorTexture            mirrorTexture = nullptr;
    OculusEyeTexture*           pEyeRenderTexture[2] = { nullptr, nullptr };
    Scene*                      roomScene = nullptr; 
    Camera*                     mainCam = nullptr;
    ovrMirrorTextureDesc        mirrorDesc = {};

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

    // Make the eye render buffers (caution if actual size < requested due to HW limits). 
    ovrRecti eyeRenderViewport[2];

    for (int eye = 0; eye < 2; ++eye)
    {
        ovrSizei idealSize = ovr_GetFovTextureSize(session, (ovrEyeType)eye, hmdDesc.DefaultEyeFov[eye], 1.0f);
        pEyeRenderTexture[eye] = new OculusEyeTexture();
        if (!pEyeRenderTexture[eye]->Init(session, idealSize.w, idealSize.h, true))
        {
            if (retryCreate) goto Done;
            FATALERROR("Failed to create eye texture.");
        }

        eyeRenderViewport[eye].Pos.x = 0;
        eyeRenderViewport[eye].Pos.y = 0;
        eyeRenderViewport[eye].Size = idealSize;
        if (!pEyeRenderTexture[eye]->TextureChain)
        {
            if (retryCreate) goto Done;
            FATALERROR("Failed to create texture.");
        }
    }

    // Create a mirror to see on the monitor.
    mirrorDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
    mirrorDesc.Width = DIRECTX.WinSizeW;
    mirrorDesc.Height = DIRECTX.WinSizeH;
    result = ovr_CreateMirrorTextureDX(session, DIRECTX.CommandQueue, &mirrorDesc, &mirrorTexture);
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

    long long frameIndex = 0;

    bool drawMirror = true;

    DIRECTX.InitFrame(drawMirror);

    // Main loop
    while (DIRECTX.HandleMessages())
    {
        ovrSessionStatus sessionStatus;
        ovr_GetSessionStatus(session, &sessionStatus);
        if (sessionStatus.ShouldQuit)
        {
            // Because the application is requested to quit, should not request retry
            retryCreate = false;
            break;
        }
        if (sessionStatus.ShouldRecenter)
            ovr_RecenterTrackingOrigin(session);

        if (sessionStatus.IsVisible)
        {
            XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, -0.05f, 0), mainCam->GetRotVec());
            XMVECTOR right   = XMVector3Rotate(XMVectorSet(0.05f, 0, 0, 0),  mainCam->GetRotVec());
            XMVECTOR mainCamPos = mainCam->GetPosVec();
            XMVECTOR mainCamRot = mainCam->GetRotVec();
            if (DIRECTX.Key['W'] || DIRECTX.Key[VK_UP])      mainCamPos = XMVectorAdd(     mainCamPos, forward);
            if (DIRECTX.Key['S'] || DIRECTX.Key[VK_DOWN])    mainCamPos = XMVectorSubtract(mainCamPos, forward);
            if (DIRECTX.Key['D'])                            mainCamPos = XMVectorAdd(     mainCamPos, right);
            if (DIRECTX.Key['A'])                            mainCamPos = XMVectorSubtract(mainCamPos, right);
            static float Yaw = 0;
            if (DIRECTX.Key[VK_LEFT])  mainCamRot = XMQuaternionRotationRollPitchYaw(0, Yaw += 0.02f, 0);
            if (DIRECTX.Key[VK_RIGHT]) mainCamRot = XMQuaternionRotationRollPitchYaw(0, Yaw -= 0.02f, 0);

            mainCam->SetPosVec(mainCamPos);
            mainCam->SetRotVec(mainCamRot);

            // Animate the cube
            static float cubeClock = 0;
            roomScene->Models[0]->Pos = XMFLOAT3(9 * sin(cubeClock), 3, 9 * cos(cubeClock += 0.015f));

            // Get both eye poses simultaneously, with IPD offset already included. 
            ovrPosef    EyeRenderPose[2];
            ovrVector3f HmdToEyeOffset[2] = { eyeRenderDesc[0].HmdToEyeOffset,
                                              eyeRenderDesc[1].HmdToEyeOffset };

            double sensorSampleTime;    // sensorSampleTime is fed into the layer later
            ovr_GetEyePoses(session, frameIndex, ovrTrue, HmdToEyeOffset, EyeRenderPose, &sensorSampleTime);

            // Render Scene to Eye Buffers
            for (int eye = 0; eye < 2; ++eye)
            {
                DIRECTX.SetActiveContext(eye == 0 ? DrawContext_EyeRenderLeft : DrawContext_EyeRenderRight);

                DIRECTX.SetActiveEye(eye);

                CD3DX12_RESOURCE_BARRIER resBar = CD3DX12_RESOURCE_BARRIER::Transition(pEyeRenderTexture[eye]->GetD3DResource(),
                                                                                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                                                       D3D12_RESOURCE_STATE_RENDER_TARGET);
                DIRECTX.CurrentFrameResources().CommandLists[DIRECTX.ActiveContext]->ResourceBarrier(1, &resBar);

                DIRECTX.SetAndClearRenderTarget(pEyeRenderTexture[eye]->GetRtv(), pEyeRenderTexture[eye]->GetDsv());
                DIRECTX.SetViewport((float)eyeRenderViewport[eye].Pos.x, (float)eyeRenderViewport[eye].Pos.y,
                                    (float)eyeRenderViewport[eye].Size.w, (float)eyeRenderViewport[eye].Size.h);
                                
                //Get the pose information in XM format
                XMVECTOR eyeQuat = XMVectorSet(EyeRenderPose[eye].Orientation.x, EyeRenderPose[eye].Orientation.y,
                                               EyeRenderPose[eye].Orientation.z, EyeRenderPose[eye].Orientation.w);
                XMVECTOR eyePos = XMVectorSet(EyeRenderPose[eye].Position.x, EyeRenderPose[eye].Position.y, EyeRenderPose[eye].Position.z, 0);

                // Get view and projection matrices for the Rift camera
                Camera finalCam(XMVectorAdd(mainCamPos, XMVector3Rotate(eyePos, mainCamRot)), XMQuaternionMultiply(eyeQuat, mainCamRot));
                XMMATRIX view = finalCam.GetViewMatrix();
                ovrMatrix4f p = ovrMatrix4f_Projection(eyeRenderDesc[eye].Fov, 0.2f, 1000.0f, ovrProjection_None);
                XMMATRIX proj = XMMatrixSet(p.M[0][0], p.M[1][0], p.M[2][0], p.M[3][0],
                                            p.M[0][1], p.M[1][1], p.M[2][1], p.M[3][1],
                                            p.M[0][2], p.M[1][2], p.M[2][2], p.M[3][2],
                                            p.M[0][3], p.M[1][3], p.M[2][3], p.M[3][3]);
                XMMATRIX prod = XMMatrixMultiply(view, proj);

                roomScene->Render(&prod, 1, 1, 1, 1, true);

                resBar = CD3DX12_RESOURCE_BARRIER::Transition(pEyeRenderTexture[eye]->GetD3DResource(),
                                                              D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                              D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                DIRECTX.CurrentFrameResources().CommandLists[DIRECTX.ActiveContext]->ResourceBarrier(1, &resBar);

                // Commit rendering to the swap chain
                pEyeRenderTexture[eye]->Commit();

                // kick off eye render command lists before ovr_SubmitFrame()
                DIRECTX.SubmitCommandList(DIRECTX.ActiveContext);
            }

            // Initialize our single full screen Fov layer.
            ovrLayerEyeFov ld = {};
            ld.Header.Type = ovrLayerType_EyeFov;
            ld.Header.Flags = 0;

            for (int eye = 0; eye < 2; ++eye)
            {
                ld.ColorTexture[eye] = pEyeRenderTexture[eye]->TextureChain;
                ld.Viewport[eye] = eyeRenderViewport[eye];
                ld.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
                ld.RenderPose[eye] = EyeRenderPose[eye];
                ld.SensorSampleTime = sensorSampleTime;
            }

            ovrLayerHeader* layers = &ld.Header;
            result = ovr_SubmitFrame(session, frameIndex, nullptr, &layers, 1);
            // exit the rendering loop if submit returns an error, will retry on ovrError_DisplayLost
            if (!OVR_SUCCESS(result))
                goto Done;
            
            frameIndex++;
        }
        
        if (drawMirror)
        {
            DIRECTX.SetActiveContext(DrawContext_Final);

            DIRECTX.SetViewport(0.0f, 0.0f, (float)hmdDesc.Resolution.w / 2, (float)hmdDesc.Resolution.h / 2);

            // Render mirror
            ID3D12Resource* mirrorTexRes = nullptr;
            ovr_GetMirrorTextureBufferDX(session, mirrorTexture, IID_PPV_ARGS(&mirrorTexRes));

            //DIRECTX.SetAndClearRenderTarget(DIRECTX.CurrentFrameResources().SwapChainRtvHandle, nullptr, 1.0f, 0.5f, 0.0f, 1.0f);

            CD3DX12_RESOURCE_BARRIER preMirrorBlitBar[] =
            {
                CD3DX12_RESOURCE_BARRIER::Transition(DIRECTX.CurrentFrameResources().SwapChainBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST),
                CD3DX12_RESOURCE_BARRIER::Transition(mirrorTexRes, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE)
            };

            // Indicate that the back buffer will now be copied into
            DIRECTX.CurrentFrameResources().CommandLists[DIRECTX.ActiveContext]->ResourceBarrier(ARRAYSIZE(preMirrorBlitBar), preMirrorBlitBar);

            DIRECTX.CurrentFrameResources().CommandLists[DIRECTX.ActiveContext]->CopyResource(DIRECTX.CurrentFrameResources().SwapChainBuffer, mirrorTexRes);

            CD3DX12_RESOURCE_BARRIER resBar = CD3DX12_RESOURCE_BARRIER::Transition(mirrorTexRes,
                                                                                   D3D12_RESOURCE_STATE_COPY_SOURCE,
                                                                                   D3D12_RESOURCE_STATE_RENDER_TARGET);
            DIRECTX.CurrentFrameResources().CommandLists[DIRECTX.ActiveContext]->ResourceBarrier(1, &resBar);
        }

        DIRECTX.SubmitCommandListAndPresent(drawMirror);
    }

    // Release resources
Done:
    delete mainCam;
    delete roomScene;
    if (mirrorTexture)
        ovr_DestroyMirrorTexture(session, mirrorTexture);

    for (int eye = 0; eye < 2; ++eye)
    {
        delete pEyeRenderTexture[eye];
    }
    DIRECTX.ReleaseDevice();
    ovr_Destroy(session);

    // Retry on ovrError_DisplayLost
    return retryCreate || (result == ovrError_DisplayLost);
}

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    // Initializes LibOVR, and the Rift
	ovrInitParams initParams = { ovrInit_RequestVersion, OVR_MINOR_VERSION, NULL, 0, 0 };
	ovrResult result = ovr_Initialize(&initParams);
    VALIDATE(OVR_SUCCESS(result), "Failed to initialize libOVR.");

    VALIDATE(DIRECTX.InitWindow(hinst, L"Oculus Room Tiny (DX12)"), "Failed to open window.");

    DIRECTX.Run(MainLoop);

    ovr_Shutdown();
    return(0);
}
