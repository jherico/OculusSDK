/************************************************************************************

Filename    :   CAPI_D3D11_DistortionRenderer.cpp
Content     :   Experimental distortion renderer
Created     :   November 11, 2013
Authors     :   Volga Aksoy, Michael Antonov, Shariq Hashme

Copyright   :   Copyright 2014 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.2

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "CAPI_D3D11_DistortionRenderer.h"

#include "OVR_CAPI_D3D.h"
#include "../CAPI_HMDState.h"
#include "Kernel/OVR_Color.h"
#include "../Textures/overdriveLut_dk2.h"

#include "../../Displays/OVR_Win32_Dxgi_Display.h" // Display driver timing info


namespace OVR { namespace CAPI { namespace D3D11 {

#include "Shaders/Distortion_ps.h"
#include "Shaders/DistortionChroma_vs.h"
#include "Shaders/DistortionChroma_ps.h"
#include "Shaders/DistortionTimewarpChroma_vs.h"
#include "Shaders/DistortionCS2x2.h"

#include "Shaders/SimpleQuad_vs.h"
#include "Shaders/SimpleQuad_ps.h"

#include "Tracing/Tracing.h"

#include <initguid.h>
DEFINE_GUID(IID_OVRDXGISwapchain, 0x868f9b4f, 0xe427, 0x46ed, 0xb0, 0x94, 0x66, 0xd1, 0x3b, 0xb, 0x48, 0xf7);

[uuid(E741B60E-3AC8-418A-AB3C-26C1D4EDD33B)]
interface IOVRDXGISwapChain : IUnknown
{
    virtual HRESULT GetDirectBuffer(REFIID riid, void** ppv) = 0;
};

#include <VersionHelpers.h>

// Distortion pixel shader lookup.
//  Bit 0: Chroma Correction
//  Bit 1: Timewarp

enum {
    DistortionVertexShaderBitMask = 3,
    DistortionVertexShaderCount = DistortionVertexShaderBitMask + 1,
    DistortionPixelShaderBitMask = 0,
    DistortionPixelShaderCount = DistortionPixelShaderBitMask + 1,
};

struct PrecompiledShader
{
    const unsigned char* ShaderData;
    size_t ShaderSize;
    const ShaderBase::Uniform* ReflectionData;
    size_t ReflectionSize;
};

// To add a new distortion shader use these macros (with or w/o reflection)
#define PCS_NOREFL(shader) { shader, sizeof(shader), NULL, 0 }
#define PCS_REFL__(shader) { shader, sizeof(shader), shader ## _refl, sizeof( shader ## _refl )/sizeof(*(shader ## _refl)) }


static PrecompiledShader DistortionVertexShaderLookup[DistortionVertexShaderCount] =
{
    PCS_REFL__(DistortionChroma_vs),
    PCS_REFL__(DistortionTimewarpChroma_vs),
    PCS_REFL__(DistortionTimewarpChroma_vs),
    { NULL, 0, NULL, 0 },
};

static PrecompiledShader DistortionPixelShaderLookup[DistortionPixelShaderCount] =
{
    PCS_REFL__(DistortionChroma_ps)
};

enum
{
    DistortionComputeShader2x2 = 0,
    DistortionComputeShaderCount
};
static PrecompiledShader DistortionComputeShaderLookup[DistortionComputeShaderCount] =
{
    PCS_REFL__(DistortionCS2x2)
};



void DistortionShaderBitIndexCheck()
{
    OVR_COMPILER_ASSERT(ovrDistortionCap_TimeWarp == 2);
}



struct DistortionVertex         // Must match the VB description DistortionMeshVertexDesc
{
    Vector2f ScreenPosNDC;
    Vector2f TanEyeAnglesR;
    Vector2f TanEyeAnglesG;
    Vector2f TanEyeAnglesB;
    Color    Col;
};

struct DistortionComputePin     // Must match the ones declared in DistortionCS*.csh
{
    Vector2f TanEyeAnglesR;
    Vector2f TanEyeAnglesG;
    Vector2f TanEyeAnglesB;
    Color Col;
    int padding[1];     // Aligns to power-of-two boundary, increases performance significantly.
};


// Vertex type; same format is used for all shapes for simplicity.
// Shapes are built by adding vertices to Model.
struct Vertex
{
    Vector3f  Pos;
    Color     C;
    float     U, V;
    Vector3f  Norm;

    Vertex(const Vector3f& p, const Color& c = Color(64, 0, 0, 255),
        float u = 0, float v = 0, Vector3f n = Vector3f(1, 0, 0))
        : Pos(p), C(c), U(u), V(v), Norm(n)
    {}
    Vertex(float x, float y, float z, const Color& c = Color(64, 0, 0, 255),
        float u = 0, float v = 0) : Pos(x, y, z), C(c), U(u), V(v)
    { }

    bool operator==(const Vertex& b) const
    {
        return Pos == b.Pos && C == b.C && U == b.U && V == b.V;
    }
};


//----------------------------------------------------------------------------
// ***** D3D11::DistortionRenderer

DistortionRenderer::DistortionRenderer()
{
    SrgbBackBuffer = false;

    EyeTextureSize[0] = Sizei(0);
    EyeRenderViewport[0] = Recti();
    EyeTextureSize[1] = Sizei(0);
    EyeRenderViewport[1] = Recti();
}

DistortionRenderer::~DistortionRenderer()
{
    destroy();
}

// static
CAPI::DistortionRenderer* DistortionRenderer::Create()
{
    return new DistortionRenderer;
}


bool DistortionRenderer::initializeRenderer(const ovrRenderAPIConfig* apiConfig)
{
    const ovrD3D11Config* config = (const ovrD3D11Config*)apiConfig;

    // Reset the frame index read failure count, as this function is called when
    // switching between windowed and fullscreen mode.
    FrameIndexFailureCount = 0;

    if (!config)
    {
        // Cleanup
        pEyeTextures[0].Clear();
        pEyeTextures[1].Clear();
        pEyeDepthTextures[0].Clear();
        pEyeDepthTextures[1].Clear();
        memset(&RParams, 0, sizeof(RParams));
        return true;
    }

    if (!config->D3D11.pDevice || !config->D3D11.pBackBufferRT)
        return false;

    if (Display::GetDirectDisplayInitialized())
    {
        Ptr<IUnknown> ovrSwapChain;
        if (config->D3D11.pSwapChain->QueryInterface(IID_OVRDXGISwapchain, (void**)&ovrSwapChain.GetRawRef()) == E_NOINTERFACE)
        {
            OVR_DEBUG_LOG_TEXT(("ovr_Initialize() or ovr_InitializeRenderingShim() wasn't called before DXGISwapChain was created."));
        }
    }

    RParams.pDevice = config->D3D11.pDevice;
    RParams.pContext = config->D3D11.pDeviceContext;
    RParams.pBackBufferRT = config->D3D11.pBackBufferRT;
    RParams.pBackBufferUAV = config->D3D11.pBackBufferUAV;
    RParams.pSwapChain = config->D3D11.pSwapChain;
    RParams.BackBufferSize = config->D3D11.Header.BackBufferSize;
    RParams.Multisample = config->D3D11.Header.Multisample;
    RParams.VidPnTargetId = 0;

    // set RParams.VidPnTargetId to the display target id for ETW tracing in order
    // to match Microsoft-Windows-DxgKrnl's VSync event
    IDXGIOutput *pOutput = NULL;
    RParams.pSwapChain->GetContainingOutput(&pOutput);
    if (pOutput)
    {
        // get the swapchain's DeviceName
        DXGI_OUTPUT_DESC desc;
        pOutput->GetDesc(&desc);

        // allocate the required buffers for QueryDisplayConfig (we don't need pModeInfoArray but it can't be NULL or less than needed)
        UINT32 NumPathArrayElements = 0, NumModeInfoArrayElements = 0;
        DISPLAYCONFIG_PATH_INFO *pPathInfoArray = NULL;
        DISPLAYCONFIG_MODE_INFO *pModeInfoArray = NULL;
        LONG st = ERROR_INSUFFICIENT_BUFFER;
        while (ERROR_INSUFFICIENT_BUFFER == st)
        {
            st = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &NumPathArrayElements, &NumModeInfoArrayElements);
            if (ERROR_SUCCESS != st)
            {
                OVR_DEBUG_LOG_TEXT(("Error: GetDisplayConfigBufferSizes failed with %ld\n", st));
                break;
            }

            pPathInfoArray = new DISPLAYCONFIG_PATH_INFO[NumPathArrayElements];
            pModeInfoArray = new DISPLAYCONFIG_MODE_INFO[NumModeInfoArrayElements];

            st = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &NumPathArrayElements, pPathInfoArray, &NumModeInfoArrayElements, pModeInfoArray, NULL);
            if (ERROR_SUCCESS != st) OVR_DEBUG_LOG_TEXT(("Error: QueryDisplayConfig failed with %ld\n", st));
        }

        // search for matching display targets for the SwapChain's display source
        if (ERROR_SUCCESS == st)
        {
            for (UINT32 i = 0; i < NumPathArrayElements; ++i)
            {
                DISPLAYCONFIG_PATH_INFO *p = &pPathInfoArray[i];

                DISPLAYCONFIG_SOURCE_DEVICE_NAME sdn;
                sdn.header.size = sizeof(sdn);
                sdn.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
                sdn.header.adapterId = p->sourceInfo.adapterId;
                sdn.header.id = p->sourceInfo.id;
                st = DisplayConfigGetDeviceInfo(&sdn.header);

                DISPLAYCONFIG_TARGET_DEVICE_NAME tdn;
                tdn.header.size = sizeof(tdn);
                tdn.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
                tdn.header.adapterId = p->targetInfo.adapterId;
                tdn.header.id = p->targetInfo.id;
                st = DisplayConfigGetDeviceInfo(&tdn.header);

                if (wcsncmp(sdn.viewGdiDeviceName, desc.DeviceName, sizeof(desc.DeviceName)) == 0)
                {
                    // pick anything if nothing was found yet, else give precedence to "Rift" monitors on this display device
                    static const wchar_t Rift[] = { L'R', L'i', L'f', L't' };
                    if (!RParams.VidPnTargetId || (wcsncmp(tdn.monitorFriendlyDeviceName, Rift, sizeof(Rift)) == 0))
                    {
                        RParams.VidPnTargetId = p->targetInfo.id;
                        OVR_DEBUG_LOG_TEXT(("Debug: Found VidPnTargetId=%d for display %d name=\"%ls\"\n", RParams.VidPnTargetId, p->sourceInfo.id, tdn.monitorFriendlyDeviceName));
                    }
                }
            }
        }

        delete [] pPathInfoArray;
        delete [] pModeInfoArray;

        pOutput->Release();
    }

    GfxState = *new GraphicsState(RParams.pContext);

    D3D11_RENDER_TARGET_VIEW_DESC backBufferDesc;
    RParams.pBackBufferRT->GetDesc(&backBufferDesc);
    SrgbBackBuffer = (backBufferDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) ||
        (backBufferDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) ||
        (backBufferDesc.Format == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB);


#if 0   // enable related section in DistortionChroma.psh shader
    // aniso requires proper sRGB sampling
    SampleMode hqFilter = (RenderState->DistortionCaps & ovrDistortionCap_HqDistortion) ? Sample_Anisotropic : Sample_Linear;
#else
    SampleMode hqFilter = Sample_Linear;
#endif

    pEyeTextures[0] = *new Texture(&RParams, Texture_RGBA, Sizei(0),
        getSamplerState(hqFilter | Sample_ClampBorder));
    pEyeTextures[1] = *new Texture(&RParams, Texture_RGBA, Sizei(0),
        getSamplerState(hqFilter | Sample_ClampBorder));

    pEyeDepthTextures[0] = *new Texture(&RParams, Texture_Depth, Sizei(0),
        getSamplerState(hqFilter | Sample_ClampBorder));
    pEyeDepthTextures[1] = *new Texture(&RParams, Texture_Depth, Sizei(0),
        getSamplerState(hqFilter | Sample_ClampBorder));

    if (!initBuffersAndShaders())
    {
        return false;
    }

    // Rasterizer state
    D3D11_RASTERIZER_DESC rs;
    memset(&rs, 0, sizeof(rs));
    rs.AntialiasedLineEnable = true;
    rs.CullMode = D3D11_CULL_BACK;
    rs.DepthClipEnable = true;
    rs.FillMode = D3D11_FILL_SOLID;
    Rasterizer = NULL;
    RParams.pDevice->CreateRasterizerState(&rs, &Rasterizer.GetRawRef());

    initOverdrive();

    // TBD: Blend state.. not used?
    // We'll want to turn off blending

    GpuProfiler.Init(RParams.pDevice, RParams.pContext);

    return true;
}

void DistortionRenderer::initOverdrive()
{
    if (RenderState->DistortionCaps & ovrDistortionCap_Overdrive)
    {
        LastUsedOverdriveTextureIndex = 0;

        D3D11_RENDER_TARGET_VIEW_DESC backBufferDesc;
        RParams.pBackBufferRT->GetDesc(&backBufferDesc);

        for (int i = 0; i < NumOverdriveTextures; i++)
        {
            pOverdriveTextures[i] = *new Texture(&RParams, Texture_RGBA, RParams.BackBufferSize,
                getSamplerState(Sample_Linear | Sample_ClampBorder));

            D3D11_TEXTURE2D_DESC dsDesc;
            dsDesc.Width = RParams.BackBufferSize.w;
            dsDesc.Height = RParams.BackBufferSize.h;
            dsDesc.MipLevels = 1;
            dsDesc.ArraySize = 1;
            dsDesc.Format = backBufferDesc.Format;
            dsDesc.SampleDesc.Count = 1;
            dsDesc.SampleDesc.Quality = 0;
            dsDesc.Usage = D3D11_USAGE_DEFAULT;
            dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            dsDesc.CPUAccessFlags = 0;
            dsDesc.MiscFlags = 0;

            HRESULT hr = RParams.pDevice->CreateTexture2D(&dsDesc, NULL, &pOverdriveTextures[i]->Tex.GetRawRef());
            if (FAILED(hr))
            {
                OVR_DEBUG_LOG_TEXT(("Failed to create overdrive texture."));
                // Remove overdrive flag since we failed to create the texture
                LastUsedOverdriveTextureIndex = -1;	// disables feature
                break;
            }

            RParams.pDevice->CreateShaderResourceView(pOverdriveTextures[i]->Tex, NULL, &pOverdriveTextures[i]->TexSv.GetRawRef());
            RParams.pDevice->CreateRenderTargetView(pOverdriveTextures[i]->Tex, NULL, &pOverdriveTextures[i]->TexRtv.GetRawRef());
        }

        const int dimSize = 256;
        OVR_COMPILER_ASSERT(dimSize * dimSize * 4 == sizeof(overdriveLut_dk2));
        OverdriveLutTexture = *new Texture(&RParams, Texture_RGBA, Sizei(dimSize, dimSize),
                                           getSamplerState(Sample_Linear | Sample_Clamp), overdriveLut_dk2, 1);
    }
    else
    {
        LastUsedOverdriveTextureIndex = -1;
    }
}

void DistortionRenderer::SubmitEye(int eyeId, const ovrTexture* eyeTexture)
{
    if (eyeTexture)
    {
        const ovrD3D11Texture* tex = (const ovrD3D11Texture*)eyeTexture;

        // Use tex->D3D11.Header.RenderViewport to update UVs for rendering in case they changed.
        // TBD: This may be optimized through some caching. 
        EyeTextureSize[eyeId] = tex->D3D11.Header.TextureSize;
        EyeRenderViewport[eyeId] = tex->D3D11.Header.RenderViewport;

        const ovrEyeRenderDesc& erd = RenderState->EyeRenderDesc[eyeId];

        ovrHmd_GetRenderScaleAndOffset(erd.Fov,
            EyeTextureSize[eyeId], EyeRenderViewport[eyeId],
            UVScaleOffset[eyeId]);

        if (RenderState->DistortionCaps & ovrDistortionCap_FlipInput)
        {
            UVScaleOffset[eyeId][0].y = -UVScaleOffset[eyeId][0].y;
            UVScaleOffset[eyeId][1].y = 1.0f - UVScaleOffset[eyeId][1].y;
        }

        // Get multisample count from texture
        D3D11_TEXTURE2D_DESC desc;
        tex->D3D11.pTexture->GetDesc(&desc);

        pEyeTextures[eyeId]->UpdatePlaceholderTexture(tex->D3D11.pTexture, tex->D3D11.pSRView,
            tex->D3D11.Header.TextureSize, desc.SampleDesc.Count);
    }
}

void DistortionRenderer::SubmitEyeWithDepth(int eyeId, const ovrTexture* eyeColorTexture, const ovrTexture* eyeDepthTexture)
{
    SubmitEye(eyeId, eyeColorTexture);

    if (eyeDepthTexture)
    {
        const ovrD3D11Texture* depthTex = (const ovrD3D11Texture*)eyeDepthTexture;

        // Use tex->D3D11.Header.RenderViewport to update UVs for rendering in case they changed.
        // TBD: This may be optimized through some caching. 
        EyeTextureSize[eyeId] = depthTex->D3D11.Header.TextureSize;
        EyeRenderViewport[eyeId] = depthTex->D3D11.Header.RenderViewport;

        const ovrEyeRenderDesc& erd = RenderState->EyeRenderDesc[eyeId];

        ovrHmd_GetRenderScaleAndOffset(erd.Fov,
            EyeTextureSize[eyeId], EyeRenderViewport[eyeId],
            UVScaleOffset[eyeId]);

        if (RenderState->DistortionCaps & ovrDistortionCap_FlipInput)
        {
            UVScaleOffset[eyeId][0].y = -UVScaleOffset[eyeId][0].y;
            UVScaleOffset[eyeId][1].y = 1.0f - UVScaleOffset[eyeId][1].y;
        }

        // Get multisample count from texture
        D3D11_TEXTURE2D_DESC desc;
        depthTex->D3D11.pTexture->GetDesc(&desc);

        pEyeDepthTextures[eyeId]->UpdatePlaceholderTexture(depthTex->D3D11.pTexture, depthTex->D3D11.pSRView,
            depthTex->D3D11.Header.TextureSize, desc.SampleDesc.Count);
    }
}

void DistortionRenderer::renderEndFrame()
{
    renderDistortion();

    if (RegisteredPostDistortionCallback)
        RegisteredPostDistortionCallback(RParams.pContext);

    if (LatencyTest2Active)
    {
        renderLatencyPixel(LatencyTest2DrawColor);
    }
}

/******************************************************************/
// Attempt to use DXGI for getting a previous vsync
double DistortionRenderer::getDXGILastVsyncTime()
{
    OVR_ASSERT(RParams.pSwapChain != nullptr);

    // If in driver mode,
    if (!RenderState->OurHMDInfo.InCompatibilityMode)
    {
        // Prefer the driver mode
        return 0.;
    }

    // If failure count is exceeded,
    if (FrameIndexFailureCount >= FrameIndexFailureLimit)
    {
        if (FrameIndexFailureCount == FrameIndexFailureLimit)
        {
            LogError("[D3D11DistortionRenderer] Performance Warning: DXGI GetFrameStatistics could not get Vsync timing.  The game should be running in fullscreen mode on the Rift to get adequate timing information.");
            ++FrameIndexFailureCount;
        }

        return 0.;
    }

    // Get frame statistics from the D3D11 renderer
    DXGI_FRAME_STATISTICS stats;
    HRESULT hr = RParams.pSwapChain->GetFrameStatistics(&stats);
    if (SUCCEEDED(hr))
    {
        FrameIndexFailureCount = 0; // Reset failure count

        // Return Vsync time in seconds
        return stats.SyncQPCTime.QuadPart * Timer::GetPerfFrequencyInverse();
    }

    FrameIndexFailureCount++; // Increment failure count
    return 0.;
}

