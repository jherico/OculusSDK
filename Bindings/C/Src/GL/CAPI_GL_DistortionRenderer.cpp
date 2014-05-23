/************************************************************************************

Filename    :   CAPI_GL_DistortionRenderer.h
Content     :   Distortion renderer header for GL
Created     :   November 11, 2013
Authors     :   David Borel, Lee Cooper

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus Inc license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

************************************************************************************/

#include "CAPI_GL_DistortionRenderer.h"

#include "CAPI_GL_DistortionShaders.h"

#include "../../OVR_CAPI_GL.h"

namespace OVR { namespace CAPI { namespace GL {

// Distortion pixel shader lookup.
//  Bit 0: Chroma Correction
//  Bit 1: Timewarp

enum {
    DistortionVertexShaderBitMask = 3,
    DistortionVertexShaderCount   = DistortionVertexShaderBitMask + 1,
    DistortionPixelShaderBitMask  = 1,
    DistortionPixelShaderCount    = DistortionPixelShaderBitMask + 1
};

struct ShaderInfo
{
    const char* ShaderData;
    size_t ShaderSize;
    const ShaderBase::Uniform* ReflectionData;
    size_t ReflectionSize;
};

// Do add a new distortion shader use these macros (with or w/o reflection)
#define SI_NOREFL(shader) { shader, sizeof(shader), NULL, 0 }
#define SI_REFL__(shader) { shader, sizeof(shader), shader ## _refl, sizeof( shader ## _refl )/sizeof(*(shader ## _refl)) }


static ShaderInfo DistortionVertexShaderLookup[DistortionVertexShaderCount] =
{
    SI_REFL__(Distortion_vs),
    SI_REFL__(DistortionChroma_vs),
    SI_REFL__(DistortionTimewarp_vs),
    SI_REFL__(DistortionTimewarpChroma_vs)
};

static ShaderInfo DistortionPixelShaderLookup[DistortionPixelShaderCount] =
{
    SI_NOREFL(Distortion_fs),
    SI_NOREFL(DistortionChroma_fs)
};

void DistortionShaderBitIndexCheck()
{
    OVR_COMPILER_ASSERT(ovrDistortionCap_Chromatic == 1);
    OVR_COMPILER_ASSERT(ovrDistortionCap_TimeWarp  == 2);
}



struct DistortionVertex
{
    Vector2f Pos;
    Vector2f TexR;
    Vector2f TexG;
    Vector2f TexB;
    Color    Col;
};


// Vertex type; same format is used for all shapes for simplicity.
// Shapes are built by adding vertices to Model.
struct LatencyVertex
{
    Vector3f  Pos;
    LatencyVertex (const Vector3f& p) : Pos(p) {}
};


//----------------------------------------------------------------------------
// ***** GL::DistortionRenderer

DistortionRenderer::DistortionRenderer(ovrHmd hmd, FrameTimeManager& timeManager,
                                       const HMDRenderState& renderState)
    : CAPI::DistortionRenderer(ovrRenderAPI_OpenGL, hmd, timeManager, renderState)
	, LatencyVAO(0)
{
	DistortionMeshVAOs[0] = 0;
	DistortionMeshVAOs[1] = 0;
}

DistortionRenderer::~DistortionRenderer()
{
    destroy();
}

// static
CAPI::DistortionRenderer* DistortionRenderer::Create(ovrHmd hmd,
                                                     FrameTimeManager& timeManager,
                                                     const HMDRenderState& renderState)
{
#if !defined(OVR_OS_MAC)
    InitGLExtensions();
#endif
    return new DistortionRenderer(hmd, timeManager, renderState);
}


bool DistortionRenderer::Initialize(const ovrRenderAPIConfig* apiConfig,
									unsigned distortionCaps)
{
	GfxState = *new GraphicsState();

    const ovrGLConfig* config = (const ovrGLConfig*)apiConfig;

    if (!config)
    {
        // Cleanup
        pEyeTextures[0].Clear();
        pEyeTextures[1].Clear();
        memset(&RParams, 0, sizeof(RParams));
        return true;
    }

	RParams.Multisample = config->OGL.Header.Multisample;
	RParams.RTSize      = config->OGL.Header.RTSize;
#if defined(OVR_OS_WIN32)
	RParams.Window      = (config->OGL.Window) ? config->OGL.Window : GetActiveWindow();
#elif defined(OVR_OS_LINUX)
    RParams.Disp        = (config->OGL.Disp) ? config->OGL.Disp : XOpenDisplay(NULL);
    RParams.Win         = config->OGL.Win;
    if (!RParams.Win)
    {
        int unused;
        XGetInputFocus(RParams.Disp, &RParams.Win, &unused);
    }
#endif
	
    DistortionCaps = distortionCaps;
	
    //DistortionWarper.SetVsync((hmdCaps & ovrHmdCap_NoVSync) ? false : true);

    pEyeTextures[0] = *new Texture(&RParams, 0, 0);
    pEyeTextures[1] = *new Texture(&RParams, 0, 0);

    initBuffersAndShaders();

    return true;
}


void DistortionRenderer::SubmitEye(int eyeId, ovrTexture* eyeTexture)
{
	// Doesn't do a lot in here??
	const ovrGLTexture* tex = (const ovrGLTexture*)eyeTexture;

	// Write in values
    eachEye[eyeId].texture = tex->OGL.TexId;

	if (tex)
	{
        // Its only at this point we discover what the viewport of the texture is.
	    // because presumably we allow users to realtime adjust the resolution.
        eachEye[eyeId].TextureSize    = tex->OGL.Header.TextureSize;
        eachEye[eyeId].RenderViewport = tex->OGL.Header.RenderViewport;

        const ovrEyeRenderDesc& erd = RState.EyeRenderDesc[eyeId];
    
        ovrHmd_GetRenderScaleAndOffset( erd.Fov,
                                        eachEye[eyeId].TextureSize, eachEye[eyeId].RenderViewport,
                                        eachEye[eyeId].UVScaleOffset );

        pEyeTextures[eyeId]->UpdatePlaceholderTexture(tex->OGL.TexId,
                                                      tex->OGL.Header.TextureSize);
	}
}

void DistortionRenderer::EndFrame(bool swapBuffers,
                                  unsigned char* latencyTesterDrawColor, unsigned char* latencyTester2DrawColor)
{
    if (!TimeManager.NeedDistortionTimeMeasurement())
    {
		if (RState.DistortionCaps & ovrDistortionCap_TimeWarp)
		{
			// Wait for timewarp distortion if it is time and Gpu idle
			FlushGpuAndWaitTillTime(TimeManager.GetFrameTiming().TimewarpPointTime);
		}

        renderDistortion(pEyeTextures[0], pEyeTextures[1]);
    }
    else
    {
        // If needed, measure distortion time so that TimeManager can better estimate
        // latency-reducing time-warp wait timing.
        WaitUntilGpuIdle();
        double  distortionStartTime = ovr_GetTimeInSeconds();

        renderDistortion(pEyeTextures[0], pEyeTextures[1]);

        WaitUntilGpuIdle();
        TimeManager.AddDistortionTimeMeasurement(ovr_GetTimeInSeconds() - distortionStartTime);
    }

    if(latencyTesterDrawColor)
    {
        renderLatencyQuad(latencyTesterDrawColor);
    }
    else if(latencyTester2DrawColor)
    {
        renderLatencyPixel(latencyTester2DrawColor);
    }

    if (swapBuffers)
    {
		bool useVsync = ((RState.EnabledHmdCaps & ovrHmdCap_NoVSync) == 0);
		int swapInterval = (useVsync) ? 1 : 0;
#if defined(OVR_OS_WIN32)
		if (wglGetSwapIntervalEXT() != swapInterval)
            wglSwapIntervalEXT(swapInterval);

        HDC dc = GetDC(RParams.Window);
		BOOL success = SwapBuffers(dc);
        ReleaseDC(RParams.Window, dc);
		OVR_ASSERT(success);
        OVR_UNUSED(success);
#elif defined(OVR_OS_MAC)
        CGLContextObj context = CGLGetCurrentContext();
        GLint currentSwapInterval = 0;
        CGLGetParameter(context, kCGLCPSwapInterval, &currentSwapInterval);
        if (currentSwapInterval != swapInterval)
            CGLSetParameter(context, kCGLCPSwapInterval, &swapInterval);
        
        CGLFlushDrawable(context);
#elif defined(OVR_OS_LINUX)
        static const char* extensions = glXQueryExtensionsString(RParams.Disp, 0);
        static bool supportsVSync = (extensions != NULL && strstr(extensions, "GLX_EXT_swap_control"));
        if (supportsVSync)
        {
            GLuint currentSwapInterval = 0;
            glXQueryDrawable(RParams.Disp, RParams.Win, GLX_SWAP_INTERVAL_EXT, &currentSwapInterval);
            if (currentSwapInterval != swapInterval)
                glXSwapIntervalEXT(RParams.Disp, RParams.Win, swapInterval);
        }

        glXSwapBuffers(RParams.Disp, RParams.Win);
#endif
    }
}

void DistortionRenderer::WaitUntilGpuIdle()
{
	glFlush();
	glFinish();
}

double DistortionRenderer::FlushGpuAndWaitTillTime(double absTime)
{
	double       initialTime = ovr_GetTimeInSeconds();
	if (initialTime >= absTime)
		return 0.0;
	
	glFlush();
	glFinish();

	double newTime   = initialTime;
	volatile int i;

	while (newTime < absTime)
	{
		for (int j = 0; j < 50; j++)
			i = 0;

		newTime = ovr_GetTimeInSeconds();
	}

	// How long we waited
	return newTime - initialTime;
}
    
    
DistortionRenderer::GraphicsState::GraphicsState()
{
    const char* glVersionString = (const char*)glGetString(GL_VERSION);
    OVR_DEBUG_LOG(("GL_VERSION STRING: %s", (const char*)glVersionString));
    char prefix[64];
    bool foundVersion = false;
    
    for (int i = 10; i < 30; ++i)
    {
        int major = i / 10;
        int minor = i % 10;
        OVR_sprintf(prefix, 64, "%d.%d", major, minor);
        if (strstr(glVersionString, prefix) == glVersionString)
        {
            GlMajorVersion = major;
            GlMinorVersion = minor;
            foundVersion = true;
            break;
        }
    }
    
    if (!foundVersion)
    {
        glGetIntegerv(GL_MAJOR_VERSION, &GlMajorVersion);
        glGetIntegerv(GL_MAJOR_VERSION, &GlMinorVersion);
	}

	OVR_ASSERT(GlMajorVersion >= 2);
    
    if (GlMajorVersion >= 3)
    {
        SupportsVao = true;
    }
    else
    {
        const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
        SupportsVao = (strstr("GL_ARB_vertex_array_object", extensions) != NULL);
    }
}
    
    
void DistortionRenderer::GraphicsState::ApplyBool(GLenum Name, GLint Value)
{
    if (Value != 0)
        glEnable(Name);
    else
        glDisable(Name);
}
    
    
void DistortionRenderer::GraphicsState::Save()
{
    glGetIntegerv(GL_VIEWPORT, Viewport);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, ClearColor);
    glGetIntegerv(GL_DEPTH_TEST, &DepthTest);
    glGetIntegerv(GL_CULL_FACE, &CullFace);
    glGetIntegerv(GL_CURRENT_PROGRAM, &Program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &ActiveTexture);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &TextureBinding);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &VertexArray);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &FrameBufferBinding);
    glGetIntegerv(GL_BLEND, &Blend);
    glGetIntegerv(GL_COLOR_WRITEMASK, ColorWritemask);
    glGetIntegerv(GL_DITHER, &Dither);
    glGetIntegerv(GL_RASTERIZER_DISCARD, &RasterizerDiscard);
    if (GlMajorVersion >= 3 && GlMajorVersion >= 2)
        glGetIntegerv(GL_SAMPLE_MASK, &SampleMask);
	glGetIntegerv(GL_SCISSOR_TEST, &ScissorTest);

	IsValid = true;
}
    

