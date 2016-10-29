/************************************************************************************

Filename    :   Render_Device.h
Content     :   Platform renderer for simple scene graph
Created     :   September 6, 2012
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
#ifndef OVR_Render_Device_h
#define OVR_Render_Device_h

#include "Extras/OVR_Math.h"
#include "Kernel/OVR_RefCount.h"
#include "Kernel/OVR_File.h"
#include "Kernel/OVR_Color.h"
#include "OVR_CAPI.h"

#include <vector>
#include <string>

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
    Shader_Compute  = 3,
    Shader_Count    = 4,
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

#define LIST_VERTEX_SHADERS(_) \
_(MV) \
_(MVP)

#define LIST_FRAGMENT_SHADERS(_) \
_(Solid) \
_(Gouraud) \
_(Texture) \
_(TextureNoClip) \
_(AlphaTexture) \
_(AlphaBlendedTexture) \
_(AlphaPremultTexture) \
_(LitGouraud) \
_(LitTexture) \
_(MultiTexture)

enum BuiltinVertexShaders
{
    #define MK_VERTEX_SHADER_ENUM(name) VShader_##name,
    LIST_VERTEX_SHADERS(MK_VERTEX_SHADER_ENUM)
    #undef MK_VERTEX_SHADER_ENUM
    VShader_Count
};

enum BuiltinFragmentShaders
{
    #define MK_FRAGMENT_SHADER_ENUM(name) FShader_##name,
    LIST_FRAGMENT_SHADERS(MK_FRAGMENT_SHADER_ENUM)
    #undef MK_FRAGMENT_SHADER_ENUM
    FShader_Count
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
    Buffer_Compute  = 16,
    Buffer_TypeMask = 0xff,
    Buffer_ReadOnly = 0x100, // Buffer must be created with Data().
};

enum TextureFormat
{
    //////////////////////////////////////////////////////////////////////////
    // These correspond to the available OVR_FORMAT enum except sRGB versions
    // which are enabled via the Texture_SRGB flag
    Texture_B5G6R5          = 0x10, // not supported as requires feature level DirectX 11.1
    Texture_BGR5A1          = 0x20, // not supported as requires feature level DirectX 11.1
    Texture_BGRA4           = 0x30, // not supported as requires feature level DirectX 11.1
    Texture_RGBA8           = 0x40, // allows sRGB
    Texture_BGRA8           = 0x50, // allows sRGB
    Texture_BGRX            = 0x60, // allows sRGB
    Texture_RGBA16f         = 0x70,
    // End of OVR_FORMAT corresponding formats
    //////////////////////////////////////////////////////////////////////////

    Texture_RGBA            = Texture_RGBA8,
    Texture_BGRA            = Texture_BGRA8,
    Texture_R               = 0x100,
    Texture_A               = 0x110,
    Texture_BC1             = 0x210,
    Texture_BC2             = 0x220,
    Texture_BC3             = 0x230,
    Texture_BC6S            = 0x240,
    Texture_BC6U            = 0x241,
    Texture_BC7             = 0x250,
    
    Texture_Depth32f        = 0x10000,   // aliased as default Texture_Depth
    Texture_Depth24Stencil8 = 0x20000,
    Texture_Depth32fStencil8= 0x40000,
    Texture_Depth16         = 0x80000,
	
    Texture_DepthMask       = 0xf0000,
    Texture_TypeMask        = 0xffff0,
    Texture_Compressed      = 0x200,
    Texture_SamplesMask     = 0x000f,

    Texture_RenderTarget    = 0x100000,
	Texture_SampleDepth		= 0x200000,
    Texture_GenMipmaps      = 0x400000,
    Texture_SRGB			= 0x800000,
    Texture_Mirror          = 0x1000000,
    Texture_SwapTextureSet  = 0x2000000,
    Texture_SwapTextureSetStatic  = 0x4000000,
    Texture_Hdcp            = 0x8000000,
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
    Ptr<Texture>   CsTextures[8];

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
            else if(stage == Shader_Compute) CsTextures[i] = tex;
            else OVR_ASSERT(false);
        }
    }
    virtual Texture* GetTexture(int i, ShaderStage stage = Shader_Pixel)
    {
        if (i < 8)
        {
                 if(stage == Shader_Pixel)      return Textures[i];
            else if(stage == Shader_Vertex)     return VtxTextures[i];
            else if(stage == Shader_Compute)    return CsTextures[i];
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
    virtual int GetFormat() const = 0;

    virtual void SetSampleMode(int sm) = 0;
    virtual void Set(int slot, ShaderStage stage = Shader_Fragment) const = 0;
	
    virtual ovrTextureSwapChain Get_ovrTextureSet() = 0;

    virtual void GenerateMips() = 0;
    // Used to commit changes to the texture swap chain
    virtual void Commit() = 0;
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
	std::vector<Planef > Planes;

	void Add(const Planef& p)
	{
		Planes.push_back(p);
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

    Vertex (const Vector3f& p = Vector3f::Zero(), const Color& c = Color(64,0,0,255), 
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

/*
struct DistortionVertex
{
    Vector2f Pos;
    Vector2f TexR;
    Vector2f TexG;
    Vector2f TexB;
    Color Col;
};
*/
struct DistortionComputePin   /*TPH pointlessly existing*/    // Needs to match the one(s) declared in Render_D3D1X_Device.cpp inside the shaders.
{
    Vector2f TanEyeAnglesR;
    Vector2f TanEyeAnglesG;
    Vector2f TanEyeAnglesB;
    Color Col;
    int padding[1];
};