void DistortionRenderer::EndFrame(uint32_t frameIndex, bool swapBuffers)
{
    // Calculate the display frame index from the last known vsync time and
    // corresponding display frame index
    Timing->CalculateTimewarpTiming(frameIndex, getDXGILastVsyncTime());

    // Don't spin if we are explicitly asked not to
    if ( (RenderState->DistortionCaps & ovrDistortionCap_TimeWarp) &&
         (RenderState->DistortionCaps & ovrDistortionCap_TimewarpJitDelay) &&
        !(RenderState->DistortionCaps & ovrDistortionCap_ProfileNoSpinWaits))
    {
        if (!Timing->NeedDistortionTimeMeasurement())
        {
            // Wait for timewarp distortion if it is time and Gpu idle
            FlushGpuAndWaitTillTime(Timing->GetTimewarpTiming()->JIT_TimewarpTime);

            renderEndFrame();
        }
        else
        {
            // If needed, measure distortion time so that TimeManager can better estimate
            // latency-reducing time-warp wait timing.
            WaitUntilGpuIdle();
            double distortionStartTime = ovr_GetTimeInSeconds();

            renderEndFrame();

            WaitUntilGpuIdle();
            Timing->AddDistortionTimeMeasurement(ovr_GetTimeInSeconds() - distortionStartTime);
        }
    }
    else
    {
        renderEndFrame();
    }

    if (LatencyTestActive)
    {
        renderLatencyQuad(LatencyTestDrawColor);
    }

    if (swapBuffers)
    {
        if (RParams.pSwapChain)
        {
            TraceDistortionPresent(RParams.VidPnTargetId, 0);

            UINT swapInterval = (RenderState->EnabledHmdCaps & ovrHmdCap_NoVSync) ? 0 : 1;
            RParams.pSwapChain->Present(swapInterval, 0);

            // Force GPU to flush the scene, resulting in the lowest possible latency.
            // It's critical that this flush is *after* present.
            // With the display driver this flush is obsolete and theoretically should
            // be a no-op.
            // Doesn't need to be done if running through the Oculus driver.
            if (RenderState->OurHMDInfo.InCompatibilityMode &&
                !(RenderState->DistortionCaps & ovrDistortionCap_ProfileNoSpinWaits))
            {
                WaitUntilGpuIdle();
            }
        }
        else
        {
            // TBD: Generate error - swapbuffer option used with null swapchain.
        }
    }

    TraceDistortionEnd(RParams.VidPnTargetId, 0);
}


