/************************************************************************************

Filename    :   Render_Device.h
Content     :   Platform renderer for simple scene graph
Created     :   September 6, 2012
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
#ifndef OVR_Render_Device_h
#define OVR_Render_Device_h

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_RefCount.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_File.h"
#include "OVR_CAPI.h"

#include "OVR_Stereo.h"

namespace OVR { namespace Render {

class RenderDevice;
struct Font;

//-----------------------------------------------------------------------------------

enum ShaderStage
{
    Shader_Vertex   = 0,
    Shader_Geometry = 1,
    Shader_Fragment = 2,
    Shader_Pixel    = 2,
    Shader_Count    = 3,
};

enum PrimitiveType
{
    Prim_Triangles,
    Prim_Lines,
    Prim_TriangleStrip,
    Prim_Unknown,
    Prim_Count
};

class Fill : public RefCountBase<Fill>
{
public:
    enum Flags
    {
        F_Solid = 1,
        F_Wireframe = 2,
    };

    virtual ~Fill() {}

    virtual void Set(PrimitiveType prim = Prim_Unknown) const = 0;
    virtual void Unset() const {}

    virtual void SetTexture(int i, class Texture* tex, ShaderStage stage = Shader_Pixel) { OVR_UNUSED3(i,tex,stage); }
    virtual Texture* GetTexture(int i, ShaderStage stage = Shader_Pixel) { OVR_UNUSED2(i,stage); return 0; }
};

enum BuiltinShaders
{
    VShader_MV                                      = 0,
    VShader_MVP                                     ,
    VShader_PostProcess                             ,
    VShader_PostProcessMesh                         ,
    VShader_PostProcessMeshTimewarp                 ,
    VShader_PostProcessMeshPositionalTimewarp       ,
    VShader_PostProcessHeightmapTimewarp            ,
    VShader_Count                                   ,
                                                    
    FShader_Solid                                   = 0,
    FShader_Gouraud                                 ,
    FShader_Texture                                 ,
    FShader_AlphaTexture                            ,
    FShader_PostProcessWithChromAb                  ,
    FShader_LitGouraud                              ,
    FShader_LitTexture                              ,
	FShader_MultiTexture                            ,
    FShader_PostProcessMeshWithChromAb              ,
    FShader_PostProcessMeshWithChromAbTimewarp      ,
    FShader_PostProcessMeshWithChromAbPositionalTimewarp ,
    FShader_PostProcessHeightmapTimewarp            ,
    FShader_Count                                   ,
};


enum MapFlags
{
    Map_Discard        = 1,
    Map_Read           = 2, // do not use
    Map_Unsynchronized = 4, // like D3D11_MAP_NO_OVERWRITE
};

enum BufferUsage
{
    Buffer_Unknown  = 0,
    Buffer_Vertex   = 1,
    Buffer_Index    = 2,
    Buffer_Uniform  = 4,
    Buffer_Feedback = 8,
    Buffer_TypeMask = 0xff,
    Buffer_ReadOnly = 0x100, // Buffer must be created with Data().
};

enum TextureFormat
{
    Texture_RGBA            = 0x100,
    Texture_R               = 0x200,
	Texture_A				= 0x400,
	Texture_BGRA            = 0x800,
    Texture_DXT1            = 0x1100,
    Texture_DXT3            = 0x1200,
    Texture_DXT5            = 0x1300,
    Texture_Depth           = 0x8000,
    Texture_TypeMask        = 0xff00,
    Texture_Compressed      = 0x1000,
    Texture_SamplesMask     = 0x00ff,
	Texture_RenderTarget    = 0x10000,
	Texture_SampleDepth		= 0x20000,
    Texture_GenMipmaps      = 0x40000,
};

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

enum MeshType
{
    Mesh_Scene,
    Mesh_Distortion,
    Mesh_Heightmap,
};

struct Color4f
{
    float r, g, b, a;

    Color4f() : r(0), g(0), b(0), a(1) {}
    Color4f(const Vector3f& v) : r(v.x), g(v.y), b(v.z), a(1) {}
    Color4f(float ir, float ig, float ib, float ia) : r(ir), g(ig), b(ib), a(ia) {}
};


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
    virtual bool UseTransposeMatrix() const { return 0; }

protected:
    virtual bool SetUniform(const char* name, int n, const float* v) { OVR_UNUSED3(name, n, v); return false; }
};


// A group of shaders, one per stage.
// Some renderers subclass this, so CreateShaderSet must be used.

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
    bool SetUniform4fv(const char* name, const Vector3f& v)
    {
        const float a[] = {v.x,v.y,v.z,1};
        return SetUniform(name, 4, a);
    }
    bool SetUniform4fvArray(const char* name, int n, const Color4f* v)
    {
        return SetUniform(name, 4*n, &v[0].r);
    }
    virtual bool SetUniform4x4f(const char* name, const Matrix4f& m)
    {
        return SetUniform(name, 16, &m.M[0][0]);
    }
    virtual bool SetUniform3x3f(const char* name, const Matrix4f& m)
    {
        // float3x3 is actually stored the same way as float4x3, with the last items ignored by the code.
        return SetUniform(name, 12, &m.M[0][0]);
    }
};

class ShaderSetMatrixTranspose : public ShaderSet
{
public:
    virtual bool SetUniform4x4f(const char* name, const Matrix4f& m)
    {
        Matrix4f mt = m.Transposed();
        return SetUniform(name, 16, &mt.M[0][0]);
    }
};

class ShaderFill : public Fill
{
    Ptr<ShaderSet> Shaders;
    Ptr<Texture>   Textures[8];
    Ptr<Texture>   VtxTextures[8];

public:
    ShaderFill(ShaderSet* sh) : Shaders(sh) {  }
    ShaderFill(ShaderSet& sh) : Shaders(sh) {  }
    void Set(PrimitiveType prim) const;
    ShaderSet* GetShaders() { return Shaders; }

    virtual void SetTexture(int i, class Texture* tex, ShaderStage stage = Shader_Pixel)
    {
        if (i < 8)
        {
                 if(stage == Shader_Pixel)  Textures[i] = tex;
            else if(stage == Shader_Vertex) VtxTextures[i] = tex;
            else OVR_ASSERT(false);
        }
    }
    virtual Texture* GetTexture(int i, ShaderStage stage = Shader_Pixel)
    {
        if (i < 8)
        {
                 if(stage == Shader_Pixel)      return Textures[i];
            else if(stage == Shader_Vertex)     return VtxTextures[i];
            else OVR_ASSERT(false);             return 0;
        }
        else
        {
            return 0;
        }
    }
};

/* Buffer for vertex or index data. Some renderers require separate buffers, so that
   is recommended. Some renderers cannot have high-performance buffers which are readable,
   so reading in Map should not be relied on.

   Constraints on buffers, such as ReadOnly, are not enforced by the api but may result in 
   rendering-system dependent undesirable behavior, such as terrible performance or unreported failure.

   Use of a buffer inconsistent with usage is also not checked by the api, but it may result in bad
   performance or even failure.

   Use the Data() function to set buffer data the first time, if possible (it may be faster).
*/

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
	
	virtual ovrTexture Get_ovrTexture() = 0;

	virtual void* GetInternalImplementation() { return NULL; };
};