void DistortionRenderer::GraphicsState::Restore()
{
	// Don't allow restore-before-save.
	if (!IsValid)
		return;

    glViewport(Viewport[0], Viewport[1], Viewport[2], Viewport[3]);
    glClearColor(ClearColor[0], ClearColor[1], ClearColor[2], ClearColor[3]);
    
    ApplyBool(GL_DEPTH_TEST, DepthTest);
    ApplyBool(GL_CULL_FACE, CullFace);
    
    glUseProgram(Program);
    glActiveTexture(ActiveTexture);
    glBindTexture(GL_TEXTURE_2D, TextureBinding);
    if (SupportsVao)
        glBindVertexArray(VertexArray);
    glBindFramebuffer(GL_FRAMEBUFFER, FrameBufferBinding);
    
    ApplyBool(GL_BLEND, Blend);
    
	glColorMask((GLboolean)ColorWritemask[0], (GLboolean)ColorWritemask[1], (GLboolean)ColorWritemask[2], (GLboolean)ColorWritemask[3]);
    ApplyBool(GL_DITHER, Dither);
    ApplyBool(GL_RASTERIZER_DISCARD, RasterizerDiscard);
    if (GlMajorVersion >= 3 && GlMajorVersion >= 2)
        ApplyBool(GL_SAMPLE_MASK, SampleMask);
    ApplyBool(GL_SCISSOR_TEST, ScissorTest);
}