void DistortionRenderer::WaitUntilGpuIdle()
{
    HRESULT hr;

    TraceDistortionWaitGPU(RParams.VidPnTargetId, 0);

    // Flush and Stall CPU while waiting for GPU to complete rendering all of the queued draw calls
    D3D11_QUERY_DESC queryDesc = { D3D11_QUERY_EVENT, 0 };
    Ptr<ID3D11Query> query;
    hr = RParams.pDevice->CreateQuery(&queryDesc, &query.GetRawRef());

    if (SUCCEEDED(hr))
    {
        RParams.pContext->End(query);

        // This flush is very important to measure Present() time in practice and prevent the
        // GPU from allowing us to queue ahead unintentionally in extended mode.
        RParams.pContext->Flush();

        for (;;)
        {
            BOOL done = FALSE;
            hr = RParams.pContext->GetData(query, &done, sizeof(done), 0);

            // Exit on failure to avoid infinite loop.
            if (FAILED(hr))
            {
                break;
            }

            // If event succeeded and it's done,
            if (SUCCEEDED(hr) && done)
            {
                break;
            }
        }
    }
}

double DistortionRenderer::FlushGpuAndWaitTillTime(double absTime)
{
    RParams.pContext->Flush();
    return WaitTillTime(absTime);
}

bool DistortionRenderer::initBuffersAndShaders()
{
    if (RenderState->DistortionCaps & ovrDistortionCap_ComputeShader)
    {
        // Compute shader distortion grid.
        // TODO - only do this if the CS is actually enabled?
        for (int eyeNum = 0; eyeNum < 2; eyeNum++)
        {
            // Compute shader setup of regular grid.
            DistortionMeshVBs[eyeNum] = NULL;
            DistortionMeshIBs[eyeNum] = NULL;

            // These constants need to match those declared in the shader in DistortionCS*.csh
            const int gridSizeInPixels = 16;
            const int pinsPerEdge = 128;


            // TODO: clean up this mess!
            ovrEyeType eyeType = RenderState->EyeRenderDesc[eyeNum].Eye;
            ovrFovPort fov = RenderState->EyeRenderDesc[eyeNum].Fov;

            HmdRenderInfo const &  hmdri = RenderState->RenderInfo;
            DistortionRenderDesc const & distortion = RenderState->Distortion[eyeType];


            // Find the mapping from TanAngle space to target NDC space.
            ScaleAndOffset2D      eyeToSourceNDC = CreateNDCScaleAndOffsetFromFov(fov);

            //const StereoEyeParams &stereoParams = ( eyeNum == 0 ) ? stereoParamsLeft : stereoParamsRight;
            OVR_ASSERT(gridSizeInPixels * (pinsPerEdge - 1) > hmdri.ResolutionInPixels.w / 2);
            OVR_ASSERT(gridSizeInPixels * (pinsPerEdge - 1) > hmdri.ResolutionInPixels.h);
            DistortionComputePin Verts[pinsPerEdge*pinsPerEdge];
            // Vertices are laid out in a vertical scanline pattern,
            // scanning right to left, then within each scan going top to bottom, like DK2.
            // If we move to a different panel orientation, we may need to flip this around.
            int vertexNum = 0;
            for (int x = 0; x < pinsPerEdge; x++)
            {
                for (int y = 0; y < pinsPerEdge; y++)
                {
                    int pixX = x * gridSizeInPixels;
                    int pixY = y * gridSizeInPixels;
#if 0
                    // Simple version, ignoring pentile offsets
                    Vector2f screenPosNdc;
                    screenPosNdc.x = 2.0f * (0.5f - ((float)pixX / (hmdri.ResolutionInPixels.w / 2)));      // Note signs!
                    screenPosNdc.y = 2.0f * (-0.5f + ((float)pixY / hmdri.ResolutionInPixels.h));      // Note signs!

                    DistortionMeshVertexData vertex = DistortionMeshMakeVertex(screenPosNdc,
                        (eyeNum == 1),
                        hmdri,
                        distortion,
                        eyeToSourceNDC);
                    DistortionComputePin *pCurVert = &(Verts[vertexNum]);
                    pCurVert->TanEyeAnglesR = vertex.TanEyeAnglesR;
                    pCurVert->TanEyeAnglesG = vertex.TanEyeAnglesG;
                    pCurVert->TanEyeAnglesB = vertex.TanEyeAnglesB;
#else
                    // Pentile offsets are messy.
                    Vector2f screenPos[3];      // R=0, G=1, B=2
                    DistortionMeshVertexData vertexRGB[3];
                    screenPos[1] = Vector2f((float)pixX, (float)pixY);
                    screenPos[0] = screenPos[1];
                    screenPos[2] = screenPos[1];


                    for (int i = 0; i < 3; i++)
                    {
                        Vector2f screenPosNdc;
                        screenPosNdc.x = 2.0f * (0.5f - (screenPos[i].x / (hmdri.ResolutionInPixels.w / 2)));      // Note signs!
                        screenPosNdc.y = 2.0f * (-0.5f + (screenPos[i].y / hmdri.ResolutionInPixels.h));      // Note signs!
                        vertexRGB[i] = DistortionMeshMakeVertex(screenPosNdc,
                            (eyeNum == 1),
                            hmdri,
                            distortion,
                            eyeToSourceNDC);
                    }
                    // Most data (fade, TW interpolate, etc) comes from the green channel.
                    DistortionMeshVertexData vertex = vertexRGB[1];
                    DistortionComputePin *pCurVert = &(Verts[vertexNum]);
                    pCurVert->TanEyeAnglesR = vertexRGB[0].TanEyeAnglesR;
                    pCurVert->TanEyeAnglesG = vertexRGB[1].TanEyeAnglesG;
                    pCurVert->TanEyeAnglesB = vertexRGB[2].TanEyeAnglesB;
#endif

                    // vertex.Shade will go negative beyond the edges to produce correct intercept with the 0.0 plane.
                    // We want to preserve this, so bias and offset to fit [-1,+1] in a byte.
                    // The reverse wll be done in the shader.
                    float shade = Alg::Clamp(vertex.Shade * 0.5f + 0.5f, 0.0f, 1.0f);
                    pCurVert->Col.R = (OVR::UByte)(floorf(shade * 255.999f));
                    pCurVert->Col.G = pCurVert->Col.R;
                    pCurVert->Col.B = pCurVert->Col.R;
                    pCurVert->Col.A = (OVR::UByte)(floorf(vertex.TimewarpLerp * 255.999f));

                    vertexNum++;
                }
            }
            DistortionPinBuffer[eyeNum] = *new Buffer(&RParams);
            DistortionPinBuffer[eyeNum]->Data(Buffer_Compute, Verts, vertexNum * sizeof(Verts[0]), sizeof(Verts[0]));
        }

    }
    else
    {
        for (int eyeNum = 0; eyeNum < 2; eyeNum++)
        {
            // Allocate & generate distortion mesh vertices.
            DistortionPinBuffer[eyeNum] = NULL;

            ovrDistortionMesh meshData;

            //        double startT = ovr_GetTimeInSeconds();

            if (!CalculateDistortionMeshFromFOV(RenderState->RenderInfo,
                                       RenderState->Distortion[eyeNum],
                                       (RenderState->EyeRenderDesc[eyeNum].Eye == ovrEye_Left ? StereoEye_Left : StereoEye_Right),
                                       RenderState->EyeRenderDesc[eyeNum].Fov,
                                       RenderState->DistortionCaps,
                                       &meshData))
            {
                OVR_ASSERT(false);
                return false;
            }

            //        double deltaT = ovr_GetTimeInSeconds() - startT;
            //        LogText("GenerateDistortion time = %f\n", deltaT);

            // Now parse the vertex data and create a render ready vertex buffer from it
            DistortionVertex *   pVBVerts = (DistortionVertex*)OVR_ALLOC(sizeof(DistortionVertex) * meshData.VertexCount);
            DistortionVertex *   pCurVBVert = pVBVerts;
            ovrDistortionVertex* pCurOvrVert = meshData.pVertexData;

            for (unsigned vertNum = 0; vertNum < meshData.VertexCount; vertNum++)
            {
                pCurVBVert->ScreenPosNDC.x = pCurOvrVert->ScreenPosNDC.x;
                pCurVBVert->ScreenPosNDC.y = pCurOvrVert->ScreenPosNDC.y;
                pCurVBVert->TanEyeAnglesR = (*(Vector2f*)&pCurOvrVert->TanEyeAnglesR);
                pCurVBVert->TanEyeAnglesG = (*(Vector2f*)&pCurOvrVert->TanEyeAnglesG);
                pCurVBVert->TanEyeAnglesB = (*(Vector2f*)&pCurOvrVert->TanEyeAnglesB);

                // Convert [0.0f,1.0f] to [0,255]
                if (RenderState->DistortionCaps & ovrDistortionCap_Vignette)
                    pCurVBVert->Col.R = (uint8_t)(Alg::Max(pCurOvrVert->VignetteFactor, 0.0f) * 255.99f);
                else
                    pCurVBVert->Col.R = 255;

                pCurVBVert->Col.G = pCurVBVert->Col.R;
                pCurVBVert->Col.B = pCurVBVert->Col.R;
                pCurVBVert->Col.A = (uint8_t)(pCurOvrVert->TimeWarpFactor * 255.99f);
                pCurOvrVert++;
                pCurVBVert++;
            }

            DistortionMeshVBs[eyeNum] = *new Buffer(&RParams);
            DistortionMeshVBs[eyeNum]->Data(Buffer_Vertex | Buffer_ReadOnly, pVBVerts, sizeof(DistortionVertex)* meshData.VertexCount);
            DistortionMeshIBs[eyeNum] = *new Buffer(&RParams);
            DistortionMeshIBs[eyeNum]->Data(Buffer_Index | Buffer_ReadOnly, meshData.pIndexData, (sizeof(INT16)* meshData.IndexCount));

            OVR_FREE(pVBVerts);
            ovrHmd_DestroyDistortionMesh(&meshData);
        }
    }


    // Uniform buffers
    for (int i = 0; i < Shader_Count; i++)
    {
        UniformBuffers[i] = *new Buffer(&RParams);
        //MaxTextureSet[i] = 0;
    }

    initShaders();

    return true;
}