/*struct HeightmapVertex
{
    Vector2f Pos;
    Vector3f Tex;
};*/

// this is stored in a uniform buffer, don't change it without fixing all renderers
struct LightingParams
{
    Color4f  Ambient;
    Color4f  LightPos[8];       // Not actually colors, but we need the extra element of padding.
    Color4f  LightColor[8];
    float    LightCount;
    int      Version;

    LightingParams() : LightCount(0), Version(0) {}

    void Update(const Matrix4f& view, const Vector3f* SceneLightPos);

    void Set(ShaderSet* s) const;
};

class AdapterNotFoundException
{

};

class SwapChainCreationFailedException
{

};

//-----------------------------------------------------------------------------------

class Model : public Node
{
public:
    std::string             AssetName;
    std::vector<Vertex>     Vertices;
    std::vector<uint16_t>   Indices;
    PrimitiveType           Type;
    Ptr<class Fill>         Fill;
    bool                    Visible;
    bool                    IsCollisionModel;

    // Some renderers will create these if they didn't exist before rendering.
    // Currently they are not updated, so vertex data should not be changed after rendering.
    Ptr<Buffer>       VertexBuffer;
    Ptr<Buffer>       IndexBuffer;

    Model(PrimitiveType t = Prim_Triangles, const char* assetName = nullptr)
        : AssetName(), Type(t), Fill(NULL), Visible(true), IsCollisionModel(false)
    {
        AssetName = "Model: ";
        if (assetName)
        {
            AssetName.append(assetName);
        }
    }
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
    uint16_t GetNextVertexIndex() const
    {
        return (uint16_t)Vertices.size();
    }

    uint16_t AddVertex(const Vertex& v)
    {
		OVR_ASSERT(!VertexBuffer && !IndexBuffer);
		size_t size = Vertices.size();
		OVR_ASSERT(size <= USHRT_MAX);      // We only use a short to store vert indices.
		uint16_t index = (uint16_t) size;
		Vertices.push_back(v);
		return index;
    }
    uint16_t AddVertex(const Vector3f& v, const Color& c, float u_ = 0, float v_ = 0)
    {
        return AddVertex(Vertex(v,c,u_,v_));
    }
    uint16_t AddVertex(float x, float y, float z, const Color& c, float u, float v)
    {
        return AddVertex(Vertex(Vector3f(x,y,z),c, u,v));
    }

    void AddLine(uint16_t a, uint16_t b)
    {
        Indices.push_back(a);
        Indices.push_back(b);
    }