struct RenderTarget
{
    Ptr<Texture>        pColorTex;
    Ptr<Texture>        pDepthTex;
    Sizei               Size;
};

//-----------------------------------------------------------------------------------

class CollisionModel : public RefCountBase<CollisionModel>
{
public:
	Array<Planef > Planes;

	void Add(const Planef& p)
	{
		Planes.PushBack(p);
	}

	// Return whether p is inside this
	bool TestPoint(const Vector3f& p) const;

	// Assumes that the origin of the ray is outside this.
	bool TestRay(const Vector3f& origin, const Vector3f& norm, float& len, Planef* ph = NULL) const;
};

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

    virtual void ClearRenderer() { }

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

struct Vertex
{
    Vector3f  Pos;
    Color     C;
    float     U, V;
	float     U2, V2;
    Vector3f  Norm;

    Vertex (const Vector3f& p, const Color& c = Color(64,0,0,255), 
            float u = 0, float v = 0, Vector3f n = Vector3f(1,0,0))
      : Pos(p), C(c), U(u), V(v), U2(u), V2(v), Norm(n) {}
    Vertex(float x, float y, float z, const Color& c = Color(64,0,0,255),
           float u = 0, float v = 0) : Pos(x,y,z), C(c), U(u), V(v), U2(u), V2(v) { }