void DistortionRenderer::renderDistortion()
{
    // XXX takes a frameIndex second parameter, how do we get that here?
    TraceDistortionBegin(RParams.VidPnTargetId, 0);

    Ptr<IOVRDXGISwapChain> ovrSwap;
    HRESULT hr = RParams.pSwapChain->QueryInterface(IID_PPV_ARGS(&ovrSwap.GetRawRef()));
    if (SUCCEEDED(hr))
    {
        Ptr<ID3D11Texture2D> texture;
        hr = ovrSwap->GetDirectBuffer(IID_PPV_ARGS(&texture.GetRawRef()));
        if (SUCCEEDED(hr))
        {
            Ptr<ID3D11RenderTargetView> rtv;
            auto it = RenderTargetMap.Find(texture.GetPtr());
            if (it == RenderTargetMap.End())
            {
                hr = RParams.pDevice->CreateRenderTargetView(texture, nullptr, &rtv.GetRawRef());
                if (SUCCEEDED(hr))
                {
                    RenderTargetMap.Add(texture.GetPtr(), rtv);
                }
            }
            else
            {
                rtv = it->Second;
            }

            if (rtv)
            {
                // The RenderTargets map holds the ref count on this for us
                RParams.pBackBufferRT = rtv;
            }
        }
    }

    RParams.pContext->HSSetShader(NULL, NULL, 0);
    RParams.pContext->DSSetShader(NULL, NULL, 0);
    RParams.pContext->GSSetShader(NULL, NULL, 0);

    RParams.pContext->RSSetState(Rasterizer);

    bool overdriveActive = IsOverdriveActive();
    int currOverdriveTextureIndex = -1;

    if (overdriveActive)
    {
        currOverdriveTextureIndex = (LastUsedOverdriveTextureIndex + 1) % NumOverdriveTextures;
        ID3D11RenderTargetView* distortionRtv = pOverdriveTextures[currOverdriveTextureIndex]->TexRtv.GetRawRef();
        ID3D11RenderTargetView* mrtRtv[2] = { distortionRtv, RParams.pBackBufferRT };
        RParams.pContext->OMSetRenderTargets(2, mrtRtv, 0);

        RParams.pContext->ClearRenderTargetView(distortionRtv, RenderState->ClearColor);
    }
    else
    {
        RParams.pContext->OMSetRenderTargets(1, &RParams.pBackBufferRT, 0);
    }

    // Not affected by viewport.
    RParams.pContext->ClearRenderTargetView(RParams.pBackBufferRT, RenderState->ClearColor);

    setViewport(Recti(0, 0, RParams.BackBufferSize.w, RParams.BackBufferSize.h));


    for (int eyeNum = 0; eyeNum < 2; eyeNum++)
    {
        ShaderFill distortionShaderFill(DistortionShader);
        distortionShaderFill.SetTexture(0, pEyeTextures[eyeNum], Shader_Pixel);

        if (pEyeDepthTextures[eyeNum]->Tex != NULL)
        {
            OVR_ASSERT(pEyeDepthTextures[eyeNum]->GetSamples() <= 4);
            DistortionShader->SetUniform1f("depthMsaaSamples", (float)pEyeDepthTextures[eyeNum]->GetSamples());

            // the shader will select the right version
            distortionShaderFill.SetTexture(2, pEyeDepthTextures[eyeNum], Shader_Vertex);   // DepthTexture4x
            switch (pEyeDepthTextures[eyeNum]->GetSamples())
            {
            case 1: distortionShaderFill.SetTexture(0, pEyeDepthTextures[eyeNum], Shader_Vertex);   break;  // Set DepthTexture1x
            case 2: distortionShaderFill.SetTexture(1, pEyeDepthTextures[eyeNum], Shader_Vertex);   break;  // Set DepthTexture2x
            case 4: distortionShaderFill.SetTexture(2, pEyeDepthTextures[eyeNum], Shader_Vertex);   break;  // Set DepthTexture4x

            default:
                OVR_ASSERT(false);  // unsupported MSAA sample count (requires shader update)
                LogError("{ERR-105} [D3D1x] Unsupported MSAA sample count (requires D3D shader update)");
            }

            if (PositionTimewarpDesc.NearClip >= 0.0f && PositionTimewarpDesc.FarClip >= 0.0f)
            {
                float NearClip = PositionTimewarpDesc.NearClip;
                float FarClip = PositionTimewarpDesc.FarClip;

                float DepthProjectorX = FarClip / (FarClip - NearClip);
                float DepthProjectorY = (-FarClip * NearClip) / (FarClip - NearClip);
                DistortionShader->SetUniform2f("DepthProjector", DepthProjectorX, DepthProjectorY);
            }
            else
            {
                OVR_ASSERT(false);
                LogError("{ERR-101} [D3D1x] Invalid ovrPositionTimewarpDesc data provided by client.");

                DistortionShader->SetUniform2f("DepthProjector", 1.0f, 1.0f);
            }

            // DepthProjector values can also be calculated as:
            //float DepthProjectorX = FarClip / (FarClip - NearClip);
            //float DepthProjectorY = (-FarClip * NearClip) / (FarClip - NearClip);
            //DistortionShader->SetUniform2f("DepthProjector", -eyeProj[eyeNum].M[2][2], eyeProj[eyeNum].M[2][3]);
            DistortionShader->SetUniform2f("DepthDimSize", (float)pEyeDepthTextures[eyeNum]->TextureSize.w,
                (float)pEyeDepthTextures[eyeNum]->TextureSize.h);
        }
        else
        {
            // -1.0 disables the use of the depth buffer
            DistortionShader->SetUniform1f("depthMsaaSamples", -1.0f);
        }

        if (RenderState->DistortionCaps & ovrDistortionCap_HqDistortion)
        {
            static float aaDerivMult = 1.0f;
            DistortionShader->SetUniform1f("AaDerivativeMult", aaDerivMult);
        }
        else
        {
            // 0.0 disables high quality anti-aliasing
            DistortionShader->SetUniform1f("AaDerivativeMult", -1.0f);
        }

        if (overdriveActive)
        {
            distortionShaderFill.SetTexture(1, pOverdriveTextures[LastUsedOverdriveTextureIndex], Shader_Pixel);
            distortionShaderFill.SetTexture(2, OverdriveLutTexture, Shader_Pixel);

            // Toggle this to compare LUTs vs analytical values for overdrive
            static bool enableLut = false;

            float overdriveScaleRegularRise;
            float overdriveScaleRegularFall;
            GetOverdriveScales(overdriveScaleRegularRise, overdriveScaleRegularFall);
            DistortionShader->SetUniform3f("OverdriveScales", enableLut ? 2.0f : 1.0f,
                                            overdriveScaleRegularRise, overdriveScaleRegularFall);
        }
        else
        {
            // -1.0f disables PLO            
            DistortionShader->SetUniform3f("OverdriveScales", -1.0f, -1.0f, -1.0f);
        }

        distortionShaderFill.SetInputLayout(DistortionVertexIL);

        DistortionShader->SetUniform2f("EyeToSourceUVScale", UVScaleOffset[eyeNum][0].x, UVScaleOffset[eyeNum][0].y);
        DistortionShader->SetUniform2f("EyeToSourceUVOffset", UVScaleOffset[eyeNum][1].x, UVScaleOffset[eyeNum][1].y);


        if (RenderState->DistortionCaps & ovrDistortionCap_TimeWarp)
        {
            Matrix4f startEndMatrices[2];
            double timewarpIMUTime = 0.;
            // TODO: if (pEyeDepthTextures[eyeNum]->Tex != NULL), need to use CalculateTimewarpFromSensors instead.
            CalculateOrientationTimewarpFromSensors(
                RenderState->EyeRenderPoses[eyeNum].Orientation,
                SensorReader, Timing->GetTimewarpTiming()->EyeStartEndTimes[eyeNum],
                startEndMatrices, timewarpIMUTime);
            Timing->SetTimewarpIMUTime(timewarpIMUTime);

            if (RenderState->DistortionCaps & ovrDistortionCap_ComputeShader)
            {
                DistortionShader->SetUniform3x3f("EyeRotationStart", startEndMatrices[0]);
                DistortionShader->SetUniform3x3f("EyeRotationEnd", startEndMatrices[1]);
            }
            else
            {
                // Can feed identity like matrices incase of concern over timewarp calculations
                DistortionShader->SetUniform4x4f("EyeRotationStart", startEndMatrices[0]);
                DistortionShader->SetUniform4x4f("EyeRotationEnd", startEndMatrices[1]);
            }
        }


        if (RenderState->DistortionCaps & ovrDistortionCap_ComputeShader)
        {
            //RParams.pContext->CSCSSetShaderResources
            //RParams.pContext->CSSetUnorderedAccessViews
            //RParams.pContext->CSSetShader
            //RParams.pContext->CSSetSamplers
            //RParams.pContext->CSSetConstantBuffers


            // These need to match the values used in the compiled shader
            //const int gridSizeInPixels = 16;        // GRID_SIZE_IN_PIXELS
            //const int pinsPerEdge = 128;            // PINS_PER_EDGE
            const int nxnBlockSizeInPixels = 2;		// NXN_BLOCK_SIZE_PIXELS
            const int simdSquareSize = 16;			// SIMD_SQUARE_SIZE

            const int invocationSizeInPixels = nxnBlockSizeInPixels * simdSquareSize;

            distortionShaderFill.SetTexture(0, pEyeTextures[eyeNum], Shader_Compute);

            DistortionShader->SetUniform1f("RightEye", (float)eyeNum);
            DistortionShader->SetUniform1f("UseOverlay", 0.0f);             // No overlay supported here.
            DistortionShader->SetUniform1f("FbSizePixelsX", (float)RParams.BackBufferSize.w);


            ShaderSet* shaders = distortionShaderFill.GetShaders();
            ShaderBase* cshader = ((ShaderBase*)shaders->GetShader(Shader_Compute));

            ID3D11UnorderedAccessView *uavRendertarget = RParams.pBackBufferUAV;
            int SizeX = RParams.BackBufferSize.w / 2;
            int SizeY = RParams.BackBufferSize.h;

            int TileNumX = (SizeX + (invocationSizeInPixels - 1)) / invocationSizeInPixels;
            int TileNumY = (SizeY + (invocationSizeInPixels - 1)) / invocationSizeInPixels;

            RParams.pContext->CSSetUnorderedAccessViews(0, 1, &uavRendertarget, NULL);


            // Incoming eye-buffer textures start at t0 onwards, so set this in slot #4
            // Subtlety - can't put this in slot 0 because fill->Set stops at the first NULL texture.
            ID3D11ShaderResourceView *d3dSrv = DistortionPinBuffer[eyeNum]->GetSrv();
            RParams.pContext->CSSetShaderResources(4, 1, &d3dSrv);

            // TODO: uniform/constant buffers
            cshader->UpdateBuffer(UniformBuffers[Shader_Compute]);
            cshader->SetUniformBuffer(UniformBuffers[Shader_Compute]);

            // Primitive type is ignored for CS.
            // This call actually sets the textures and does pContext->CSSetShader(). Primitive type is ignored.
            distortionShaderFill.Set(Prim_Unknown);

            RParams.pContext->Dispatch(TileNumX, TileNumY, 1);
        }
        else
        {
            renderPrimitives(&distortionShaderFill, DistortionMeshVBs[eyeNum], DistortionMeshIBs[eyeNum],
                NULL, 0, (int)DistortionMeshIBs[eyeNum]->GetSize() / 2, Prim_Triangles);
        }
    }

    LastUsedOverdriveTextureIndex = currOverdriveTextureIndex;

    // Re-activate to only draw on back buffer
    if (overdriveActive)
    {
        RParams.pContext->OMSetRenderTargets(1, &RParams.pBackBufferRT, 0);
    }
}

