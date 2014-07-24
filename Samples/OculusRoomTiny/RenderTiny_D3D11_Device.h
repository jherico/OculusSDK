/************************************************************************************

Filename    :   RenderTiny_D3D11_Device.h
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

#ifndef INC_RenderTiny_D3D11_Device_h
#define INC_RenderTiny_D3D11_Device_h

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Color.h"
#include <d3d11.h>

namespace OVR { namespace RenderTiny {


class RenderDevice;
class Buffer;


//-----------------------------------------------------------------------------------

// Rendering primitive type used to render Model.
enum PrimitiveType
{
    Prim_Triangles,
    Prim_Lines,
    Prim_TriangleStrip,
    Prim_Unknown,
    Prim_Count
};

// Types of shaders taht can be stored together in a ShaderSet.
enum ShaderStage
{
    Shader_Vertex   = 0,
    Shader_Fragment = 2,
    Shader_Pixel    = 2,
    Shader_Count    = 3,
};

// Built-in shader types; used by LoadBuiltinShader.
enum BuiltinShaders
{
    VShader_MV                      = 0,
    VShader_MVP                     = 1,
    VShader_Count                   = 2,

    FShader_Solid                   = 0,
    FShader_Gouraud                 = 1,
    FShader_Texture                 = 2,    
    FShader_LitGouraud              = 3,
    FShader_LitTexture              = 4,
    FShader_Count
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
    Texture_GenMipmaps      = 0x20000,
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
    Sample_AddressMask  =12,

    Sample_Count        =13,
};


// Base class for vertex and pixel shaders. Stored in ShaderSet.
class ShaderBase : public RefCountBase<ShaderBase>
{
    friend class ShaderSet;

protected:
    ShaderStage Stage;

public:
    RenderDevice*   Ren;
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
		String Name;
		VarType Type;
		int    Offset, Size;
	};
	Array<Uniform> UniformInfo;

    ShaderBase(RenderDevice* r, ShaderStage stage);
    ShaderBase(ShaderStage s) : Stage(s) {}

	~ShaderBase();

    ShaderStage GetStage() const { return Stage; }

    virtual void Set(PrimitiveType) const { }
    virtual void SetUniformBuffer(class Buffer* buffers, int i = 0) { OVR_UNUSED2(buffers, i); }
    
	void InitUniforms(ID3D10Blob* s);
    void InitUniforms(void* s, size_t sizeS);
	virtual bool SetUniform(const char* name, int n, const float* v);
	virtual bool SetUniformBool(const char* name, int n, const bool* v);
 
    void UpdateBuffer(Buffer* b);
};

template<ShaderStage SStage, class D3DShaderType>
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
    Shader(RenderDevice* r, void* s, size_t size) : ShaderBase(r, SStage)
    {
        Load(s, size);
        InitUniforms(s, size);
    }
    ~Shader()
    {
        if (D3DShader)        
            D3DShader->Release();        
    }
    bool Load(ID3D10Blob* shader)
    {
        return Load(shader->GetBufferPointer(), shader->GetBufferSize());
    }

    // These functions have specializations.
    bool Load(void* shader, size_t size);
    void Set(PrimitiveType prim) const;
    void SetUniformBuffer(Buffer* buffers, int i = 0);
};

typedef Shader<Shader_Vertex,  ID3D11VertexShader> VertexShader;
typedef Shader<Shader_Fragment, ID3D11PixelShader> PixelShader;


// A group of shaders, one per stage.
// A ShaderSet is applied to a RenderDevice for rendering with a given fill.
class ShaderSet : public RefCountBase<ShaderSet>
{
protected:
    Ptr<ShaderBase> Shaders[Shader_Count];

public:
    ShaderSet() { }
    ~ShaderSet() { }

    virtual void SetShader(ShaderBase *s)
    {
        Shaders[s->GetStage()] = s;
    }
    virtual void UnsetShader(int stage)
    {
        Shaders[stage] = NULL;
    }
    ShaderBase* GetShader(int stage) { return Shaders[stage]; }

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
    bool SetUniform4fv(const char* name, int n, const Vector4f* v)
    {
        return SetUniform(name, 4*n, &v[0].x);
    }
    virtual bool SetUniform4x4f(const char* name, const Matrix4f& m)
    {
        Matrix4f mt = m.Transposed();
        return SetUniform(name, 16, &mt.M[0][0]);
    }
};


// Fill combines a ShaderSet (vertex, pixel) with textures, if any.
// Every model has a fill.
class ShaderFill : public RefCountBase<ShaderFill>
{
    Ptr<ShaderSet>     Shaders;
    Ptr<class Texture> Textures[8];
    void*              InputLayout; // HACK this should be abstracted

public:
    ShaderFill(ShaderSet* sh) : Shaders(sh) { InputLayout = NULL; }
    ShaderFill(ShaderSet& sh) : Shaders(sh) { InputLayout = NULL; }    

    ShaderSet*  GetShaders() { return Shaders; }


    void* GetInputLayout() { return InputLayout; }

    virtual void Set(PrimitiveType prim = Prim_Unknown) const;   
    virtual void SetTexture(int i, class Texture* tex) { if (i < 8) Textures[i] = tex; }
    void SetInputLayout(void* newIL) { InputLayout = (void*)newIL; }
};

// Buffer for vertex or index data. Some renderers require separate buffers, so that
// is recommended. Some renderers cannot have high-performance buffers which are readable,
// so reading in Map should not be relied on.
//
// Constraints on buffers, such as ReadOnly, are not enforced by the API but may result in 
// rendering-system dependent undesirable behavior, such as terrible performance or unreported failure.
//
// Use of a buffer inconsistent with usage is also not checked by the API, but it may result in bad
// performance or even failure.
//
// Use the Data() function to set buffer data the first time, if possible (it may be faster).

class Buffer : public RefCountBase<Buffer>
{
public:
    RenderDevice*     Ren;
    Ptr<ID3D11Buffer> D3DBuffer;
    size_t            Size;
    int               Use;
    bool              Dynamic;

public:
    Buffer(RenderDevice* r) : Ren(r), Size(0), Use(0) {}
    virtual ~Buffer() {}

    ID3D11Buffer* GetBuffer()
    {
        return D3DBuffer;
    }

    virtual size_t GetSize()
    {
        return Size;
    }
    virtual void*  Map(size_t start, size_t size, int flags = 0);
    virtual bool   Unmap(void *m);
    // Allocates a buffer, optionally filling it with data.
    virtual bool   Data(int use, const void* buffer, size_t size);
};

class Texture : public RefCountBase<Texture>
{
public:
    RenderDevice*                   Ren;
    Ptr<ID3D11Texture2D>            Tex;
    Ptr<ID3D11ShaderResourceView>   TexSv;
    Ptr<ID3D11RenderTargetView>     TexRtv;
    Ptr<ID3D11DepthStencilView>     TexDsv;
    mutable Ptr<ID3D11SamplerState> Sampler;
    int                             Width, Height;
    int                             Samples;

    Texture(RenderDevice* r, int fmt, int w, int h);
    virtual ~Texture();

    virtual int GetWidth() const    { return Width; }
    virtual int GetHeight() const   { return Height; }
    virtual int GetSamples() const  { return Samples; }

    virtual void SetSampleMode(int sm);

    // Updates texture to point to specified resources
    //  - used for slave rendering.
    void UpdatePlaceholderTexture(ID3D11Texture2D* texture, ID3D11ShaderResourceView* psrv)
    {
        Tex     = texture;
        TexSv   = psrv;
        TexRtv.Clear();
        TexDsv.Clear();

        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        Width = desc.Width;
        Height= desc.Height;
    }


    virtual void Set(int slot, ShaderStage stage = Shader_Fragment) const;
};


//-----------------------------------------------------------------------------------

// Node is a base class for geometry in a Scene, it contains base position
// and orientation data.
// Model and Container both derive from it.
// 
class Node : public RefCountBase<Node>
{
    Vector3f     Pos;
    Quatf        Rot;

    mutable Matrix4f  Mat;
    mutable bool      MatCurrent;

public:
    Node() : Pos(Vector3f(0)), MatCurrent(1) { }
    virtual ~Node() { }

    enum NodeType
    {
        Node_NonDisplay,
        Node_Container,
        Node_Model
    };
    virtual NodeType GetType() const { return Node_NonDisplay; }

    const Vector3f&  GetPosition() const      { return Pos; }
    const Quatf&     GetOrientation() const   { return Rot; }
    void             SetPosition(Vector3f p)  { Pos = p; MatCurrent = 0; }
    void             SetOrientation(Quatf q)  { Rot = q; MatCurrent = 0; }

    void             Move(Vector3f p)         { Pos += p; MatCurrent = 0; }
    void             Rotate(Quatf q)          { Rot = q * Rot; MatCurrent = 0; }


    // For testing only; causes Position an Orientation
    void  SetMatrix(const Matrix4f& m)
    {
        MatCurrent = true;
        Mat = m;        
    }

    const Matrix4f&  GetMatrix() const 
    {
        if (!MatCurrent)
        {
            Mat = Matrix4f(Rot);
            Mat = Matrix4f::Translation(Pos) * Mat;
            MatCurrent = 1;
        }
        return Mat;
    }

    virtual void     Render(const Matrix4f& ltw, RenderDevice* ren) { OVR_UNUSED2(ltw, ren); }
};


// Vertex type; same format is used for all shapes for simplicity.
// Shapes are built by adding vertices to Model.
struct Vertex
{
    Vector3f  Pos;
    Color     C;
    float     U, V;	
    Vector3f  Norm;

    Vertex (const Vector3f& p, const Color& c = Color(64,0,0,255), 
        float u = 0, float v = 0, Vector3f n = Vector3f(1,0,0))
        : Pos(p), C(c), U(u), V(v), Norm(n)
    {}
    Vertex(float x, float y, float z, const Color& c = Color(64,0,0,255),
        float u = 0, float v = 0) : Pos(x,y,z), C(c), U(u), V(v)
    { }

    bool operator==(const Vertex& b) const
    {
        return Pos == b.Pos && C == b.C && U == b.U && V == b.V;
    }
};


// LightingParams are stored in a uniform buffer, don't change it without fixing all renderers
// Scene contains a set of LightingParams that is uses for rendering.
struct LightingParams
{
    Vector4f Ambient;
    Vector4f LightPos[8];
    Vector4f LightColor[8];
    float    LightCount;    
    int      Version;

    LightingParams() : LightCount(0), Version(0) {}


    void Update(const Matrix4f& view, const Vector4f* SceneLightPos)
    {    
        Version++;
        for (int i = 0; i < LightCount; i++)
        {
            LightPos[i] = view.Transform(SceneLightPos[i]);
        }
    }

    void Set(ShaderSet* s) const
    {
        s->SetUniform4fv("Ambient", 1, &Ambient);
        s->SetUniform1f("LightCount", LightCount);
        s->SetUniform4fv("LightPos", (int)LightCount, LightPos);
        s->SetUniform4fv("LightColor", (int)LightCount, LightColor);
    }
};


//-----------------------------------------------------------------------------------

// Model is a triangular mesh with a fill that can be added to scene.
// 
class Model : public Node
{
public:
    Array<Vertex>     Vertices;
    Array<uint16_t>   Indices;
    PrimitiveType     Type;
    Ptr<ShaderFill>   Fill;
    bool              Visible;	

    // Some renderers will create these if they didn't exist before rendering.
    // Currently they are not updated, so vertex data should not be changed after rendering.
    Ptr<Buffer>       VertexBuffer;
    Ptr<Buffer>       IndexBuffer;

    Model(PrimitiveType t = Prim_Triangles) : Type(t), Fill(NULL), Visible(true) { }
    ~Model() { }

    PrimitiveType GetPrimType() const      { return Type; }

    void          SetVisible(bool visible) { Visible = visible; }
    bool          IsVisible() const        { return Visible; }


    // Node implementation.
    virtual NodeType GetType() const       { return Node_Model; }
    virtual void    Render(const Matrix4f& ltw, RenderDevice* ren);


    // Returns the index next added vertex will have.
    uint16_t GetNextVertexIndex() const
    {
        return (uint16_t)Vertices.GetSize();
    }

    uint16_t AddVertex(const Vertex& v)
    {
        OVR_ASSERT(!VertexBuffer && !IndexBuffer);
        uint16_t index = (uint16_t)Vertices.GetSize();
        Vertices.PushBack(v);
        return index;
    }

    void AddTriangle(uint16_t a, uint16_t b, uint16_t c)
    {
        Indices.PushBack(a);
        Indices.PushBack(b);
        Indices.PushBack(c);
    }

    // Uses texture coordinates for uniform world scaling (must use a repeat sampler).
    void  AddSolidColorBox(float x1, float y1, float z1,
                           float x2, float y2, float z2,
                           Color c);
};


// Container stores a collection of rendering nodes (Models or other containers).
class Container : public Node
{
public:
    Array<Ptr<Node> > Nodes;

    Container()  { }
    ~Container() { }

    virtual NodeType GetType() const { return Node_Container; }

    virtual void Render(const Matrix4f& ltw, RenderDevice* ren);

    void Add(Node *n)  { Nodes.PushBack(n); }	
    void Clear()       { Nodes.Clear(); }	
};


// Scene combines a collection of model 
class Scene : public NewOverrideBase
{
public:
    Container			World;
    Vector4f			LightPos[8];
    LightingParams		Lighting;

public:
    void Render(RenderDevice* ren, const Matrix4f& view);

    void SetAmbient(Vector4f color)
    {
        Lighting.Ambient = color;
    }

    void AddLight(Vector3f pos, Vector4f color)
    {
        int n = (int)Lighting.LightCount;
        OVR_ASSERT(n < 8);
        LightPos[n] = pos;
        Lighting.LightColor[n] = color;
        Lighting.LightCount++;
    }

    void Clear()
    {
        World.Clear();
        Lighting.Ambient = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
        Lighting.LightCount = 0;
    }
};


//-----------------------------------------------------------------------------------

enum DisplayMode
{
    Display_Window     = 0,
    Display_Fullscreen = 1
};


// Rendering parameters used by RenderDevice::CreateDevice.
struct RendererParams
{
    int     Multisample;
    int     Fullscreen;
    // Resolution of the rendering buffer used during creation.
    // Allows buffer of different size then the widow if not zero.
    Sizei   Resolution;

    // Windows - Monitor name for fullscreen mode.
    String  MonitorName;
    // MacOS
    long    DisplayId;

    RendererParams(int ms = 1) : Multisample(ms), Fullscreen(0), Resolution(0) {}

    bool IsDisplaySet() const
    {
        return MonitorName.GetLength() || DisplayId;
    }
};

class RenderDevice : public RefCountBase<RenderDevice>
{


protected:
    int             WindowWidth, WindowHeight;
    RendererParams  Params;

    Matrix4f        Proj;
    Ptr<Buffer>     pTextVertexBuffer;

    // For lighting on platforms with uniform buffers
    Ptr<Buffer>     LightingBuffer;

public:
    enum CompareFunc
    {
        Compare_Always  = 0,
        Compare_Less    = 1,
        Compare_Greater = 2,
        Compare_Count
    };

    Ptr<IDXGIFactory>           DXGIFactory;
    HWND                        Window;

    Ptr<ID3D11Device>           Device;
    Ptr<ID3D11DeviceContext>    Context;
    Ptr<IDXGISwapChain>         SwapChain;
    Ptr<IDXGIAdapter>           Adapter;
    Ptr<IDXGIOutput>            FullscreenOutput;
    int                         FSDesktopX, FSDesktopY;    

    Ptr<ID3D11Texture2D>        BackBuffer;
    Ptr<ID3D11RenderTargetView> BackBufferRT;
    Ptr<Texture>                CurRenderTarget;
    Ptr<Texture>                CurDepthBuffer;
    Ptr<ID3D11RasterizerState>  Rasterizer;
    Ptr<ID3D11BlendState>       BlendState;    
    D3D11_VIEWPORT              D3DViewport;

    Ptr<ID3D11DepthStencilState> DepthStates[1 + 2 * Compare_Count];
    Ptr<ID3D11DepthStencilState> CurDepthState;
    Ptr<ID3D11InputLayout>      ModelVertexIL;

    Ptr<ID3D11SamplerState>     SamplerStates[Sample_Count];

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

    // Slave parameters are used to create a renderer that uses an externally
    // specified device.
    struct SlaveRendererParams
    {
		ID3D11Device*			pDevice;
        ID3D11DeviceContext*    pDeviceContext;
        ID3D11RenderTargetView* pBackBufferRT;
        Sizei                   RTSize;
        int                     Multisample;
    };

    RenderDevice();
    RenderDevice(const RendererParams& p, HWND window);
    RenderDevice(const SlaveRendererParams& p);
    virtual ~RenderDevice();

    // Implement static initializer function to create this class.
    // Creates a new rendering device
    static RenderDevice* CreateDevice(const RendererParams& rp, void* oswnd);
    
    // Creates a "slave" renderer existing device.
    static RenderDevice* CreateSlaveDevice(const SlaveRendererParams& srp);

    

    // Constructor helper
    void  initShadersAndStates();
	void  InitShaders( const char * vertex_shader, const char * pixel_shader, ShaderSet ** pShaders, ID3D11InputLayout ** pVertexIL,
				  D3D11_INPUT_ELEMENT_DESC * DistortionMeshVertexDesc, int num_elements);



    void        UpdateMonitorOutputs();

    void         SetViewport(int x, int y, int w, int h) { SetViewport(Recti(x,y,w,h)); }
    // Set viewport ignoring any adjustments used for the stereo mode.
    virtual void SetViewport(const Recti& vp);
    virtual void SetFullViewport();

    virtual bool SetParams(const RendererParams& newParams);
    const RendererParams& GetParams() const { return Params; }
  
    virtual void Present(bool vsyncEnabled);

    // Waits for rendering to complete; important for reducing latency.
    virtual void WaitUntilGpuIdle();

    // Don't call these directly, use App/Platform instead
    virtual bool SetFullscreen(DisplayMode fullscreen);

    virtual void Clear(float r = 0, float g = 0, float b = 0, float a = 1, float depth = 1);

    // Resources
    virtual Buffer* CreateBuffer();
    virtual Texture* CreateTexture(int format, int width, int height, const void* data, int mipcount=1);

    // Placeholder texture to come in externally
    virtual Texture* CreatePlaceholderTexture(int format);

    virtual ShaderSet* CreateShaderSet() { return new ShaderSet; }

    Texture* GetDepthBuffer(int w, int h, int ms);

    // Begin drawing directly to the currently selected render target, no post-processing.
    virtual void BeginRendering();

    // Begin drawing the primary scene, starting up whatever post-processing may be needed.
    virtual void BeginScene();
    virtual void FinishScene();

    // Texture must have been created with Texture_RenderTarget. Use NULL for the default render target.
    // NULL depth buffer means use an internal, temporary one.
    virtual void SetRenderTarget(Texture* color,
                                 Texture* depth = NULL,
                                 Texture* stencil = NULL);
            void SetDefaultRenderTarget() { SetRenderTarget(NULL, NULL); }
    virtual void SetDepthMode(bool enable, bool write, CompareFunc func = Compare_Less);
    virtual void SetProjection(const Matrix4f& proj);
    virtual void SetWorldUniforms(const Matrix4f& proj);
    // The index 0 is reserved for non-buffer uniforms, and so cannot be used with this function.
    virtual void SetCommonUniformBuffer(int i, Buffer* buffer);
    // The data is not copied, it must remain valid until the end of the frame
    virtual void SetLighting(const LightingParams* light);

    virtual Matrix4f GetProjection() const { return Proj; }

    // This is a View matrix only, it will be combined with the projection matrix from SetProjection
    virtual void Render(const Matrix4f& view, Model* model);
    virtual void Render(const ShaderFill* fill, Buffer* vertices, Buffer* indices,int stride);
    virtual void Render(const ShaderFill* fill, Buffer* vertices, Buffer* indices,int stride,
                        const Matrix4f& matrix, int offset, int count, PrimitiveType prim = Prim_Triangles, bool updateUniformData = true);

    virtual ShaderFill *CreateSimpleFill() { return DefaultFill; }
    ShaderFill *        CreateTextureFill(Texture* tex);

    virtual ShaderBase *LoadBuiltinShader(ShaderStage stage, int shader);

    bool                RecreateSwapChain();
    virtual ID3D10Blob* CompileShader(const char* profile, const char* src, const char* mainName = "main");
 
    ID3D11SamplerState* GetSamplerState(int sm);

    void                SetTexture(ShaderStage stage, int slot, const Texture* t);
};

int GetNumMipLevels(int w, int h);

// Filter an rgba image with a 2x2 box filter, for mipmaps.
// Image size must be a power of 2.
void FilterRgba2x2(const uint8_t* src, int w, int h, uint8_t* dest);

}}


//Anything including this file, uses these
using namespace OVR;
using namespace OVR::RenderTiny;


#endif