void DistortionRenderer::initBuffersAndShaders()
{
    for ( int eyeNum = 0; eyeNum < 2; eyeNum++ )
    {
        // Allocate & generate distortion mesh vertices.
        ovrDistortionMesh meshData;

//        double startT = ovr_GetTimeInSeconds();

        if (!ovrHmd_CreateDistortionMesh( HMD,
                                          RState.EyeRenderDesc[eyeNum].Eye,
                                          RState.EyeRenderDesc[eyeNum].Fov,
                                          RState.DistortionCaps,
                                          &meshData) )
        {
            OVR_ASSERT(false);
            continue;
        }

        // Now parse the vertex data and create a render ready vertex buffer from it
        DistortionVertex *   pVBVerts    = (DistortionVertex*)OVR_ALLOC ( sizeof(DistortionVertex) * meshData.VertexCount );
        DistortionVertex *   pCurVBVert  = pVBVerts;
        ovrDistortionVertex* pCurOvrVert = meshData.pVertexData;

        for ( unsigned vertNum = 0; vertNum < meshData.VertexCount; vertNum++ )
        {
            pCurVBVert->Pos.x = pCurOvrVert->Pos.x;
            pCurVBVert->Pos.y = pCurOvrVert->Pos.y;
            pCurVBVert->TexR  = (*(Vector2f*)&pCurOvrVert->TexR);
            pCurVBVert->TexG  = (*(Vector2f*)&pCurOvrVert->TexG);
            pCurVBVert->TexB  = (*(Vector2f*)&pCurOvrVert->TexB);
            // Convert [0.0f,1.0f] to [0,255]
            pCurVBVert->Col.R = (OVR::UByte)( pCurOvrVert->VignetteFactor * 255.99f );
            pCurVBVert->Col.G = pCurVBVert->Col.R;
            pCurVBVert->Col.B = pCurVBVert->Col.R;
            pCurVBVert->Col.A = (OVR::UByte)( pCurOvrVert->TimeWarpFactor * 255.99f );;
            pCurOvrVert++;
            pCurVBVert++;
        }

        DistortionMeshVBs[eyeNum] = *new Buffer(&RParams);
        DistortionMeshVBs[eyeNum]->Data ( Buffer_Vertex | Buffer_ReadOnly, pVBVerts, sizeof(DistortionVertex) * meshData.VertexCount );
        DistortionMeshIBs[eyeNum] = *new Buffer(&RParams);
        DistortionMeshIBs[eyeNum]->Data ( Buffer_Index | Buffer_ReadOnly, meshData.pIndexData, ( sizeof(SInt16) * meshData.IndexCount ) );

        OVR_FREE ( pVBVerts );
        ovrHmd_DestroyDistortionMesh( &meshData );
    }

    initShaders();
}

