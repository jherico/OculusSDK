/************************************************************************************

Filename    :   Render_GL_Device.h
Content     :   RenderDevice implementation header for OpenGL
Created     :   September 10, 2012
Authors     :   Andrew Reisse, David Borel

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

#ifndef OVR_Render_GL_Device_h
#define OVR_Render_GL_Device_h

#include "../Render/Render_Device.h"

#if defined(OVR_OS_WIN32)
    #include <Windows.h>
    #include <GL/gl.h>
    #include <GL/glext.h>
    #include <GL/wglext.h>
#elif defined(OVR_OS_MAC)
    #include <OpenGL/gl3.h>
    #include <OpenGL/gl3ext.h>
#else
    #include <GL/gl.h>
    #include <GL/glext.h>
    #include <GL/glx.h>
#endif


namespace OVR { namespace Render { namespace GL {
    
#if !defined(OVR_OS_MAC)

// GL extension Hooks for PC.
#if defined(OVR_OS_WIN32)

extern PFNWGLGETSWAPINTERVALEXTPROC             wglGetSwapIntervalEXT;
extern PFNWGLSWAPINTERVALEXTPROC                wglSwapIntervalEXT;
extern PFNWGLCHOOSEPIXELFORMATARBPROC           wglChoosePixelFormatARB;
extern PFNWGLCREATECONTEXTATTRIBSARBPROC        wglCreateContextAttribsARB;

#elif defined(OVR_OS_LINUX)

extern PFNGLXSWAPINTERVALEXTPROC                glXSwapIntervalEXT;

#endif

extern PFNGLGENFRAMEBUFFERSPROC                 glGenFramebuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC              glDeleteFramebuffers;
extern PFNGLDELETESHADERPROC                    glDeleteShader;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC          glCheckFramebufferStatus;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC         glFramebufferRenderbuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC            glFramebufferTexture2D;
extern PFNGLBINDFRAMEBUFFEREXTPROC              glBindFramebuffer;
extern PFNGLACTIVETEXTUREPROC                   glActiveTexture;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC        glDisableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC             glVertexAttribPointer;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC         glEnableVertexAttribArray;
extern PFNGLBINDBUFFERPROC                      glBindBuffer;
extern PFNGLUNIFORMMATRIX4FVPROC                glUniformMatrix4fv;
extern PFNGLDELETEBUFFERSPROC                   glDeleteBuffers;
extern PFNGLBUFFERDATAPROC                      glBufferData;
extern PFNGLGENBUFFERSPROC                      glGenBuffers;
extern PFNGLMAPBUFFERPROC                       glMapBuffer;
extern PFNGLUNMAPBUFFERPROC                     glUnmapBuffer;
extern PFNGLGETSHADERINFOLOGPROC                glGetShaderInfoLog;
extern PFNGLGETSHADERIVPROC                     glGetShaderiv;
extern PFNGLCOMPILESHADERPROC                   glCompileShader;
extern PFNGLSHADERSOURCEPROC                    glShaderSource;
extern PFNGLCREATESHADERPROC                    glCreateShader;
extern PFNGLCREATEPROGRAMPROC                   glCreateProgram;
extern PFNGLATTACHSHADERPROC                    glAttachShader;
extern PFNGLDETACHSHADERPROC                    glDetachShader;
extern PFNGLDELETEPROGRAMPROC                   glDeleteProgram;
extern PFNGLUNIFORM1IPROC                       glUniform1i;
extern PFNGLGETUNIFORMLOCATIONPROC              glGetUniformLocation;
extern PFNGLGETACTIVEUNIFORMPROC                glGetActiveUniform;
extern PFNGLUSEPROGRAMPROC                      glUseProgram;
extern PFNGLGETPROGRAMINFOLOGPROC               glGetProgramInfoLog;
extern PFNGLGETPROGRAMIVPROC                    glGetProgramiv;
extern PFNGLLINKPROGRAMPROC                     glLinkProgram;
extern PFNGLBINDATTRIBLOCATIONPROC              glBindAttribLocation;
extern PFNGLUNIFORM4FVPROC                      glUniform4fv;
extern PFNGLUNIFORM3FVPROC                      glUniform3fv;
extern PFNGLUNIFORM2FVPROC                      glUniform2fv;
extern PFNGLUNIFORM1FVPROC                      glUniform1fv;
extern PFNGLCOMPRESSEDTEXIMAGE2DPROC            glCompressedTexImage2D;
extern PFNGLRENDERBUFFERSTORAGEPROC             glRenderbufferStorage;
extern PFNGLBINDRENDERBUFFERPROC                glBindRenderbuffer;
extern PFNGLGENRENDERBUFFERSPROC                glGenRenderbuffers;
extern PFNGLDELETERENDERBUFFERSPROC             glDeleteRenderbuffers;
extern PFNGLGENVERTEXARRAYSPROC                 glGenVertexArrays;
extern PFNGLDELETEVERTEXARRAYSPROC              glDeleteVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC                 glBindVertexArray;

extern void InitGLExtensions();

#endif

class RenderDevice;

class Buffer : public Render::Buffer
{
public:
    RenderDevice* Ren;
    size_t        Size;
    GLenum        Use;
    GLuint        GLBuffer;

public:
    Buffer(RenderDevice* r) : Ren(r), Size(0), Use(0), GLBuffer(0) {}
    ~Buffer();

    GLuint         GetBuffer() { return GLBuffer; }

    virtual size_t GetSize() { return Size; }
    virtual void*  Map(size_t start, size_t size, int flags = 0);
    virtual bool   Unmap(void *m);
    virtual bool   Data(int use, const void* buffer, size_t size);
};

class Texture : public Render::Texture
{
public:
    RenderDevice* Ren;
    GLuint        TexId;
    int           Width, Height;

    Texture(RenderDevice* r, int w, int h);
    ~Texture();

    virtual int GetWidth() const { return Width; }
    virtual int GetHeight() const { return Height; }

    virtual void SetSampleMode(int);
	virtual ovrTexture Get_ovrTexture();

    virtual void Set(int slot, ShaderStage stage = Shader_Fragment) const;
};

class Shader : public Render::Shader
{
public:
    GLuint      GLShader;

    Shader(RenderDevice*, ShaderStage st, GLuint s) : Render::Shader(st), GLShader(s) {}
    Shader(RenderDevice*, ShaderStage st, const char* src) : Render::Shader(st), GLShader(0)
    {
        Compile(src);
    }

	~Shader();
    bool Compile(const char* src);

    GLenum GLStage() const
    {
        switch (Stage)
        {
        default:  OVR_ASSERT(0); return GL_NONE;
        case Shader_Vertex: return GL_VERTEX_SHADER;
        case Shader_Fragment: return GL_FRAGMENT_SHADER;
        }
    }

    //void Set(PrimitiveType prim) const;
    //void SetUniformBuffer(Render::Buffer* buffers, int i = 0);
};

class ShaderSet : public Render::ShaderSet
{
public:
    GLuint Prog;

    struct Uniform
    {
        String Name;
        int    Location, Size;
        int    Type; // currently number of floats in vector
    };
    Array<Uniform> UniformInfo;

    int     ProjLoc, ViewLoc;
    int     TexLoc[8];
    bool    UsesLighting;
    int     LightingVer;

    ShaderSet();
    ~ShaderSet();

    virtual void SetShader(Render::Shader *s);
	virtual void UnsetShader(int stage);

    virtual void Set(PrimitiveType prim) const;

    // Set a uniform (other than the standard matrices). It is undefined whether the
    // uniforms from one shader occupy the same space as those in other shaders
    // (unless a buffer is used, then each buffer is independent).     
    virtual bool SetUniform(const char* name, int n, const float* v);
    virtual bool SetUniform4x4f(const char* name, const Matrix4f& m);

    bool Link();
};

class RBuffer : public RefCountBase<RBuffer>
{
 public:
    int    Width, Height;
    GLuint BufId;

    RBuffer(GLenum format, GLint w, GLint h);
    ~RBuffer();
};

class RenderDevice : public Render::RenderDevice
{
    Ptr<Shader>        VertexShaders[VShader_Count];
    Ptr<Shader>        FragShaders[FShader_Count];

    Ptr<ShaderFill> DefaultFill;

    Matrix4f    Proj;

	GLuint Vao;

protected:
    Ptr<Texture>             CurRenderTarget;
    Array<Ptr<Texture> >     DepthBuffers;
    GLuint                   CurrentFbo;

    const LightingParams*    Lighting;
    bool SupportsVao;
    
public:
    RenderDevice(const RendererParams& p);
    virtual ~RenderDevice();

    virtual void Shutdown();
	
    virtual void FillTexturedRect(float left, float top, float right, float bottom, float ul, float vt, float ur, float vb, Color c, Ptr<OVR::Render::Texture> tex);

    virtual void SetViewport(const Recti& vp);
		
    virtual void WaitUntilGpuIdle();

    virtual void Clear(float r = 0, float g = 0, float b = 0, float a = 1, float depth = 1,
                       bool clearColor = true, bool clearDepth = true);
    virtual void Rect(float left, float top, float right, float bottom) { OVR_UNUSED4(left,top,right,bottom); }

    virtual void BeginRendering();
    virtual void SetDepthMode(bool enable, bool write, CompareFunc func = Compare_Less);
    virtual void SetWorldUniforms(const Matrix4f& proj);

    Texture* GetDepthBuffer(int w, int h, int ms);

    virtual void Present (bool withVsync){OVR_UNUSED(withVsync);};
    virtual void SetRenderTarget(Render::Texture* color,
                                 Render::Texture* depth = NULL, Render::Texture* stencil = NULL);

    virtual void SetLighting(const LightingParams* lt);

    virtual void Render(const Matrix4f& matrix, Model* model);
    virtual void Render(const Fill* fill, Render::Buffer* vertices, Render::Buffer* indices,
                        const Matrix4f& matrix, int offset, int count, PrimitiveType prim = Prim_Triangles, MeshType meshType = Mesh_Scene);
    virtual void RenderWithAlpha(const Fill* fill, Render::Buffer* vertices, Render::Buffer* indices,
                                 const Matrix4f& matrix, int offset, int count, PrimitiveType prim = Prim_Triangles);

    virtual Buffer* CreateBuffer();
    virtual Texture* CreateTexture(int format, int width, int height, const void* data, int mipcount=1);
    virtual ShaderSet* CreateShaderSet() { return new ShaderSet; }

    virtual Fill *CreateSimpleFill(int flags = Fill::F_Solid);

    virtual Shader *LoadBuiltinShader(ShaderStage stage, int shader);

    void SetTexture(Render::ShaderStage, int slot, const Texture* t);
};

}}}

#endif
