/************************************************************************************

Filename    :   CAPI_D3D11_Util.h
Content     :   D3D11 utility classes for rendering
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

#ifndef OVR_CAPI_D3D11_Util_h
#define OVR_CAPI_D3D11_Util_h

#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_RefCount.h"
#include "Extras/OVR_Math.h"
#include "Util/Util_Direct3D.h"
#include <comdef.h> // for _COM_SMARTPTR_TYPEDEF()

namespace OVR { namespace CAPI { namespace D3D11 {

class Buffer;

// Rendering parameters/pointers describing D3DX rendering setup.
struct RenderParams
{
    ID3D11Device*			    pDevice;
    ID3D11DeviceContext*        pContext;
    ID3D11RenderTargetView*     pBackBufferRT;
    ID3D11UnorderedAccessView*  pBackBufferUAV;
    IDXGISwapChain*             pSwapChain;
    Sizei                       BackBufferSize;
    int                         Multisample;
    UINT32                      VidPnTargetId; // display miniport target id for tracing
};


// Rendering primitive type used to render Model.
enum PrimitiveType
{
    Prim_Triangles,
    Prim_Lines,
    Prim_TriangleStrip,
    Prim_Unknown,
    Prim_Count
};

// Types of shaders that can be stored together in a ShaderSet.
enum ShaderStage
{
    Shader_Vertex   = 0,
    Shader_Fragment = 2,
    Shader_Pixel    = 2,
    Shader_Compute  = 3,        // DX11+ only
    Shader_Count    = 4,
};

enum MapFlags
{
    Map_Discard        = 1,
    Map_Read           = 2, // do not use
    Map_Unsynchronized = 4, // like D3D11_MAP_NO_OVERWRITE
};


// Buffer types used for uploading geometry & constants.
enum BufferUsage
{
    Buffer_Unknown  = 0,
    Buffer_Vertex   = 1,
    Buffer_Index    = 2,
    Buffer_Uniform  = 4,
    Buffer_Compute  = 8,
    Buffer_TypeMask = 0xff,
    Buffer_ReadOnly = 0x100, // Buffer must be created with Data().
};

enum TextureFormat
{
    Texture_RGBA            = 0x0100,
    Texture_Depth           = 0x8000,
    Texture_TypeMask        = 0xff00,
    Texture_SamplesMask     = 0x00ff,
    Texture_RenderTarget    = 0x10000,
    Texture_SampleDepth		= 0x20000,
    Texture_GenMipmaps      = 0x40000,
};

// Texture sampling modes.
enum SampleMode
{
    Sample_Linear       = 0,
    Sample_Nearest      = 1,
    Sample_Anisotropic  = 2,
    Sample_FilterMask   = 3,

    Sample_Repeat       = 0,
    Sample_Clamp        = 4,
    Sample_ClampBorder  = 8, // If unsupported Clamp is used instead.
    Sample_Mirror       =12,
    Sample_AddressMask  =12,

    Sample_Count        =16,
};

// Base class for vertex and pixel shaders. Stored in ShaderSet.
class Shader : public RefCountBase<Shader>
{
    friend class ShaderSet;

protected:
    ShaderStage Stage;

public:
    Shader(ShaderStage s) : Stage(s) {}
    virtual ~Shader() {}

    ShaderStage GetStage() const { return Stage; }

    virtual void Set(PrimitiveType) const { }
    virtual void SetUniformBuffer(class Buffer* buffers, int i = 0) { OVR_UNUSED2(buffers, i); }

protected:
    virtual bool SetUniform(const char* name, int n, const float* v) { OVR_UNUSED3(name, n, v); return false; }
    virtual bool SetUniformBool(const char* name, int n, const bool* v) { OVR_UNUSED3(name, n, v); return false; }
};



// A group of shaders, one per stage.
// A ShaderSet is applied to a RenderDevice for rendering with a given fill.
class ShaderSet : public RefCountBase<ShaderSet>
{
protected:
    Ptr<Shader> Shaders[Shader_Count];

public:
    ShaderSet() { }
    ~ShaderSet() { }

    virtual void SetShader(Shader *s)
    {
        Shaders[s->GetStage()] = s;
    }
    virtual void UnsetShader(int stage)
    {
        Shaders[stage] = NULL;
    }
    Shader* GetShader(int stage) { return Shaders[stage]; }

    virtual void Set(PrimitiveType prim) const
    {
        for (int i = 0; i < Shader_Count; i++)
            if (Shaders[i])
                Shaders[i]->Set(prim);
    }

    // Set a uniform (other than the standard matrices). It is undefined whether the
    // uniforms from one shader occupy the same space as those in other shaders
    // (unless a buffer is used, then each buffer is independent).     
    virtual bool SetUniform(const char* name, int n, const float* v)
    {
        bool result = 0;
        for (int i = 0; i < Shader_Count; i++)
            if (Shaders[i])
                result |= Shaders[i]->SetUniform(name, n, v);

        return result;
    }
    bool SetUniform1f(const char* name, float x)
    {
        const float v[] = {x};
        return SetUniform(name, 1, v);
    }
    bool SetUniform2f(const char* name, float x, float y)
    {
        const float v[] = {x,y};
        return SetUniform(name, 2, v);
    }
    bool SetUniform3f(const char* name, float x, float y, float z)
    {
        const float v[] = {x,y,z};
        return SetUniform(name, 3, v);
    }
    bool SetUniform4f(const char* name, float x, float y, float z, float w = 1)
    {
        const float v[] = {x,y,z,w};
        return SetUniform(name, 4, v);
    }

    bool SetUniformv(const char* name, const Vector3f& v)
    {
        const float a[] = {v.x,v.y,v.z,1};
        return SetUniform(name, 4, a);
    }
 
    virtual bool SetUniform4x4f(const char* name, const Matrix4f& m)
    {
        Matrix4f mt = m.Transposed();
        return SetUniform(name, 16, &mt.M[0][0]);
    }
    virtual bool SetUniform3x3f(const char* name, const Matrix4f& m)
    {
        // float3x3 is actually stored the same way as float4x3, with the last items ignored by the code.
        Matrix4f mt = m.Transposed();
        return SetUniform(name, 12, &mt.M[0][0]);
    }

};


// Fill combines a ShaderSet (vertex, pixel) with textures, if any.
// Every model has a fill.
class ShaderFill : public RefCountBase<ShaderFill>
{
    Ptr<ShaderSet>     Shaders;
    Ptr<class Texture> PsTextures[8];
    Ptr<class Texture> VsTextures[8];
    Ptr<class Texture> CsTextures[8];
    void*              InputLayout; // HACK this should be abstracted

public:
    ShaderFill(ShaderSet* sh) : Shaders(sh) { InputLayout = NULL; }
    ShaderFill(ShaderSet& sh) : Shaders(sh) { InputLayout = NULL; }    

    ShaderSet*  GetShaders() const      { return Shaders; }
    void*       GetInputLayout() const  { return InputLayout; }

    virtual void Set(PrimitiveType prim = Prim_Unknown) const;   

    virtual void SetTexture(int i, class Texture* tex, ShaderStage stage)
    {
        if (i < 8)
        {
                 if(stage == Shader_Pixel)  PsTextures[i] = tex;
            else if(stage == Shader_Vertex) VsTextures[i] = tex;
            else if(stage == Shader_Compute) CsTextures[i] = tex;
            else OVR_ASSERT(false);
        }
    }
    void SetInputLayout(void* newIL) { InputLayout = (void*)newIL; }
};


class ShaderBase : public Shader
{
public:    
    RenderParams*   pParams;
    unsigned char*  UniformData;
    int             UniformsSize;

	enum VarType
	{
		VARTYPE_FLOAT,
		VARTYPE_INT,
		VARTYPE_BOOL,
	};

	struct Uniform
	{
		const char* Name;
		VarType     Type;
        int         Offset;
        int         Size;
	};
    const Uniform*  UniformRefl;
    size_t          UniformReflSize;

	ShaderBase(RenderParams* rp, ShaderStage stage);
	~ShaderBase();

    ShaderStage GetStage() const { return Stage; }

    void InitUniforms(const Uniform* refl, size_t reflSize);
	bool SetUniform(const char* name, int n, const float* v);
	bool SetUniformBool(const char* name, int n, const bool* v);
 
    void UpdateBuffer(Buffer* b);
};


template<ShaderStage SStage, class D3DShaderType>
class ShaderImpl : public ShaderBase
{
public:
    D3DShaderType*  D3DShader;

    ShaderImpl(RenderParams* rp, void* s, size_t size, const Uniform* refl, size_t reflSize) : ShaderBase(rp, SStage)
    {
        Load(s, size);
        InitUniforms(refl, reflSize);
    }
    ~ShaderImpl()
    {
        if (D3DShader)        
            D3DShader->Release();        
    }

    // These functions have specializations.
    bool Load(void* shader, size_t size);
    void Set(PrimitiveType prim) const;
    void SetUniformBuffer(Buffer* buffers, int i = 0);
};

typedef ShaderImpl<Shader_Vertex,  ID3D11VertexShader> VertexShader;
typedef ShaderImpl<Shader_Fragment, ID3D11PixelShader> PixelShader;
typedef ShaderImpl<Shader_Compute, ID3D11ComputeShader> ComputeShader;


class Buffer : public RefCountBase<Buffer>
{
public:
    RenderParams*                   pParams;
    Ptr<ID3D11Buffer>               D3DBuffer;
    Ptr<ID3D11ShaderResourceView>   D3DSrv;
    Ptr<ID3D11UnorderedAccessView>  D3DUav;
    size_t            Size;
    int               Use;
    bool              Dynamic;

public:
    Buffer(RenderParams* rp) : pParams(rp), D3DBuffer(), D3DSrv(),
                               D3DUav(),
                               Size(0), Use(0), Dynamic(false) {}
    ~Buffer();

    ID3D11Buffer* GetBuffer() const
    {
        return D3DBuffer;
    }

    ID3D11ShaderResourceView* GetSrv() const
    {
        return D3DSrv;
    }

    ID3D11UnorderedAccessView* GetUav() const
    {
        return D3DUav;
    }

    virtual size_t GetSize()        { return Size; }
    virtual void*  Map(size_t start, size_t size, int flags = 0);
    virtual bool   Unmap(void *m);
    virtual bool   Data(int use, const void* buffer, size_t size, int computeBufferStride = -1);
};


class Texture : public RefCountBase<Texture>
{
public:
    RenderParams*                   pParams;
    Ptr<ID3D11Texture2D>            Tex;
    Ptr<ID3D11ShaderResourceView>   TexSv;
    Ptr<ID3D11RenderTargetView>     TexRtv;
    Ptr<ID3D11DepthStencilView>     TexDsv;
    // TODO: add UAV...
    mutable Ptr<ID3D11SamplerState> Sampler;
    Sizei                           TextureSize;
    int                             Samples;

    Texture(RenderParams* rp, int fmt, const Sizei texSize,
            ID3D11SamplerState* sampler, int samples = 1);
    Texture(RenderParams* rp, int fmt, const Sizei texSize,
            ID3D11SamplerState* sampler, const void* data, int mipcount);
    ~Texture();

    void GenerateSubresourceData(
        unsigned imageWidth, unsigned imageHeight, int format, unsigned imageDimUpperLimit,
        const void* rawBytes, D3D11_SUBRESOURCE_DATA* subresData,
        unsigned& largestMipWidth, unsigned& largestMipHeight, unsigned& byteSize, unsigned& effectiveMipCount);
    
    virtual Sizei GetSize() const     { return TextureSize; }    
    virtual int   GetSamples() const  { return Samples; }

  //  virtual void SetSampleMode(int sm);

    // Updates texture to point to specified resources
    //  - used for slave rendering.
    void UpdatePlaceholderTexture(ID3D11Texture2D* texture,
                                  ID3D11ShaderResourceView* psrv,
                                  const Sizei& textureSize, const int sampleCount)
    {
        Tex     = texture;
        TexSv   = psrv;
        TexRtv.Clear();
        TexDsv.Clear();

        TextureSize = textureSize;
        Samples = sampleCount;

#ifdef OVR_BUILD_DEBUG
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        OVR_ASSERT(TextureSize == Sizei(desc.Width, desc.Height));
#endif
    }


    virtual void Set(int slot, ShaderStage stage = Shader_Fragment) const;

    int GetNumMipLevels(int w, int h)
    {
        int n = 1;
        while(w > 1 || h > 1)
        {
            w >>= 1;
            h >>= 1;
            n++;
        }
        return n;
    }

    void FilterRgba2x2(const uint8_t* src, int w, int h, uint8_t* dest)
    {
        for(int j = 0; j < (h & ~1); j += 2)
        {
            const uint8_t* psrc = src + (w * j * 4);
            uint8_t*       pdest = dest + ((w >> 1) * (j >> 1) * 4);

            for(int i = 0; i < w >> 1; i++, psrc += 8, pdest += 4)
            {
                pdest[0] = (((int)psrc[0]) + psrc[4] + psrc[w * 4 + 0] + psrc[w * 4 + 4]) >> 2;
                pdest[1] = (((int)psrc[1]) + psrc[5] + psrc[w * 4 + 1] + psrc[w * 4 + 5]) >> 2;
                pdest[2] = (((int)psrc[2]) + psrc[6] + psrc[w * 4 + 2] + psrc[w * 4 + 6]) >> 2;
                pdest[3] = (((int)psrc[3]) + psrc[7] + psrc[w * 4 + 3] + psrc[w * 4 + 7]) >> 2;
            }
        }
    }
};


class GpuTimer : public RefCountBase<GpuTimer>
{
public:
    GpuTimer()
        : QuerySets(MaxNumQueryFrames)
        , D3dDevice(NULL)
        , Context(NULL)
        , LastQueuedFrame(-1)
        , LastTimedFrame(-1)
    { }

    void Init(ID3D11Device* device, ID3D11DeviceContext* content);

    void BeginQuery();
    void EndQuery();

    // Returns -1 if timing is invalid
    float GetTiming(bool blockUntilValid);

protected:
    static const unsigned MaxNumQueryFrames = 10;
    
    int GotoNextFrame(int frame)
    {
        return (frame + 1) % MaxNumQueryFrames;
    }
    
    _COM_SMARTPTR_TYPEDEF(ID3D11Query, __uuidof(ID3D11Query));

    struct GpuQuerySets
    {
        ID3D11QueryPtr DisjointQuery;
        ID3D11QueryPtr TimeStartQuery;
        ID3D11QueryPtr TimeEndQuery;
        bool QueryStarted;
        bool QueryAwaitingTiming;

        GpuQuerySets() : QueryStarted(false), QueryAwaitingTiming(false) {}
    };
    Array<GpuQuerySets> QuerySets;
    
    int LastQueuedFrame;
    int LastTimedFrame;

    Ptr<ID3D11Device> D3dDevice;
    Ptr<ID3D11DeviceContext> Context;
};

}}} // OVR::CAPI::D3D11

#endif // OVR_CAPI_D3D11_Util_h
