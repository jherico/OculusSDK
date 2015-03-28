/************************************************************************************

Filename    :   CAPI_D3D11_Util.cpp
Content     :   D3DX11 utility classes for rendering
Created     :   September 10, 2012
Authors     :   Andrew Reisse

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

#include "CAPI_D3D11_Util.h"

namespace OVR { namespace CAPI { namespace D3D11 {


//-------------------------------------------------------------------------------------
// ***** ShaderFill

void ShaderFill::Set(PrimitiveType prim) const
{
    Shaders->Set(prim);

	for(int i = 0; i < 8; ++i)
    {
        if ( VsTextures[i] != NULL )
        {
		    VsTextures[i]->Set(i, Shader_Vertex);
        }
    }

	for(int i = 0; i < 8; ++i)
    {
        if ( CsTextures[i] != NULL )
        {
		    CsTextures[i]->Set(i, Shader_Compute);
        }
    }

	for(int i = 0; i < 8; ++i)
    {
        if ( PsTextures[i] != NULL )
        {
		    PsTextures[i]->Set(i, Shader_Fragment);
        }
    }
}


//-------------------------------------------------------------------------------------
// ***** Buffer

Buffer::~Buffer()
{
}

bool Buffer::Data(int use, const void *buffer, size_t size, int computeBufferStride /*=-1*/)
{
    HRESULT hr;

    if (D3DBuffer && Size >= size)
    {
        if (Dynamic)
        {
            if (!buffer)
                return true;

            void* v = Map(0, size, Map_Discard);
            if (v)
            {
                memcpy(v, buffer, size);
                Unmap(v);
                return true;
            }
        }
        else
        {
            OVR_ASSERT (!(use & Buffer_ReadOnly));
            pParams->pContext->UpdateSubresource(D3DBuffer, 0, NULL, buffer, 0, 0);
            return true;
        }
    }
    if (D3DBuffer)
    {
        D3DBuffer = NULL;
        Size = 0;
        Use = 0;
        Dynamic = false;
    }
    D3DSrv = NULL;
    D3DUav = NULL;

    D3D11_BUFFER_DESC desc;
    memset(&desc, 0, sizeof(desc));
    if (use & Buffer_ReadOnly)
    {
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.CPUAccessFlags = 0;
    }
    else
    {
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Dynamic = true;
    }

    switch(use & Buffer_TypeMask)
    {
    case Buffer_Vertex:  desc.BindFlags = D3D11_BIND_VERTEX_BUFFER; break;
    case Buffer_Index:   desc.BindFlags = D3D11_BIND_INDEX_BUFFER;  break;
    case Buffer_Uniform:
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        size = ((size + 15) & ~15);
        break;
    case Buffer_Compute:
        // There's actually a bunch of options for buffers bound to a CS.
        // Right now this is the most appropriate general-purpose one. Add more as needed.

        // NOTE - if you want D3D11_(CPU_ACCESS_WRITE), it MUST be either D3D11_(USAGE_DYNAMIC) or D3D11_(USAGE_STAGING).
        // TODO: we want a resource that is rarely written to, in which case we'd need two surfaces - one a STAGING
        // that the CPU writes to, and one a DEFAULT, and we CopyResource from one to the other. Hassle!
        // Setting it as D3D11_(USAGE_DYNAMIC) will get the job done for now.
        // Also for fun - you can't have a D3D11_(USAGE_DYNAMIC) buffer that is also a D3D11_(BIND_UNORDERED_ACCESS).
        OVR_ASSERT ( !(use & Buffer_ReadOnly) );
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.Usage     = D3D11_USAGE_DYNAMIC;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        OVR_ASSERT ( computeBufferStride > 0 );
        desc.StructureByteStride = computeBufferStride; // sizeof(DistortionComputePin);

        Dynamic = true;
        size = ((size + 15) & ~15);
        break;
    }

    desc.ByteWidth = (unsigned)size;

    D3D11_SUBRESOURCE_DATA sr;
    sr.pSysMem = buffer;
    sr.SysMemPitch = 0;
    sr.SysMemSlicePitch = 0;

    D3DBuffer = NULL;
    hr = pParams->pDevice->CreateBuffer(&desc, buffer ? &sr : NULL, &D3DBuffer.GetRawRef());
    OVR_D3D_CHECK_RET_FALSE(hr);

    Use = 0;
    Size = 0;

    if ( ( use & Buffer_TypeMask ) == Buffer_Compute )
    {
        hr = pParams->pDevice->CreateShaderResourceView ( D3DBuffer, NULL, &D3DSrv.GetRawRef() );
        OVR_D3D_CHECK_RET_FALSE(hr);

#if 0           // Right now we do NOT ask for UAV access (see flags above).
        hr = Ren->Device->CreateUnorderedAccessView ( D3DBuffer, NULL, &D3DUav.GetRawRef() );
        OVR_D3D_CHECK_RET_FALSE(hr);
#endif
    }

    Use = use;
    Size = desc.ByteWidth;

    return true;

}

