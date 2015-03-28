/************************************************************************************

Filename    :   Render_D3D11_Device.h
Content     :   RenderDevice implementation header for D3D11.
Created     :   September 10, 2012
Authors     :   Andrew Reisse

Copyright   :   Copyright 2012 Oculus VR, LLC. All Rights reserved.

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

#ifndef OVR_Render_D3D11_Device_h
#define OVR_Render_D3D11_Device_h

#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Array.h"

#include "../Render/Render_Device.h"

#include "Util/Util_Direct3D.h"

namespace OVR { namespace Render { namespace D3D11 {

class RenderDevice;

class Buffer;

class ShaderBase : public Render::Shader
{
public:
    RenderDevice*   Ren;
    unsigned char*  UniformData;
    int             UniformsSize;

    struct Uniform
    {
        String Name;
        int    Offset;
        int    Size;
    };
    Array<Uniform> UniformInfo;

    ShaderBase(RenderDevice* r, ShaderStage stage);
    ~ShaderBase();

    void InitUniforms(ID3D10Blob* s);
    bool SetUniform(const char* name, int n, const float* v);
    //virtual bool UseTransposeMatrix() const { return 1; }

    void UpdateBuffer(Buffer* b);
};


template<Render::ShaderStage SStage, class D3DShaderType>
class Shader : public ShaderBase
{
public:
    D3DShaderType*  D3DShader;

    Shader(RenderDevice* r, D3DShaderType* s) : ShaderBase(r, SStage), D3DShader(s) {}
    Shader(RenderDevice* r, ID3D10Blob* s) : ShaderBase(r, SStage)
    {
        Load(s);
        InitUniforms(s);
    }
    ~Shader()
    {
        if (D3DShader)
        {
            D3DShader->Release();
        }
    }
    bool Load(ID3D10Blob* shader)
    {
        return Load(shader->GetBufferPointer(), shader->GetBufferSize());
    }

    // These functions have specializations.
    bool Load(void* shader, size_t size);
    void Set(PrimitiveType prim) const;
    void SetUniformBuffer(Render::Buffer* buffers, int i = 0);
};

typedef Shader<Render::Shader_Vertex, ID3D11VertexShader> VertexShader;
typedef Shader<Render::Shader_Geometry, ID3D11GeometryShader> GeomShader;
typedef Shader<Render::Shader_Fragment, ID3D11PixelShader> PixelShader;


class Buffer : public Render::Buffer
{
public:
    RenderDevice*     Ren;
    Ptr<ID3D11Buffer> D3DBuffer;
    Ptr<ID3D11ShaderResourceView> D3DSrv;
    Ptr<ID3D11UnorderedAccessView> D3DUav;
    size_t            Size;
    int               Use;
    bool              Dynamic;

public:
    Buffer(RenderDevice* r) :
        Ren(r),
        Size(0),
        Use(0),
        Dynamic(false)
    {
    }
    ~Buffer();

    ID3D11Buffer* GetBuffer()
    {
        return D3DBuffer;
    }

    ID3D11ShaderResourceView* GetSrv()
    {
        return D3DSrv;
    }

    ID3D11UnorderedAccessView* GetUav()
    {
        return D3DUav;
    }

    virtual size_t GetSize()
    {
        return Size;
    }
    virtual void*  Map(size_t start, size_t size, int flags = 0);
    virtual bool   Unmap(void* m);
    virtual bool   Data(int use, const void* buffer, size_t size);
};


class Texture : public Render::Texture
{
public:
    RenderDevice*                   Ren;
    Ptr<ID3D11Texture2D>            Tex;
    // TODO: Add UAV...
    Ptr<ID3D11ShaderResourceView>   TexSv;
    Ptr<ID3D11RenderTargetView>     TexRtv;
    Ptr<ID3D11DepthStencilView>     TexDsv;
    Ptr<ID3D11Texture2D>			TexStaging;
    mutable Ptr<ID3D11SamplerState> Sampler;
    int                             Width, Height;
    int                             Samples;
    int                             Format;

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

    virtual void Set(int slot, Render::ShaderStage stage = Render::Shader_Fragment) const;

    virtual ovrTexture Get_ovrTexture() OVR_OVERRIDE;

    virtual void* GetInternalImplementation();
};


class RenderDevice : public Render::RenderDevice
{
public:
    Ptr<IDXGIFactory>               DXGIFactory;
    HWND                            Window;

    Ptr<ID3D11Device>               Device;
    Ptr<ID3D11DeviceContext>        Context;
    Ptr<IDXGISwapChain>             SwapChain;
    Ptr<IDXGIAdapter>               Adapter;
    Ptr<IDXGIOutput>                FullscreenOutput;
    int                             FSDesktopX, FSDesktopY;
    int                             PreFullscreenX, PreFullscreenY, PreFullscreenW, PreFullscreenH;

    Ptr<ID3D11Texture2D>            BackBuffer;
    Ptr<ID3D11RenderTargetView>     BackBufferRT;
    Ptr<ID3D11UnorderedAccessView>  BackBufferUAV;
    Ptr<Texture>                    CurRenderTarget;
    Ptr<Texture>                    CurDepthBuffer;
    Ptr<ID3D11RasterizerState>      Rasterizer;
    Ptr<ID3D11BlendState>           BlendState;
    D3D11_VIEWPORT                  D3DViewport;

    Ptr<ID3D11DepthStencilState>    DepthStates[1 + 2 * Compare_Count];
    Ptr<ID3D11DepthStencilState>    CurDepthState;
    Ptr<ID3D11InputLayout>          ModelVertexIL;
    Ptr<ID3D11InputLayout>          DistortionVertexIL;
    Ptr<ID3D11InputLayout>          HeightmapVertexIL;

    Ptr<ID3D11SamplerState>         SamplerStates[Sample_Count];

    struct StandardUniformData
    {
        Matrix4f  Proj;
        Matrix4f  View;
    }                        StdUniforms;
    Ptr<Buffer>              UniformBuffers[Shader_Count];
    int                      MaxTextureSet[Shader_Count];

    Ptr<VertexShader>        VertexShaders[VShader_Count];
    Ptr<PixelShader>         PixelShaders[FShader_Count];
    Ptr<GeomShader>          pStereoShaders[Prim_Count];
    Ptr<Buffer>              CommonUniforms[8];
    Ptr<ShaderSet>           ExtraShaders;
    Ptr<ShaderFill>          DefaultFill;

    Ptr<Buffer>              QuadVertexBuffer;

    Array<Ptr<Texture> >     DepthBuffers;

public:
    RenderDevice(const RendererParams& p, HWND window);
    ~RenderDevice();

    // Implement static initializer function to create this class.
    static Render::RenderDevice* CreateDevice(const RendererParams& rp, void* oswnd);

    // if needRecreate == true it will recreate DXGIFactory and Adapter
    // to get the latest info about monitors (including just connected/
    // disconnected ones). Note, SwapChain will be released in this case
    // and it should be recreated.
    void         UpdateMonitorOutputs(bool needRecreate = false);

    virtual void SetViewport(const Recti& vp);
    virtual void SetWindowSize(int w, int h);
    virtual bool SetParams(const RendererParams& newParams);

    // Returns details needed by CAPI distortion rendering.
    virtual ovrRenderAPIConfig Get_ovrRenderAPIConfig() const;

    virtual void Present(bool withVsync);
    virtual void WaitUntilGpuIdle();
    virtual void Flush();

    virtual bool SetFullscreen(DisplayMode fullscreen);
    virtual size_t QueryGPUMemorySize();

    virtual void Clear(float r = 0, float g = 0, float b = 0, float a = 1,
        float depth = 1,
        bool clearColor = true, bool clearDepth = true);
    virtual void Rect(float left, float top, float right, float bottom)
    {
        OVR_UNUSED4(left, top, right, bottom);
    }

    virtual Buffer* CreateBuffer();
    virtual Texture* CreateTexture(int format, int width, int height, const void* data, int mipcount = 1);

    static void GenerateSubresourceData(
        unsigned imageWidth, unsigned imageHeight, int format, unsigned imageDimUpperLimit,
        const void* rawBytes,
        D3D11_SUBRESOURCE_DATA* subresData,
        unsigned& largestMipWidth, unsigned& largestMipHeight, unsigned& byteSize, unsigned& effectiveMipCount);

    Texture* GetDepthBuffer(int w, int h, int ms);

    virtual void ResolveMsaa(Render::Texture* msaaTex, Render::Texture* outputTex) OVR_OVERRIDE;

    virtual void BeginRendering();
    virtual void SetRenderTarget(Render::Texture* color,
        Render::Texture* depth = NULL, Render::Texture* stencil = NULL);
    virtual void SetDepthMode(bool enable, bool write, CompareFunc func = Compare_Less);
    virtual void SetWorldUniforms(const Matrix4f& proj);
    virtual void SetCommonUniformBuffer(int i, Render::Buffer* buffer);
    virtual void SetExtraShaders(ShaderSet* s)
    {
        ExtraShaders = s;
    }

    // Overridden to apply proper blend state.
    virtual void FillRect(float left, float top, float right, float bottom, Color c, const Matrix4f* view = NULL);
    virtual void FillGradientRect(float left, float top, float right, float bottom, Color col_top, Color col_btm, const Matrix4f* view);
    virtual void RenderText(const struct Font* font, const char* str, float x, float y, float size, Color c, const Matrix4f* view = NULL);
    virtual void RenderImage(float left, float top, float right, float bottom, ShaderFill* image, unsigned char alpha = 255, const Matrix4f* view = NULL);

    virtual void Render(const Matrix4f& matrix, Model* model);
    virtual void Render(const Fill* fill, Render::Buffer* vertices, Render::Buffer* indices,
        const Matrix4f& matrix, int offset, int count, PrimitiveType prim = Prim_Triangles, MeshType meshType = Mesh_Scene);
    virtual void RenderWithAlpha(const Fill* fill, Render::Buffer* vertices, Render::Buffer* indices,
        const Matrix4f& matrix, int offset, int count, PrimitiveType prim = Prim_Triangles);
    virtual void RenderCompute(const Fill* fill, Render::Buffer* buffer, int invocationSizeInPixels);
    virtual Fill *CreateSimpleFill(int flags = Fill::F_Solid);

    virtual Render::Shader *LoadBuiltinShader(ShaderStage stage, int shader);

    bool RecreateSwapChain();
    virtual ID3D10Blob* CompileShader(const char* profile, const char* src, const char* mainName = "main");
    virtual ShaderBase* CreateStereoShader(PrimitiveType prim, Render::Shader* vs);

    ID3D11SamplerState* GetSamplerState(int sm);

    void SetTexture(Render::ShaderStage stage, int slot, const Texture* t);

    // GPU Profiling
    virtual void BeginGpuEvent(const char* markerText, uint32_t markerColor);
    virtual void EndGpuEvent();
};


}}} // namespace OVR::Render::D3D11

#endif // OVR_Render_D3D11_Device_h