void DistortionRenderer::renderDistortion(Texture* leftEyeTexture, Texture* rightEyeTexture)
{
    GraphicsState* glState = (GraphicsState*)GfxState.GetPtr();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    setViewport( Recti(0,0, RParams.RTSize.w, RParams.RTSize.h) );

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glDisable(GL_DITHER);
    glDisable(GL_RASTERIZER_DISCARD);
    if (glState->GlMajorVersion >= 3 && glState->GlMajorVersion >= 2)
        glDisable(GL_SAMPLE_MASK);
    glDisable(GL_SCISSOR_TEST);
        
	glClearColor(
		RState.ClearColor[0],
		RState.ClearColor[1],
		RState.ClearColor[2],
		RState.ClearColor[3] );

    glClear(GL_COLOR_BUFFER_BIT);

    for (int eyeNum = 0; eyeNum < 2; eyeNum++)
    {        
		ShaderFill distortionShaderFill(DistortionShader);
        distortionShaderFill.SetTexture(0, eyeNum == 0 ? leftEyeTexture : rightEyeTexture);

		DistortionShader->SetUniform2f("EyeToSourceUVScale",  eachEye[eyeNum].UVScaleOffset[0].x, eachEye[eyeNum].UVScaleOffset[0].y);
		DistortionShader->SetUniform2f("EyeToSourceUVOffset", eachEye[eyeNum].UVScaleOffset[1].x, eachEye[eyeNum].UVScaleOffset[1].y);
        
		if (DistortionCaps & ovrDistortionCap_TimeWarp)
		{                       
            ovrMatrix4f timeWarpMatrices[2];            
            ovrHmd_GetEyeTimewarpMatrices(HMD, (ovrEyeType)eyeNum,
                                          RState.EyeRenderPoses[eyeNum], timeWarpMatrices);

            // Feed identity like matrices in until we get proper timewarp calculation going on
			DistortionShader->SetUniform4x4f("EyeRotationStart", Matrix4f(timeWarpMatrices[0]).Transposed());
			DistortionShader->SetUniform4x4f("EyeRotationEnd",   Matrix4f(timeWarpMatrices[1]).Transposed());

            renderPrimitives(&distortionShaderFill, DistortionMeshVBs[eyeNum], DistortionMeshIBs[eyeNum],
                            0, (int)DistortionMeshIBs[eyeNum]->GetSize()/2, Prim_Triangles, &DistortionMeshVAOs[eyeNum], true);
		}
        else
        {
            renderPrimitives(&distortionShaderFill, DistortionMeshVBs[eyeNum], DistortionMeshIBs[eyeNum],
                            0, (int)DistortionMeshIBs[eyeNum]->GetSize()/2, Prim_Triangles, &DistortionMeshVAOs[eyeNum], true);
        }
    }
}