	// for multiple UV coords
	Vertex(const Vector3f& p, const Color& c,
           float u, float v, float u2, float v2, Vector3f n) : Pos(p), C(c), U(u), V(v), U2(u2), V2(v2), Norm(n) { }

    bool operator==(const Vertex& b) const
    {
        return Pos == b.Pos && C == b.C && U == b.U && V == b.V;
    }
};

struct DistortionVertex
{
    Vector2f Pos;
    Vector2f TexR;
    Vector2f TexG;
    Vector2f TexB;
    Color Col;
};

struct HeightmapVertex
{
    Vector2f Pos;
    Vector3f Tex;
};

// this is stored in a uniform buffer, don't change it without fixing all renderers
struct LightingParams
{
    Color4f  Ambient;
    Color4f  LightPos[8];       // Not actually colours, but we need the extra element of padding.
    Color4f  LightColor[8];
    float    LightCount;
    int      Version;

    LightingParams() : LightCount(0), Version(0) {}

    void Update(const Matrix4f& view, const Vector3f* SceneLightPos);

    void Set(ShaderSet* s) const;
};

//-----------------------------------------------------------------------------------

class Model : public Node
{
public:
    Array<Vertex>     Vertices;
    Array<UInt16>     Indices;
    PrimitiveType     Type;
    Ptr<class Fill>   Fill;
    bool              Visible;
	bool			  IsCollisionModel;

    // Some renderers will create these if they didn't exist before rendering.
    // Currently they are not updated, so vertex data should not be changed after rendering.
    Ptr<Buffer>       VertexBuffer;
    Ptr<Buffer>       IndexBuffer;

    Model(PrimitiveType t = Prim_Triangles) : Type(t), Fill(NULL), Visible(true) { }
    ~Model() { }

    virtual NodeType GetType() const { return Node_Model; }

    virtual void Render(const Matrix4f& ltw, RenderDevice* ren);

    PrimitiveType GetPrimType() const { return Type; }

    void SetVisible(bool visible) { Visible = visible; }
    bool IsVisible() const        { return Visible; }

    void ClearRenderer()
    {
        VertexBuffer.Clear();
        IndexBuffer.Clear();
    }

    // Returns the index next added vertex will have.
    UInt16 GetNextVertexIndex() const
    {
        return (UInt16)Vertices.GetSize();
    }

    UInt16 AddVertex(const Vertex& v)
    {
		OVR_ASSERT(!VertexBuffer && !IndexBuffer);
		UPInt size = Vertices.GetSize();
		OVR_ASSERT(size <= USHRT_MAX);      // We only use a short to store vert indices.
		UInt16 index = (UInt16) size;
		Vertices.PushBack(v);
		return index;
    }
    UInt16 AddVertex(const Vector3f& v, const Color& c, float u_ = 0, float v_ = 0)
    {
        return AddVertex(Vertex(v,c,u_,v_));
    }
    UInt16 AddVertex(float x, float y, float z, const Color& c, float u, float v)
    {
        return AddVertex(Vertex(Vector3f(x,y,z),c, u,v));
    }

    void AddLine(UInt16 a, UInt16 b)
    {
        Indices.PushBack(a);
        Indices.PushBack(b);
    }

    UInt16 AddVertex(float x, float y, float z, const Color& c,
                     float u, float v, float nx, float ny, float nz)
    {
        return AddVertex(Vertex(Vector3f(x,y,z),c, u,v, Vector3f(nx,ny,nz)));
    }