void DistortionRenderer::createDrawQuad()
{
    const int numQuadVerts = 4;
    LatencyTesterQuadVB = *new Buffer(&RParams);
    if (!LatencyTesterQuadVB)
    {
        return;
    }

    LatencyTesterQuadVB->Data(Buffer_Vertex, NULL, numQuadVerts * sizeof(Vertex));
    Vertex* vertices = (Vertex*)LatencyTesterQuadVB->Map(0, numQuadVerts * sizeof(Vertex), Map_Discard);
    if (!vertices)
    {
        OVR_ASSERT(false); // failed to lock vertex buffer
        return;
    }

    const float left = -1.0f;
    const float top = -1.0f;
    const float right = 1.0f;
    const float bottom = 1.0f;

    vertices[0] = Vertex(Vector3f(left, top, 0.0f), Color(255, 255, 255, 255));
    vertices[1] = Vertex(Vector3f(left, bottom, 0.0f), Color(255, 255, 255, 255));
    vertices[2] = Vertex(Vector3f(right, top, 0.0f), Color(255, 255, 255, 255));
    vertices[3] = Vertex(Vector3f(right, bottom, 0.0f), Color(255, 255, 255, 255));

    LatencyTesterQuadVB->Unmap(vertices);
}

void DistortionRenderer::renderLatencyQuad(unsigned char* latencyTesterDrawColor)
{
    const int numQuadVerts = 4;

    if (!LatencyTesterQuadVB)
    {
        createDrawQuad();
    }

    ShaderFill quadFill(SimpleQuadShader);
    quadFill.SetInputLayout(SimpleQuadVertexIL);

    setViewport(Recti(0, 0, RParams.BackBufferSize.w, RParams.BackBufferSize.h));

    float testerLuminance = (float)latencyTesterDrawColor[0] / 255.99f;
    if (SrgbBackBuffer)
    {
        testerLuminance = pow(testerLuminance, 2.2f);
    }

    SimpleQuadShader->SetUniform2f("Scale", 0.3f, 0.3f);
    SimpleQuadShader->SetUniform4f("Color", testerLuminance, testerLuminance, testerLuminance, 1.0f);

    for (int eyeNum = 0; eyeNum < 2; eyeNum++)
    {
        SimpleQuadShader->SetUniform2f("PositionOffset", eyeNum == 0 ? -0.5f : 0.5f, 0.0f);
        renderPrimitives(&quadFill, LatencyTesterQuadVB, NULL, NULL, 0, numQuadVerts, Prim_TriangleStrip);
    }
}