	void AddQuad(Vertex v0, Vertex v1, Vertex v2, Vertex v3)
	{
		int t = GetNextVertexIndex();
		AddVertex(v0);
		AddVertex(v1);
		AddVertex(v2);
		AddVertex(v3);
		AddTriangle(uint16_t(t), uint16_t(t + 3), uint16_t(t + 1));
		AddTriangle(uint16_t(t), uint16_t(t + 2), uint16_t(t + 3));
	}


    uint16_t AddVertex(float x, float y, float z, const Color& c,
                     float u, float v, float nx, float ny, float nz)
    {
        return AddVertex(Vertex(Vector3f(x,y,z),c, u,v, Vector3f(nx,ny,nz)));
    }

	uint16_t AddVertex(float x, float y, float z, const Color& c,
                     float u1, float v1, float u2, float v2, float nx, float ny, float nz)
    {
        return AddVertex(Vertex(Vector3f(x,y,z), c, u1, v1, u2, v2, Vector3f(nx,ny,nz)));
    }

    void AddLine(const Vertex& a, const Vertex& b)
    {
        AddLine(AddVertex(a), AddVertex(b));
    }

    void AddTriangle(uint16_t a, uint16_t b, uint16_t c)
    {
        Indices.push_back(a);
        Indices.push_back(b);
        Indices.push_back(c);
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
    std::vector<Ptr<Node> > Nodes;

    ~Container()
    {

    }

    void ClearRenderer()
    {
        for (size_t i=0; i< Nodes.size(); i++)
            Nodes[i]->ClearRenderer();
    }

    virtual NodeType GetType() const { return Node_Container; }

    virtual void Render(const Matrix4f& ltw, RenderDevice* ren);

    void Add(Node *n) { Nodes.push_back(n); }
    void Add(Model *n, class Fill *f) { n->Fill = f; Nodes.push_back(n); }
    void RemoveLast() { Nodes.pop_back(); }
    void Clear() { Nodes.clear(); }

	bool               CollideChildren;

	Container() : CollideChildren(1) {}
};

class Scene
{
public:
    Container                   World;
    Vector3f                    LightPos[8];
    LightingParams	            Lighting;
    std::vector<Ptr<Model> >	Models;

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
		Models.clear();
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


enum RenderAPIType
{
    RenderAPI_None   = 0,       ///< No API
    RenderAPI_OpenGL = 1,       ///< OpenGL
    RenderAPI_D3D11  = 2,       ///< DirectX 11
    RenderAPI_D3D12  = 3        ///< DirectX 12
};


struct RendererParams
{
    RenderAPIType    RenderAPI;                  // RenderAPI_D3D11, RenderAPI_OpenGL, etc.
    bool             SrgbBackBuffer;             // If true then an SRGB back buffer is requested.
    Sizei            Resolution;                 // Resolution of the rendering buffer used during creation. Allows buffer of different size then the widow if not zero.
    bool             DebugEnabled;               // If true then the renderer (e.g. DirectX, OpenGL) is created with debug support enabled.

    // Relevant only when OpenGL is used.
    int              GLMajorVersion;             // Requested OpenGL major version (WGL_CONTEXT_MAJOR_VERSION_ARB).
    int              GLMinorVersion;             // Requested OpenGL minor version (WGL_CONTEXT_MINOR_VERSION_ARB).
    bool             GLCoreProfile;              // True if a core profile context was requested (WGL_CONTEXT_CORE_PROFILE_BIT_ARB).
    bool             GLCompatibilityProfile;     // True if a compatibility profile context was requested (WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB).
    bool             GLForwardCompatibleProfile; // True if a forward compatible context was requested (WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB).

    RendererParams() :
        RenderAPI(RenderAPI_None), SrgbBackBuffer(false), Resolution(0), DebugEnabled(false),
        GLMajorVersion(2), GLMinorVersion(1), GLCoreProfile(false), GLCompatibilityProfile(false), GLForwardCompatibleProfile(false){}
};



//-----------------------------------------------------------------------------------
// ***** RenderDevice

class RenderDevice : public RefCountBase<RenderDevice>
{
    friend class StereoGeomShaders;
protected:
    ovrSession          Session;
    int                 WindowWidth, WindowHeight;
    RendererParams      Params;
    Recti               VP;

