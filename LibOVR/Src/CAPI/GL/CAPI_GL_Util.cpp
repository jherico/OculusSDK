/************************************************************************************

Filename    :   Render_GL_Device.cpp
Content     :   RenderDevice implementation for OpenGL
Created     :   September 10, 2012
Authors     :   David Borel, Andrew Reisse

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

#include "CAPI_GL_Util.h"
#include "../../Kernel/OVR_Log.h"

namespace OVR { namespace CAPI { namespace GL {



// GL Hooks for PC.
#if defined(OVR_OS_WIN32)

PFNWGLGETPROCADDRESS                     wglGetProcAddress;

PFNGLCLEARPROC                           glClear;
PFNGLCLEARCOLORPROC                      glClearColor;
PFNGLCLEARDEPTHPROC                      glClearDepth;
PFNGLVIEWPORTPROC                        glViewport;
PFNGLDRAWELEMENTSPROC                    glDrawElements;
PFNGLTEXPARAMETERIPROC                   glTexParameteri;
PFNGLFLUSHPROC                           glFlush;
PFNGLFINISHPROC                          glFinish;
PFNGLDRAWARRAYSPROC                      glDrawArrays;
PFNGLGENTEXTURESPROC                     glGenTextures;
PFNGLDELETETEXTURESPROC                  glDeleteTextures;
PFNGLBINDTEXTUREPROC                     glBindTexture;

PFNWGLGETSWAPINTERVALEXTPROC             wglGetSwapIntervalEXT;
PFNWGLSWAPINTERVALEXTPROC                wglSwapIntervalEXT;
PFNGLGENFRAMEBUFFERSEXTPROC              glGenFramebuffersEXT;
PFNGLDELETESHADERPROC                    glDeleteShader;
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC       glCheckFramebufferStatusEXT;
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC      glFramebufferRenderbufferEXT;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC         glFramebufferTexture2DEXT;
PFNGLBINDFRAMEBUFFEREXTPROC              glBindFramebufferEXT;
PFNGLACTIVETEXTUREPROC                   glActiveTexture;
PFNGLDISABLEVERTEXATTRIBARRAYPROC        glDisableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC             glVertexAttribPointer;
PFNGLENABLEVERTEXATTRIBARRAYPROC         glEnableVertexAttribArray;
PFNGLBINDBUFFERPROC                      glBindBuffer;
PFNGLUNIFORMMATRIX3FVPROC                glUniformMatrix3fv;
PFNGLUNIFORMMATRIX4FVPROC                glUniformMatrix4fv;
PFNGLDELETEBUFFERSPROC                   glDeleteBuffers;
PFNGLBUFFERDATAPROC                      glBufferData;
PFNGLGENBUFFERSPROC                      glGenBuffers;
PFNGLMAPBUFFERPROC                       glMapBuffer;
PFNGLUNMAPBUFFERPROC                     glUnmapBuffer;
PFNGLGETSHADERINFOLOGPROC                glGetShaderInfoLog;
PFNGLGETSHADERIVPROC                     glGetShaderiv;
PFNGLCOMPILESHADERPROC                   glCompileShader;
PFNGLSHADERSOURCEPROC                    glShaderSource;
PFNGLCREATESHADERPROC                    glCreateShader;
PFNGLCREATEPROGRAMPROC                   glCreateProgram;
PFNGLATTACHSHADERPROC                    glAttachShader;
PFNGLDETACHSHADERPROC                    glDetachShader;
PFNGLDELETEPROGRAMPROC                   glDeleteProgram;
PFNGLUNIFORM1IPROC                       glUniform1i;
PFNGLGETUNIFORMLOCATIONPROC              glGetUniformLocation;
PFNGLGETACTIVEUNIFORMPROC                glGetActiveUniform;
PFNGLUSEPROGRAMPROC                      glUseProgram;
PFNGLGETPROGRAMINFOLOGPROC               glGetProgramInfoLog;
PFNGLGETPROGRAMIVPROC                    glGetProgramiv;
PFNGLLINKPROGRAMPROC                     glLinkProgram;
PFNGLBINDATTRIBLOCATIONPROC              glBindAttribLocation;
PFNGLGETATTRIBLOCATIONPROC               glGetAttribLocation;
PFNGLUNIFORM4FVPROC                      glUniform4fv;
PFNGLUNIFORM3FVPROC                      glUniform3fv;
PFNGLUNIFORM2FVPROC                      glUniform2fv;
PFNGLUNIFORM1FVPROC                      glUniform1fv;
PFNGLCOMPRESSEDTEXIMAGE2DPROC            glCompressedTexImage2D;
PFNGLRENDERBUFFERSTORAGEEXTPROC          glRenderbufferStorageEXT;
PFNGLBINDRENDERBUFFEREXTPROC             glBindRenderbufferEXT;
PFNGLGENRENDERBUFFERSEXTPROC             glGenRenderbuffersEXT;
PFNGLDELETERENDERBUFFERSEXTPROC          glDeleteRenderbuffersEXT;

PFNGLGENVERTEXARRAYSPROC                 glGenVertexArrays;

void InitGLExtensions()
{
	HINSTANCE hInst = LoadLibrary( L"Opengl32.dll" );
	if (!hInst)
		return;

	glClear = (PFNGLCLEARPROC)GetProcAddress( hInst, "glClear" );
	glClearColor = (PFNGLCLEARCOLORPROC)GetProcAddress( hInst, "glClearColor" );
	glClearDepth = (PFNGLCLEARDEPTHPROC)GetProcAddress( hInst, "glClearDepth" );
	glViewport = (PFNGLVIEWPORTPROC)GetProcAddress( hInst, "glViewport" );
	glDrawElements = (PFNGLDRAWELEMENTSPROC)GetProcAddress( hInst, "glDrawElements" );
	glTexParameteri = (PFNGLTEXPARAMETERIPROC)GetProcAddress( hInst, "glTexParameteri" );
	glFlush = (PFNGLFLUSHPROC)GetProcAddress( hInst, "glFlush" );
	glFinish = (PFNGLFINISHPROC)GetProcAddress( hInst, "glFinish" );
    glDrawArrays = (PFNGLDRAWARRAYSPROC)GetProcAddress( hInst, "glDrawArrays" );
    glGenTextures = (PFNGLGENTEXTURESPROC)GetProcAddress( hInst,"glGenTextures" );
    glDeleteTextures = (PFNGLDELETETEXTURESPROC)GetProcAddress( hInst,"glDeleteTextures" );
    glBindTexture = (PFNGLBINDTEXTUREPROC)GetProcAddress( hInst,"glBindTexture" );

	wglGetProcAddress = (PFNWGLGETPROCADDRESS)GetProcAddress( hInst, "wglGetProcAddress" );

    if (glGenFramebuffersEXT)
        return;	
	
    wglGetSwapIntervalEXT =             (PFNWGLGETSWAPINTERVALEXTPROC)             wglGetProcAddress("wglGetSwapIntervalEXT");
    wglSwapIntervalEXT =                (PFNWGLSWAPINTERVALEXTPROC)                wglGetProcAddress("wglSwapIntervalEXT");
    glGenFramebuffersEXT =              (PFNGLGENFRAMEBUFFERSEXTPROC)              wglGetProcAddress("glGenFramebuffersEXT");
    glDeleteShader =                    (PFNGLDELETESHADERPROC)                    wglGetProcAddress("glDeleteShader");
    glCheckFramebufferStatusEXT =       (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)       wglGetProcAddress("glCheckFramebufferStatusEXT");
    glFramebufferRenderbufferEXT =      (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)      wglGetProcAddress("glFramebufferRenderbufferEXT");
    glFramebufferTexture2DEXT =         (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)         wglGetProcAddress("glFramebufferTexture2DEXT");
    glBindFramebufferEXT =              (PFNGLBINDFRAMEBUFFEREXTPROC)              wglGetProcAddress("glBindFramebufferEXT");
    glActiveTexture =                   (PFNGLACTIVETEXTUREPROC)                   wglGetProcAddress("glActiveTexture");
    glDisableVertexAttribArray =        (PFNGLDISABLEVERTEXATTRIBARRAYPROC)        wglGetProcAddress("glDisableVertexAttribArray");
    glVertexAttribPointer =             (PFNGLVERTEXATTRIBPOINTERPROC)             wglGetProcAddress("glVertexAttribPointer");
    glEnableVertexAttribArray =         (PFNGLENABLEVERTEXATTRIBARRAYPROC)         wglGetProcAddress("glEnableVertexAttribArray");
    glBindBuffer =                      (PFNGLBINDBUFFERPROC)                      wglGetProcAddress("glBindBuffer");
    glUniformMatrix3fv =                (PFNGLUNIFORMMATRIX3FVPROC)                wglGetProcAddress("glUniformMatrix3fv");
    glUniformMatrix4fv =                (PFNGLUNIFORMMATRIX4FVPROC)                wglGetProcAddress("glUniformMatrix4fv");
    glDeleteBuffers =                   (PFNGLDELETEBUFFERSPROC)                   wglGetProcAddress("glDeleteBuffers");
    glBufferData =                      (PFNGLBUFFERDATAPROC)                      wglGetProcAddress("glBufferData");
    glGenBuffers =                      (PFNGLGENBUFFERSPROC)                      wglGetProcAddress("glGenBuffers");
    glMapBuffer =                       (PFNGLMAPBUFFERPROC)                       wglGetProcAddress("glMapBuffer");
    glUnmapBuffer =                     (PFNGLUNMAPBUFFERPROC)                     wglGetProcAddress("glUnmapBuffer");
    glGetShaderInfoLog =                (PFNGLGETSHADERINFOLOGPROC)                wglGetProcAddress("glGetShaderInfoLog");
    glGetShaderiv =                     (PFNGLGETSHADERIVPROC)                     wglGetProcAddress("glGetShaderiv");
    glCompileShader =                   (PFNGLCOMPILESHADERPROC)                   wglGetProcAddress("glCompileShader");
    glShaderSource =                    (PFNGLSHADERSOURCEPROC)                    wglGetProcAddress("glShaderSource");
    glCreateShader =                    (PFNGLCREATESHADERPROC)                    wglGetProcAddress("glCreateShader");
    glCreateProgram =                   (PFNGLCREATEPROGRAMPROC)                   wglGetProcAddress("glCreateProgram");
    glAttachShader =                    (PFNGLATTACHSHADERPROC)                    wglGetProcAddress("glAttachShader");
    glDetachShader =                    (PFNGLDETACHSHADERPROC)                    wglGetProcAddress("glDetachShader");
    glDeleteProgram =                   (PFNGLDELETEPROGRAMPROC)                   wglGetProcAddress("glDeleteProgram");
    glUniform1i =                       (PFNGLUNIFORM1IPROC)                       wglGetProcAddress("glUniform1i");
    glGetUniformLocation =              (PFNGLGETUNIFORMLOCATIONPROC)              wglGetProcAddress("glGetUniformLocation");
    glGetActiveUniform =                (PFNGLGETACTIVEUNIFORMPROC)                wglGetProcAddress("glGetActiveUniform");
    glUseProgram =                      (PFNGLUSEPROGRAMPROC)                      wglGetProcAddress("glUseProgram");
    glGetProgramInfoLog =               (PFNGLGETPROGRAMINFOLOGPROC)               wglGetProcAddress("glGetProgramInfoLog");
    glGetProgramiv =                    (PFNGLGETPROGRAMIVPROC)                    wglGetProcAddress("glGetProgramiv");
    glLinkProgram =                     (PFNGLLINKPROGRAMPROC)                     wglGetProcAddress("glLinkProgram");
    glBindAttribLocation =              (PFNGLBINDATTRIBLOCATIONPROC)              wglGetProcAddress("glBindAttribLocation");
    glGetAttribLocation =               (PFNGLGETATTRIBLOCATIONPROC)               wglGetProcAddress("glGetAttribLocation");
    glUniform4fv =                      (PFNGLUNIFORM4FVPROC)                      wglGetProcAddress("glUniform4fv");
    glUniform3fv =                      (PFNGLUNIFORM3FVPROC)                      wglGetProcAddress("glUniform3fv");
    glUniform2fv =                      (PFNGLUNIFORM2FVPROC)                      wglGetProcAddress("glUniform2fv");
    glUniform1fv =                      (PFNGLUNIFORM1FVPROC)                      wglGetProcAddress("glUniform1fv");
    glCompressedTexImage2D =            (PFNGLCOMPRESSEDTEXIMAGE2DPROC)            wglGetProcAddress("glCompressedTexImage2D");
    glRenderbufferStorageEXT =          (PFNGLRENDERBUFFERSTORAGEEXTPROC)          wglGetProcAddress("glRenderbufferStorageEXT");
    glBindRenderbufferEXT =             (PFNGLBINDRENDERBUFFEREXTPROC)             wglGetProcAddress("glBindRenderbufferEXT");
    glGenRenderbuffersEXT =             (PFNGLGENRENDERBUFFERSEXTPROC)             wglGetProcAddress("glGenRenderbuffersEXT");
    glDeleteRenderbuffersEXT =          (PFNGLDELETERENDERBUFFERSEXTPROC)          wglGetProcAddress("glDeleteRenderbuffersEXT");


    glGenVertexArrays =                 (PFNGLGENVERTEXARRAYSPROC)                 wglGetProcAddress("glGenVertexArrays");
}

#endif

Buffer::Buffer(RenderParams* rp) : pParams(rp), Size(0), Use(0), GLBuffer(0)
{
}

Buffer::~Buffer()
{
    if (GLBuffer)
        glDeleteBuffers(1, &GLBuffer);
}

bool Buffer::Data(int use, const void* buffer, size_t size)
{
	Size = size;

    switch (use & Buffer_TypeMask)
    {
    case Buffer_Index:     Use = GL_ELEMENT_ARRAY_BUFFER; break;
    default:               Use = GL_ARRAY_BUFFER; break;
    }

    if (!GLBuffer)
        glGenBuffers(1, &GLBuffer);

    int mode = GL_DYNAMIC_DRAW;
    if (use & Buffer_ReadOnly)
        mode = GL_STATIC_DRAW;

    glBindBuffer(Use, GLBuffer);
    glBufferData(Use, size, buffer, mode);
    glBindBuffer(Use, 0);
    return 1;
}

void* Buffer::Map(size_t, size_t, int)
{
    int mode = GL_WRITE_ONLY;
    //if (flags & Map_Unsynchronized)
    //    mode |= GL_MAP_UNSYNCHRONIZED;
    
    glBindBuffer(Use, GLBuffer);
    void* v = glMapBuffer(Use, mode);
    glBindBuffer(Use, 0);
    return v;
}

bool Buffer::Unmap(void*)
{
    glBindBuffer(Use, GLBuffer);
    int r = glUnmapBuffer(Use);
    glBindBuffer(Use, 0);
    return r != 0;
}

ShaderSet::ShaderSet()
{
    Prog = glCreateProgram();
}
ShaderSet::~ShaderSet()
{
    glDeleteProgram(Prog);
}

GLint ShaderSet::GetGLShader(Shader* s)
{
	switch (s->Stage)
	{
	case Shader_Vertex: {
		ShaderImpl<Shader_Vertex, GL_VERTEX_SHADER>* gls = (ShaderImpl<Shader_Vertex, GL_VERTEX_SHADER>*)s;
		return gls->GLShader;
	} break;
	case Shader_Fragment: {
		ShaderImpl<Shader_Fragment, GL_FRAGMENT_SHADER>* gls = (ShaderImpl<Shader_Fragment, GL_FRAGMENT_SHADER>*)s;
		return gls->GLShader;
	} break;
	}

	return -1;
}

void ShaderSet::SetShader(Shader *s)
{
    Shaders[s->Stage] = s;
	GLint GLShader = GetGLShader(s);
    glAttachShader(Prog, GLShader);
    if (Shaders[Shader_Vertex] && Shaders[Shader_Fragment])
        Link();
}

void ShaderSet::UnsetShader(int stage)
{
    if (Shaders[stage] == NULL)
		return;

	GLint GLShader = GetGLShader(Shaders[stage]);
    glDetachShader(Prog, GLShader);

    Shaders[stage] = NULL;

    //Link();
}

bool ShaderSet::SetUniform(const char* name, int n, const float* v)
{
    for (unsigned int i = 0; i < UniformInfo.GetSize(); i++)
        if (!strcmp(UniformInfo[i].Name.ToCStr(), name))
        {
            OVR_ASSERT(UniformInfo[i].Location >= 0);
            glUseProgram(Prog);
            switch (UniformInfo[i].Type)
            {
            case 1:   glUniform1fv(UniformInfo[i].Location, n, v); break;
            case 2:   glUniform2fv(UniformInfo[i].Location, n/2, v); break;
            case 3:   glUniform3fv(UniformInfo[i].Location, n/3, v); break;
            case 4:   glUniform4fv(UniformInfo[i].Location, n/4, v); break;
            case 12:  glUniformMatrix3fv(UniformInfo[i].Location, 1, 1, v); break;
            case 16:  glUniformMatrix4fv(UniformInfo[i].Location, 1, 1, v); break;
            default: OVR_ASSERT(0);
            }
            return 1;
        }

    OVR_DEBUG_LOG(("Warning: uniform %s not present in selected shader", name));
    return 0;
}

bool ShaderSet::Link()
{
    glLinkProgram(Prog);
    GLint r;
    glGetProgramiv(Prog, GL_LINK_STATUS, &r);
    if (!r)
    {
        GLchar msg[1024];
        glGetProgramInfoLog(Prog, sizeof(msg), 0, msg);
        OVR_DEBUG_LOG(("Linking shaders failed: %s\n", msg));
        if (!r)
            return 0;
    }
    glUseProgram(Prog);

    UniformInfo.Clear();
    LightingVer = 0;
    UsesLighting = 0;

	GLint uniformCount = 0;
	glGetProgramiv(Prog, GL_ACTIVE_UNIFORMS, &uniformCount);
	OVR_ASSERT(uniformCount >= 0);

    for(GLuint i = 0; i < (GLuint)uniformCount; i++)
    {
        GLsizei namelen;
        GLint size = 0;
        GLenum type;
        GLchar name[32];
        glGetActiveUniform(Prog, i, sizeof(name), &namelen, &size, &type, name);

        if (size)
        {
            int l = glGetUniformLocation(Prog, name);
            char *np = name;
            while (*np)
            {
                if (*np == '[')
                    *np = 0;
                np++;
            }
            Uniform u;
            u.Name = name;
            u.Location = l;
            u.Size = size;
            switch (type)
            {
            case GL_FLOAT:      u.Type = 1; break;
            case GL_FLOAT_VEC2: u.Type = 2; break;
            case GL_FLOAT_VEC3: u.Type = 3; break;
            case GL_FLOAT_VEC4: u.Type = 4; break;
            case GL_FLOAT_MAT3: u.Type = 12; break;
            case GL_FLOAT_MAT4: u.Type = 16; break;
            default:
                continue;
            }
            UniformInfo.PushBack(u);
            if (!strcmp(name, "LightCount"))
                UsesLighting = 1;
        }
        else
            break;
    }

    ProjLoc = glGetUniformLocation(Prog, "Proj");
    ViewLoc = glGetUniformLocation(Prog, "View");
    for (int i = 0; i < 8; i++)
    {
        char texv[32];
        OVR_sprintf(texv, 10, "Texture%d", i);
        TexLoc[i] = glGetUniformLocation(Prog, texv);
        if (TexLoc[i] < 0)
            break;

        glUniform1i(TexLoc[i], i);
    }
    if (UsesLighting)
        OVR_ASSERT(ProjLoc >= 0 && ViewLoc >= 0);
    return 1;
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
    if(!refl)
    {
        UniformRefl = NULL;
        UniformReflSize = 0;

        UniformsSize = 0;
        if (UniformData)
        {
            OVR_FREE(UniformData);
            UniformData = 0;
        }
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

Texture::Texture(RenderParams* rp, int w, int h) : IsUserAllocated(true), pParams(rp), TexId(0), Width(w), Height(h)
{
	if (w && h)
		glGenTextures(1, &TexId);
}

Texture::~Texture()
{
    if (TexId && !IsUserAllocated)
        glDeleteTextures(1, &TexId);
}

void Texture::Set(int slot, ShaderStage) const
{
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, TexId);
	glActiveTexture(GL_TEXTURE0);
}

void Texture::SetSampleMode(int sm)
{
    glBindTexture(GL_TEXTURE_2D, TexId);
    switch (sm & Sample_FilterMask)
    {
    case Sample_Linear:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
        break;

    case Sample_Anisotropic:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);
        break;

    case Sample_Nearest:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
        break;
    }

    switch (sm & Sample_AddressMask)
    {
    case Sample_Repeat:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        break;

    case Sample_Clamp:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        break;

    case Sample_ClampBorder:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        break;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::UpdatePlaceholderTexture(GLuint texId, const Sizei& textureSize)
{
	if (!IsUserAllocated && TexId && texId != TexId)
		glDeleteTextures(1, &TexId);

    TexId = texId;
	Width = textureSize.w;
	Height = textureSize.h;

	IsUserAllocated = true;
}

}}}