	UInt16 AddVertex(float x, float y, float z, const Color& c,
                     float u1, float v1, float u2, float v2, float nx, float ny, float nz)
    {
        return AddVertex(Vertex(Vector3f(x,y,z), c, u1, v1, u2, v2, Vector3f(nx,ny,nz)));
    }

    void AddLine(const Vertex& a, const Vertex& b)
    {
        AddLine(AddVertex(a), AddVertex(b));
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


    static Model* CreateAxisFaceColorBox(float x1, float x2, Color xcolor,
                                         float y1, float y2, Color ycolor,
                                         float z1, float z2, Color zcolor);
   

    // Adds box at specified location to current vertices.
    void AddBox(Color c, Vector3f origin, Vector3f size);


    // Uses texture coordinates for exactly covering each surface once.
    static Model* CreateBox(Color c, Vector3f origin, Vector3f size);
    static Model* CreateCylinder(Color c, Vector3f origin, float height, float radius, int sides = 20);
    static Model* CreateCone(Color c, Vector3f origin, float height, float radius, int sides = 20);
    static Model* CreateSphere(Color c, Vector3f origin, float radius, int sides = 20);

    // Grid having halfx,halfy lines in each direction from the origin
    static Model* CreateGrid(Vector3f origin, Vector3f stepx, Vector3f stepy,
                             int halfx, int halfy, int nmajor = 5,
							 Color minor = Color(64,64,64,192), Color major = Color(128,128,128,192));
};

class Container : public Node
{
public:
    Array<Ptr<Node> > Nodes;

    ~Container()
    {

    }

    void ClearRenderer()
    {
        for (UPInt i=0; i< Nodes.GetSize(); i++)
            Nodes[i]->ClearRenderer();
    }

    virtual NodeType GetType() const { return Node_Container; }

    virtual void Render(const Matrix4f& ltw, RenderDevice* ren);

    void Add(Node *n) { Nodes.PushBack(n); }
	void Add(Model *n, class Fill *f) { n->Fill = f; Nodes.PushBack(n); }
    void RemoveLast() { Nodes.PopBack(); }
	void Clear() { Nodes.Clear(); }

	bool               CollideChildren;

	Container() : CollideChildren(1) {}
};

class Scene
{
public:
    Container			World;
    Vector3f			LightPos[8];
    LightingParams		Lighting;
	Array<Ptr<Model> >	Models;

public:
    void Render(RenderDevice* ren, const Matrix4f& view);

    void SetAmbient(Color4f color)
    {
        Lighting.Ambient = color;
    }
    void AddLight(Vector3f pos, Color4f color)
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
		Models.Clear();
		Lighting.Ambient = Color4f(0.0f, 0.0f, 0.0f, 0.0f);
		Lighting.LightCount = 0;
	}

    void ClearRenderer()
    {
        World.ClearRenderer();
    }
};

class SceneView : public Node
{
public:
    Matrix4f GetViewMatrix() const;
};


//-----------------------------------------------------------------------------------

enum RenderCaps
{
    Cap_VertexBuffer = 1,
};

// Post-processing type to apply to scene after rendering. PostProcess_Distortion
// applied distortion as described by DistortionRenderDesc.
enum PostProcessType
{
    PostProcess_None,
    PostProcess_PixelDistortion,
    PostProcess_MeshDistortion,
    PostProcess_MeshDistortionTimewarp,
    PostProcess_MeshDistortionPositionalTimewarp,
    PostProcess_MeshDistortionHeightmapTimewarp,
    PostProcess_NoDistortion,
};

enum DisplayMode
{
    Display_Window = 0,
    Display_Fullscreen = 1,
    Display_FakeFullscreen
};
    
struct DisplayId
{
    // Windows
    String MonitorName; // Monitor name for fullscreen mode
    
    // MacOS
    int   CgDisplayId; // CGDirectDisplayID
    
    DisplayId() : CgDisplayId(0) {}
    DisplayId(int id) : CgDisplayId(id) {}
    DisplayId(String m, int id=0) : MonitorName(m), CgDisplayId(id) {}
    