void*  Buffer::Map(size_t start, size_t size, int flags)
{
    OVR_UNUSED(size);

    D3D11_MAP mapFlags = D3D11_MAP_WRITE;
    if (flags & Map_Discard)    
        mapFlags = D3D11_MAP_WRITE_DISCARD;    
    if (flags & Map_Unsynchronized)    
        mapFlags = D3D11_MAP_WRITE_NO_OVERWRITE;

    D3D11_MAPPED_SUBRESOURCE map;
    if (SUCCEEDED(pParams->pContext->Map(D3DBuffer, 0, mapFlags, 0, &map)))
        return ((char*)map.pData) + start;

    return NULL;
}

bool   Buffer::Unmap(void *m)
{
    OVR_UNUSED(m);

    pParams->pContext->Unmap(D3DBuffer, 0);
    return true;
}


//-------------------------------------------------------------------------------------
// Shaders

template<> bool ShaderImpl<Shader_Vertex, ID3D11VertexShader>::Load(void* shader, size_t size)
{
    HRESULT hr = pParams->pDevice->CreateVertexShader(shader, size, nullptr, &D3DShader);
    OVR_D3D_CHECK_RET_FALSE(hr);
    return true;
}
template<> bool ShaderImpl<Shader_Pixel, ID3D11PixelShader>::Load(void* shader, size_t size)
{
    HRESULT hr = pParams->pDevice->CreatePixelShader(shader, size, nullptr, &D3DShader);
    OVR_D3D_CHECK_RET_FALSE(hr);
    return true;
}
template<> bool ShaderImpl<Shader_Compute, ID3D11ComputeShader>::Load(void* shader, size_t size)
{
    HRESULT hr = pParams->pDevice->CreateComputeShader(shader, size, nullptr, &D3DShader);
    OVR_D3D_CHECK_RET_FALSE(hr);
    return true;
}

template<> void ShaderImpl<Shader_Vertex, ID3D11VertexShader>::Set(PrimitiveType) const
{
    pParams->pContext->VSSetShader(D3DShader, nullptr, 0);
}
template<> void ShaderImpl<Shader_Pixel, ID3D11PixelShader>::Set(PrimitiveType) const
{
    pParams->pContext->PSSetShader(D3DShader, nullptr, 0);
}
template<> void ShaderImpl<Shader_Compute, ID3D11ComputeShader>::Set(PrimitiveType) const
{
    pParams->pContext->CSSetShader(D3DShader, nullptr, 0);
}

template<> void ShaderImpl<Shader_Vertex, ID3D11VertexShader>::SetUniformBuffer(Buffer* buffer, int i)
{
    pParams->pContext->VSSetConstantBuffers(i, 1, &((Buffer*)buffer)->D3DBuffer.GetRawRef());
}
template<> void ShaderImpl<Shader_Pixel, ID3D11PixelShader>::SetUniformBuffer(Buffer* buffer, int i)
{
    pParams->pContext->PSSetConstantBuffers(i, 1, &((Buffer*)buffer)->D3DBuffer.GetRawRef());
}
template<> void ShaderImpl<Shader_Compute, ID3D11ComputeShader>::SetUniformBuffer(Buffer* buffer, int i)
{
    pParams->pContext->CSSetConstantBuffers(i, 1, &((Buffer*)buffer)->D3DBuffer.GetRawRef());
}

//-------------------------------------------------------------------------------------
// ***** Shader Base

ShaderBase::ShaderBase(RenderParams* rp, ShaderStage stage) :
    Shader(stage),
    pParams(rp),
    UniformData(NULL),
    UniformsSize(0),
    UniformRefl(NULL),
    UniformReflSize(0)
{
}

