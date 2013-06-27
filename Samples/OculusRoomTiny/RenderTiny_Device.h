/************************************************************************************

Filename    :   RenderTiny_Device.h
Content     :   Minimal possible renderer for RoomTiny sample
Created     :   September 6, 2012
Authors     :   Andrew Reisse, Michael Antonov

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

#ifndef OVR_RenderTiny_Device_h
#define OVR_RenderTiny_Device_h

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_RefCount.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_File.h"
#include "Kernel/OVR_Color.h"

#include "Util/Util_Render_Stereo.h"

namespace OVR { namespace RenderTiny {

using namespace OVR::Util::Render;

class RenderDevice;


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
    VShader_PostProcess             = 2,
    VShader_Count                   = 3,

    FShader_Solid                   = 0,
    FShader_Gouraud                 = 1,
    FShader_Texture                 = 2,    
    FShader_PostProcess             = 3,
    FShader_PostProcessWithChromAb  = 4,
    FShader_LitGouraud              = 5,
    FShader_LitTexture              = 6,
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

// A vector with a dummy w component for alignment in uniform buffers (and for float colors).
// The w component is not used in any calculations.
struct Vector4f : public Vector3f
{
    float w;

    Vector4f() : w(1) {}
    Vector4f(const Vector3f& v) : Vector3f(v), w(1) {}
    Vector4f(float r, float g, float b, float a) : Vector3f(r,g,b), w(a) {}
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

public:
    ShaderFill(ShaderSet* sh) : Shaders(sh) {  }
    ShaderFill(ShaderSet& sh) : Shaders(sh) {  }    

    ShaderSet*  GetShaders() { return Shaders; }

    virtual void Set(PrimitiveType prim = Prim_Unknown) const;   
    virtual void SetTexture(int i, class Texture* tex) { if (i < 8) Textures[i] = tex; }
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
    virtual ~Buffer() {}

    virtual size_t GetSize() = 0;
    virtual void*  Map(size_t start, size_t size, int flags = 0) = 0;
    virtual bool   Unmap(void *m) = 0;

    // Allocates a buffer, optionally filling it with data.
    virtual bool   Data(int use, const void* buffer, size_t size) = 0;
};

class Texture : public RefCountBase<Texture>
{
public:
    virtual ~Texture() {}

    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
    virtual int GetSamples() const { return 1; }

    virtual void SetSampleMode(int sm) = 0;
    virtual void Set(int slot, ShaderStage stage = Shader_Fragment) const = 0;
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
            Mat = Rot;
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
    Array<UInt16>     Indices;
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
    UInt16 GetNextVertexIndex() const
    {
        return (UInt16)Vertices.GetSize();
    }

    UInt16 AddVertex(const Vertex& v)
    {
        assert(!VertexBuffer && !IndexBuffer);
        UInt16 index = (UInt16)Vertices.GetSize();
        Vertices.PushBack(v);
        return index;
    }

    void AddTriangle(UInt16 a, UInt16 b, UInt16 c)
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
class Scene
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

// Post-processing type to apply to scene after rendering. PostProcess_Distortion
// applied distortion as described by DistortionConfig.
enum PostProcessType
{
    PostProcess_None,
    PostProcess_Distortion
};

enum DisplayMode
{
    Display_Window     = 0,
    Display_Fullscreen = 1
};
    

// Rendering parameters used by RenderDevice::CreateDevice.
struct RendererParams
{
    int  Multisample;
    int  Fullscreen;

    // Windows - Monitor name for fullscreen mode.
    String MonitorName;
    // MacOS
    long   DisplayId;

    RendererParams(int ms = 1) : Multisample(ms), Fullscreen(0) {}
    
    bool IsDisplaySet() const
    {
        return MonitorName.GetLength() || DisplayId;
    }
};



//-----------------------------------------------------------------------------------
// ***** RenderDevice

// Rendering device abstraction.
// Provides platform-independent part of implementation, with platform-specific
// part being in a separate derived class/file, such as D3D10::RenderDevice.
// 
class RenderDevice : public RefCountBase<RenderDevice>
{    
protected:
    int             WindowWidth, WindowHeight;
    RendererParams  Params;
    Viewport        VP;

    Matrix4f        Proj;
    Ptr<Buffer>     pTextVertexBuffer;

    // For rendering with lens warping
    PostProcessType CurPostProcess;
    Ptr<Texture>    pSceneColorTex;  // Distortion render target, both eyes.
    int             SceneColorTexW;
    int             SceneColorTexH;
    Ptr<ShaderSet>  pPostProcessShader;
    Ptr<Buffer>     pFullScreenVertexBuffer;
    float           SceneRenderScale;
    DistortionConfig Distortion;    

    // For lighting on platforms with uniform buffers
    Ptr<Buffer>     LightingBuffer;

    void FinishScene1();

public:
    enum CompareFunc
    {
        Compare_Always  = 0,
        Compare_Less    = 1,
        Compare_Greater = 2,
        Compare_Count
    };
    RenderDevice();
    virtual ~RenderDevice() { Shutdown(); }

    // This static function is implemented in each derived class
    // to support a specific renderer type.
    //static RenderDevice* CreateDevice(const RendererParams& rp, void* oswnd);


    virtual void Init() {}
    virtual void Shutdown() {}
    virtual bool SetParams(const RendererParams&) { return 0; }

    const RendererParams& GetParams() const { return Params; }

    
    // StereoParams apply Viewport, Projection and Distortion simultaneously,
    // doing full configuration for one eye.
    void        ApplyStereoParams(const StereoEyeParams& params)
    {
        SetViewport(params.VP);
        SetProjection(params.Projection);
        if (params.pDistortion)
            SetDistortionConfig(*params.pDistortion, params.Eye);
    }

    virtual void SetViewport(const Viewport& vp);
    void         SetViewport(int x, int y, int w, int h) { SetViewport(Viewport(x,y,w,h)); }

    // PostProcess distortion
    void          SetSceneRenderScale(float ss);

    void          SetDistortionConfig(const DistortionConfig& config, StereoEye eye = StereoEye_Left)
    {
        Distortion = config;
        if (eye == StereoEye_Right)
            Distortion.XCenterOffset = -Distortion.XCenterOffset;
    }
   
    // Set viewport ignoring any adjustments used for the stereo mode.
    virtual void SetRealViewport(const Viewport& vp) = 0;    

    virtual void Clear(float r = 0, float g = 0, float b = 0, float a = 1, float depth = 1) = 0;   
 
    virtual bool IsFullscreen() const { return Params.Fullscreen != Display_Window; }
    virtual void Present() = 0;
    // Waits for rendering to complete; important for reducing latency.
    virtual void ForceFlushGPU() { }

    // Resources
    virtual Buffer*  CreateBuffer() { return NULL; }
    virtual Texture* CreateTexture(int format, int width, int height, const void* data, int mipcount=1)
    { OVR_UNUSED5(format,width,height,data, mipcount); return NULL; }
    

    virtual ShaderSet* CreateShaderSet() { return new ShaderSet; }
    virtual Shader*    LoadBuiltinShader(ShaderStage stage, int shader) = 0;

    // Rendering

    // Begin drawing directly to the currently selected render target, no post-processing.
    virtual void BeginRendering() {}
    // Begin drawing the primary scene. This will have post-processing applied (if enabled)
    // during FinishScene.
    virtual void BeginScene(PostProcessType pp = PostProcess_None);
    // Postprocess the scene and return to the screen render target.
    virtual void FinishScene();

    // Texture must have been created with Texture_RenderTarget. Use NULL for the default render target.
    // NULL depth buffer means use an internal, temporary one.
    virtual void SetRenderTarget(Texture* color, Texture* depth = NULL, Texture* stencil = NULL)
    { OVR_UNUSED3(color, depth, stencil); }
    virtual void SetDepthMode(bool enable, bool write, CompareFunc func = Compare_Less) = 0;
    virtual void SetProjection(const Matrix4f& proj);
    virtual void SetWorldUniforms(const Matrix4f& proj) = 0;

    // The data is not copied, it must remain valid until the end of the frame
    virtual void SetLighting(const LightingParams* light);

    // The index 0 is reserved for non-buffer uniforms, and so cannot be used with this function.
    virtual void SetCommonUniformBuffer(int i, Buffer* buffer) { OVR_UNUSED2(i, buffer); }
    
    virtual Matrix4f GetProjection() const { return Proj; }

    // This is a View matrix only, it will be combined with the projection matrix from SetProjection
    virtual void Render(const Matrix4f& matrix, Model* model) = 0;
    // offset is in bytes; indices can be null.
    virtual void Render(const ShaderFill* fill, Buffer* vertices, Buffer* indices,
                        const Matrix4f& matrix, int offset, int count, PrimitiveType prim = Prim_Triangles) = 0;

    virtual ShaderFill *CreateSimpleFill() = 0;
    ShaderFill *        CreateTextureFill(Texture* tex);

 
    // Don't call these directly, use App/Platform instead
    virtual bool SetFullscreen(DisplayMode fullscreen) { OVR_UNUSED(fullscreen); return false; }    
    

    enum PostProcessShader
    {
        PostProcessShader_Distortion                = 0,
        PostProcessShader_DistortionAndChromAb      = 1,
        PostProcessShader_Count
    };

    PostProcessShader GetPostProcessShader()
    {
        return PostProcessShaderActive;
    }

    void SetPostProcessShader(PostProcessShader newShader)
    {
        PostProcessShaderRequested = newShader;
    }

protected:
    // Stereo & post-processing
    virtual bool  initPostProcessSupport(PostProcessType pptype);
   
private:
    PostProcessShader   PostProcessShaderRequested;
    PostProcessShader   PostProcessShaderActive;
};

int GetNumMipLevels(int w, int h);

// Filter an rgba image with a 2x2 box filter, for mipmaps.
// Image size must be a power of 2.
void FilterRgba2x2(const UByte* src, int w, int h, UByte* dest);

}}  // OVR::RenderTiny

#endif