    operator bool () const
    {
        return MonitorName.GetLength() || CgDisplayId;
    }
    
    bool operator== (const DisplayId& b) const
    {
        return CgDisplayId == b.CgDisplayId &&
            (strstr(MonitorName.ToCStr(), b.MonitorName.ToCStr()) ||
             strstr(b.MonitorName.ToCStr(), MonitorName.ToCStr()));
    }
};

struct RendererParams
{
    int  Multisample;
    int  Fullscreen;
    DisplayId Display;

    RendererParams(int ms = 1) : Multisample(ms), Fullscreen(0) {}
    
    bool IsDisplaySet() const
    {
        return Display;
    }
};



//-----------------------------------------------------------------------------------
// ***** RenderDevice

class RenderDevice : public RefCountBase<RenderDevice>
{
    friend class StereoGeomShaders;
protected:
    int                 WindowWidth, WindowHeight;
    RendererParams      Params;
    Recti               VP;

    Matrix4f            Proj;
    Ptr<Buffer>         pTextVertexBuffer;

    // For rendering with lens warping
    PostProcessType     PostProcessingType;

    Ptr<ShaderSet>      pPostProcessShader;
    Ptr<ShaderSet>      pPostProcessHeightmapShader;
    Ptr<Buffer>         pFullScreenVertexBuffer;
    Color               DistortionClearColor;
    UPInt		        TotalTextureMemoryUsage;
    float               FadeOutBorderFraction;
    
    int                 DistortionMeshNumTris[2];
    Ptr<Buffer>         pDistortionMeshVertexBuffer[2];
    Ptr<Buffer>         pDistortionMeshIndexBuffer[2];

    int                 HeightmapMeshNumTris[2];
    Ptr<Buffer>         pHeightmapMeshVertexBuffer[2];
    Ptr<Buffer>         pHeightmapMeshIndexBuffer[2];

    // For lighting on platforms with uniform buffers
    Ptr<Buffer>         LightingBuffer;

    RenderTarget        HeightmapTimewarpRTs[2];  // one for each eye

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
    virtual void Shutdown();
    virtual bool SetParams(const RendererParams& rp)
    {
        Params = rp;
        return true;
    }

    const RendererParams& GetParams() const { return Params; }

	// Returns details needed by CAPI distortion rendering.
	virtual ovrRenderAPIConfig Get_ovrRenderAPIConfig() const = 0;

    // StereoParams apply Viewport, Projection and Distortion simultaneously,
    // doing full configuration for one eye.
    void        ApplyStereoParams(const StereoEyeParams& params)
    {
        SetViewport(params.RenderedViewport);
        SetProjection(params.RenderedProjection);
    }

    void        ApplyStereoParams(const Recti& vp, const Matrix4f& projection)
    {
        SetViewport(vp);
        SetProjection(projection);
    }
    // Apply "orthographic" stereo parameters used for rendering 2D HUD overlays.
    void        ApplyStereoParams2D(StereoEyeParams const &params, Matrix4f const &ortho)
    {
        SetViewport(params.RenderedViewport);
        SetProjection(ortho);
    }

    


    virtual void SetViewport(const Recti& vp) = 0;
    void         SetViewport(int x, int y, int w, int h) { SetViewport(Recti(x,y,w,h)); }

    virtual void Clear(float r = 0, float g = 0, float b = 0, float a = 1,
                       float depth = 1,
                       bool clearColor = true, bool clearDepth = true) = 0;
    virtual void Rect(float left, float top, float right, float bottom) = 0;

    inline void Clear(const Color &c, float depth = 1)
    {
        float r, g, b, a;
        c.GetRGBA(&r, &g, &b, &a);
        Clear(r, g, b, a, depth);
    }

    virtual bool IsFullscreen() const { return Params.Fullscreen != Display_Window; }
    virtual void Present ( bool withVsync ) = 0;
    // Waits for rendering to complete; important for reducing latency.
    virtual void WaitUntilGpuIdle() { }