void DistortionRenderer::renderLatencyPixel(unsigned char* latencyTesterPixelColor)
{
    const int numQuadVerts = 4;

    if (!LatencyTesterQuadVB)
    {
        createDrawQuad();
    }

    ShaderFill quadFill(SimpleQuadShader);
    quadFill.SetInputLayout(SimpleQuadVertexIL);

    setViewport(Recti(0, 0, RParams.BackBufferSize.w, RParams.BackBufferSize.h));

    Vector3f testerColor = Vector3f((float)latencyTesterPixelColor[0] / 255.99f,
        (float)latencyTesterPixelColor[1] / 255.99f,
        (float)latencyTesterPixelColor[2] / 255.99f);
    if (SrgbBackBuffer)
    {
        // 2.2 gamma is close enough for our purposes of matching sRGB
        testerColor.x = pow(testerColor.x, 2.2f);
        testerColor.y = pow(testerColor.y, 2.2f);
        testerColor.z = pow(testerColor.z, 2.2f);
    }

#ifdef OVR_BUILD_DEBUG
    SimpleQuadShader->SetUniform4f("Color", testerColor.x, testerColor.y, testerColor.z, 1.0f);

    Vector2f scale(20.0f / RParams.BackBufferSize.w, 20.0f / RParams.BackBufferSize.h);
#else
    // sending in as gray scale
    SimpleQuadShader->SetUniform4f("Color", testerColor.x, testerColor.x, testerColor.x, 1.0f);

    Vector2f scale(1.0f / RParams.BackBufferSize.w, 1.0f / RParams.BackBufferSize.h);
#endif
    SimpleQuadShader->SetUniform2f("Scale", scale.x, scale.y);

    float xOffset = RenderState->RenderInfo.OffsetLatencyTester ? -0.5f * scale.x : 1.0f - scale.x;
    float yOffset = 1.0f - scale.y;

    // Render the latency tester quad in the correct location.
    if (RenderState->RenderInfo.Rotation == 270)
    {
        xOffset = -xOffset;
    }
    else if (RenderState->RenderInfo.Rotation == 180)
    {
        xOffset = -xOffset;
        yOffset = -yOffset;
    }
    else if (RenderState->RenderInfo.Rotation == 90)
    {
        yOffset = -yOffset;
    }

    SimpleQuadShader->SetUniform2f("PositionOffset", xOffset, yOffset);

    renderPrimitives(&quadFill, LatencyTesterQuadVB, NULL, NULL, 0, numQuadVerts, Prim_TriangleStrip);
}

