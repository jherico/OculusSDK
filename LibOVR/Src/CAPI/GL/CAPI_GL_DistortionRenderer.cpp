/************************************************************************************

Filename    :   CAPI_GL_DistortionRenderer.h
Content     :   Distortion renderer header for GL
Created     :   November 11, 2013
Authors     :   David Borel, Lee Cooper

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

#include "CAPI_GL_DistortionRenderer.h"

#include "CAPI_GL_DistortionShaders.h"

#include "OVR_CAPI_GL.h"
#include "Kernel/OVR_Color.h"

#if defined(OVR_OS_MAC)
    #include <CoreGraphics/CGDirectDisplay.h>
    #include <OpenGL/OpenGL.h>
#endif

namespace OVR { namespace CAPI { namespace GL {


// Distortion pixel shader lookup.
//  Bit 0: Orientation Timewarp
//  Bit 1: Depth-based Timewarp

enum {
    DistortionVertexShaderBitMask = 3,
    DistortionVertexShaderCount   = DistortionVertexShaderBitMask + 1,
    DistortionPixelShaderBitMask  = 0,
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
    SI_REFL__(DistortionChroma_vs),
    { NULL, 0, NULL, 0 },
    SI_REFL__(DistortionTimewarpChroma_vs),
    { NULL, 0, NULL, 0 },
};

static ShaderInfo DistortionPixelShaderLookup[DistortionPixelShaderCount] =
{
    SI_NOREFL(DistortionChroma_fs)
};

void DistortionShaderBitIndexCheck()
{
    OVR_COMPILER_ASSERT(ovrDistortionCap_TimeWarp  == 2);
}



struct DistortionVertex
{
    Vector2f ScreenPosNDC;
    Vector2f TanEyeAnglesR;
    Vector2f TanEyeAnglesG;
    Vector2f TanEyeAnglesB;
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

DistortionRenderer::DistortionRenderer() :
    LatencyVAO(0),
    OverdriveFbo(0)
{
	DistortionMeshVAOs[0] = 0;
	DistortionMeshVAOs[1] = 0;

    // Initialize render params.
    memset(&RParams, 0, sizeof(RParams));
}

DistortionRenderer::~DistortionRenderer()
{
    destroy();
}

// static
CAPI::DistortionRenderer* DistortionRenderer::Create()
{
    InitGLExtensions();

    return new DistortionRenderer;
}


bool DistortionRenderer::initializeRenderer(const ovrRenderAPIConfig* apiConfig)
{
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
	RParams.BackBufferSize      = config->OGL.Header.BackBufferSize;
#if defined(OVR_OS_WIN32)
	RParams.Window      = (config->OGL.Window) ? config->OGL.Window : GetActiveWindow();
    RParams.DC          = config->OGL.DC;
#elif defined(OVR_OS_LINUX)
    if (config->OGL.Disp)
    {
        RParams.Disp = config->OGL.Disp;
    }
    if (!RParams.Disp)
    {
        RParams.Disp = glXGetCurrentDisplay();
    }
    if (!RParams.Disp)
    {
        OVR_DEBUG_LOG(("glXGetCurrentDisplay failed."));
        return false;
    }
#endif
	
    DistortionMeshVAOs[0] = 0;
    DistortionMeshVAOs[1] = 0;

    LatencyVAO = 0;

    GL::AutoContext autoGLContext(distortionContext); // Initializes distortionContext if not already, saves the current GL context, binds distortionContext, then at the end of scope re-binds the current GL context.

    pEyeTextures[0] = *new Texture(&RParams, 0, 0);
    pEyeTextures[1] = *new Texture(&RParams, 0, 0);

    if (!initBuffersAndShaders())
    {
        return false;
    }

    initOverdrive();

    return true;
}


void DistortionRenderer::initOverdrive()
{
	if(RenderState->DistortionCaps & ovrDistortionCap_Overdrive)
	{
		LastUsedOverdriveTextureIndex = 0;
        
        glGenFramebuffers(1, &OverdriveFbo);
        
        GLint internalFormat = (RenderState->DistortionCaps & ovrDistortionCap_SRGB) ? GL_SRGB_ALPHA : GL_RGBA;
        
		for (int i = 0; i < NumOverdriveTextures ; i++)
		{
            pOverdriveTextures[i] = *new Texture(&RParams, RParams.BackBufferSize.w, RParams.BackBufferSize.h);
            
            glBindTexture(GL_TEXTURE_2D, pOverdriveTextures[i]->TexId);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, RParams.BackBufferSize.w, RParams.BackBufferSize.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            OVR_ASSERT( glGetError() == GL_NO_ERROR );

            pOverdriveTextures[i]->SetSampleMode(Sample_ClampBorder | Sample_Linear);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            OVR_ASSERT(glGetError() == 0);

            // clear the new buffer
            glBindFramebuffer(GL_FRAMEBUFFER, OverdriveFbo );
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pOverdriveTextures[i]->TexId, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
            OVR_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
            GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0};
            glDrawBuffers(OVR_ARRAY_COUNT(drawBuffers), drawBuffers);
            glClearColor(0,0,0,1);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        {
            OverdriveBackBufferTexture = *new Texture(&RParams, RParams.BackBufferSize.w, RParams.BackBufferSize.h);

            glBindTexture(GL_TEXTURE_2D, OverdriveBackBufferTexture->TexId);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, RParams.BackBufferSize.w, RParams.BackBufferSize.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            OVR_ASSERT(glGetError() == 0);

            OverdriveBackBufferTexture->SetSampleMode(Sample_ClampBorder | Sample_Linear);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            OVR_ASSERT(glGetError() == 0);

            // clear the new buffer
            glBindFramebuffer(GL_FRAMEBUFFER, OverdriveFbo );
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OverdriveBackBufferTexture->TexId, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
            OVR_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
            GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0};
            glDrawBuffers(OVR_ARRAY_COUNT(drawBuffers), drawBuffers);
            glClearColor(0,0,0,1);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	else
	{
		LastUsedOverdriveTextureIndex = -1;
	}
}

void DistortionRenderer::SubmitEye(int eyeId, const ovrTexture* eyeTexture)
{
	if (eyeTexture)
	{
        // Doesn't do a lot in here??
        const ovrGLTexture* tex = (const ovrGLTexture*)eyeTexture;

        // Write in values
        eachEye[eyeId].texture = tex->OGL.TexId;

        // Its only at this point we discover what the viewport of the texture is.
	    // because presumably we allow users to realtime adjust the resolution.
        eachEye[eyeId].TextureSize    = tex->OGL.Header.TextureSize;
        eachEye[eyeId].RenderViewport = tex->OGL.Header.RenderViewport;

        const ovrEyeRenderDesc& erd = RenderState->EyeRenderDesc[eyeId];
    
        // Modify viewport offset since OpenGL uses bottom left as the origin
        eachEye[eyeId].RenderViewport.y = eachEye[eyeId].TextureSize.h - eachEye[eyeId].RenderViewport.h - eachEye[eyeId].RenderViewport.y;

        ovrHmd_GetRenderScaleAndOffset( erd.Fov,
                                        eachEye[eyeId].TextureSize, eachEye[eyeId].RenderViewport,
                                        eachEye[eyeId].UVScaleOffset );

		if (!(RenderState->DistortionCaps & ovrDistortionCap_FlipInput))
		{
			eachEye[eyeId].UVScaleOffset[0].y = -eachEye[eyeId].UVScaleOffset[0].y;
			eachEye[eyeId].UVScaleOffset[1].y = 1.0f - eachEye[eyeId].UVScaleOffset[1].y;
		}

        pEyeTextures[eyeId]->UpdatePlaceholderTexture(tex->OGL.TexId,
                                                      tex->OGL.Header.TextureSize);
	}
}

void DistortionRenderer::SubmitEyeWithDepth(int eyeId, const ovrTexture* eyeColorTexture, const ovrTexture* eyeDepthTexture)
{
    SubmitEye(eyeId, eyeColorTexture);

    OVR_UNUSED(eyeDepthTexture);
}

void DistortionRenderer::renderEndFrame()
{
    renderDistortion(pEyeTextures[0], pEyeTextures[1]);

    // TODO: Add rendering context to callback.
    if(RegisteredPostDistortionCallback)
       RegisteredPostDistortionCallback(NULL);

    if(LatencyTest2Active)
    {
        renderLatencyPixel(LatencyTest2DrawColor);
    }
}

void DistortionRenderer::EndFrame(uint32_t frameIndex, bool swapBuffers)
{
    // OGL does not support frame timing statistics.
    Timing->CalculateTimewarpTiming(frameIndex);

    Context currContext;
    currContext.InitFromCurrent();
#if defined(OVR_OS_MAC)
    distortionContext.SetSurface( currContext );
#endif

    // Don't spin if we are explicitly asked not to
    if ( (RenderState->DistortionCaps & ovrDistortionCap_TimeWarp) &&
         (RenderState->DistortionCaps & ovrDistortionCap_TimewarpJitDelay) &&
        !(RenderState->DistortionCaps & ovrDistortionCap_ProfileNoSpinWaits))
    {
        if (!Timing->NeedDistortionTimeMeasurement())
        {
            // Wait for timewarp distortion if it is time and Gpu idle
            FlushGpuAndWaitTillTime(Timing->GetTimewarpTiming()->JIT_TimewarpTime);

            distortionContext.Bind();
            renderEndFrame();
        }
        else
        {
            // If needed, measure distortion time so that TimeManager can better estimate
            // latency-reducing time-warp wait timing.
            WaitUntilGpuIdle();
            double  distortionStartTime = ovr_GetTimeInSeconds();

            distortionContext.Bind();
            renderEndFrame();

            WaitUntilGpuIdle();
            Timing->AddDistortionTimeMeasurement(ovr_GetTimeInSeconds() - distortionStartTime);
        }
    }
    else
    {
        distortionContext.Bind();
        renderEndFrame();
    }

    if(LatencyTestActive)
    {
        renderLatencyQuad(LatencyTestDrawColor);
    }

    if (swapBuffers)
    {
		bool useVsync = ((RenderState->EnabledHmdCaps & ovrHmdCap_NoVSync) == 0);
        int ourSwapInterval = (useVsync) ? 1 : 0;
        int originalSwapInterval;
        
#if defined(OVR_OS_WIN32)
        originalSwapInterval = wglGetSwapIntervalEXT();
        
        if (ourSwapInterval != originalSwapInterval)
            wglSwapIntervalEXT(ourSwapInterval);

        HDC dc = (RParams.DC != NULL) ? RParams.DC : GetDC(RParams.Window);
		BOOL success = SwapBuffers(dc);
        OVR_ASSERT_AND_UNUSED(success, success);

        if (RParams.DC == NULL)
            ReleaseDC(RParams.Window, dc);
        
#elif defined(OVR_OS_MAC)
        originalSwapInterval = 0;
        CGLContextObj context = CGLGetCurrentContext();
        CGLError err = CGLGetParameter(context, kCGLCPSwapInterval, &originalSwapInterval);
        OVR_ASSERT_AND_UNUSED(err == kCGLNoError, err);
        
        if (ourSwapInterval != originalSwapInterval)
            CGLSetParameter(context, kCGLCPSwapInterval, &ourSwapInterval);
        
        CGLFlushDrawable(context);
        
#elif defined(OVR_OS_LINUX)
        originalSwapInterval = 0;
        GLXDrawable drawable = glXGetCurrentDrawable();
        struct _XDisplay* x11Display = RParams.Disp;

        if(GLE_GLX_EXT_swap_control)
        {
            static_assert(sizeof(GLuint) == sizeof(originalSwapInterval), "size mismatch");
            glXQueryDrawable(x11Display, drawable, GLX_SWAP_INTERVAL_EXT, (GLuint*)&originalSwapInterval);

            if (ourSwapInterval != originalSwapInterval)
                glXSwapIntervalEXT(x11Display, drawable, ourSwapInterval);
        }
        else if (GLE_MESA_swap_control) // There is also GLX_SGI_swap_control
        {
            originalSwapInterval = glXGetSwapIntervalMESA();

            if (ourSwapInterval != originalSwapInterval)
                glXSwapIntervalMESA(ourSwapInterval);
        }

        glXSwapBuffers(x11Display, drawable);
#endif

        // Force GPU to flush the scene, resulting in the lowest possible latency.
        // It's critical that this flush is *after* present, because it results in the wait
        // below completing after the vsync.
        // With the display driver (direct mode) this flush is obsolete and theoretically
        // should be a no-op and so doesn't need to be done if running in direct mode.
        if (RenderState->OurHMDInfo.InCompatibilityMode &&
            !(RenderState->DistortionCaps & ovrDistortionCap_ProfileNoSpinWaits))
        {
            WaitUntilGpuIdle();
        }

        // Restore the original swap interval if we changed it above.
        if (originalSwapInterval != ourSwapInterval)
        {
#if defined(OVR_OS_WIN32)
            wglSwapIntervalEXT(originalSwapInterval);
#elif defined(OVR_OS_MAC)
            CGLSetParameter(context, kCGLCPSwapInterval, &originalSwapInterval);
#elif defined(OVR_OS_LINUX)
            if(GLE_GLX_EXT_swap_control)
                glXSwapIntervalEXT(x11Display, drawable, (GLuint)originalSwapInterval);
            else if(GLE_MESA_swap_control)
                glXSwapIntervalMESA(originalSwapInterval);
#endif
        }
    }

    currContext.Bind();
}

void DistortionRenderer::WaitUntilGpuIdle()
{
	glFinish(); // Block until current OpenGL commands (including swap) are complete.
}

double DistortionRenderer::FlushGpuAndWaitTillTime(double absTime)
{
    // because glFlush() is not strict enough certain GL drivers
    // we do a glFinish(), but before doing so, we make sure we're not
    // running late
    double initialTime = ovr_GetTimeInSeconds();
    if (initialTime >= absTime)
        return 0.0;

    glFinish();

    return WaitTillTime(absTime);
}

bool DistortionRenderer::initBuffersAndShaders()
{
    for ( int eyeNum = 0; eyeNum < 2; eyeNum++ )
    {
        // Allocate & generate distortion mesh vertices.
        ovrDistortionMesh meshData;

        if (!CalculateDistortionMeshFromFOV(RenderState->RenderInfo,
                                    RenderState->Distortion[eyeNum],
                                    (RenderState->EyeRenderDesc[eyeNum].Eye == ovrEye_Left ? StereoEye_Left : StereoEye_Right),
                                    RenderState->EyeRenderDesc[eyeNum].Fov,
                                    RenderState->DistortionCaps,
                                    &meshData))
        {
            OVR_ASSERT(false);
            return false;
        }

        // Now parse the vertex data and create a render ready vertex buffer from it
        DistortionVertex *   pVBVerts    = (DistortionVertex*)OVR_ALLOC ( sizeof(DistortionVertex) * meshData.VertexCount );
        DistortionVertex *   pCurVBVert  = pVBVerts;
        ovrDistortionVertex* pCurOvrVert = meshData.pVertexData;

        for ( unsigned vertNum = 0; vertNum < meshData.VertexCount; vertNum++ )
        {
            pCurVBVert->ScreenPosNDC.x = pCurOvrVert->ScreenPosNDC.x;
            pCurVBVert->ScreenPosNDC.y = pCurOvrVert->ScreenPosNDC.y;

            // Previous code here did this: pCurVBVert->TanEyeAnglesR = (*(Vector2f*)&pCurOvrVert->TanEyeAnglesR); However that's an usafe
            // cast of unrelated types which can result in undefined behavior by a conforming compiler. A safe equivalent is simply memcpy.
            static_assert(sizeof(OVR::Vector2f) == sizeof(ovrVector2f), "Mismatch of structs that are presumed binary equivalents.");
            memcpy(&pCurVBVert->TanEyeAnglesR, &pCurOvrVert->TanEyeAnglesR, sizeof(pCurVBVert->TanEyeAnglesR));
            memcpy(&pCurVBVert->TanEyeAnglesG, &pCurOvrVert->TanEyeAnglesG, sizeof(pCurVBVert->TanEyeAnglesG));
            memcpy(&pCurVBVert->TanEyeAnglesB, &pCurOvrVert->TanEyeAnglesB, sizeof(pCurVBVert->TanEyeAnglesB));

            // Convert [0.0f,1.0f] to [0,255]
			if (RenderState->DistortionCaps & ovrDistortionCap_Vignette)
            {
                if(RenderState->DistortionCaps & ovrDistortionCap_SRGB)
                    pCurOvrVert->VignetteFactor = pow(pCurOvrVert->VignetteFactor, 2.1f);

                pCurVBVert->Col.R = (uint8_t)( Alg::Max ( pCurOvrVert->VignetteFactor, 0.0f ) * 255.99f );
            }
			else
				pCurVBVert->Col.R = 255;

            pCurVBVert->Col.G = pCurVBVert->Col.R;
            pCurVBVert->Col.B = pCurVBVert->Col.R;
            pCurVBVert->Col.A = (uint8_t)( pCurOvrVert->TimeWarpFactor * 255.99f );;
            pCurOvrVert++;
            pCurVBVert++;
        }

        DistortionMeshVBs[eyeNum] = *new Buffer(&RParams);
        DistortionMeshVBs[eyeNum]->Data ( Buffer_Vertex | Buffer_ReadOnly, pVBVerts, sizeof(DistortionVertex) * meshData.VertexCount );
        DistortionMeshIBs[eyeNum] = *new Buffer(&RParams);
        DistortionMeshIBs[eyeNum]->Data ( Buffer_Index | Buffer_ReadOnly, meshData.pIndexData, ( sizeof(int16_t) * meshData.IndexCount ) );

        OVR_FREE ( pVBVerts );
        ovrHmd_DestroyDistortionMesh( &meshData );
    }

    initShaders();

    return true;
}

void DistortionRenderer::renderDistortion(Texture* leftEyeTexture, Texture* rightEyeTexture)
{
    bool overdriveActive = IsOverdriveActive();
    int currOverdriveTextureIndex = -1;

    if(overdriveActive)
    {
        currOverdriveTextureIndex = (LastUsedOverdriveTextureIndex + 1) % NumOverdriveTextures;

        //glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, OverdriveFbo );
        
        GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(OVR_ARRAY_COUNT(drawBuffers), drawBuffers);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pOverdriveTextures[currOverdriveTextureIndex]->TexId, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, OverdriveBackBufferTexture->TexId, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
        OVR_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    setViewport( Recti(0,0, RParams.BackBufferSize.w, RParams.BackBufferSize.h) );

	if (RenderState->DistortionCaps & ovrDistortionCap_SRGB)
		glEnable(GL_FRAMEBUFFER_SRGB);
    else
        glDisable(GL_FRAMEBUFFER_SRGB);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
    
    if (GLE_EXT_draw_buffers2)
    {
        glDisablei(GL_BLEND, 0);
        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    }
    else
    {
        glDisable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    }
    
    glDisable(GL_DITHER);
    glDisable(GL_RASTERIZER_DISCARD);
    if (GLEContext::GetCurrentContext()->WholeVersion >= 302)
    {
        glDisable(GL_SAMPLE_MASK);
    }
        
	glClearColor(
		RenderState->ClearColor[0],
		RenderState->ClearColor[1],
		RenderState->ClearColor[2],
		RenderState->ClearColor[3] );

    glClear(GL_COLOR_BUFFER_BIT);

    for (int eyeNum = 0; eyeNum < 2; eyeNum++)
    {
		ShaderFill distortionShaderFill(DistortionShader);
        distortionShaderFill.SetTexture(0, eyeNum == 0 ? leftEyeTexture : rightEyeTexture);

        if(overdriveActive)
        {
            distortionShaderFill.SetTexture(1, pOverdriveTextures[LastUsedOverdriveTextureIndex]);

            float overdriveScaleRegularRise;
            float overdriveScaleRegularFall;
            GetOverdriveScales(overdriveScaleRegularRise, overdriveScaleRegularFall);
            DistortionShader->SetUniform3f("OverdriveScales_IsSrgb", overdriveScaleRegularRise, overdriveScaleRegularFall,
																	(RenderState->DistortionCaps & ovrDistortionCap_SRGB) ? 1.0f : -1.0f);
        }
        else
        {
            // -1.0f disables PLO            
            DistortionShader->SetUniform3f("OverdriveScales_IsSrgb", -1.0f, -1.0f, -1.0f);
        }

		DistortionShader->SetUniform2f("EyeToSourceUVScale",  eachEye[eyeNum].UVScaleOffset[0].x, eachEye[eyeNum].UVScaleOffset[0].y);
		DistortionShader->SetUniform2f("EyeToSourceUVOffset", eachEye[eyeNum].UVScaleOffset[1].x, eachEye[eyeNum].UVScaleOffset[1].y);
        
        if (RenderState->DistortionCaps & ovrDistortionCap_TimeWarp)
		{                       
            Matrix4f startEndMatrices[2];
            double timewarpIMUTime = 0.;
            CalculateOrientationTimewarpFromSensors(
                RenderState->EyeRenderPoses[eyeNum].Orientation,
                SensorReader, Timing->GetTimewarpTiming()->EyeStartEndTimes[eyeNum],
                startEndMatrices, timewarpIMUTime);
            Timing->SetTimewarpIMUTime(timewarpIMUTime);

            // Feed identity like matrices in until we get proper timewarp calculation going on
            DistortionShader->SetUniform4x4f("EyeRotationStart", startEndMatrices[0].Transposed());
            DistortionShader->SetUniform4x4f("EyeRotationEnd", startEndMatrices[1].Transposed());

            renderPrimitives(&distortionShaderFill, DistortionMeshVBs[eyeNum], DistortionMeshIBs[eyeNum],
                            0, (int)DistortionMeshIBs[eyeNum]->GetSize()/2, Prim_Triangles, &DistortionMeshVAOs[eyeNum], true);
		}
        else
        {
            renderPrimitives(&distortionShaderFill, DistortionMeshVBs[eyeNum], DistortionMeshIBs[eyeNum],
                            0, (int)DistortionMeshIBs[eyeNum]->GetSize()/2, Prim_Triangles, &DistortionMeshVAOs[eyeNum], true);
        }
    }

    LastUsedOverdriveTextureIndex = currOverdriveTextureIndex;

    // Re-activate to only draw on back buffer
    if(overdriveActive)
    {
        GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0};
        glDrawBuffers(OVR_ARRAY_COUNT(drawBuffers), drawBuffers);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        //glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
        //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 0, 0);
        OVR_ASSERT(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        glBindFramebuffer( GL_READ_FRAMEBUFFER, OverdriveFbo );
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OverdriveBackBufferTexture->TexId, 0);
        glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
        OVR_ASSERT(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        glBlitFramebuffer( 0, 0, OverdriveBackBufferTexture->GetWidth(), OverdriveBackBufferTexture->GetHeight(),
                           0, 0, OverdriveBackBufferTexture->GetWidth(), OverdriveBackBufferTexture->GetHeight(),
                           GL_COLOR_BUFFER_BIT, GL_NEAREST );

        glBindFramebuffer( GL_FRAMEBUFFER, 0 );
        GLint err = glGetError();
        OVR_ASSERT(!err); OVR_UNUSED(err);
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
       
    Ptr<ShaderSet> quadShader = (RenderState->DistortionCaps & ovrDistortionCap_SRGB) ? SimpleQuadGammaShader : SimpleQuadShader;
    ShaderFill quadFill(quadShader);
    //quadFill.SetInputLayout(SimpleQuadVertexIL);

    setViewport(Recti(0,0, RParams.BackBufferSize.w, RParams.BackBufferSize.h));

    quadShader->SetUniform2f("Scale", 0.3f, 0.3f);
    quadShader->SetUniform4f("Color", (float)latencyTesterDrawColor[0] / 255.99f,
                                      (float)latencyTesterDrawColor[0] / 255.99f,
                                      (float)latencyTesterDrawColor[0] / 255.99f,
                                      1.0f);

    for(int eyeNum = 0; eyeNum < 2; eyeNum++)
    {
        quadShader->SetUniform2f("PositionOffset", eyeNum == 0 ? -0.5f : 0.5f, 0.0f);    
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

    Ptr<ShaderSet> quadShader = (RenderState->DistortionCaps & ovrDistortionCap_SRGB) ? SimpleQuadGammaShader : SimpleQuadShader;
    ShaderFill quadFill(quadShader);

    setViewport(Recti(0,0, RParams.BackBufferSize.w, RParams.BackBufferSize.h));

#ifdef OVR_BUILD_DEBUG
    quadShader->SetUniform4f("Color", (float)latencyTesterPixelColor[0] / 255.99f,
                                      (float)latencyTesterPixelColor[1] / 255.99f,
                                      (float)latencyTesterPixelColor[2] / 255.99f,
                                      1.0f);

    Vector2f scale(20.0f / RParams.BackBufferSize.w, 20.0f / RParams.BackBufferSize.h); 
#else
    quadShader->SetUniform4f("Color", (float)latencyTesterPixelColor[0] / 255.99f,
                                      (float)latencyTesterPixelColor[0] / 255.99f,
                                      (float)latencyTesterPixelColor[0] / 255.99f,
                                      1.0f);

    Vector2f scale(1.0f / RParams.BackBufferSize.w, 1.0f / RParams.BackBufferSize.h); 
#endif
    quadShader->SetUniform2f("Scale", scale.x, scale.y);

    float xOffset = RenderState->RenderInfo.OffsetLatencyTester ? -0.5f * scale.x : 1.0f - scale.x;
    float yOffset = 1.0f - scale.y;

    // Render the latency tester quad in the correct location.
    if (RenderState->RenderInfo.Rotation == 270)
    {
        xOffset = -xOffset;
    }
    else if (RenderState->RenderInfo.Rotation == 180)
    {
        xOffset = -xOffset;
        yOffset = -yOffset;
    }
    else if (RenderState->RenderInfo.Rotation == 90)
    {
        yOffset = -yOffset;
    }

    quadShader->SetUniform2f("PositionOffset", xOffset, yOffset);

    renderPrimitives(&quadFill, LatencyTesterQuadVB, NULL, 0, numQuadVerts, Prim_TriangleStrip, &LatencyVAO, false);
}

void DistortionRenderer::renderPrimitives(
                          const ShaderFill* fill,
                          Buffer* vertices, Buffer* indices,
                          int offset, int count,
                          PrimitiveType rprim, GLuint* vao, bool isDistortionMesh)
{
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

            glBindVertexArray(0);
		}
		else
		{
            if (GL_ARB_vertex_array_object)
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

				glVertexAttribPointer(locs[0], 2, GL_FLOAT, false, sizeof(DistortionVertex), reinterpret_cast<char*>(offset)+offsetof(DistortionVertex, ScreenPosNDC));
				glVertexAttribPointer(locs[1], 4, GL_UNSIGNED_BYTE, true, sizeof(DistortionVertex), reinterpret_cast<char*>(offset)+offsetof(DistortionVertex, Col));
				glVertexAttribPointer(locs[2], 2, GL_FLOAT, false, sizeof(DistortionVertex), reinterpret_cast<char*>(offset)+offsetof(DistortionVertex, TanEyeAnglesR));
				glVertexAttribPointer(locs[3], 2, GL_FLOAT, false, sizeof(DistortionVertex), reinterpret_cast<char*>(offset)+offsetof(DistortionVertex, TanEyeAnglesG));
				glVertexAttribPointer(locs[4], 2, GL_FLOAT, false, sizeof(DistortionVertex), reinterpret_cast<char*>(offset)+offsetof(DistortionVertex, TanEyeAnglesB));
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


            if (!GL_ARB_vertex_array_object)
            {
				for (int i = 0; i < attributeCount; ++i)
                    glDisableVertexAttribArray(locs[i]);
            }

			delete[] locs;

            if (GL_ARB_vertex_array_object)
            {
                glBindVertexArray(0);
            }
		}
	}
}

void DistortionRenderer::setViewport(const Recti& vp)
{
    glViewport(vp.x, vp.y, vp.w, vp.h);
}


void DistortionRenderer::initShaders()
{
    const char* shaderPrefix = (GLEContext::GetCurrentContext()->WholeVersion >= 302) ? glsl3Prefix : glsl2Prefix;

    {
		ShaderInfo vsInfo = DistortionVertexShaderLookup[DistortionVertexShaderBitMask & RenderState->DistortionCaps];
        if(vsInfo.ShaderData != NULL)
        {
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
        }
        else
        {
            OVR_ASSERT_M(false, "Unsupported distortion feature used\n");
        }

		ShaderInfo psInfo = DistortionPixelShaderLookup[DistortionPixelShaderBitMask & RenderState->DistortionCaps];
        if(psInfo.ShaderData != NULL)
        {
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
        else
        {
            OVR_ASSERT_M(false, "Unsupported distortion feature used\n");
        }
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
    {
        size_t vsSize = strlen(shaderPrefix)+sizeof(SimpleQuad_vs);
        char* vsSource = new char[vsSize];
        OVR_strcpy(vsSource, vsSize, shaderPrefix);
        OVR_strcat(vsSource, vsSize, SimpleQuad_vs);

        Ptr<GL::VertexShader> vs = *new GL::VertexShader(
            &RParams,
            (void*)vsSource, vsSize,
            SimpleQuad_vs_refl, sizeof(SimpleQuad_vs_refl) / sizeof(SimpleQuad_vs_refl[0]));

        SimpleQuadGammaShader = *new ShaderSet;
        SimpleQuadGammaShader->SetShader(vs);

        delete[](vsSource);

        size_t psSize = strlen(shaderPrefix)+sizeof(SimpleQuadGamma_fs);
        char* psSource = new char[psSize];
        OVR_strcpy(psSource, psSize, shaderPrefix);
        OVR_strcat(psSource, psSize, SimpleQuadGamma_fs);

        Ptr<GL::FragmentShader> ps  = *new GL::FragmentShader(
            &RParams,
            (void*)psSource, psSize,
            SimpleQuadGamma_fs_refl, sizeof(SimpleQuadGamma_fs_refl) / sizeof(SimpleQuadGamma_fs_refl[0]));

        SimpleQuadGammaShader->SetShader(ps);

        delete[](psSource);
    }
}


void DistortionRenderer::destroy()
{
    Context currContext;
    currContext.InitFromCurrent();
    
    distortionContext.Bind();

	for(int eyeNum = 0; eyeNum < 2; eyeNum++)
	{
        if (GL_ARB_vertex_array_object)
        {
            glDeleteVertexArrays(1, &DistortionMeshVAOs[eyeNum]);
        }

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

    if(LatencyVAO != 0)
    {
        glDeleteVertexArrays(1, &LatencyVAO);
	    LatencyVAO = 0;
    }

    if(OverdriveFbo != 0)
    {
        glDeleteFramebuffers(1, &OverdriveFbo);
    }

    currContext.Bind();
    distortionContext.Destroy();
    // Who is responsible for destroying the app's context?
}


}}} // OVR::CAPI::GL