void DistortionRenderer::createDrawQuad()
{
    const int numQuadVerts = 4;
    LatencyTesterQuadVB = *new Buffer(&RParams);
    if(!LatencyTesterQuadVB)
    {
        return;
    }

    LatencyTesterQuadVB->Data(Buffer_Vertex, NULL, numQuadVerts * sizeof(LatencyVertex));
    LatencyVertex* vertices = (LatencyVertex*)LatencyTesterQuadVB->Map(0, numQuadVerts * sizeof(LatencyVertex), Map_Discard);
    if(!vertices)
    {
        OVR_ASSERT(false); // failed to lock vertex buffer
        return;
    }

    const float left   = -1.0f;
    const float top    = -1.0f;
    const float right  =  1.0f;
    const float bottom =  1.0f;

    vertices[0] = LatencyVertex(Vector3f(left,  top,    0.0f));
    vertices[1] = LatencyVertex(Vector3f(left,  bottom, 0.0f));
    vertices[2] = LatencyVertex(Vector3f(right, top,    0.0f));
    vertices[3] = LatencyVertex(Vector3f(right, bottom, 0.0f));

    LatencyTesterQuadVB->Unmap(vertices);
}

void DistortionRenderer::renderLatencyQuad(unsigned char* latencyTesterDrawColor)
{
    const int numQuadVerts = 4;

    if(!LatencyTesterQuadVB)
    {
        createDrawQuad();
    }
       
    ShaderFill quadFill(SimpleQuadShader);
    //quadFill.SetInputLayout(SimpleQuadVertexIL);

    setViewport(Recti(0,0, RParams.RTSize.w, RParams.RTSize.h));

    SimpleQuadShader->SetUniform2f("Scale", 0.2f, 0.2f);
    SimpleQuadShader->SetUniform4f("Color", (float)latencyTesterDrawColor[0] / 255.99f,
                                            (float)latencyTesterDrawColor[0] / 255.99f,
                                            (float)latencyTesterDrawColor[0] / 255.99f,
                                            1.0f);

    for(int eyeNum = 0; eyeNum < 2; eyeNum++)
    {
        SimpleQuadShader->SetUniform2f("PositionOffset", eyeNum == 0 ? -0.4f : 0.4f, 0.0f);    
        renderPrimitives(&quadFill, LatencyTesterQuadVB, NULL, 0, numQuadVerts, Prim_TriangleStrip, &LatencyVAO, false);
    }
}

