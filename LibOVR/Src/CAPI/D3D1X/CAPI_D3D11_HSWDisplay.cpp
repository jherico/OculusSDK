/************************************************************************************

Filename    :   CAPI_D3D11_HSWDisplay.cpp
Content     :   Implements Health and Safety Warning system.
Created     :   July 7, 2014
Authors     :   Paul Pedriana

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

#include "Util/Util_Direct3D.h"
#include "OVR_CAPI_D3D.h"
#include "CAPI_D3D11_HSWDisplay.h"
#include "Kernel/OVR_File.h"
#include "Kernel/OVR_SysFile.h"
#include "Kernel/OVR_Allocator.h"
#include "Kernel/OVR_Color.h"
#include "Extras/OVR_Math.h"

// We currently borrow the SimpleQuad shaders
#include "Shaders/SimpleTexturedQuad_vs.h"
#include "Shaders/SimpleTexturedQuad_ps.h"

// For a given DXGI format: if the format is a typeless one then this function returns a 
// suitable typed one. If the format is a typed one then this function returns it as-is.
static DXGI_FORMAT GetFullyTypedDXGIFormat(DXGI_FORMAT textureFormat)
{
    // http://msdn.microsoft.com/en-us/library/windows/desktop/bb173059%28v=vs.85%29.aspx

    DXGI_FORMAT fullyTypedFormat = textureFormat;

    switch (textureFormat)
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;   // or DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_SINT

    case DXGI_FORMAT_R32G32B32_TYPELESS:
        return DXGI_FORMAT_R32G32B32_FLOAT;      // or DXGI_FORMAT_R32G32B32_UINT, DXGI_FORMAT_R32G32B32_SINT

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        return DXGI_FORMAT_R16G16B16A16_UNORM;   // or DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_UINT, DXGI_FORMAT_R16G16B16A16_SNORM, DXGI_FORMAT_R16G16B16A16_SINT

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        return DXGI_FORMAT_R8G8B8A8_UNORM;      // or DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R8G8B8A8_SINT

    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

        // Others which we don't currently support:
        //case DXGI_FORMAT_R32G32_TYPELESS:
        //case DXGI_FORMAT_R32G8X24_TYPELESS:
        //case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        //case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        //case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        //case DXGI_FORMAT_R16G16_TYPELESS:
        //case DXGI_FORMAT_R32_TYPELESS:
        //case DXGI_FORMAT_R24G8_TYPELESS:
        //case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        //case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        //case DXGI_FORMAT_R8G8_TYPELESS:
        //case DXGI_FORMAT_R16_TYPELESS:
        //case DXGI_FORMAT_R8_TYPELESS:
        //case DXGI_FORMAT_BC1_TYPELESS:
        //case DXGI_FORMAT_BC2_TYPELESS:
        //case DXGI_FORMAT_BC3_TYPELESS:
        //case DXGI_FORMAT_BC4_TYPELESS:
        //case DXGI_FORMAT_BC5_TYPELESS:
        //case DXGI_FORMAT_BC6H_TYPELESS:
        //case DXGI_FORMAT_BC7_TYPELESS:
    }

    return fullyTypedFormat;
}



namespace OVR { namespace CAPI {

// To do Need to move LoadTextureTgaData to a shared location.
uint8_t* LoadTextureTgaData(OVR::File* f, uint8_t alpha, int& width, int& height);

namespace D3D11 {

// This is a temporary function implementation, and it functionality needs to be implemented in a more generic way.
Texture* LoadTextureTga(RenderParams& rParams, ID3D11SamplerState* pSamplerState, OVR::File* f, uint8_t alpha)
{
    Texture* pTexture = NULL;

    int width, height;
    const uint8_t* pRGBA = LoadTextureTgaData(f, alpha, width, height);

    if (pRGBA)
    {
        pTexture = new Texture(&rParams, Texture_RGBA, OVR::Sizei(0, 0), pSamplerState, 1);

        // Create the D3D texture
        D3D11_TEXTURE2D_DESC dsDesc;
        dsDesc.Width = width;
        dsDesc.Height = height;
        dsDesc.MipLevels = 1;
        dsDesc.ArraySize = 1;
        dsDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        dsDesc.SampleDesc.Count = 1;
        dsDesc.SampleDesc.Quality = 0;
        dsDesc.Usage = D3D11_USAGE_DEFAULT;
        dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        dsDesc.CPUAccessFlags = 0;
        dsDesc.MiscFlags = 0;

        HRESULT hr = rParams.pDevice->CreateTexture2D(&dsDesc, NULL, &pTexture->Tex.GetRawRef());

        if (SUCCEEDED(hr))
        {
            if (dsDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
                rParams.pDevice->CreateShaderResourceView(pTexture->Tex, NULL, &pTexture->TexSv.GetRawRef());

            rParams.pContext->UpdateSubresource(pTexture->Tex, 0, NULL, pRGBA, width * 4, width * height * 4);
        }
        else
        {
            OVR_DEBUG_LOG_TEXT(("[LoadTextureTga] CreateTexture2D failed"));
            pTexture->Release();
        }

        OVR_FREE(const_cast<uint8_t*>(pRGBA));
    }

    return pTexture;
}


// Loads a texture from a memory image of a TGA file.
Texture* LoadTextureTga(RenderParams& rParams, ID3D11SamplerState* pSamplerState, const uint8_t* pData, int dataSize, uint8_t alpha)
{
    MemoryFile memoryFile("", pData, dataSize);

    return LoadTextureTga(rParams, pSamplerState, &memoryFile, alpha);
}


// Loads a texture from a disk TGA file.
Texture* LoadTextureTga(RenderParams& rParams, ID3D11SamplerState* pSamplerState, const char* pFilePath, uint8_t alpha)
{
    SysFile sysFile;

    if (sysFile.Open(pFilePath, FileConstants::Open_Read | FileConstants::Open_Buffered))
        return LoadTextureTga(rParams, pSamplerState, &sysFile, alpha);

    return NULL;
}



// To do: This needs to be promoted to a central version, possibly in CAPI_HSWDisplay.h
struct HASWVertex
{
    Vector3f  Pos;
    Color     C;
    float     U, V;

    HASWVertex(const Vector3f& p, const Color& c = Color(64, 0, 0, 255), float u = 0, float v = 0)
        : Pos(p), C(c), U(u), V(v)
    {}

    HASWVertex(float x, float y, float z, const Color& c = Color(64, 0, 0, 255), float u = 0, float v = 0)
        : Pos(x, y, z), C(c), U(u), V(v)
    {}

    bool operator==(const HASWVertex& b) const
    {
        return (Pos == b.Pos) && (C == b.C) && (U == b.U) && (V == b.V);
    }
};



// The texture below may conceivably be shared between HSWDisplay instances. However,  
// beware that sharing may not be possible if two HMDs are using different locales  
// simultaneously. As of this writing it's not clear if that can occur in practice.

HSWDisplay::HSWDisplay(ovrRenderAPIType api, ovrHmd hmd, const HMDRenderState& renderState)
    : OVR::CAPI::HSWDisplay(api, hmd, renderState),
    RenderParams()
{
}

bool HSWDisplay::Initialize(const ovrRenderAPIConfig* apiConfig)
{
    const ovrD3D11Config* config = reinterpret_cast<const ovrD3D11Config*>(apiConfig);

    if (config)
    {
        RenderParams.pDevice = config->D3D11.pDevice;
        RenderParams.pContext = config->D3D11.pDeviceContext;
        RenderParams.pBackBufferUAV = config->D3D11.pBackBufferUAV;
        RenderParams.pBackBufferRT = config->D3D11.pBackBufferRT;
        RenderParams.pSwapChain = config->D3D11.pSwapChain;
        RenderParams.BackBufferSize = config->D3D11.Header.BackBufferSize;
        RenderParams.Multisample = config->D3D11.Header.Multisample;
        RenderParams.VidPnTargetId = 0;

        // We may want to create RasterizerState, or alternatively let the DistortionRenderer handle it.
    }
    // else do any necessary cleanup

    return true;
}

void HSWDisplay::Shutdown()
{
    UnloadGraphics();
}


void HSWDisplay::DisplayInternal()
{
    HSWDISPLAY_LOG(("[HSWDisplay D3D11] DisplayInternal()"));
    // We may want to call LoadGraphics here instead of within Render.
}


void HSWDisplay::DismissInternal()
{
    HSWDISPLAY_LOG(("[HSWDisplay D3D11] DismissInternal()"));
    UnloadGraphics();
}


void HSWDisplay::UnloadGraphics()
{
    //RenderParams: nothing to do.
    pSamplerState.Clear();
    pTexture.Clear();
    pVB.Clear();
    for (size_t i = 0; i < OVR_ARRAY_COUNT(UniformBufferArray); i++)
        UniformBufferArray[i].Clear();
    pShaderSet.Clear();
    pVertexInputLayout.Clear();
    pBlendState.Clear();
    pRasterizerState.Clear();
    // OrthoProjection: No need to clear.
}

void HSWDisplay::LoadGraphics()
{
    // Load the graphics if not loaded already.
    if (!pSamplerState)
    {
        D3D11_SAMPLER_DESC sDesc;

        memset(&sDesc, 0, sizeof(sDesc));
        sDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

        RenderParams.pDevice->CreateSamplerState(&sDesc, &pSamplerState.GetRawRef());
    }

#if defined(OVR_BUILD_DEBUG)
    if (!pTexture)
        pTexture = *LoadTextureTga(RenderParams, pSamplerState, "C:\\TestPath\\TestFile.tga", 255);
#endif

    if (!pTexture) // To do: Add support for .dds files, which would be significantly smaller than the size of the tga.
    {
        size_t textureSize;
        const uint8_t* TextureData = GetDefaultTexture(textureSize);
        pTexture = *LoadTextureTga(RenderParams, pSamplerState, TextureData, (int)textureSize, 255);
    }

    if (!UniformBufferArray[0])
    {
        for (size_t i = 0; i < OVR_ARRAY_COUNT(UniformBufferArray); i++)
            UniformBufferArray[i] = *new Buffer(&RenderParams);
    }

    if (!pShaderSet)
    {
        pShaderSet = *new ShaderSet;

        // Setup the vertex shader
        const D3D11_INPUT_ELEMENT_DESC VertexDescription[] = {
            { "Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(HASWVertex, Pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "Color", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(HASWVertex, C), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(HASWVertex, U), D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        Ptr<VertexShader> vs = *new VertexShader(&RenderParams, (void*)SimpleTexturedQuad_vs, sizeof(SimpleTexturedQuad_vs), SimpleTexturedQuad_vs_refl, OVR_ARRAY_COUNT(SimpleTexturedQuad_vs_refl));
        pVertexInputLayout = NULL; // Make sure it's cleared in case it wasn't.
        ID3D11InputLayout** ppD3DInputLayout = &pVertexInputLayout.GetRawRef();
        HRESULT hResult = RenderParams.pDevice->CreateInputLayout(VertexDescription, OVR_ARRAY_COUNT(VertexDescription), SimpleTexturedQuad_vs, sizeof(SimpleTexturedQuad_vs), ppD3DInputLayout);
        OVR_ASSERT(SUCCEEDED(hResult));
        if (SUCCEEDED(hResult))
            pShaderSet->SetShader(vs);

        // Setup the pixel shader
        Ptr<PixelShader> ps = *new PixelShader(&RenderParams, (void*)SimpleTexturedQuad_ps, sizeof(SimpleTexturedQuad_ps), SimpleTexturedQuad_ps_refl, OVR_ARRAY_COUNT(SimpleTexturedQuad_ps_refl));
        pShaderSet->SetShader(ps);

        if (!pBlendState)
        {
            D3D11_BLEND_DESC bm;
            memset(&bm, 0, sizeof(bm));
            bm.RenderTarget[0].BlendEnable = TRUE;
            bm.RenderTarget[0].BlendOp = bm.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            bm.RenderTarget[0].SrcBlend = bm.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
            bm.RenderTarget[0].DestBlend = bm.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            bm.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

            RenderParams.pDevice->CreateBlendState(&bm, &pBlendState.GetRawRef());
        }

        if (!pRasterizerState)
        {
            D3D11_RASTERIZER_DESC rs;
            memset(&rs, 0, sizeof(rs));
            rs.AntialiasedLineEnable = true;
            rs.CullMode = D3D11_CULL_BACK;
            rs.DepthClipEnable = true;
            rs.FillMode = D3D11_FILL_SOLID;

            RenderParams.pDevice->CreateRasterizerState(&rs, &pRasterizerState.GetRawRef());
        }
    }

    if (!pVB)
    {
        pVB = *new Buffer(&RenderParams);

        if (pVB)
        {
            const size_t vertexCount = 4;

            pVB->Data(Buffer_Vertex, NULL, vertexCount * sizeof(HASWVertex));
            HASWVertex* pVertices = (HASWVertex*)pVB->Map(0, vertexCount * sizeof(HASWVertex), Map_Discard);
            OVR_ASSERT(pVertices);

            if (pVertices)
            {
                const bool  flip = ((RenderState.DistortionCaps & ovrDistortionCap_FlipInput) != 0);
                const float left = -1.0f; // We currently draw this in normalized device coordinates with an stereo translation
                const float top = -1.1f; // applied as a vertex shader uniform. In the future when we have a more formal graphics
                const float right = 1.0f; // API abstraction we may move this draw to an overlay layer or to a more formal 
                const float bottom = 0.9f; // model/mesh scheme with a perspective projection.

                // See warning in LoadTextureTgaData() about this TGA being loaded "upside down", i.e. UV origin is at bottom-left.
                pVertices[0] = HASWVertex(left, top, 0.f, Color(255, 255, 255, 255), 0.f, flip ? 1.f : 0.f);
                pVertices[1] = HASWVertex(left, bottom, 0.f, Color(255, 255, 255, 255), 0.f, flip ? 0.f : 1.f);
                pVertices[2] = HASWVertex(right, top, 0.f, Color(255, 255, 255, 255), 1.f, flip ? 1.f : 0.f);
                pVertices[3] = HASWVertex(right, bottom, 0.f, Color(255, 255, 255, 255), 1.f, flip ? 0.f : 1.f);

                pVB->Unmap(pVertices);
            }
        }
    }
}


// Note: If we are drawing this warning onto the eye texture before distortion, the "time warp" functionality
// will cause the warning to shake on the screen when the user moves their head. One solution is to disable
// time warping while the warning or any screen-static GUI elements are present.

void HSWDisplay::RenderInternal(ovrEyeType eye, const ovrTexture* eyeTexture)
{
    if (RenderEnabled && eyeTexture)
    {
        // We need to render to the eyeTexture with the texture viewport.
        // Setup rendering to the texture.
        ovrD3D11Texture* eyeTextureD3D = const_cast<ovrD3D11Texture*>(reinterpret_cast<const ovrD3D11Texture*>(eyeTexture));
        OVR_ASSERT(eyeTextureD3D->Texture.Header.API == ovrRenderAPI_D3D11);

        // Load the graphics if not loaded already.
        if (!pVB)
            LoadGraphics();

        // Calculate ortho projection.
        GetOrthoProjection(RenderState, OrthoProjection);

        // Save settings
        // To do: Merge this saved state with that done by DistortionRenderer::GraphicsState::Save(), and put them in a shared location.
        Ptr<ID3D11BlendState> pBlendStateSaved;
        FLOAT blendFactorSaved[4];
        UINT blendSampleMaskSaved;
        RenderParams.pContext->OMGetBlendState(&pBlendStateSaved.GetRawRef(), blendFactorSaved, &blendSampleMaskSaved);

        Ptr<ID3D11RasterizerState> pRasterizerStateSaved;
        RenderParams.pContext->RSGetState(&pRasterizerStateSaved.GetRawRef());

        Ptr<ID3D11RenderTargetView> pTextureRenderTargetViewSaved;
        Ptr<ID3D11DepthStencilView> pDepthStencilViewSaved;
        RenderParams.pContext->OMGetRenderTargets(1, &pTextureRenderTargetViewSaved.GetRawRef(), &pDepthStencilViewSaved.GetRawRef());

        D3D11_VIEWPORT d3dViewportSaved[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        UINT viewportCountSaved = OVR_ARRAY_COUNT(d3dViewportSaved);
        RenderParams.pContext->RSGetViewports(&viewportCountSaved, d3dViewportSaved);

        UINT stencilRefSaved;
        Ptr<ID3D11DepthStencilState> pDepthStencilStateSaved;
        RenderParams.pContext->OMGetDepthStencilState(&pDepthStencilStateSaved.GetRawRef(), &stencilRefSaved);

        Ptr<ID3D11InputLayout> pInputLayoutSaved;
        RenderParams.pContext->IAGetInputLayout(&pInputLayoutSaved.GetRawRef());

        Ptr<ID3D11Buffer> pVertexBufferSaved;
        UINT vertexStrideSaved[1];
        UINT vertexOffsetSaved[1];
        RenderParams.pContext->IAGetVertexBuffers(0, 1, &pVertexBufferSaved.GetRawRef(), vertexStrideSaved, vertexOffsetSaved);

        D3D11_PRIMITIVE_TOPOLOGY topologySaved;
        RenderParams.pContext->IAGetPrimitiveTopology(&topologySaved);


        // Set our settings
        RenderParams.pContext->OMSetBlendState(pBlendState, NULL, 0xffffffff);
        RenderParams.pContext->RSSetState(pRasterizerState);

        // We can't necessarily use a NULL D3D11_RENDER_TARGET_VIEW_DESC argument to CreateRenderTargetView, because we are rendering to
        // a texture that somebody else created and which may have been created in a typeless format (e.g. DXGI_FORMAT_R8G8B8A8_TYPELESS).
        // So what we do is check to see if the texture format is a typeless format and if see we pass a suitable D3D11_RENDER_TARGET_VIEW_DESC
        // to CreateRenderTargetView instead of NULL.
        D3D11_TEXTURE2D_DESC texture2DDesc;
        eyeTextureD3D->D3D11.pTexture->GetDesc(&texture2DDesc);

        D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
        memset(&renderTargetViewDesc, 0, sizeof(renderTargetViewDesc));
        renderTargetViewDesc.Format = GetFullyTypedDXGIFormat(texture2DDesc.Format); // DXGI_FORMAT. If this is a typeless format then GetFullyTypedFormat converts it to a fully typed format. 
        renderTargetViewDesc.ViewDimension = (texture2DDesc.SampleDesc.Count > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
        renderTargetViewDesc.Texture2D.MipSlice = 0;
        Ptr<ID3D11RenderTargetView> pTextureRenderTargetView;
        HRESULT hResult = RenderParams.pDevice->CreateRenderTargetView(eyeTextureD3D->D3D11.pTexture, (renderTargetViewDesc.Format == texture2DDesc.Format) ? NULL : &renderTargetViewDesc, &pTextureRenderTargetView.GetRawRef());

        if (SUCCEEDED(hResult))
        {
            RenderParams.pContext->OMSetRenderTargets(1, &pTextureRenderTargetView.GetRawRef(), NULL); // We currently don't bind a depth buffer.

            D3D11_VIEWPORT D3DViewport;

            OVR_DISABLE_MSVC_WARNING(4244) // conversion from int to float
            D3DViewport.TopLeftX = eyeTextureD3D->Texture.Header.RenderViewport.Pos.x;
            D3DViewport.TopLeftY = eyeTextureD3D->Texture.Header.RenderViewport.Pos.y;
            D3DViewport.Width = eyeTextureD3D->Texture.Header.RenderViewport.Size.w;
            D3DViewport.Height = eyeTextureD3D->Texture.Header.RenderViewport.Size.h;
            D3DViewport.MinDepth = 0;
            D3DViewport.MaxDepth = 1;
            RenderParams.pContext->RSSetViewports(1, &D3DViewport);
            OVR_RESTORE_MSVC_WARNING()

            // We don't set up a world/view/projection matrix because we are using 
            // normalized device coordinates below.

            // We don't set the depth state because we aren't using it.
            //     RenderParams.pContext->OMSetDepthStencilState(<depth state>, 0);

            ShaderFill fill(pShaderSet);
            fill.SetInputLayout(pVertexInputLayout);
            if (pTexture)
                fill.SetTexture(0, pTexture, Shader_Pixel);

            const float scale = HSWDISPLAY_SCALE * ((RenderState.OurHMDInfo.HmdType == HmdType_DK1) ? 0.70f : 1.f);
            pShaderSet->SetUniform2f("Scale", scale, scale / 2.f); // X and Y scale. Y is a fixed proportion to X in order to give a certain aspect ratio.
            pShaderSet->SetUniform4f("Color", 1.f, 1.f, 1.f, 1.f);
            pShaderSet->SetUniform2f("PositionOffset", OrthoProjection[eye].GetTranslation().x, 0.0f);

            RenderParams.pContext->IASetInputLayout((ID3D11InputLayout*)fill.GetInputLayout());

            ID3D11Buffer* vertexBuffer = pVB->GetBuffer();
            UINT          vertexStride = sizeof(HASWVertex);
            UINT          vertexOffset = 0;
            RenderParams.pContext->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);

            ShaderBase*    vShaderBase = (ShaderBase*)pShaderSet->GetShader(OVR::CAPI::D3D11::Shader_Vertex);
            unsigned char* vertexData = vShaderBase->UniformData;

            if (vertexData)
            {
                UniformBufferArray[OVR::CAPI::D3D11::Shader_Vertex]->Data(OVR::CAPI::D3D11::Buffer_Uniform, vertexData, vShaderBase->UniformsSize);
                vShaderBase->SetUniformBuffer(UniformBufferArray[OVR::CAPI::D3D11::Shader_Vertex]);
            }

            for (int i = (OVR::CAPI::D3D11::Shader_Vertex + 1); i < OVR::CAPI::D3D11::Shader_Count; i++)
            {
                if (pShaderSet->GetShader(i))
                {
                    ((ShaderBase*)pShaderSet->GetShader(i))->UpdateBuffer(UniformBufferArray[i]);
                    ((ShaderBase*)pShaderSet->GetShader(i))->SetUniformBuffer(UniformBufferArray[i]);
                }
            }

            RenderParams.pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            fill.Set(Prim_TriangleStrip);

            RenderParams.pContext->Draw(4, 0);
        }
        else
        {
            HSWDISPLAY_LOG(("[HSWDisplay D3D11] CreateRenderTargetView() failed"));
        }


        // Restore settings
        RenderParams.pContext->IASetPrimitiveTopology(topologySaved);
        RenderParams.pContext->IASetVertexBuffers(0, 1, &pVertexBufferSaved.GetRawRef(), &vertexStrideSaved[0], &vertexOffsetSaved[0]);
        RenderParams.pContext->IASetInputLayout(pInputLayoutSaved);
        RenderParams.pContext->OMSetDepthStencilState(pDepthStencilStateSaved, stencilRefSaved);
        RenderParams.pContext->RSSetViewports(viewportCountSaved, d3dViewportSaved);
        RenderParams.pContext->OMSetRenderTargets(1, &pTextureRenderTargetViewSaved.GetRawRef(), pDepthStencilViewSaved);
        RenderParams.pContext->RSSetState(pRasterizerStateSaved);
        RenderParams.pContext->OMSetBlendState(pBlendStateSaved, blendFactorSaved, blendSampleMaskSaved);
    }
}

}}} // namespace OVR::CAPI::D3D11