ShaderBase::~ShaderBase()
{
    if (UniformData)
    {
        OVR_FREE(UniformData);
        UniformData = NULL;
    }

    // UniformRefl does not need to be freed
    UniformRefl = NULL;
}

bool ShaderBase::SetUniform(const char* name, int n, const float* v)
{
    for(unsigned i = 0; i < UniformReflSize; i++)
    {
        if (!strcmp(UniformRefl[i].Name, name))
        {
            memcpy(UniformData + UniformRefl[i].Offset, v, n * sizeof(float));
            return 1;
        }
    }
    return 0;
}

bool ShaderBase::SetUniformBool(const char* name, int n, const bool* v) 
{
    OVR_UNUSED(n);
    for(unsigned i = 0; i < UniformReflSize; i++)
    {
        if (!strcmp(UniformRefl[i].Name, name))
        {
            memcpy(UniformData + UniformRefl[i].Offset, v, UniformRefl[i].Size);
            return 1;
        }
    }
    return 0;
}

void ShaderBase::InitUniforms(const Uniform* refl, size_t reflSize)
{
    UniformsSize = 0;
    if (UniformData)
    {
        OVR_FREE(UniformData);
        UniformData = 0;
    }

    if (!refl)
    {
        UniformRefl = NULL;
        UniformReflSize = 0;
        return; // no reflection data
    }

    UniformRefl = refl;
    UniformReflSize = reflSize;
    
    UniformsSize = UniformRefl[UniformReflSize-1].Offset + UniformRefl[UniformReflSize-1].Size;
    UniformData = (unsigned char*)OVR_ALLOC(UniformsSize);
}

void ShaderBase::UpdateBuffer(Buffer* buf)
{
    if (UniformsSize)
    {
        buf->Data(Buffer_Uniform, UniformData, UniformsSize);
    }
}


//-------------------------------------------------------------------------------------
// ***** Texture
// 
Texture::Texture(RenderParams* rp, int fmt, const Sizei texSize,
                 ID3D11SamplerState* sampler, int samples)
    : pParams(rp), Tex(NULL), TexSv(NULL), TexRtv(NULL), TexDsv(NULL),
    TextureSize(texSize),
    Sampler(sampler),
    Samples(samples)
{
    OVR_UNUSED(fmt);    
}