    // Resources
    virtual Buffer*  CreateBuffer() { return NULL; }
    virtual Texture* CreateTexture(int format, int width, int height, const void* data, int mipcount=1)
    { OVR_UNUSED5(format,width,height,data, mipcount); return NULL; }
   
    virtual bool     GetSamplePositions(Render::Texture*, Vector3f* pos) { pos[0] = Vector3f(0); return 1; }

    virtual ShaderSet* CreateShaderSet() { return new ShaderSetMatrixTranspose; }
    virtual Shader* LoadBuiltinShader(ShaderStage stage, int shader) = 0;

    // Rendering


    // Begin drawing directly to the currently selected render target, no post-processing.
    virtual void BeginRendering() {}
    // Begin drawing the primary scene, starting up whatever post-processing may be needed.
    virtual void BeginScene(PostProcessType pp = PostProcess_None);
    // Call when any of the stereo options change, so precalculation can happen.
    virtual void PrecalculatePostProcess(PostProcessType pptype,
                                         const StereoEyeParams &stereoParamsLeft, const StereoEyeParams &stereoParamsRight,
                                         const HmdRenderInfo &hmdRenderInfo );
    // Perform postprocessing
    virtual void ApplyPostProcess(Matrix4f const &matNowFromWorldStart,   Matrix4f const &matNowFromWorldEnd,
                                  Matrix4f const &matRenderFromWorldLeft, Matrix4f const &matRenderFromWorldRight,
                                  StereoEyeParams const &stereoParamsLeft, StereoEyeParams const &stereoParamsRight,
                                  RenderTarget* pHmdSpaceLayerRenderTargetLeftOrBothEyes,
                                  RenderTarget* pHmdSpaceLayerRenderTargetRight,
                                  RenderTarget* pStaticLayerRenderTargetLeftOrBothEyes,
                                  RenderTarget* pStaticLayerRenderTargetRight);

    // Finish scene.
    virtual void FinishScene();

    // Texture must have been created with Texture_RenderTarget. Use NULL for the default render target.
    // NULL depth buffer means use an internal, temporary one.
    virtual void SetRenderTarget(Texture* color, Texture* depth = NULL, Texture* stencil = NULL)
    { OVR_UNUSED3(color, depth, stencil); }
    void SetRenderTarget(const RenderTarget& renderTarget)
    {
        SetRenderTarget(renderTarget.pColorTex, renderTarget.pDepthTex);
    }
    // go to back buffer
    void SetDefaultRenderTarget() { SetRenderTarget(NULL, NULL); }
    virtual void SetDepthMode(bool enable, bool write, CompareFunc func = Compare_Less) = 0;
    virtual void SetProjection(const Matrix4f& proj);
    virtual void SetWorldUniforms(const Matrix4f& proj) = 0;

    // The data is not copied, it must remain valid until the end of the frame
    virtual void SetLighting(const LightingParams* light);

    // The index 0 is reserved for non-buffer uniforms, and so cannot be used with this function.
    virtual void SetCommonUniformBuffer(int i, Buffer* buffer) { OVR_UNUSED2(i, buffer); }

    virtual void SetExtraShaders(ShaderSet* s) { OVR_UNUSED(s); }
    virtual Matrix4f GetProjection() const { return Proj; }

    // This is a View matrix only, it will be combined with the projection matrix from SetProjection
    virtual void Render(const Matrix4f& matrix, Model* model) = 0;
    // offset is in bytes; indices can be null.
    virtual void Render(const Fill* fill, Buffer* vertices, Buffer* indices,
                        const Matrix4f& matrix, int offset, int count, PrimitiveType prim = Prim_Triangles, MeshType meshType = Mesh_Scene) = 0;
    virtual void RenderWithAlpha(const Fill* fill, Render::Buffer* vertices, Render::Buffer* indices,
                        const Matrix4f& matrix, int offset, int count, PrimitiveType prim = Prim_Triangles) = 0;

    // Returns width of text in same units as drawing. If strsize is not null, stores width and height.
    // Can optionally return char-range selection rectangle.
    float        MeasureText(const Font* font, const char* str, float size, float strsize[2] = NULL,
                             const UPInt charRange[2] = 0, Vector2f charRangeRect[2] = 0);
    virtual void RenderText(const Font* font, const char* str, float x, float y, float size, Color c);

