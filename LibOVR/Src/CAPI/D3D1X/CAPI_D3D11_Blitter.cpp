/************************************************************************************

Filename    :   CAPI_D3D11_Blitter.cpp
Content     :   D3D11 implementation for blitting, supporting scaling & rotation
Created     :   February 24, 2015
Authors     :   Reza Nourai

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

#include "CAPI_D3D11_Blitter.h"

#ifdef OVR_OS_MS

#include "Util/Util_Direct3D.h"
#include "Compositor/Shaders/Blt_vs.h"
#include "Compositor/Shaders/Blt_ps.h"

namespace OVR { namespace CAPI {

//-------------------------------------------------------------------------------------
// ***** CAPI::Blitter

Blitter::Blitter(const std::shared_ptr<GraphicsContext>& graphics)
    : Graphics(graphics)
{
    OVR_ASSERT(graphics);
    OVR_ASSERT(graphics->Device);
}

Blitter::~Blitter()
{
}

OVRError Blitter::Initialize()
{
    Ptr<ID3D11Device1> device1;
    HRESULT hr = Graphics->Device->QueryInterface(IID_PPV_ARGS(&device1.GetRawRef()));
    OVR_HR_CHECK_RET_ERROR(ovrError_DisplayInit, hr, "Blitter QueryInterface");

    UINT deviceFlags = device1->GetCreationFlags();
    D3D_FEATURE_LEVEL featureLevel = device1->GetFeatureLevel();

    // If the device is single threaded, the context state must be too
    UINT stateFlags = 0;
    if (deviceFlags & D3D11_CREATE_DEVICE_SINGLETHREADED)
    {
        stateFlags |= D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED;
    }

    hr = device1->CreateDeviceContextState(stateFlags, &featureLevel, 1, D3D11_SDK_VERSION, __uuidof(ID3D11Device1), nullptr, &BltState.GetRawRef());
    OVR_HR_CHECK_RET_ERROR(ovrError_DisplayInit, hr, "Blitter CreateDeviceContextState");
    OVR_D3D_TAG_OBJECT(BltState);

    hr = device1->CreateVertexShader(Blt_vs, sizeof(Blt_vs), nullptr, &VS.GetRawRef());
    OVR_HR_CHECK_RET_ERROR(ovrError_DisplayInit, hr, "Blitter CreateVertexShader");
    OVR_D3D_TAG_OBJECT(VS);

    hr = device1->CreatePixelShader(Blt_ps, sizeof(Blt_ps), nullptr, &PS.GetRawRef());
    OVR_HR_CHECK_RET_ERROR(ovrError_DisplayInit, hr, "Blitter CreatePixelShader");
    OVR_D3D_TAG_OBJECT(PS);

    D3D11_INPUT_ELEMENT_DESC elems[2] = {};
    elems[0].Format = DXGI_FORMAT_R32G32_FLOAT;
    elems[0].SemanticName = "POSITION";
    elems[1].AlignedByteOffset = sizeof(float) * 2;
    elems[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    elems[1].SemanticName = "TEXCOORD";

    hr = device1->CreateInputLayout(elems, _countof(elems), Blt_vs, sizeof(Blt_vs), &IL.GetRawRef());
    OVR_HR_CHECK_RET_ERROR(ovrError_DisplayInit, hr, "Blitter CreateInputLayout");
    OVR_D3D_TAG_OBJECT(IL);

    // Quad with texcoords designed to rotate the source 90deg clockwise
    BltVertex vertices[] =
    {
        { -1, 1, 0, 0 },
        { 1, 1, 1, 0 },
        { 1, -1, 1, 1 },
        { -1, 1, 0, 0 },
        { 1, -1, 1, 1 },
        { -1, -1, 0, 1 }
    };

    BltVertex verticesRotated[] =
    {
        { -1, 1, 0, 1 },
        { 1, 1, 0, 0 },
        { 1, -1, 1, 0 },
        { -1, 1, 0, 1 },
        { 1, -1, 1, 0 },
        { -1, -1, 1, 1 }
    };

    D3D11_BUFFER_DESC bd = {};
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = sizeof(vertices);
    bd.StructureByteStride = sizeof(BltVertex);
    bd.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = vertices;
    init.SysMemPitch = sizeof(vertices);
    init.SysMemSlicePitch = init.SysMemPitch;

    hr = device1->CreateBuffer(&bd, &init, &VB.GetRawRef());
    OVR_HR_CHECK_RET_ERROR(ovrError_DisplayInit, hr, "Blitter CreateBuffer VB");
    OVR_D3D_TAG_OBJECT(VB);

    bd.ByteWidth = sizeof(verticesRotated);

    init.pSysMem = verticesRotated;
    init.SysMemPitch = sizeof(verticesRotated);
    init.SysMemSlicePitch = init.SysMemPitch;

    hr = device1->CreateBuffer(&bd, &init, &VBRotate.GetRawRef());
    OVR_HR_CHECK_RET_ERROR(ovrError_DisplayInit, hr, "Blitter CreateBuffer VBRotate");
    OVR_D3D_TAG_OBJECT(VBRotate);

    D3D11_SAMPLER_DESC ss = {};
    ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    ss.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    ss.MaxLOD = 15;
    hr = device1->CreateSamplerState(&ss, &Sampler.GetRawRef());
    OVR_HR_CHECK_RET_ERROR(ovrError_DisplayInit, hr, "Blitter CreateSamplerState");

    GraphicsContext::SafeContext context = Graphics->LockContext();

    Ptr<ID3D11DeviceContext1> context1;
    context.Context->QueryInterface(IID_PPV_ARGS(&context1.GetRawRef()));

    // Swap to our blt state to set it up
    Ptr<ID3DDeviceContextState> existingState;
    context1->SwapDeviceContextState(BltState, &existingState.GetRawRef());

    context1->IASetInputLayout(IL);
    context1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context1->VSSetShader(VS, nullptr, 0);
    context1->PSSetShader(PS, nullptr, 0);
    context1->PSSetSamplers(0, 1, &Sampler.GetRawRef());

    // Swap back
    context1->SwapDeviceContextState(existingState, nullptr);
    return OVRError::Success();
}

bool Blitter::Blt(ID3D11RenderTargetView* dest, ID3D11ShaderResourceView* source, int rotation)
{
    ID3D11RenderTargetView* nullRTVs[] = { nullptr, nullptr, nullptr, nullptr };
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr, nullptr };

    GraphicsContext::SafeContext context = Graphics->LockContext();

    context.Context->OMSetRenderTargets(_countof(nullRTVs), nullRTVs, nullptr);
    context.Context->PSSetShaderResources(0, _countof(nullSRVs), nullSRVs);

    Ptr<ID3D11DeviceContext1> context1;
    HRESULT hr = context.Context->QueryInterface(IID_PPV_ARGS(&context1.GetRawRef()));
    if (FAILED(hr))
    {
        OVR_ASSERT(false);
        return false;
    }

    // Switch to our state
    Ptr<ID3DDeviceContextState> existingState;
    context1->SwapDeviceContextState(BltState, &existingState.GetRawRef());
    
    context1->OMSetRenderTargets(_countof(nullRTVs), nullRTVs, nullptr);
    context1->PSSetShaderResources(0, _countof(nullSRVs), nullSRVs);

    // Set the mirror as the render target
    context1->OMSetRenderTargets(1, &dest, nullptr);

    Ptr<ID3D11Resource> resource;
    dest->GetResource(&resource.GetRawRef());
    Ptr<ID3D11Texture2D> texture;
    hr = resource->QueryInterface(IID_PPV_ARGS(&texture.GetRawRef()));
    if (FAILED(hr))
    {
        OVR_ASSERT(false);
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)desc.Width;
    vp.Height = (float)desc.Height;
    vp.MaxDepth = 1.0f;
    context1->RSSetViewports(1, &vp);

    context1->PSSetShaderResources(0, 1, &source);

    const bool rotate = (rotation == 90) || (rotation == 270);

    static const uint32_t stride = sizeof(BltVertex);
    static const uint32_t offset = 0;
    context1->IASetVertexBuffers(0, 1,
        rotate ? &VBRotate.GetRawRef() : &VB.GetRawRef(),
        &stride, &offset);

    context1->Draw(6, 0);

    context1->OMSetRenderTargets(_countof(nullRTVs), nullRTVs, nullptr);
    context1->PSSetShaderResources(0, _countof(nullSRVs), nullSRVs);

    // Switch back to app state
    context1->SwapDeviceContextState(existingState, nullptr);

    return true;
}

}} // namespace OVR::CAPI

#endif // OVR_OS_MS