    Matrix4f            Proj;
    Vector4f            GlobalTint;
    Ptr<Buffer>         pTextVertexBuffer;

    size_t		        TotalTextureMemoryUsage;

    // For lighting on platforms with uniform buffers
    Ptr<Buffer>         LightingBuffer;

public:
    enum CompareFunc
    {
        Compare_Always  = 0,
        Compare_Less    = 1,
        Compare_Greater = 2,
        Compare_Count
    };

    RenderDevice(ovrSession session);
    virtual ~RenderDevice();

    // This static function is implemented in each derived class
    // to support a specific renderer type.
    //static RenderDevice* CreateDevice(const RendererParams& rp, void* oswnd);

    virtual void Shutdown();
    virtual bool SetParams(const RendererParams& rp) = 0;

    // Called to clear out texture fills by the app layer before it exits
    virtual void DeleteFills() = 0;

    const RendererParams& GetParams() const { return Params; }

    // StereoParams apply Viewport, Projection and Distortion simultaneously,
    // doing full configuration for one eye.
    void ApplyStereoParams(const Recti& vp, const Matrix4f& projection)
    {
        SetViewport(vp);
        SetProjection(projection);
    }

    virtual void SetViewport(const Recti& vp) = 0;
    void         SetViewport(int x, int y, int w, int h) { SetViewport(Recti(x,y,w,h)); }

    virtual void Clear(float r = 0, float g = 0, float b = 0, float a = 1,
                       float depth = 1,
                       bool clearColor = true, bool clearDepth = true) = 0;

    inline void Clear(const Color &c, float depth = 1)
    {
        float r, g, b, a;
        c.GetRGBA(&r, &g, &b, &a);
        Clear(r, g, b, a, depth);
    }

    virtual bool Present ( bool withVsync ) = 0;
    // Waits for rendering to complete; important for reducing latency.
    virtual void Flush() = 0;

    // Resources
    virtual Buffer*  CreateBuffer() = 0;
    virtual Texture* CreateTexture(int format, int width, int height, const void* data, int mipcount = 1, ovrResult* error = nullptr) = 0;

    virtual ShaderSet* CreateShaderSet() { return new ShaderSetMatrixTranspose; }
    virtual Shader* LoadBuiltinShader(ShaderStage stage, int shader) = 0;

    virtual void Blt(Texture* texture) = 0;

    // Begin drawing directly to the currently selected render target, no post-processing.
    virtual void BeginRendering() = 0;
    // Begin drawing the primary scene.
    void BeginScene();

    // Finish scene.
    void FinishScene();

    virtual void ResolveMsaa(Texture* msaaTex, Texture* outputTex) = 0;

    // Texture must have been created with Texture_RenderTarget. Use NULL for the default render target.
    // NULL depth buffer means use an internal, temporary one.
    virtual void SetRenderTarget(Texture* color, Texture* depth = nullptr, Texture* stencil = nullptr) = 0;
    void SetRenderTarget(const RenderTarget& renderTarget)
    {
        SetRenderTarget(renderTarget.pColorTex, renderTarget.pDepthTex);
    }
    // go to back buffer
    void SetDefaultRenderTarget() { SetRenderTarget(nullptr, nullptr); }
    virtual void SetDepthMode(bool enable, bool write, CompareFunc func = Compare_Less) = 0;
    void SetProjection(const Matrix4f& proj);
    void SetGlobalTint(const Vector4f& globalTint);
    virtual void SetWorldUniforms(const Matrix4f& proj, const Vector4f& globalTint) = 0;

    // The data is not copied, it must remain valid until the end of the frame
    virtual void SetLighting(const LightingParams* light);

    enum CullMode
    {
        Cull_Off,
        Cull_Back,
        Cull_Front,
    };

    virtual void SetCullMode(CullMode cullMode) = 0;

    // The index 0 is reserved for non-buffer uniforms, and so cannot be used with this function.
    virtual void SetCommonUniformBuffer(int i, Buffer* buffer) { OVR_UNUSED2(i, buffer); }

