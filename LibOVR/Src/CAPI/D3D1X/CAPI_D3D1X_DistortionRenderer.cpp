/************************************************************************************

Filename    :   CAPI_D3D1X_DistortionRenderer.cpp
Content     :   Experimental distortion renderer
Created     :   November 11, 2013
Authors     :   Volga Aksoy, Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.1 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.1 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "CAPI_D3D1X_DistortionRenderer.h"

#include "../../OVR_CAPI_D3D.h"

namespace OVR { namespace CAPI { namespace D3D_NS {

#include "../Shaders/Distortion_vs.h"
#include "../Shaders/Distortion_vs_refl.h"
#include "../Shaders/Distortion_ps.h"
#include "../Shaders/Distortion_ps_refl.h"
#include "../Shaders/DistortionChroma_vs.h"
#include "../Shaders/DistortionChroma_vs_refl.h"
#include "../Shaders/DistortionChroma_ps.h"
#include "../Shaders/DistortionChroma_ps_refl.h"
#include "../Shaders/DistortionTimewarp_vs.h"
#include "../Shaders/DistortionTimewarp_vs_refl.h"
#include "../Shaders/DistortionTimewarpChroma_vs.h"
#include "../Shaders/DistortionTimewarpChroma_vs_refl.h"
    
#include "../Shaders/SimpleQuad_vs.h"
#include "../Shaders/SimpleQuad_vs_refl.h"
#include "../Shaders/SimpleQuad_ps.h"
#include "../Shaders/SimpleQuad_ps_refl.h"

// Distortion pixel shader lookup.
//  Bit 0: Chroma Correction
//  Bit 1: Timewarp

enum {
    DistortionVertexShaderBitMask = 3,
    DistortionVertexShaderCount   = DistortionVertexShaderBitMask + 1,
    DistortionPixelShaderBitMask  = 1,
    DistortionPixelShaderCount    = DistortionPixelShaderBitMask + 1
};

struct PrecompiledShader
{
    const unsigned char* ShaderData;
    size_t ShaderSize;
    const ShaderBase::Uniform* ReflectionData;
    size_t ReflectionSize;
};

// Do add a new distortion shader use these macros (with or w/o reflection)
#define PCS_NOREFL(shader) { shader, sizeof(shader), NULL, 0 }
#define PCS_REFL__(shader) { shader, sizeof(shader), shader ## _refl, sizeof( shader ## _refl )/sizeof(*(shader ## _refl)) }


static PrecompiledShader DistortionVertexShaderLookup[DistortionVertexShaderCount] =
{
    PCS_REFL__(Distortion_vs),
    PCS_REFL__(DistortionChroma_vs),
    PCS_REFL__(DistortionTimewarp_vs),
    PCS_REFL__(DistortionTimewarpChroma_vs),
};

static PrecompiledShader DistortionPixelShaderLookup[DistortionPixelShaderCount] =
{
    PCS_NOREFL(Distortion_ps),
    PCS_NOREFL(DistortionChroma_ps)
};

void DistortionShaderBitIndexCheck()
{
    OVR_COMPILER_ASSERT(ovrDistortion_Chromatic == 1);
    OVR_COMPILER_ASSERT(ovrDistortion_TimeWarp  == 2);
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


//----------------------------------------------------------------------------
// ***** D3D1X::DistortionRenderer

DistortionRenderer::DistortionRenderer(ovrHmd hmd, FrameTimeManager& timeManager,
                                       const HMDRenderState& renderState)
    : CAPI::DistortionRenderer(ovrRenderAPI_D3D11, hmd, timeManager, renderState)
{
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
    return new DistortionRenderer(hmd, timeManager, renderState);
}


bool DistortionRenderer::Initialize(const ovrRenderAPIConfig* apiConfig,
                                    unsigned hmdCaps, unsigned distortionCaps)
{
    // TBD: Decide if hmdCaps are needed here or are a part of RenderState
    OVR_UNUSED(hmdCaps);

    const ovrD3D1X(Config)* config = (const ovrD3D1X(Config)*)apiConfig;

    if (!config)
    {
        // Cleanup
        pEyeTextures[0].Clear();
        pEyeTextures[1].Clear();
        memset(&RParams, 0, sizeof(RParams));
        return true;
    }

    if (!config->D3D_NS.pDevice || !config->D3D_NS.pBackBufferRT)
        return false;
    
    RParams.pDevice		   = config->D3D_NS.pDevice;    
    RParams.pContext       = D3DSELECT_10_11(config->D3D_NS.pDevice, config->D3D_NS.pDeviceContext);
    RParams.pBackBufferRT  = config->D3D_NS.pBackBufferRT;
    RParams.pSwapChain     = config->D3D_NS.pSwapChain;
    RParams.RTSize         = config->D3D_NS.Header.RTSize;
    RParams.Multisample    = config->D3D_NS.Header.Multisample;

    DistortionCaps = distortionCaps;

    //DistortionWarper.SetVsync((hmdCaps & ovrHmdCap_NoVSync) ? false : true);

    pEyeTextures[0] = *new Texture(&RParams, Texture_RGBA, Sizei(0),
                                   getSamplerState(Sample_Linear|Sample_ClampBorder));
    pEyeTextures[1] = *new Texture(&RParams, Texture_RGBA, Sizei(0),
                                   getSamplerState(Sample_Linear|Sample_ClampBorder));

    initBuffersAndShaders();

    // Rasterizer state
    D3D1X_(RASTERIZER_DESC) rs;
    memset(&rs, 0, sizeof(rs));
    rs.AntialiasedLineEnable = true;
    rs.CullMode              = D3D1X_(CULL_BACK);    
    rs.DepthClipEnable       = true;
    rs.FillMode              = D3D1X_(FILL_SOLID);
    RParams.pDevice->CreateRasterizerState(&rs, &Rasterizer.GetRawRef());

    // TBD: Blend state.. not used?
    // We'll want to turn off blending

#if (OVR_D3D_VERSION == 11)
    GpuProfiler.Init(RParams.pDevice, RParams.pContext);
#endif

    return true;
}


void DistortionRenderer::SubmitEye(int eyeId, ovrTexture* eyeTexture)
{
    const ovrD3D1X(Texture)* tex = (const ovrD3D1X(Texture)*)eyeTexture;

    if (eyeTexture)
    {
        // Use tex->D3D_NS.Header.RenderViewport to update UVs for rendering in case they changed.
        // TBD: This may be optimized through some caching. 
        ovrEyeDesc     ed = RState.EyeRenderDesc[eyeId].Desc;
        ed.TextureSize    = tex->D3D_NS.Header.TextureSize;
        ed.RenderViewport = tex->D3D_NS.Header.RenderViewport;

        ovrHmd_GetRenderScaleAndOffset(HMD, ed, DistortionCaps, UVScaleOffset[eyeId]);

        pEyeTextures[eyeId]->UpdatePlaceholderTexture(tex->D3D_NS.pTexture, tex->D3D_NS.pSRView,
                                                      tex->D3D_NS.Header.TextureSize);
    }
}

void DistortionRenderer::EndFrame(bool swapBuffers, unsigned char* latencyTesterDrawColor,
                                                    unsigned char* latencyTester2DrawColor)
{

#if 0

    // MA:  This causes orientation and positional stutter!! NOT USABLE.
    if (!TimeManager.NeedDistortionTimeMeasurement() &&
        (RState.DistortionCaps & ovrDistortion_TimeWarp))
    {
        // Wait for timewarp distortion if it is time
        FlushGpuAndWaitTillTime(TimeManager.GetFrameTiming().TimewarpPointTime);
    }

    // Always measure distortion time so that TimeManager can better
    // estimate latency-reducing time-warp wait timing.
    {
        GpuProfiler.BeginQuery();

        renderDistortion(pEyeTextures[0], pEyeTextures[1]);

        GpuProfiler.EndQuery();
        TimeManager.AddDistortionTimeMeasurement(GpuProfiler.GetTiming(false));
    }
#else

    if (!TimeManager.NeedDistortionTimeMeasurement())
    {
		if (RState.DistortionCaps & ovrDistortion_TimeWarp)
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
#endif

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
        if (RParams.pSwapChain)
        {
            UINT swapInterval = (RState.HMDCaps & ovrHmdCap_NoVSync) ? 0 : 1;
            RParams.pSwapChain->Present(swapInterval, 0);
            
            // Force GPU to flush the scene, resulting in the lowest possible latency.
            // It's critical that this flush is *after* present.
            WaitUntilGpuIdle();
        }
        else
        {
            // TBD: Generate error - swapbuffer option used with null swapchain.
        }
    }
}


void DistortionRenderer::WaitUntilGpuIdle()
{
    // Flush and Stall CPU while waiting for GPU to complete rendering all of the queued draw calls
    D3D1x_QUERY_DESC queryDesc = { D3D1X_(QUERY_EVENT), 0 };
    Ptr<ID3D1xQuery> query;
    BOOL             done = FALSE;

    if (RParams.pDevice->CreateQuery(&queryDesc, &query.GetRawRef()) == S_OK)
    {
        D3DSELECT_10_11(query->End(),
                        RParams.pContext->End(query));

        // GetData will returns S_OK for both done == TRUE or FALSE.
        // Exit on failure to avoid infinite loop.
        do { }
        while(!done &&
              !FAILED(D3DSELECT_10_11(query->GetData(&done, sizeof(BOOL), 0),
                                      RParams.pContext->GetData(query, &done, sizeof(BOOL), 0)))
             );
    }
}

double DistortionRenderer::FlushGpuAndWaitTillTime(double absTime)
{
	double       initialTime = ovr_GetTimeInSeconds();
	if (initialTime >= absTime)
		return 0.0;

	// Flush and Stall CPU while waiting for GPU to complete rendering all of the queued draw calls
    D3D1x_QUERY_DESC queryDesc = { D3D1X_(QUERY_EVENT), 0 };
    Ptr<ID3D1xQuery> query;
    BOOL             done = FALSE;
	bool             callGetData = false;

    if (RParams.pDevice->CreateQuery(&queryDesc, &query.GetRawRef()) == S_OK)
    {
        D3DSELECT_10_11(query->End(),
                        RParams.pContext->End(query));
		callGetData = true;
	}

	double newTime   = initialTime;
	volatile int i;

	while (newTime < absTime)
	{
		if (callGetData)
		{
			// GetData will returns S_OK for both done == TRUE or FALSE.
			// Stop calling GetData on failure.
			callGetData = !FAILED(D3DSELECT_10_11(query->GetData(&done, sizeof(BOOL), 0),
					                              RParams.pContext->GetData(query, &done, sizeof(BOOL), 0))) && !done;
		}
		else
		{
			for (int j = 0; j < 50; j++)
				i = 0;
		}
		newTime = ovr_GetTimeInSeconds();
	}

	// How long we waited
	return newTime - initialTime;
}

void DistortionRenderer::initBuffersAndShaders()
{
    for ( int eyeNum = 0; eyeNum < 2; eyeNum++ )
    {
        // Allocate & generate distortion mesh vertices.
        ovrDistortionMesh meshData;

//        double startT = ovr_GetTimeInSeconds();

        if (!ovrHmd_CreateDistortionMesh( HMD, RState.EyeRenderDesc[eyeNum].Desc,
                                          RState.DistortionCaps,
                                          UVScaleOffset[eyeNum], &meshData) )
        {
            OVR_ASSERT(false);
            continue;
        }

//        double deltaT = ovr_GetTimeInSeconds() - startT;
//        LogText("GenerateDistortion time = %f\n", deltaT);

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
        DistortionMeshVBs[eyeNum]->Data ( Buffer_Vertex, pVBVerts, sizeof(DistortionVertex) * meshData.VertexCount );
        DistortionMeshIBs[eyeNum] = *new Buffer(&RParams);
        DistortionMeshIBs[eyeNum]->Data ( Buffer_Index, meshData.pIndexData, ( sizeof(INT16) * meshData.IndexCount ) );

        OVR_FREE ( pVBVerts );
        ovrHmd_DestroyDistortionMesh( &meshData );
    }

    // Uniform buffers
    for(int i = 0; i < Shader_Count; i++)
    {
        UniformBuffers[i] = *new Buffer(&RParams);
        //MaxTextureSet[i] = 0;
    }

    initShaders();
}

void DistortionRenderer::renderDistortion(Texture* leftEyeTexture, Texture* rightEyeTexture)
{
    RParams.pContext->RSSetState(Rasterizer);

    RParams.pContext->OMSetRenderTargets(1, &RParams.pBackBufferRT, 0);
    
    setViewport(Recti(0,0, RParams.RTSize.w, RParams.RTSize.h));
        
    // Not affected by viewport.
    RParams.pContext->ClearRenderTargetView(RParams.pBackBufferRT, RState.ClearColor);

    for(int eyeNum = 0; eyeNum < 2; eyeNum++)
    {        
		ShaderFill distortionShaderFill(DistortionShader);
        distortionShaderFill.SetTexture(0, eyeNum == 0 ? leftEyeTexture : rightEyeTexture);
        distortionShaderFill.SetInputLayout(DistortionVertexIL);

        DistortionShader->SetUniform2f("EyeToSourceUVScale",  UVScaleOffset[eyeNum][0].x, UVScaleOffset[eyeNum][0].y);
        DistortionShader->SetUniform2f("EyeToSourceUVOffset", UVScaleOffset[eyeNum][1].x, UVScaleOffset[eyeNum][1].y);
        
		if (DistortionCaps & ovrDistortion_TimeWarp)
		{                       
            ovrMatrix4f timeWarpMatrices[2];            
            ovrHmd_GetEyeTimewarpMatrices(HMD, (ovrEyeType)eyeNum,
                                          RState.EyeRenderPoses[eyeNum], timeWarpMatrices);

            // Feed identity like matrices in until we get proper timewarp calculation going on
			DistortionShader->SetUniform4x4f("EyeRotationStart", Matrix4f(timeWarpMatrices[0]));
			DistortionShader->SetUniform4x4f("EyeRotationEnd",   Matrix4f(timeWarpMatrices[1]));

            renderPrimitives(&distortionShaderFill, DistortionMeshVBs[eyeNum], DistortionMeshIBs[eyeNum],
                            NULL, 0, (int)DistortionMeshVBs[eyeNum]->GetSize(), Prim_Triangles);
		}
        else
        {
            renderPrimitives(&distortionShaderFill, DistortionMeshVBs[eyeNum], DistortionMeshIBs[eyeNum],
                            NULL, 0, (int)DistortionMeshVBs[eyeNum]->GetSize(), Prim_Triangles);
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

    LatencyTesterQuadVB->Data(Buffer_Vertex, NULL, numQuadVerts * sizeof(Vertex));
    Vertex* vertices = (Vertex*)LatencyTesterQuadVB->Map(0, numQuadVerts * sizeof(Vertex), Map_Discard);
    if(!vertices)
    {
        OVR_ASSERT(false); // failed to lock vertex buffer
        return;
    }

    const float left   = -1.0f;
    const float top    = -1.0f;
    const float right  =  1.0f;
    const float bottom =  1.0f;

    vertices[0] = Vertex(Vector3f(left,  top,    0.0f), Color(255, 255, 255, 255));
    vertices[1] = Vertex(Vector3f(left,  bottom, 0.0f), Color(255, 255, 255, 255));
    vertices[2] = Vertex(Vector3f(right, top,    0.0f), Color(255, 255, 255, 255));
    vertices[3] = Vertex(Vector3f(right, bottom, 0.0f), Color(255, 255, 255, 255));

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
    quadFill.SetInputLayout(SimpleQuadVertexIL);

    setViewport(Recti(0,0, RParams.RTSize.w, RParams.RTSize.h));

    SimpleQuadShader->SetUniform2f("Scale", 0.2f, 0.2f);
    SimpleQuadShader->SetUniform4f("Color", (float)latencyTesterDrawColor[0] / 255.99f,
                                            (float)latencyTesterDrawColor[0] / 255.99f,
                                            (float)latencyTesterDrawColor[0] / 255.99f,
                                            1.0f);

    for(int eyeNum = 0; eyeNum < 2; eyeNum++)
    {
        SimpleQuadShader->SetUniform2f("PositionOffset", eyeNum == 0 ? -0.4f : 0.4f, 0.0f);    
        renderPrimitives(&quadFill, LatencyTesterQuadVB, NULL, NULL, 0, numQuadVerts, Prim_TriangleStrip);
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
    quadFill.SetInputLayout(SimpleQuadVertexIL);

    setViewport(Recti(0,0, RParams.RTSize.w, RParams.RTSize.h));

    SimpleQuadShader->SetUniform4f("Color", (float)latencyTesterPixelColor[0] / 255.99f,
                                            (float)latencyTesterPixelColor[0] / 255.99f,
                                            (float)latencyTesterPixelColor[0] / 255.99f,
                                            1.0f);

    Vector2f scale(2.0f / RParams.RTSize.w, 2.0f / RParams.RTSize.h); 
    SimpleQuadShader->SetUniform2f("Scale", scale.x, scale.y);
    SimpleQuadShader->SetUniform2f("PositionOffset", 1.0f, 1.0f);
    renderPrimitives(&quadFill, LatencyTesterQuadVB, NULL, NULL, 0, numQuadVerts, Prim_TriangleStrip);
}

void DistortionRenderer::renderPrimitives(
                          const ShaderFill* fill,
                          Buffer* vertices, Buffer* indices,
                          Matrix4f* viewMatrix, int offset, int count,
                          PrimitiveType rprim)
{
    OVR_ASSERT(fill->GetInputLayout() != 0);    
    RParams.pContext->IASetInputLayout((ID3D1xInputLayout*)fill->GetInputLayout());    

    if (indices)
    {
        RParams.pContext->IASetIndexBuffer(indices->GetBuffer(), DXGI_FORMAT_R16_UINT, 0);
    }

    ID3D1xBuffer* vertexBuffer = vertices->GetBuffer();
    UINT          vertexStride = sizeof(Vertex);
    UINT          vertexOffset = offset;
    RParams.pContext->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);

    ShaderSet* shaders = ((ShaderFill*)fill)->GetShaders();

    ShaderBase*     vshader = ((ShaderBase*)shaders->GetShader(Shader_Vertex));
    unsigned char*  vertexData = vshader->UniformData;
    if (vertexData)
    {
		// TODO: some VSes don't start with StandardUniformData!
		if ( viewMatrix )
		{
			StandardUniformData* stdUniforms = (StandardUniformData*) vertexData;
			stdUniforms->View = viewMatrix->Transposed();
			stdUniforms->Proj = StdUniforms.Proj;
		}
		UniformBuffers[Shader_Vertex]->Data(Buffer_Uniform, vertexData, vshader->UniformsSize);
		vshader->SetUniformBuffer(UniformBuffers[Shader_Vertex]);
    }

    for(int i = Shader_Vertex + 1; i < Shader_Count; i++)
    {
        if (shaders->GetShader(i))
        {
            ((ShaderBase*)shaders->GetShader(i))->UpdateBuffer(UniformBuffers[i]);
            ((ShaderBase*)shaders->GetShader(i))->SetUniformBuffer(UniformBuffers[i]);
        }
    }

    D3D1X_(PRIMITIVE_TOPOLOGY) prim;
    switch(rprim)
    {
    case Prim_Triangles:
        prim = D3D1X_(PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        break;
    case Prim_Lines:
        prim = D3D1X_(PRIMITIVE_TOPOLOGY_LINELIST);
        break;
    case Prim_TriangleStrip:
        prim = D3D1X_(PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        break;
    default:
        OVR_ASSERT(0);
        return;
    }
    RParams.pContext->IASetPrimitiveTopology(prim);

    fill->Set(rprim);

    if (indices)
    {
        RParams.pContext->DrawIndexed(count, 0, 0);
    }
    else
    {
        RParams.pContext->Draw(count, 0);
    }
}

void DistortionRenderer::setViewport(const Recti& vp)
{
    D3D1x_VIEWPORT d3dvp;

    d3dvp.Width    = D3DSELECT_10_11(vp.w, (float)vp.w);
    d3dvp.Height   = D3DSELECT_10_11(vp.h, (float)vp.h);
    d3dvp.TopLeftX = D3DSELECT_10_11(vp.x, (float)vp.x);
    d3dvp.TopLeftY = D3DSELECT_10_11(vp.y, (float)vp.y);
    d3dvp.MinDepth = 0;
    d3dvp.MaxDepth = 1;
    RParams.pContext->RSSetViewports(1, &d3dvp);
}




static D3D1X_(INPUT_ELEMENT_DESC) DistortionMeshVertexDesc[] =
{
    {"Position", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,   D3D1X_(INPUT_PER_VERTEX_DATA), 0},
    {"TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 8,   D3D1X_(INPUT_PER_VERTEX_DATA), 0},
    {"TexCoord", 1, DXGI_FORMAT_R32G32_FLOAT,   0, 16,	D3D1X_(INPUT_PER_VERTEX_DATA), 0},
    {"TexCoord", 2, DXGI_FORMAT_R32G32_FLOAT,   0, 24,	D3D1X_(INPUT_PER_VERTEX_DATA), 0},
    {"Color",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 32,  D3D1X_(INPUT_PER_VERTEX_DATA), 0},
};

static D3D1X_(INPUT_ELEMENT_DESC) SimpleQuadMeshVertexDesc[] =
{
    {"Position", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,   D3D1X_(INPUT_PER_VERTEX_DATA), 0},
};

// TODO: this is D3D specific
void DistortionRenderer::initShaders()
{  
    {
        PrecompiledShader vsShaderByteCode = DistortionVertexShaderLookup[DistortionVertexShaderBitMask & DistortionCaps];
        Ptr<D3D_NS::VertexShader> vtxShader = *new D3D_NS::VertexShader(
            &RParams,
            (void*)vsShaderByteCode.ShaderData, vsShaderByteCode.ShaderSize,
            vsShaderByteCode.ReflectionData, vsShaderByteCode.ReflectionSize);

        ID3D1xInputLayout** objRef   = &DistortionVertexIL.GetRawRef();

        HRESULT validate = RParams.pDevice->CreateInputLayout(
            DistortionMeshVertexDesc, sizeof(DistortionMeshVertexDesc) / sizeof(DistortionMeshVertexDesc[0]),
            vsShaderByteCode.ShaderData, vsShaderByteCode.ShaderSize, objRef);
        OVR_UNUSED(validate);

        DistortionShader = *new ShaderSet;
        DistortionShader->SetShader(vtxShader);

        PrecompiledShader psShaderByteCode = DistortionPixelShaderLookup[DistortionPixelShaderBitMask & DistortionCaps];

        Ptr<D3D_NS::PixelShader> ps  = *new D3D_NS::PixelShader(
            &RParams,
            (void*)psShaderByteCode.ShaderData, psShaderByteCode.ShaderSize,
            psShaderByteCode.ReflectionData, psShaderByteCode.ReflectionSize);

        DistortionShader->SetShader(ps);
    }

    {
        Ptr<D3D_NS::VertexShader> vtxShader = *new D3D_NS::VertexShader(
            &RParams,
            (void*)SimpleQuad_vs, sizeof(SimpleQuad_vs),
            SimpleQuad_vs_refl, sizeof(SimpleQuad_vs_refl) / sizeof(SimpleQuad_vs_refl[0]));
            //NULL, 0);

        ID3D1xInputLayout** objRef   = &SimpleQuadVertexIL.GetRawRef();

        HRESULT validate = RParams.pDevice->CreateInputLayout(
            SimpleQuadMeshVertexDesc, sizeof(SimpleQuadMeshVertexDesc) / sizeof(SimpleQuadMeshVertexDesc[0]),
            (void*)SimpleQuad_vs, sizeof(SimpleQuad_vs), objRef);
        OVR_UNUSED(validate);

        SimpleQuadShader = *new ShaderSet;
        SimpleQuadShader->SetShader(vtxShader);

        Ptr<D3D_NS::PixelShader> ps  = *new D3D_NS::PixelShader(
            &RParams,
            (void*)SimpleQuad_ps, sizeof(SimpleQuad_ps),
            SimpleQuad_ps_refl, sizeof(SimpleQuad_ps_refl) / sizeof(SimpleQuad_ps_refl[0]));

        SimpleQuadShader->SetShader(ps);
    }
}



ID3D1xSamplerState* DistortionRenderer::getSamplerState(int sm)
{
    if (SamplerStates[sm])    
        return SamplerStates[sm];

    D3D1X_(SAMPLER_DESC) ss;
    memset(&ss, 0, sizeof(ss));
    if (sm & Sample_Clamp)    
        ss.AddressU = ss.AddressV = ss.AddressW = D3D1X_(TEXTURE_ADDRESS_CLAMP);    
    else if (sm & Sample_ClampBorder)    
        ss.AddressU = ss.AddressV = ss.AddressW = D3D1X_(TEXTURE_ADDRESS_BORDER);    
    else    
        ss.AddressU = ss.AddressV = ss.AddressW = D3D1X_(TEXTURE_ADDRESS_WRAP);

    if (sm & Sample_Nearest)
    {
        ss.Filter = D3D1X_(FILTER_MIN_MAG_MIP_POINT);
    }
    else if (sm & Sample_Anisotropic)
    {
        ss.Filter = D3D1X_(FILTER_ANISOTROPIC);
        ss.MaxAnisotropy = 8;
    }
    else
    {
        ss.Filter = D3D1X_(FILTER_MIN_MAG_MIP_LINEAR);
    }
    ss.MaxLOD = 15;
    RParams.pDevice->CreateSamplerState(&ss, &SamplerStates[sm].GetRawRef());
    return SamplerStates[sm];
}


void DistortionRenderer::destroy()
{
	for(int eyeNum = 0; eyeNum < 2; eyeNum++)
	{
		DistortionMeshVBs[eyeNum].Clear();
		DistortionMeshIBs[eyeNum].Clear();
	}

	DistortionVertexIL.Clear();

	if (DistortionShader)
    {
        DistortionShader->UnsetShader(Shader_Vertex);
	    DistortionShader->UnsetShader(Shader_Pixel);
	    DistortionShader.Clear();
    }

    LatencyTesterQuadVB.Clear();
}

}}} // OVR::CAPI::D3D1X