void DistortionRenderer::renderLatencyPixel(unsigned char* latencyTesterPixelColor)
{
    const int numQuadVerts = 4;

    if(!LatencyTesterQuadVB)
    {
        createDrawQuad();
    }

    ShaderFill quadFill(SimpleQuadShader);

    setViewport(Recti(0,0, RParams.RTSize.w, RParams.RTSize.h));

    SimpleQuadShader->SetUniform4f("Color", (float)latencyTesterPixelColor[0] / 255.99f,
                                            (float)latencyTesterPixelColor[0] / 255.99f,
                                            (float)latencyTesterPixelColor[0] / 255.99f,
                                            1.0f);

    Vector2f scale(2.0f / RParams.RTSize.w, 2.0f / RParams.RTSize.h); 
    SimpleQuadShader->SetUniform2f("Scale", scale.x, scale.y);
    SimpleQuadShader->SetUniform2f("PositionOffset", 1.0f, 1.0f);
	renderPrimitives(&quadFill, LatencyTesterQuadVB, NULL, 0, numQuadVerts, Prim_TriangleStrip, &LatencyVAO, false);
}

void DistortionRenderer::renderPrimitives(
                          const ShaderFill* fill,
                          Buffer* vertices, Buffer* indices,
                          int offset, int count,
                          PrimitiveType rprim, GLuint* vao, bool isDistortionMesh)
{
    GraphicsState* glState = (GraphicsState*)GfxState.GetPtr();

    GLenum prim;
    switch (rprim)
    {
    case Prim_Triangles:
        prim = GL_TRIANGLES;
        break;
    case Prim_Lines:
        prim = GL_LINES;
        break;
    case Prim_TriangleStrip:
        prim = GL_TRIANGLE_STRIP;
        break;
    default:
        OVR_ASSERT(false);
        return;
    }

    fill->Set();
    
    GLuint prog = fill->GetShaders()->Prog;

	if (vao != NULL)
	{
		if (*vao != 0)
		{
			glBindVertexArray(*vao);

			if (isDistortionMesh)
				glDrawElements(prim, count, GL_UNSIGNED_SHORT, NULL);
			else
				glDrawArrays(prim, 0, count);
		}
		else
		{
            if (glState->SupportsVao)
            {
                glGenVertexArrays(1, vao);
                glBindVertexArray(*vao);
			}

			int attributeCount = (isDistortionMesh) ? 5 : 1;
			int* locs = new int[attributeCount];

			glBindBuffer(GL_ARRAY_BUFFER, ((Buffer*)vertices)->GLBuffer);

			if (isDistortionMesh)
			{
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ((Buffer*)indices)->GLBuffer);

				locs[0] = glGetAttribLocation(prog, "Position");
				locs[1] = glGetAttribLocation(prog, "Color");
				locs[2] = glGetAttribLocation(prog, "TexCoord0");
				locs[3] = glGetAttribLocation(prog, "TexCoord1");
				locs[4] = glGetAttribLocation(prog, "TexCoord2");

				glVertexAttribPointer(locs[0], 2, GL_FLOAT, false, sizeof(DistortionVertex), reinterpret_cast<char*>(offset)+offsetof(DistortionVertex, Pos));
				glVertexAttribPointer(locs[1], 4, GL_UNSIGNED_BYTE, true, sizeof(DistortionVertex), reinterpret_cast<char*>(offset)+offsetof(DistortionVertex, Col));
				glVertexAttribPointer(locs[2], 2, GL_FLOAT, false, sizeof(DistortionVertex), reinterpret_cast<char*>(offset)+offsetof(DistortionVertex, TexR));
				glVertexAttribPointer(locs[3], 2, GL_FLOAT, false, sizeof(DistortionVertex), reinterpret_cast<char*>(offset)+offsetof(DistortionVertex, TexG));
				glVertexAttribPointer(locs[4], 2, GL_FLOAT, false, sizeof(DistortionVertex), reinterpret_cast<char*>(offset)+offsetof(DistortionVertex, TexB));
			}
			else
			{
				locs[0] = glGetAttribLocation(prog, "Position");

				glVertexAttribPointer(locs[0], 3, GL_FLOAT, false, sizeof(LatencyVertex), reinterpret_cast<char*>(offset)+offsetof(LatencyVertex, Pos));
			}

            for (int i = 0; i < attributeCount; ++i)
                glEnableVertexAttribArray(locs[i]);
            
			if (isDistortionMesh)
				glDrawElements(prim, count, GL_UNSIGNED_SHORT, NULL);
			else
				glDrawArrays(prim, 0, count);


            if (!glState->SupportsVao)
            {
				for (int i = 0; i < attributeCount; ++i)
                    glDisableVertexAttribArray(locs[i]);
            }

			delete[] locs;
		}
	}
}