    virtual void FillRect(float left, float top, float right, float bottom, Color c);
    virtual void RenderLines ( int NumLines, Color c, float *x, float *y, float *z = NULL );
    virtual void FillTexturedRect(float left, float top, float right, float bottom, float ul, float vt, float ur, float vb, Color c, Ptr<Texture> tex);
    virtual void FillGradientRect(float left, float top, float right, float bottom, Color col_top, Color col_btm);
    virtual void RenderImage(float left, float top, float right, float bottom, ShaderFill* image, unsigned char alpha=255);

    virtual Fill *CreateSimpleFill(int flags = Fill::F_Solid) = 0;
    Fill *        CreateTextureFill(Texture* tex, bool useAlpha = false);

    // Sets the color that is applied around distortion.
    void          SetDistortionClearColor(Color clearColor)
    {
        DistortionClearColor = clearColor;
    }

    // Don't call these directly, use App/Platform instead
    virtual bool SetFullscreen(DisplayMode fullscreen)
    {
        Params.Fullscreen = fullscreen;
        return true;
    }
    virtual void SetWindowSize(int w, int h)
	{
		WindowWidth = w;
		WindowHeight = h;	
		VP = Recti( 0, 0, WindowWidth, WindowHeight );
	}

    UPInt GetTotalTextureMemoryUsage() const
    {
        return TotalTextureMemoryUsage;
    }

    enum PostProcessShader
    {
        PostProcessShader_DistortionAndChromAb              = 0,
        PostProcessShader_MeshDistortionAndChromAb,
        PostProcessShader_MeshDistortionAndChromAbTimewarp,
        PostProcessShader_MeshDistortionAndChromAbPositionalTimewarp,
        PostProcessShader_MeshDistortionAndChromAbHeightmapTimewarp,
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

    void SetFadeOutBorderFraction ( float newVal )
    {
        FadeOutBorderFraction = newVal;
    }

    // GPU Profiling
    // using (void) to avoid "unused param" warnings
    virtual void BeginGpuEvent(const char* markerText, UInt32 markerColor) { (void)markerText; (void)markerColor; }
    virtual void EndGpuEvent() { }

protected:
    // Stereo & post-processing
    virtual bool  initPostProcessSupport(PostProcessType pptype);
    
    virtual Shader* CreateStereoShader(PrimitiveType prim, Shader* vs)
    { OVR_UNUSED2(prim, vs); return NULL; }

private:
    PostProcessShader   PostProcessShaderRequested;
    PostProcessShader   PostProcessShaderActive;
};

//-----------------------------------------------------------------------------------
// GPU profile marker helper to encapsulate a given scope block
class AutoGpuProf
{
public:
    AutoGpuProf(RenderDevice* device, const char* markerText, UInt32 color)
        : mDevice(device)
    { device->BeginGpuEvent(markerText, color); }

    // Generates random color if one is not provided
    AutoGpuProf(RenderDevice* device, const char* markerText)
        : mDevice(device)
    { 
        UInt32 color =  ((rand() & 0xFF) << 24) +
                        ((rand() & 0xFF) << 16) +
                        ((rand() & 0xFF) <<  8) +
                         (rand() & 0xFF);
        device->BeginGpuEvent(markerText, color);
    }

    ~AutoGpuProf() { mDevice->EndGpuEvent(); }
         
private:
    RenderDevice* mDevice;
    AutoGpuProf() { };
};
//-----------------------------------------------------------------------------------

int GetNumMipLevels(int w, int h);
int GetTextureSize(int format, int w, int h);

// Filter an rgba image with a 2x2 box filter, for mipmaps.
// Image size must be a power of 2.
void FilterRgba2x2(const UByte* src, int w, int h, UByte* dest);

Texture* LoadTextureTga(RenderDevice* ren, File* f, unsigned char alpha = 255);
Texture* LoadTextureDDS(RenderDevice* ren, File* f);

}}

#endif