void DistortionRenderer::renderPrimitives(
    const ShaderFill* fill,
    Buffer* vertices, Buffer* indices,
    Matrix4f* viewMatrix, int offset, int count,
    PrimitiveType rprim)
{
    OVR_ASSERT(fill->GetInputLayout() != 0);
    RParams.pContext->IASetInputLayout((ID3D11InputLayout*)fill->GetInputLayout());

    if (indices)
    {
        RParams.pContext->IASetIndexBuffer(indices->GetBuffer(), DXGI_FORMAT_R16_UINT, 0);
    }

    ID3D11Buffer* vertexBuffer = vertices->GetBuffer();
    UINT          vertexStride = sizeof(Vertex);
    UINT          vertexOffset = offset;
    RParams.pContext->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);

    ShaderSet* shaders = ((ShaderFill*)fill)->GetShaders();

    ShaderBase*     vshader = ((ShaderBase*)shaders->GetShader(Shader_Vertex));
    unsigned char*  vertexData = vshader->UniformData;
    if (vertexData)
    {
        // TODO: some VSes don't start with StandardUniformData!
        if (viewMatrix)
        {
            StandardUniformData* stdUniforms = (StandardUniformData*)vertexData;
            stdUniforms->View = viewMatrix->Transposed();
            stdUniforms->Proj = StdUniforms.Proj;
        }
        UniformBuffers[Shader_Vertex]->Data(Buffer_Uniform, vertexData, vshader->UniformsSize);
        vshader->SetUniformBuffer(UniformBuffers[Shader_Vertex]);
    }

    for (int i = Shader_Vertex + 1; i < Shader_Count; i++)
    {
        if (shaders->GetShader(i))
        {
            ((ShaderBase*)shaders->GetShader(i))->UpdateBuffer(UniformBuffers[i]);
            ((ShaderBase*)shaders->GetShader(i))->SetUniformBuffer(UniformBuffers[i]);
        }
    }

    D3D11_PRIMITIVE_TOPOLOGY prim;
    switch (rprim)
    {
    case Prim_Triangles:
        prim = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        break;
    case Prim_Lines:
        prim = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        break;
    case Prim_TriangleStrip:
        prim = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        break;
    default:
        OVR_ASSERT(0);
        return;
    }
    RParams.pContext->IASetPrimitiveTopology(prim);

    fill->Set(rprim);

    if (indices)
    {
        RParams.pContext->DrawIndexed(count, 0, 0);
    }
    else
    {
        RParams.pContext->Draw(count, 0);
    }
}

void DistortionRenderer::setViewport(const Recti& vp)
{
    D3D11_VIEWPORT d3dvp;

    d3dvp.Width = (float)vp.w;
    d3dvp.Height = (float)vp.h;
    d3dvp.TopLeftX = (float)vp.x;
    d3dvp.TopLeftY = (float)vp.y;
    d3dvp.MinDepth = 0;
    d3dvp.MaxDepth = 1;
    RParams.pContext->RSSetViewports(1, &d3dvp);
}



// Must match struct DistortionVertex
static D3D11_INPUT_ELEMENT_DESC DistortionMeshVertexDesc[] =
{
    { "Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TexCoord", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TexCoord", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "Color", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

static D3D11_INPUT_ELEMENT_DESC SimpleQuadMeshVertexDesc[] =
{
    { "Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};


void DistortionRenderer::initShaders()
{
    if ((RenderState->DistortionCaps & ovrDistortionCap_ComputeShader) != 0)
    {
        // Compute shader
        DistortionShader = *new ShaderSet;

        int shaderNum = DistortionComputeShader2x2;        

        PrecompiledShader psShaderByteCode = DistortionComputeShaderLookup[shaderNum];
        Ptr<D3D11::ComputeShader> cs = *new D3D11::ComputeShader(
            &RParams,
            (void*)psShaderByteCode.ShaderData, psShaderByteCode.ShaderSize,
            psShaderByteCode.ReflectionData, psShaderByteCode.ReflectionSize);

        DistortionShader->SetShader(cs);
    }
    else
    {
        // Vertex + pixel distortion shader.
        PrecompiledShader& vsShaderByteCode = DistortionVertexShaderLookup[DistortionVertexShaderBitMask & RenderState->DistortionCaps];
        if (vsShaderByteCode.ShaderData != NULL)
        {
            Ptr<D3D11::VertexShader> vtxShader = *new D3D11::VertexShader(
                &RParams,
                (void*)vsShaderByteCode.ShaderData, vsShaderByteCode.ShaderSize,
                vsShaderByteCode.ReflectionData, vsShaderByteCode.ReflectionSize);

            DistortionVertexIL = NULL;
            ID3D11InputLayout** objRef = &DistortionVertexIL.GetRawRef();

            HRESULT validate = RParams.pDevice->CreateInputLayout(
                DistortionMeshVertexDesc, sizeof(DistortionMeshVertexDesc) / sizeof(DistortionMeshVertexDesc[0]),
                vsShaderByteCode.ShaderData, vsShaderByteCode.ShaderSize, objRef);
            OVR_UNUSED(validate);

            DistortionShader = *new ShaderSet;
            DistortionShader->SetShader(vtxShader);
        }
        else
        {
            OVR_ASSERT_M(false, "Unsupported distortion feature used\n");
        }

        PrecompiledShader& psShaderByteCode = DistortionPixelShaderLookup[DistortionPixelShaderBitMask & RenderState->DistortionCaps];
        if (psShaderByteCode.ShaderData)
        {
            Ptr<D3D11::PixelShader> ps = *new D3D11::PixelShader(
                &RParams,
                (void*)psShaderByteCode.ShaderData, psShaderByteCode.ShaderSize,
                psShaderByteCode.ReflectionData, psShaderByteCode.ReflectionSize);

            DistortionShader->SetShader(ps);
        }
        else
        {
            OVR_ASSERT_M(false, "Unsupported distortion feature used\n");
        }
    }

    {
        Ptr<D3D11::VertexShader> vtxShader = *new D3D11::VertexShader(
            &RParams,
            (void*)SimpleQuad_vs, sizeof(SimpleQuad_vs),
            SimpleQuad_vs_refl, sizeof(SimpleQuad_vs_refl) / sizeof(SimpleQuad_vs_refl[0]));
            //NULL, 0);

        SimpleQuadVertexIL = NULL;
        ID3D11InputLayout** objRef = &SimpleQuadVertexIL.GetRawRef();

        HRESULT validate = RParams.pDevice->CreateInputLayout(
            SimpleQuadMeshVertexDesc, sizeof(SimpleQuadMeshVertexDesc) / sizeof(SimpleQuadMeshVertexDesc[0]),
            (void*)SimpleQuad_vs, sizeof(SimpleQuad_vs), objRef);
        OVR_UNUSED(validate);

        SimpleQuadShader = *new ShaderSet;
        SimpleQuadShader->SetShader(vtxShader);

        Ptr<D3D11::PixelShader> ps = *new D3D11::PixelShader(
            &RParams,
            (void*)SimpleQuad_ps, sizeof(SimpleQuad_ps),
            SimpleQuad_ps_refl, sizeof(SimpleQuad_ps_refl) / sizeof(SimpleQuad_ps_refl[0]));

        SimpleQuadShader->SetShader(ps);
    }
}



ID3D11SamplerState* DistortionRenderer::getSamplerState(int sm)
{
    if (SamplerStates[sm])
        return SamplerStates[sm];

    D3D11_SAMPLER_DESC ss;
    memset(&ss, 0, sizeof(ss));
    switch(sm & Sample_AddressMask)
    {
    case Sample_Clamp:          ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;  break;
    case Sample_ClampBorder:    ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_BORDER; break;
    case Sample_Repeat:         ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;   break;
    case Sample_Mirror:         ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR; break;
    default:    OVR_ASSERT(false);
    }

    switch(sm & Sample_FilterMask)
    {
    case Sample_Linear:
        ss.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        break;

    case Sample_Nearest:
        ss.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        break;

    case Sample_Anisotropic:
        ss.Filter = D3D11_FILTER_ANISOTROPIC;
        ss.MaxAnisotropy = 4;
        break;

    default:    OVR_ASSERT(false);
    }

    ss.MaxLOD = 15;
    RParams.pDevice->CreateSamplerState(&ss, &SamplerStates[sm].GetRawRef());
    return SamplerStates[sm];
}


void DistortionRenderer::destroy()
{
    for (int eyeNum = 0; eyeNum < 2; eyeNum++)
    {
        DistortionMeshVBs[eyeNum].Clear();
        DistortionMeshIBs[eyeNum].Clear();
        DistortionPinBuffer[eyeNum].Clear();
    }

    DistortionVertexIL.Clear();

    if (DistortionShader)
    {
        DistortionShader->UnsetShader(Shader_Vertex);
        DistortionShader->UnsetShader(Shader_Pixel);
        DistortionShader->UnsetShader(Shader_Compute);
        DistortionShader.Clear();
    }

    LatencyTesterQuadVB.Clear();
}


DistortionRenderer::GraphicsState::GraphicsState(ID3D11DeviceContext* c)
    : context(c)
    , memoryCleared(TRUE)
    , rasterizerState(NULL)
    //samplerStates[]
    , inputLayoutState(NULL)
    //psShaderResourceState[]
    //vsShaderResourceState[]
    //psConstantBuffersState[]
    //vsConstantBuffersState[]
    //renderTargetViewState[]
    , depthStencilViewState(NULL)
    , omBlendState(NULL)
    //omBlendFactorState[]
    , omSampleMaskState(0xffffffff)
    , primitiveTopologyState(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
    , iaIndexBufferPointerState(NULL)
    , iaIndexBufferFormatState(DXGI_FORMAT_UNKNOWN)
    , iaIndexBufferOffsetState(0)
    //iaVertexBufferPointersState[]
    //iaVertexBufferStridesState[]
    //iaVertexBufferOffsetsState[]
    , currentPixelShader(NULL)
    , currentVertexShader(NULL)
    , currentGeometryShader(NULL)
    , currentHullShader(NULL)
    , currentDomainShader(NULL)
    , currentComputeShader(NULL)
{
    for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
    {
        psSamplerStates[i] = NULL;
        vsSamplerStates[i] = NULL;
        csSamplerStates[i] = NULL;
    }

    for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
    {
        psShaderResourceState[i] = NULL;
        vsShaderResourceState[i] = NULL;
        csShaderResourceState[i] = NULL;
    }

    for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
    {
        psConstantBuffersState[i] = NULL;
        vsConstantBuffersState[i] = NULL;
        csConstantBuffersState[i] = NULL;
    }

    for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
        renderTargetViewState[i] = NULL;
        csUnorderedAccessViewState[i] = NULL;
    }

    for (int i = 0; i < 4; i++)
        omBlendFactorState[i] = NULL;

    for (int i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
    {
        iaVertexBufferPointersState[i] = NULL;
        iaVertexBufferStridesState[i] = NULL;
        iaVertexBufferOffsetsState[i] = NULL;
    }
}

#define SAFE_RELEASE(x) if ( (x) != NULL ) { (x)->Release(); (x)=NULL; }

void DistortionRenderer::GraphicsState::clearMemory()
{
    SAFE_RELEASE(rasterizerState);

    for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
    {
        SAFE_RELEASE(psSamplerStates[i]);
        SAFE_RELEASE(vsSamplerStates[i]);
        SAFE_RELEASE(csSamplerStates[i]);
    }

    SAFE_RELEASE(inputLayoutState);

    for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
    {
        SAFE_RELEASE(psShaderResourceState[i]);
        SAFE_RELEASE(vsShaderResourceState[i]);
        SAFE_RELEASE(csShaderResourceState[i]);
    }

    for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
    {
        SAFE_RELEASE(psConstantBuffersState[i]);
        SAFE_RELEASE(vsConstantBuffersState[i]);
        SAFE_RELEASE(csConstantBuffersState[i]);
    }

    for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
        SAFE_RELEASE(renderTargetViewState[i]);
        SAFE_RELEASE(csUnorderedAccessViewState[i]);
    }

    SAFE_RELEASE(depthStencilViewState);
    SAFE_RELEASE(omBlendState);
    SAFE_RELEASE(iaIndexBufferPointerState);

    for (int i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
    {
        SAFE_RELEASE(iaVertexBufferPointersState[i]);
    }

    SAFE_RELEASE(currentPixelShader);
    SAFE_RELEASE(currentVertexShader);
    SAFE_RELEASE(currentGeometryShader);

    SAFE_RELEASE(currentHullShader);
    SAFE_RELEASE(currentDomainShader);
    SAFE_RELEASE(currentComputeShader);

    memoryCleared = TRUE;
}

#undef SAFE_RELEASE

DistortionRenderer::GraphicsState::~GraphicsState()
{
    clearMemory();
}


void DistortionRenderer::GraphicsState::Save()
{
    if (!memoryCleared)
        clearMemory();

    memoryCleared = FALSE;

    context->RSGetState(&rasterizerState);
    context->IAGetInputLayout(&inputLayoutState);

    context->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, psShaderResourceState);
    context->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, psSamplerStates);
    context->PSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, psConstantBuffersState);

    context->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, vsShaderResourceState);
    context->VSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, vsSamplerStates);
    context->VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, vsConstantBuffersState);

    context->CSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, csShaderResourceState);
    context->CSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, csSamplerStates);
    context->CSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, csConstantBuffersState);
    context->CSGetUnorderedAccessViews(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, csUnorderedAccessViewState);

    context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, renderTargetViewState, &depthStencilViewState);

    context->OMGetBlendState(&omBlendState, omBlendFactorState, &omSampleMaskState);

    context->IAGetPrimitiveTopology(&primitiveTopologyState);

    context->IAGetIndexBuffer(&iaIndexBufferPointerState, &iaIndexBufferFormatState, &iaIndexBufferOffsetState);

    context->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, iaVertexBufferPointersState, iaVertexBufferStridesState, iaVertexBufferOffsetsState);

    context->PSGetShader(&currentPixelShader, NULL, NULL);
    context->VSGetShader(&currentVertexShader, NULL, NULL);
    context->GSGetShader(&currentGeometryShader, NULL, NULL);
    context->HSGetShader(&currentHullShader, NULL, NULL);
    context->DSGetShader(&currentDomainShader, NULL, NULL);
    context->CSGetShader(&currentComputeShader, NULL, NULL);
    /* maybe above doesn't work; then do something with this (must test on dx11)
    ID3D11ClassInstance* blank_array[0];
    UINT blank_uint = 0;
    context->PSGetShader(&currentPixelShader, blank_array, blank_uint);
    context->VSGetShader(&currentVertexShader, blank_array, blank_uint);
    context->GSGetShader(&currentGeometryShader, blank_array, blank_uint);
    context->HSGetShader(&currentHullShader, blank_array, blank_uint);
    context->DSGetShader(&currentDomainShader, blank_array, blank_uint);
    context->CSGetShader(&currentComputeShader, blank_array, blank_uint);
    */
}