void DistortionRenderer::setViewport(const Recti& vp)
{
    glViewport(vp.x, vp.y, vp.w, vp.h);
}


void DistortionRenderer::initShaders()
{
    GraphicsState* glState = (GraphicsState*)GfxState.GetPtr();

    const char* shaderPrefix =
        (glState->GlMajorVersion < 3 || (glState->GlMajorVersion == 3 && glState->GlMinorVersion < 2)) ?
            glsl2Prefix : glsl3Prefix;

    {
		ShaderInfo vsInfo = DistortionVertexShaderLookup[DistortionVertexShaderBitMask & DistortionCaps];

		size_t vsSize = strlen(shaderPrefix)+vsInfo.ShaderSize;
		char* vsSource = new char[vsSize];
		OVR_strcpy(vsSource, vsSize, shaderPrefix);
		OVR_strcat(vsSource, vsSize, vsInfo.ShaderData);

        Ptr<GL::VertexShader> vs = *new GL::VertexShader(
            &RParams,
			(void*)vsSource, vsSize,
			vsInfo.ReflectionData, vsInfo.ReflectionSize);

        DistortionShader = *new ShaderSet;
        DistortionShader->SetShader(vs);

		delete[](vsSource);

		ShaderInfo psInfo = DistortionPixelShaderLookup[DistortionPixelShaderBitMask & DistortionCaps];

		size_t psSize = strlen(shaderPrefix)+psInfo.ShaderSize;
		char* psSource = new char[psSize];
		OVR_strcpy(psSource, psSize, shaderPrefix);
		OVR_strcat(psSource, psSize, psInfo.ShaderData);

        Ptr<GL::FragmentShader> ps  = *new GL::FragmentShader(
            &RParams,
			(void*)psSource, psSize,
			psInfo.ReflectionData, psInfo.ReflectionSize);

        DistortionShader->SetShader(ps);

		delete[](psSource);
    }
	{
		size_t vsSize = strlen(shaderPrefix)+sizeof(SimpleQuad_vs);
		char* vsSource = new char[vsSize];
		OVR_strcpy(vsSource, vsSize, shaderPrefix);
		OVR_strcat(vsSource, vsSize, SimpleQuad_vs);

        Ptr<GL::VertexShader> vs = *new GL::VertexShader(
            &RParams,
            (void*)vsSource, vsSize,
			SimpleQuad_vs_refl, sizeof(SimpleQuad_vs_refl) / sizeof(SimpleQuad_vs_refl[0]));

        SimpleQuadShader = *new ShaderSet;
		SimpleQuadShader->SetShader(vs);

		delete[](vsSource);

		size_t psSize = strlen(shaderPrefix)+sizeof(SimpleQuad_fs);
		char* psSource = new char[psSize];
		OVR_strcpy(psSource, psSize, shaderPrefix);
		OVR_strcat(psSource, psSize, SimpleQuad_fs);

        Ptr<GL::FragmentShader> ps  = *new GL::FragmentShader(
            &RParams,
            (void*)psSource, psSize,
            SimpleQuad_fs_refl, sizeof(SimpleQuad_fs_refl) / sizeof(SimpleQuad_fs_refl[0]));

		SimpleQuadShader->SetShader(ps);

		delete[](psSource);
	}
}


void DistortionRenderer::destroy()
{
    GraphicsState* glState = (GraphicsState*)GfxState.GetPtr();

	for(int eyeNum = 0; eyeNum < 2; eyeNum++)
	{
        if (glState->SupportsVao)
            glDeleteVertexArrays(1, &DistortionMeshVAOs[eyeNum]);

		DistortionMeshVAOs[eyeNum] = 0;

		DistortionMeshVBs[eyeNum].Clear();
		DistortionMeshIBs[eyeNum].Clear();
	}

	if (DistortionShader)
    {
        DistortionShader->UnsetShader(Shader_Vertex);
	    DistortionShader->UnsetShader(Shader_Pixel);
	    DistortionShader.Clear();
    }

    LatencyTesterQuadVB.Clear();
	LatencyVAO = 0;
}

}}} // OVR::CAPI::GL
