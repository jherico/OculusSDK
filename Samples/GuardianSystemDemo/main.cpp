/************************************************************************************
Authors     :   Bruno Evangelista
Copyright   :   Copyright 2016 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.3 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at
http://www.oculusvr.com/licenses/LICENSE-3.3 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/

#define   OVR_D3D_VERSION 11
#pragma warning(disable: 4324)
#include "Win32_DirectXAppUtil.h" // DirectX Helper
#include "OVR_CAPI_D3D.h" // Oculus SDK
#include <chrono>
DirectX11 DIRECTX;
typedef std::chrono::time_point<std::chrono::high_resolution_clock> high_resolution_clock;
float randVelocity() { return (rand() % 201) * 0.01f - 1.0f; }
float randColor() { return (rand() % 81 + 20) * 0.01f; };


class GuardianSystemDemo
{
public:
    void Start(HINSTANCE hinst);
    void InitRenderTargets(const ovrHmdDesc& hmdDesc);
    void InitSceneGraph();

    float UpdateTimeWithBoundaryTest();
    void  UpdateBoundaryLookAndFeel();
    void  UpdateObjectsCollisionWithBoundary(float elapsedTimeSec);
    void  Render();

private:
    XMVECTOR mObjPosition[Scene::MAX_MODELS];                               // Objects cached position 
    XMVECTOR mObjVelocity[Scene::MAX_MODELS];                               // Objects velocity
    Scene mDynamicScene;                                                    // Scene graph

    ovrSession mSession = nullptr;
    high_resolution_clock mLastUpdateClock;                                 // Stores last update time
    float mGlobalTimeSec = 0;                                               // Game global time

    uint32_t mFrameIndex = 0;                                               // Global frame counter
    ovrVector3f mHmdToEyeOffset[ovrEye_Count] = {};                         // Offset from the center of the HMD to each eye
    ovrRecti mEyeRenderViewport[ovrEye_Count] = {};                         // Eye render target viewport

    ovrLayerEyeFov mEyeRenderLayer = {};                                    // OVR  - Eye render layers description
    ovrTextureSwapChain mTextureChain[ovrEye_Count] = {};                   // OVR  - Eye render target swap chain
    ID3D11DepthStencilView* mEyeDepthTarget[ovrEye_Count] = {};             // DX11 - Eye depth view
    std::vector<ID3D11RenderTargetView*> mEyeRenderTargets[ovrEye_Count];   // DX11 - Eye render view

    bool mShouldQuit = false;
    bool mSlowMotionMode = false;                                           // Slow motion gets enabled when too close to the boundary
};


void GuardianSystemDemo::InitRenderTargets(const ovrHmdDesc& hmdDesc)
{
    // For each eye
    for (int i = 0; i < ovrEye_Count; ++i) {
        // Viewport
        const float kPixelsPerDisplayPixel = 1.0f;
        ovrSizei idealSize = ovr_GetFovTextureSize(mSession, (ovrEyeType)i, hmdDesc.DefaultEyeFov[i], kPixelsPerDisplayPixel);
        mEyeRenderViewport[i] = { 0, 0, idealSize.w, idealSize.h };

        // Create Swap Chain
        ovrTextureSwapChainDesc desc = {
            ovrTexture_2D, OVR_FORMAT_R8G8B8A8_UNORM_SRGB, 1, idealSize.w, idealSize.h, 1, 1, 
            ovrFalse, ovrTextureMisc_DX_Typeless, ovrTextureBind_DX_RenderTarget
        };

        // Configure Eye render layers
        mEyeRenderLayer.Header.Type = ovrLayerType_EyeFov;
        mEyeRenderLayer.Viewport[i] = mEyeRenderViewport[i];
        mEyeRenderLayer.Fov[i] = hmdDesc.DefaultEyeFov[i];
        mHmdToEyeOffset[i] = ovr_GetRenderDesc(mSession, (ovrEyeType)i, hmdDesc.DefaultEyeFov[i]).HmdToEyeOffset;

        // DirectX 11 - Generate RenderTargetView from textures in swap chain
        // ----------------------------------------------------------------------
        ovrResult result = ovr_CreateTextureSwapChainDX(mSession, DIRECTX.Device, &desc, &mTextureChain[i]);
        if (!OVR_SUCCESS(result)) {
            printf("ovr_CreateTextureSwapChainDX failed"); exit(-1);
        }

        // Render Target, normally triple-buffered
        int textureCount = 0;
        ovr_GetTextureSwapChainLength(mSession, mTextureChain[i], &textureCount);
        for (int j = 0; j < textureCount; ++j) {
            ID3D11Texture2D* renderTexture = nullptr;
            ovr_GetTextureSwapChainBufferDX(mSession, mTextureChain[i], j, IID_PPV_ARGS(&renderTexture));
            
            D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {
                DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_RTV_DIMENSION_TEXTURE2D
            };

            ID3D11RenderTargetView* renderTargetView = nullptr;
            DIRECTX.Device->CreateRenderTargetView(renderTexture, &renderTargetViewDesc, &renderTargetView);
            mEyeRenderTargets[i].push_back(renderTargetView);
            renderTexture->Release();
        }

        // DirectX 11 - Generate Depth
        // ----------------------------------------------------------------------
        D3D11_TEXTURE2D_DESC depthTextureDesc = {
            (UINT)idealSize.w, (UINT)idealSize.h, 1, 1, DXGI_FORMAT_D32_FLOAT, {1, 0},
            D3D11_USAGE_DEFAULT, D3D11_BIND_DEPTH_STENCIL, 0, 0
        };
        
        ID3D11Texture2D* depthTexture = nullptr;
        DIRECTX.Device->CreateTexture2D(&depthTextureDesc, NULL, &depthTexture);
        DIRECTX.Device->CreateDepthStencilView(depthTexture, NULL, &mEyeDepthTarget[i]);
        depthTexture->Release();
    }
}


void GuardianSystemDemo::InitSceneGraph()
{
    for (int32_t i = 0; i < Scene::MAX_MODELS; ++i) {
        TriangleSet mesh;
        mesh.AddSolidColorBox(-0.035f, -0.035f, -0.035f, 0.035f, 0.035f, 0.035f, 0xFFFFFFFF);

        // Objects start 1 meter high
        mObjPosition[i] = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
        // Objects have random velocity
        mObjVelocity[i] = XMVectorSet(randVelocity(), randVelocity() * 0.5f, randVelocity(), 0);
        mObjVelocity[i] = XMVector3Normalize(mObjVelocity[i]) * 0.3f;

        Material* mat = new Material(new Texture(false, 256, 256, Texture::AUTO_FLOOR));
        mDynamicScene.Add(new Model(&mesh, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1), mat));
    }
}


void GuardianSystemDemo::Start(HINSTANCE hinst)
{
    ovrResult result;
    result = ovr_Initialize(nullptr);
    if (!OVR_SUCCESS(result)) {
        printf("ovr_Initialize failed"); exit(-1);
    }

    ovrGraphicsLuid luid;
    result = ovr_Create(&mSession, &luid);
    if (!OVR_SUCCESS(result)) {
        printf("ovr_Create failed"); exit(-1);
    }

    if (!DIRECTX.InitWindow(hinst, L"GuardianSystemDemo")) {
        printf("DIRECTX.InitWindow failed"); exit(-1);
    }

    // Use HMD desc to initialize device
    ovrHmdDesc hmdDesc = ovr_GetHmdDesc(mSession);
    if (!DIRECTX.InitDevice(hmdDesc.Resolution.w / 2, hmdDesc.Resolution.h / 2, reinterpret_cast<LUID*>(&luid))) {
        printf("DIRECTX.InitDevice failed"); exit(-1);
    }

    // Use FloorLevel tracking origin
    ovr_SetTrackingOriginType(mSession, ovrTrackingOrigin_FloorLevel);
    
    InitRenderTargets(hmdDesc);
    InitSceneGraph();
    mLastUpdateClock = std::chrono::high_resolution_clock::now();

    // Main Loop
    while (DIRECTX.HandleMessages() && !mShouldQuit)
    {
        ovrSessionStatus sessionStatus;
        ovr_GetSessionStatus(mSession, &sessionStatus);
        if (sessionStatus.ShouldQuit)
            break;

        float elapsedTimeSec = UpdateTimeWithBoundaryTest();
        UpdateBoundaryLookAndFeel();
        UpdateObjectsCollisionWithBoundary(elapsedTimeSec);
        Render();
    }

    ovr_Shutdown();
}


float GuardianSystemDemo::UpdateTimeWithBoundaryTest()
{
    // Calculate elapsed time
    auto clockNow = std::chrono::high_resolution_clock::now();
    float elapsedTimeSec = ((std::chrono::duration<float>)(clockNow - mLastUpdateClock)).count();
    mLastUpdateClock = clockNow;

    // Check if ANY tracked device is triggering the outer boundary
    ovrBoundaryTestResult test;
    ovr_TestBoundary(mSession, ovrTrackedDevice_All, ovrBoundary_Outer, &test);

    const float kSlowMotionStartDistance = 0.5f;    // Slow motion start at half a meter
    const float kStopMotionDistance = 0.1f;         // Stops motion at 10cm
    const float kMotionDistanceRange = kSlowMotionStartDistance - kStopMotionDistance;
    if (test.ClosestDistance < kSlowMotionStartDistance) {
        elapsedTimeSec *= (max(0.0f, test.ClosestDistance - kStopMotionDistance) / kMotionDistanceRange);
    }

    mGlobalTimeSec += elapsedTimeSec;
    srand((uint32_t)mGlobalTimeSec);
    return elapsedTimeSec;
}


void GuardianSystemDemo::UpdateBoundaryLookAndFeel()
{
    if ((uint32_t)mGlobalTimeSec % 2 == 1) {
        ovrBoundaryLookAndFeel lookAndFeel = {};
        lookAndFeel.Color = { randColor(), randColor(), randColor(), 1.0f };
        ovr_SetBoundaryLookAndFeel(mSession, &lookAndFeel);
        ovr_RequestBoundaryVisible(mSession, ovrTrue);
    }
    else {
        ovr_ResetBoundaryLookAndFeel(mSession);
        ovr_RequestBoundaryVisible(mSession, ovrFalse);
    }
}


void GuardianSystemDemo::UpdateObjectsCollisionWithBoundary(float elapsedTimeSec)
{
    if (mGlobalTimeSec < 1.0f) return; // Start update after 1s

    for (int32_t i = 0; i < mDynamicScene.numModels; ++i) {
        XMFLOAT3 newPosition;
        XMVECTOR newPositionVec = XMVectorAdd(mObjPosition[i], XMVectorScale(mObjVelocity[i], elapsedTimeSec));
        XMStoreFloat3(&newPosition, newPositionVec);

        // Test object collision with boundary
        ovrBoundaryTestResult test;
        ovr_TestBoundaryPoint(mSession, (ovrVector3f*)&newPosition.x, ovrBoundary_Outer, &test);

        // Collides with surface at 2cm
        if (test.ClosestDistance < 0.02f) {
            XMVECTOR surfaceNormal = XMVectorSet(test.ClosestPointNormal.x, test.ClosestPointNormal.y, test.ClosestPointNormal.z, 0.0f);
            mObjVelocity[i] = XMVector3Reflect(mObjVelocity[i], surfaceNormal);

            newPositionVec = XMVectorAdd(mObjPosition[i], XMVectorScale(mObjVelocity[i], elapsedTimeSec));
            XMStoreFloat3(&newPosition, newPositionVec);
        }

        mObjPosition[i] = newPositionVec;
        mDynamicScene.Models[i]->Pos = newPosition;
    }
}


void GuardianSystemDemo::Render()
{
    // Get current eye pose for rendering
    double eyePoseTime = 0;
    ovrPosef eyePose[ovrEye_Count] = {};
    ovr_GetEyePoses(mSession, mFrameIndex, ovrTrue, mHmdToEyeOffset, eyePose, &eyePoseTime);

    // Render each eye
    for (int i = 0; i < ovrEye_Count; ++i) {
        int renderTargetIndex = 0;
        ovr_GetTextureSwapChainCurrentIndex(mSession, mTextureChain[i], &renderTargetIndex);
        ID3D11RenderTargetView* renderTargetView = mEyeRenderTargets[i][renderTargetIndex];
        ID3D11DepthStencilView* depthTargetView = mEyeDepthTarget[i];

        // Clear and set render/depth target and viewport
        DIRECTX.SetAndClearRenderTarget(renderTargetView, depthTargetView, 0.2f, 0.2f, 0.2f, 1.0f);
        DIRECTX.SetViewport((float)mEyeRenderViewport[i].Pos.x, (float)mEyeRenderViewport[i].Pos.y, 
            (float)mEyeRenderViewport[i].Size.w, (float)mEyeRenderViewport[i].Size.h);

        // Eye
        XMVECTOR eyeRot = XMVectorSet(eyePose[i].Orientation.x, eyePose[i].Orientation.y, 
            eyePose[i].Orientation.z, eyePose[i].Orientation.w);
        XMVECTOR eyePos = XMVectorSet(eyePose[i].Position.x, eyePose[i].Position.y, eyePose[i].Position.z, 0);
        XMVECTOR eyeForward = XMVector3Rotate(XMVectorSet(0, 0, -1, 0), eyeRot);

        // Matrices
        XMMATRIX viewMat = XMMatrixLookAtRH(eyePos, XMVectorAdd(eyePos, eyeForward), 
            XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), eyeRot));
        ovrMatrix4f proj = ovrMatrix4f_Projection(mEyeRenderLayer.Fov[i], 0.001f, 1000.0f, ovrProjection_None);
        XMMATRIX projMat = XMMatrixTranspose(XMMATRIX(&proj.M[0][0]));
        XMMATRIX viewProjMat = XMMatrixMultiply(viewMat, projMat);

        // Render and commit to swap chain
        mDynamicScene.Render(&viewProjMat, 1.0f, 1.0f, 1.0f, 1.0f, true);
        ovr_CommitTextureSwapChain(mSession, mTextureChain[i]);

        // Update eye layer
        mEyeRenderLayer.ColorTexture[i] = mTextureChain[i];
        mEyeRenderLayer.RenderPose[i] = eyePose[i];
        mEyeRenderLayer.SensorSampleTime = eyePoseTime;
    }

    // Submit frames
    ovrLayerHeader* layers = &mEyeRenderLayer.Header;
    ovrResult result = ovr_SubmitFrame(mSession, mFrameIndex++, nullptr, &layers, 1);
    if (!OVR_SUCCESS(result)) {
        printf("ovr_SubmitFrame failed"); exit(-1);
    }
}


int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    GuardianSystemDemo* instance = new (_aligned_malloc(sizeof(GuardianSystemDemo), 16)) GuardianSystemDemo();
    instance->Start(hinst);
    delete instance;
    return 0;
}