void DistortionRenderer::GraphicsState::Restore()
{
    if (rasterizerState != NULL)
        context->RSSetState(rasterizerState);

    if (inputLayoutState != NULL)
        context->IASetInputLayout(inputLayoutState);

    context->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, psSamplerStates);
    if (psShaderResourceState != NULL)
        context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, psShaderResourceState);
    if (psConstantBuffersState != NULL)
        context->PSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, psConstantBuffersState);

    context->VSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, vsSamplerStates);
    if (vsShaderResourceState != NULL)
        context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, vsShaderResourceState);
    if (vsConstantBuffersState != NULL)
        context->VSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, vsConstantBuffersState);

    context->CSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, csSamplerStates);
    if (csShaderResourceState != NULL)
        context->CSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, csShaderResourceState);
    if (csConstantBuffersState != NULL)
        context->CSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, csConstantBuffersState);
    if (csUnorderedAccessViewState != NULL)
        context->CSSetUnorderedAccessViews(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, csUnorderedAccessViewState, NULL);

    if (depthStencilViewState != NULL || renderTargetViewState != NULL)
        context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, renderTargetViewState, depthStencilViewState);

    if (omBlendState != NULL)
        context->OMSetBlendState(omBlendState, omBlendFactorState, omSampleMaskState);

    context->IASetPrimitiveTopology(primitiveTopologyState);

    if (iaIndexBufferPointerState != NULL)
        context->IASetIndexBuffer(iaIndexBufferPointerState, iaIndexBufferFormatState, iaIndexBufferOffsetState);

    if (iaVertexBufferPointersState != NULL)
        context->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, iaVertexBufferPointersState, iaVertexBufferStridesState, iaVertexBufferOffsetsState);

    if (currentPixelShader != NULL)
        context->PSSetShader(currentPixelShader, NULL, 0);
    if (currentVertexShader != NULL)
        context->VSSetShader(currentVertexShader, NULL, 0);
    if (currentGeometryShader != NULL)
        context->GSSetShader(currentGeometryShader, NULL, 0);
    if (currentHullShader != NULL)
        context->HSSetShader(currentHullShader, NULL, 0);
    if (currentDomainShader != NULL)
        context->DSSetShader(currentDomainShader, NULL, 0);
    if (currentComputeShader != NULL)
        context->CSSetShader(currentComputeShader, NULL, 0);

    clearMemory();
}

}}} // OVR::CAPI::D3D11