    // This is a View matrix only, it will be combined with the projection matrix from SetProjection
    virtual void Render(const Matrix4f& matrix, Model* model) = 0;
    // offset is in bytes; indices can be null.
    virtual void Render(const Fill* fill, Buffer* vertices, Buffer* indices,
                        const Matrix4f& matrix, int offset, int count,
                        PrimitiveType prim = Prim_Triangles) = 0;
	virtual void RenderWithAlpha(const Fill* fill, Render::Buffer* vertices, Render::Buffer* indices,
		const Matrix4f& matrix, int offset, int count, PrimitiveType prim = Prim_Triangles) = 0;

    // Returns width of text in same units as drawing. If strsize is not null, stores width and height.
    // Can optionally return char-range selection rectangle.
    static float MeasureText(const Font* font, const char* str, float size, float strsize[2] = NULL,
                             const size_t charRange[2] = 0, Vector2f charRangeRect[2] = 0);
    virtual void RenderText(const Font* font, const char* str, float x, float y, float size, Color c, const Matrix4f* view = NULL);

    virtual void FillRect(float left, float top, float right, float bottom, Color c, const Matrix4f* view = NULL);
    void RenderLines ( int NumLines, Color c, float *x, float *y, float *z = nullptr );
    virtual void FillTexturedRect(
        float left, float top, float right, float bottom, float ul, float vt, float ur, float vb,
        Color c, Ptr<Texture> tex, const Matrix4f* view, bool premultAlpha = false);
    virtual void FillGradientRect(float left, float top, float right, float bottom, Color col_top, Color col_btm, const Matrix4f* view);
    virtual void RenderImage(float left, float top, float right, float bottom, ShaderFill* image, unsigned char alpha=255, const Matrix4f* view = nullptr);

    virtual Fill *GetSimpleFill(int flags = Fill::F_Solid) = 0;
    virtual Fill *GetTextureFill(Texture* tex, bool useAlpha = false, bool usePremult = false) = 0;
    Fill *        CreateTextureFill(Texture* tex, bool useAlpha = false, bool usePremult = false);

    virtual void SetWindowSize(int w, int h)
	{
		WindowWidth = w;
		WindowHeight = h;	
		VP = Recti( 0, 0, WindowWidth, WindowHeight );
	}

    size_t GetTotalTextureMemoryUsage() const
    {
        return TotalTextureMemoryUsage;
    }

    // GPU Profiling
    // using (void) to avoid "unused param" warnings
    virtual void BeginGpuEvent(const char* markerText, uint32_t markerColor) { (void)markerText; (void)markerColor; }
    virtual void EndGpuEvent() { }

private:
};

//-----------------------------------------------------------------------------------
// GPU profile marker helper to encapsulate a given scope block
class AutoGpuProf
{
public:
    AutoGpuProf(RenderDevice* device, const char* markerText, uint32_t color)
        : mDevice(device)
    { device->BeginGpuEvent(markerText, color); }

    // Generates random color if one is not provided
    AutoGpuProf(RenderDevice* device, const char* markerText)
        : mDevice(device)
    { 
        uint32_t color =  ((rand() & 0xFF) << 24) +
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
void FilterRgba2x2(const uint8_t* src, int w, int h, uint8_t* dest);

enum TextureLoadFlags
{
    TextureLoad_SrgbAware           = 0x0001,
    TextureLoad_Anisotropic         = 0x0002,
    TextureLoad_MakePremultAlpha    = 0x0004,
    TextureLoad_SwapTextureSet      = 0x0008,
    TextureLoad_StoreCompressed     = 0x0010,
    TextureLoad_Hdcp                = 0x0020,
};

Texture* LoadTextureTgaTopDown (RenderDevice* ren, File* f, int textureLoadFlags, unsigned char alpha = 255);
Texture* LoadTextureTgaBottomUp(RenderDevice* ren, File* f, int textureLoadFlags, unsigned char alpha = 255);
Texture* LoadTextureDDSTopDown (RenderDevice* ren, File* f, int textureLoadFlags);


}} // namespace OVR::Render

#endif
