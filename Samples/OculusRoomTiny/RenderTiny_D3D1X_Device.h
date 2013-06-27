/************************************************************************************

Filename    :   RenderTiny_D3D1X_Device.h
Content     :   RenderDevice implementation header for D3DX10.
Created     :   September 10, 2012
Authors     :   Andrew Reisse

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef INC_RenderTiny_D3D1X_Device_h
#define INC_RenderTiny_D3D1X_Device_h

#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Array.h"

#include "RenderTiny_Device.h"
#include <Windows.h>

#define _OVR_RENDERER_D3D10
#include <d3d10.h>

namespace OVR { namespace RenderTiny { namespace D3D10 {

class RenderDevice;
class Buffer;

typedef ID3D10Device            ID3D1xDevice;
typedef ID3D10Device            ID3D1xDeviceContext;
typedef ID3D10RenderTargetView  ID3D1xRenderTargetView;
typedef ID3D10Texture2D         ID3D1xTexture2D;
typedef ID3D10ShaderResourceView ID3D1xShaderResourceView;
typedef ID3D10DepthStencilView  ID3D1xDepthStencilView;
typedef ID3D10DepthStencilState ID3D1xDepthStencilState;
typedef ID3D10InputLayout       ID3D1xInputLayout;
typedef ID3D10Buffer            ID3D1xBuffer;
typedef ID3D10VertexShader      ID3D1xVertexShader;
typedef ID3D10PixelShader       ID3D1xPixelShader;
typedef ID3D10GeometryShader    ID3D1xGeometryShader;
typedef ID3D10BlendState        ID3D1xBlendState;
typedef ID3D10RasterizerState   ID3D1xRasterizerState;
typedef ID3D10SamplerState      ID3D1xSamplerState;
typedef ID3D10Query             ID3D1xQuery;
typedef ID3D10Blob              ID3D1xBlob;
typedef D3D10_VIEWPORT          D3D1x_VIEWPORT;
typedef D3D10_QUERY_DESC        D3D1x_QUERY_DESC;
#define D3D1x_(x)               D3D10_##x
#define ID3D1x(x)               ID3D10##x



class ShaderBase : public RenderTiny::Shader
{
public:
    RenderDevice*   Ren;
    unsigned char*  UniformData;
    int             UniformsSize;

    struct Uniform
    {
        String Name;
        int    Offset, Size;
    };
    Array<Uniform> UniformInfo;

    ShaderBase(RenderDevice* r, ShaderStage stage);
    ~ShaderBase();

    void InitUniforms(ID3D10Blob* s);
    bool SetUniform(const char* name, int n, const float* v);
 
    void UpdateBuffer(Buffer* b);
};

template<RenderTiny::ShaderStage SStage, class D3DShaderType>
class Shader : public ShaderBase
{
public:
    D3DShaderType*  D3DShader;

    Shader(RenderDevice* r, D3DShaderType* s) : ShaderBase(r, SStage), D3DShader(s) {}
    Shader(RenderDevice* r, ID3D1xBlob* s) : ShaderBase(r, SStage)
    {
        Load(s);
        InitUniforms(s);
    }
    ~Shader()
    {
        if (D3DShader)        
            D3DShader->Release();        
    }
    bool Load(ID3D1xBlob* shader)
    {
        return Load(shader->GetBufferPointer(), shader->GetBufferSize());
    }

    // These functions have specializations.
    bool Load(void* shader, size_t size);
    void Set(PrimitiveType prim) const;
    void SetUniformBuffer(RenderTiny::Buffer* buffers, int i = 0);
};

typedef Shader<RenderTiny::Shader_Vertex,  ID3D1xVertexShader> VertexShader;
typedef Shader<RenderTiny::Shader_Fragment, ID3D1xPixelShader> PixelShader;


class Buffer : public RenderTiny::Buffer
{
public:
    RenderDevice*     Ren;
    Ptr<ID3D1xBuffer> D3DBuffer;
    size_t            Size;
    int               Use;
    bool              Dynamic;

public:
    Buffer(RenderDevice* r) : Ren(r), Size(0), Use(0) {}
    ~Buffer();

    ID3D1xBuffer* GetBuffer()
    {
        return D3DBuffer;
    }

    virtual size_t GetSize()
    {
        return Size;
    }
    virtual void*  Map(size_t start, size_t size, int flags = 0);
    virtual bool   Unmap(void *m);
    virtual bool   Data(int use, const void* buffer, size_t size);
};

class Texture : public RenderTiny::Texture
{
public:
    RenderDevice*                    Ren;
    Ptr<ID3D1xTexture2D>            Tex;
    Ptr<ID3D1xShaderResourceView>   TexSv;
    Ptr<ID3D1xRenderTargetView>     TexRtv;
    Ptr<ID3D1xDepthStencilView>     TexDsv;
    mutable Ptr<ID3D1xSamplerState> Sampler;
    int                             Width, Height;
    int                             Samples;

    Texture(RenderDevice* r, int fmt, int w, int h);
    ~Texture();

    virtual int GetWidth() const
    {
        return Width;
    }
    virtual int GetHeight() const
    {
        return Height;
    }
    virtual int GetSamples() const
    {
        return Samples;
    }

    virtual void SetSampleMode(int sm);

    virtual void Set(int slot, RenderTiny::ShaderStage stage = RenderTiny::Shader_Fragment) const;
};

class RenderDevice : public RenderTiny::RenderDevice
{
public:
    Ptr<IDXGIFactory>           DXGIFactory;
    HWND                        Window;

    Ptr<ID3D1xDevice>           Device;
    Ptr<ID3D1xDeviceContext>    Context;
    Ptr<IDXGISwapChain>         SwapChain;
    Ptr<IDXGIAdapter>           Adapter;
    Ptr<IDXGIOutput>            FullscreenOutput;
    int                         FSDesktopX, FSDesktopY;    

    Ptr<ID3D1xTexture2D>        BackBuffer;
    Ptr<ID3D1xRenderTargetView> BackBufferRT;
    Ptr<Texture>                CurRenderTarget;
    Ptr<Texture>                CurDepthBuffer;
    Ptr<ID3D1xRasterizerState>  Rasterizer;
    Ptr<ID3D1xBlendState>       BlendState;    
    D3D1x_VIEWPORT              D3DViewport;

    Ptr<ID3D1xDepthStencilState> DepthStates[1 + 2 * Compare_Count];
    Ptr<ID3D1xDepthStencilState> CurDepthState;
    Ptr<ID3D1xInputLayout>      ModelVertexIL;

    Ptr<ID3D1xSamplerState>     SamplerStates[Sample_Count];

    struct StandardUniformData
    {
        Matrix4f  Proj;
        Matrix4f  View;
    }                        StdUniforms;
    Ptr<Buffer>              UniformBuffers[Shader_Count];
    int                      MaxTextureSet[Shader_Count];

    Ptr<VertexShader>        VertexShaders[VShader_Count];
    Ptr<PixelShader>         PixelShaders[FShader_Count];  
    Ptr<Buffer>              CommonUniforms[8];
    Ptr<ShaderFill>          DefaultFill;

    Ptr<Buffer>              QuadVertexBuffer;

    Array<Ptr<Texture> >     DepthBuffers;

public:
    RenderDevice(const RendererParams& p, HWND window);
    ~RenderDevice();

    // Implement static initializer function to create this class.
    static RenderTiny::RenderDevice* CreateDevice(const RendererParams& rp, void* oswnd);

    void        UpdateMonitorOutputs();

    virtual void SetRealViewport(const Viewport& vp);
    virtual bool SetParams(const RendererParams& newParams);
  
    virtual void Present();
    virtual void ForceFlushGPU();

    virtual bool SetFullscreen(DisplayMode fullscreen);

    virtual void Clear(float r = 0, float g = 0, float b = 0, float a = 1, float depth = 1);

    virtual Buffer* CreateBuffer();
    virtual Texture* CreateTexture(int format, int width, int height, const void* data, int mipcount=1);

    Texture* GetDepthBuffer(int w, int h, int ms);

    virtual void BeginRendering();
    virtual void SetRenderTarget(RenderTiny::Texture* color,
                                 RenderTiny::Texture* depth = NULL, RenderTiny::Texture* stencil = NULL);
    virtual void SetDepthMode(bool enable, bool write, CompareFunc func = Compare_Less);
    virtual void SetWorldUniforms(const Matrix4f& proj);
    virtual void SetCommonUniformBuffer(int i, RenderTiny::Buffer* buffer);

    virtual void Render(const Matrix4f& matrix, Model* model);
    virtual void Render(const ShaderFill* fill, RenderTiny::Buffer* vertices, RenderTiny::Buffer* indices,
                        const Matrix4f& matrix, int offset, int count, PrimitiveType prim = Prim_Triangles);

    virtual ShaderFill *CreateSimpleFill() { return DefaultFill; }

    virtual RenderTiny::Shader *LoadBuiltinShader(ShaderStage stage, int shader);

    bool                RecreateSwapChain();
    virtual ID3D10Blob* CompileShader(const char* profile, const char* src, const char* mainName = "main");

    ID3D1xSamplerState* GetSamplerState(int sm);

    void                SetTexture(RenderTiny::ShaderStage stage, int slot, const Texture* t);
};

}}} // Render::D3D10

#endif