Texture::Texture(RenderParams* rp, int format, const Sizei texSize,
    ID3D11SamplerState* sampler, const void* data, int mipcount)
    : pParams(rp), Tex(NULL), TexSv(NULL), TexRtv(NULL), TexDsv(NULL),
    TextureSize(texSize),
    Sampler(sampler),
    Samples(1)
{
    OVR_ASSERT(rp->pDevice != NULL);

    OVR_UNUSED(mipcount);

    //if (format == Texture_DXT1 || format == Texture_DXT3 || format == Texture_DXT5)
    //{
    //    int convertedFormat;
    //    switch (format)
    //    {
    //    case Texture_DXT1:  convertedFormat = DXGI_FORMAT_BC1_UNORM;    break;
    //    case Texture_DXT3:  convertedFormat = DXGI_FORMAT_BC2_UNORM;    break;
    //    case Texture_DXT5:
    //    default:            convertedFormat = DXGI_FORMAT_BC3_UNORM;    break;
    //    }
    //    unsigned largestMipWidth   = 0;
    //    unsigned largestMipHeight  = 0;
    //    unsigned effectiveMipCount = mipcount;
    //    unsigned textureSize       = 0;

    //    D3D11_SUBRESOURCE_DATA* subresData =
    //        (D3D11_SUBRESOURCE_DATA*) OVR_ALLOC(sizeof(D3D11_SUBRESOURCE_DATA) * mipcount);
    //    GenerateSubresourceData(width, height, convertedFormat, imageDimUpperLimit, data, subresData, largestMipWidth,
    //        largestMipHeight, textureSize, effectiveMipCount);
    //    TotalTextureMemoryUsage += textureSize;

    //    if (!Device || !subresData)
    //    {
    //        return NULL;
    //    }

    //    Texture* NewTex = new Texture(this, format, largestMipWidth, largestMipHeight);
    //    // BCn/DXTn - no AA.
    //    Samples = 1;

    //    D3D11_TEXTURE2D_DESC desc;
    //    desc.Width      = largestMipWidth;
    //    desc.Height     = largestMipHeight;
    //    desc.MipLevels  = effectiveMipCount;
    //    desc.ArraySize  = 1;
    //    desc.Format     = static_cast<DXGI_FORMAT>(convertedFormat);
    //    desc.SampleDesc.Count = 1;
    //    desc.SampleDesc.Quality = 0;
    //    desc.Usage      = D3D11_USAGE_DEFAULT;
    //    desc.BindFlags  = D3D11_BIND_SHADER_RESOURCE;
    //    desc.CPUAccessFlags = 0;
    //    desc.MiscFlags  = 0;

    //    Tex = NULL;
    //    HRESULT hr = Device->CreateTexture2D(&desc, static_cast<D3D11_SUBRESOURCE_DATA*>(subresData),
    //        &Tex.GetRawRef());
    //    OVR_FREE(subresData);
    //    if (FAILED(hr))
    //    {
    //        OVR_LOG_COM_ERROR(hr);
    //    }

    //    if (SUCCEEDED(hr) && NewTex != 0)
    //    {
    //        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    //        memset(&SRVDesc, 0, sizeof(SRVDesc));
    //        SRVDesc.Format = static_cast<DXGI_FORMAT>(format);
    //        SRVDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
    //        SRVDesc.Texture2D.MipLevels = desc.MipLevels;

    //        TexSv = NULL;
    //        hr = Device->CreateShaderResourceView(Tex, NULL, &TexSv.GetRawRef());

    //        if (FAILED(hr))
    //        {
    //            OVR_LOG_COM_ERROR(hr);
    //            Release();
    //            return NULL;
    //        }
    //        return NewTex;
    //    }

    //    return NULL;
    //}
    //else
    {
        int samples = (format & Texture_SamplesMask);
        if (samples < 1)
        {
            samples = 1;
        }

        bool createDepthSrv = (format & Texture_SampleDepth) > 0;

        DXGI_FORMAT d3dformat;
        int         bpp;
        switch(format & Texture_TypeMask)
        {
            //case Texture_BGRA:
            //    bpp = 4;
            //    d3dformat = (format & Texture_SRGB) ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
            //    break;
        case Texture_RGBA:
            bpp = 4;
            //d3dformat = (format & Texture_SRGB) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
            d3dformat = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
            //case Texture_R:
            //    bpp = 1;
            //    d3dformat = DXGI_FORMAT_R8_UNORM;
            //    break;
            //case Texture_A:
            //    bpp = 1;
            //    d3dformat = DXGI_FORMAT_A8_UNORM;
            //    break;
        case Texture_Depth:
            bpp = 0;
            d3dformat = createDepthSrv ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_D32_FLOAT;
            break;
        default:
            bpp = 4;
            d3dformat = DXGI_FORMAT_R8G8B8A8_UNORM;
            OVR_ASSERT(0);
        }

        D3D11_TEXTURE2D_DESC dsDesc;
        dsDesc.Width     = texSize.w;
        dsDesc.Height    = texSize.h;
        dsDesc.MipLevels = (format == (Texture_RGBA | Texture_GenMipmaps) && data) ? GetNumMipLevels(texSize.w, texSize.h) : 1;
        dsDesc.ArraySize = 1;
        dsDesc.Format    = d3dformat;
        dsDesc.SampleDesc.Count = samples;
        dsDesc.SampleDesc.Quality = 0;
        dsDesc.Usage     = D3D11_USAGE_DEFAULT;
        dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        dsDesc.CPUAccessFlags = 0;
        dsDesc.MiscFlags      = 0;

        if (format & Texture_RenderTarget)
        {
            if ((format & Texture_TypeMask) == Texture_Depth)
            {
                dsDesc.BindFlags = createDepthSrv ? (dsDesc.BindFlags | D3D11_BIND_DEPTH_STENCIL) : D3D11_BIND_DEPTH_STENCIL;
            }
            else
            {
                dsDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
            }
        }

        Tex = NULL;
        HRESULT hr = rp->pDevice->CreateTexture2D(&dsDesc, NULL, &Tex.GetRawRef());
        if (FAILED(hr))
        {
            OVR_ASSERT(0);
            //OVR_DEBUG_LOG_TEXT(("Failed to create 2D D3D texture."));
            Release();
            return;
        }
        if (dsDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
        {
            if((dsDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL) > 0 && createDepthSrv)
            {
                D3D11_SHADER_RESOURCE_VIEW_DESC depthSrv;
                depthSrv.Format = DXGI_FORMAT_R32_FLOAT;
                depthSrv.ViewDimension = samples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
                depthSrv.Texture2D.MostDetailedMip = 0;
                depthSrv.Texture2D.MipLevels = dsDesc.MipLevels;
                TexSv = NULL;
                hr = rp->pDevice->CreateShaderResourceView(Tex, &depthSrv, &TexSv.GetRawRef());
                if (FAILED(hr))
                {
                    OVR_ASSERT(0);
                }
            }
            else
            {
                TexSv = NULL;
                hr = rp->pDevice->CreateShaderResourceView(Tex, NULL, &TexSv.GetRawRef());
                if (FAILED(hr))
                {
                    OVR_ASSERT(0);
                }
            }
        }

        if (data)
        {
            rp->pContext->UpdateSubresource(Tex, 0, NULL, data, texSize.w * bpp, texSize.w * texSize.h * bpp);
            if (format == (Texture_RGBA | Texture_GenMipmaps))
            {
                int srcw = texSize.w, srch = texSize.h;
                int level = 0;
                uint8_t* mipmaps = NULL;
                do
                {
                    level++;
                    int mipw = srcw >> 1;
                    if (mipw < 1)
                    {
                        mipw = 1;
                    }
                    int miph = srch >> 1;
                    if (miph < 1)
                    {
                        miph = 1;
                    }
                    if (mipmaps == NULL)
                    {
                        mipmaps = (uint8_t*)OVR_ALLOC(mipw * miph * 4);
                    }
                    FilterRgba2x2(level == 1 ? (const uint8_t*)data : mipmaps, srcw, srch, mipmaps);
                    rp->pContext->UpdateSubresource(Tex, level, NULL, mipmaps, mipw * bpp, miph * bpp);
                    srcw = mipw;
                    srch = miph;
                }
                while(srcw > 1 || srch > 1);

                if (mipmaps != NULL)
                {
                    OVR_FREE(mipmaps);
                }
            }
        }

        if (format & Texture_RenderTarget)
        {
            if ((format & Texture_TypeMask) == Texture_Depth)
            {
                D3D11_DEPTH_STENCIL_VIEW_DESC depthDsv;
                ZeroMemory(&depthDsv, sizeof(depthDsv));
                depthDsv.Format = DXGI_FORMAT_D32_FLOAT;
                depthDsv.ViewDimension = samples > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
                depthDsv.Texture2D.MipSlice = 0;
                TexDsv = NULL;
                hr = rp->pDevice->CreateDepthStencilView(Tex, createDepthSrv ? &depthDsv : NULL, &TexDsv.GetRawRef());
                if (FAILED(hr))
                {
                    OVR_ASSERT(0);
                }
            }
            else
            {
                TexRtv = NULL;
                hr = rp->pDevice->CreateRenderTargetView(Tex, NULL, &TexRtv.GetRawRef());
                if (FAILED(hr))
                {
                    OVR_ASSERT(0);
                }
            }
        }
    }
}

Texture::~Texture()
{
}

void Texture::Set(int slot, ShaderStage stage) const
{    
    ID3D11ShaderResourceView* texSv = TexSv.GetPtr();

    switch(stage)
    {
    case Shader_Fragment:
        pParams->pContext->PSSetShaderResources(slot, 1, &texSv);
        pParams->pContext->PSSetSamplers(slot, 1, &Sampler.GetRawRef());        
        break;

    case Shader_Vertex:
        pParams->pContext->VSSetShaderResources(slot, 1, &texSv);
        pParams->pContext->VSSetSamplers(slot, 1, &Sampler.GetRawRef());
        break;

    case Shader_Compute:
        pParams->pContext->CSSetShaderResources(slot, 1, &texSv);
        pParams->pContext->CSSetSamplers(slot, 1, &Sampler.GetRawRef());
        break;

    default: OVR_ASSERT ( false ); break;
    }
}


//-------------------------------------------------------------------------------------
// ***** GpuTimer
// 
#define D3DQUERY_EXEC(_context_, _query_, _command_, ...)  _context_->_command_(_query_, __VA_ARGS__)


void GpuTimer::Init(ID3D11Device* device, ID3D11DeviceContext* content)
{
    D3dDevice = device;
    Context = content;    
}

void GpuTimer::BeginQuery()
{
    HRESULT hr;

    if(GotoNextFrame(LastQueuedFrame) == LastTimedFrame)
    {
        OVR_ASSERT(false); // too many queries queued
        return;
    }

    LastQueuedFrame = GotoNextFrame(LastQueuedFrame);

    GpuQuerySets& newQuerySet = QuerySets[LastQueuedFrame];
    if(newQuerySet.DisjointQuery == NULL)
    {
        // Create the queries
        D3D11_QUERY_DESC desc;
        desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        desc.MiscFlags = 0;
        hr = D3dDevice->CreateQuery(&desc, &newQuerySet.DisjointQuery);
        OVR_D3D_CHECK_RET(hr);

        desc.Query = D3D11_QUERY_TIMESTAMP;
        hr = D3dDevice->CreateQuery(&desc, &newQuerySet.TimeStartQuery);
        OVR_D3D_CHECK_RET(hr);
        hr = D3dDevice->CreateQuery(&desc, &newQuerySet.TimeEndQuery);
        OVR_D3D_CHECK_RET(hr);
    }

    OVR_ASSERT(!newQuerySet.QueryStarted);
    OVR_ASSERT(!newQuerySet.QueryAwaitingTiming);

    
    D3DQUERY_EXEC(Context, QuerySets[LastQueuedFrame].DisjointQuery, Begin, );  // First start a disjoint query
    D3DQUERY_EXEC(Context, QuerySets[LastQueuedFrame].TimeStartQuery, End, );   // Insert start timestamp
    
    newQuerySet.QueryStarted = true;
    newQuerySet.QueryAwaitingTiming = false;
    //newQuerySet.QueryTimed = false;
}

void GpuTimer::EndQuery()
{
    if(LastQueuedFrame > 0 && !QuerySets[LastQueuedFrame].QueryStarted)
        return;

    GpuQuerySets& doneQuerySet = QuerySets[LastQueuedFrame];
    OVR_ASSERT(doneQuerySet.QueryStarted);
    OVR_ASSERT(!doneQuerySet.QueryAwaitingTiming);

    // Insert the end timestamp
    D3DQUERY_EXEC(Context, doneQuerySet.TimeEndQuery, End, );

    // End the disjoint query
    D3DQUERY_EXEC(Context, doneQuerySet.DisjointQuery, End, );

    doneQuerySet.QueryStarted = false;
    doneQuerySet.QueryAwaitingTiming = true;
}

float GpuTimer::GetTiming(bool blockUntilValid)
{
    float time = -1.0f;

    // loop until we hit a query that is not ready yet, or we have read all queued queries
    while(LastTimedFrame != LastQueuedFrame)
    {
        int timeTestFrame = GotoNextFrame(LastTimedFrame);

        GpuQuerySets& querySet = QuerySets[timeTestFrame];

        OVR_ASSERT(!querySet.QueryStarted && querySet.QueryAwaitingTiming);

        UINT64 startTime = 0;
        UINT64 endTime = 0;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;

        if(blockUntilValid)
        {
            while(D3DQUERY_EXEC(Context, querySet.TimeStartQuery, GetData, &startTime, sizeof(startTime), 0) != S_OK);
            while(D3DQUERY_EXEC(Context, querySet.TimeEndQuery, GetData, &endTime, sizeof(endTime), 0) != S_OK);
            while(D3DQUERY_EXEC(Context, querySet.DisjointQuery, GetData, &disjointData, sizeof(disjointData), 0) != S_OK);
        }
        else
        {
// Early return if we fail to get data for any of these
            if(D3DQUERY_EXEC(Context, querySet.TimeStartQuery, GetData, &startTime, sizeof(startTime), 0) != S_OK)    return time;
            if(D3DQUERY_EXEC(Context, querySet.TimeEndQuery, GetData, &endTime, sizeof(endTime), 0) != S_OK)          return time;
            if(D3DQUERY_EXEC(Context, querySet.DisjointQuery, GetData, &disjointData, sizeof(disjointData), 0) != S_OK)    return time;
        }

        querySet.QueryAwaitingTiming = false;
        LastTimedFrame = timeTestFrame; // successfully retrieved the timing data

        if(disjointData.Disjoint == false)
        {
            UINT64 delta = endTime - startTime;
            float frequency = (float)(disjointData.Frequency);
            time = (delta / frequency);
        }
    }
    
    return time;
}

}}} // OVR::CAPI::D3D11
