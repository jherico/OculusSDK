/************************************************************************************

Filename    :   RenderTiny_Device.cpp
Content     :   Platform renderer for simple scene graph - implementation
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

#include "RenderTiny_Device.h"

#include "Kernel/OVR_Log.h"

namespace OVR { namespace RenderTiny {

void Model::Render(const Matrix4f& ltw, RenderDevice* ren)
{
    if (Visible)
    {
        Matrix4f m = ltw * GetMatrix();
        ren->Render(m, this);
    }
}

void Container::Render(const Matrix4f& ltw, RenderDevice* ren)
{
    Matrix4f m = ltw * GetMatrix();
    for(unsigned i = 0; i < Nodes.GetSize(); i++)
    {
        Nodes[i]->Render(m, ren);
    }
}

void Scene::Render(RenderDevice* ren, const Matrix4f& view)
{
    Lighting.Update(view, LightPos);

    ren->SetLighting(&Lighting);

    World.Render(view, ren);
}



UInt16 CubeIndices[] =
{
    0, 1, 3,
    3, 1, 2,

    5, 4, 6,
    6, 4, 7,

    8, 9, 11,
    11, 9, 10,

    13, 12, 14,
    14, 12, 15,

    16, 17, 19,
    19, 17, 18,

    21, 20, 22,
    22, 20, 23
};


void Model::AddSolidColorBox(float x1, float y1, float z1,
                             float x2, float y2, float z2,
                             Color c)
{
    float t;

    if(x1 > x2)
    {
        t = x1;
        x1 = x2;
        x2 = t;
    }
    if(y1 > y2)
    {
        t = y1;
        y1 = y2;
        y2 = t;
    }
    if(z1 > z2)
    {
        t = z1;
        z1 = z2;
        z2 = t;
    }

    // Cube vertices and their normals.
    Vector3f CubeVertices[][3] =
    {
        Vector3f(x1, y2, z1), Vector3f(z1, x1), Vector3f(0.0f, 1.0f, 0.0f),
        Vector3f(x2, y2, z1), Vector3f(z1, x2), Vector3f(0.0f, 1.0f, 0.0f),
        Vector3f(x2, y2, z2), Vector3f(z2, x2), Vector3f(0.0f, 1.0f, 0.0f),
        Vector3f(x1, y2, z2), Vector3f(z2, x1), Vector3f(0.0f, 1.0f, 0.0f),

        Vector3f(x1, y1, z1), Vector3f(z1, x1), Vector3f(0.0f, -1.0f, 0.0f),
        Vector3f(x2, y1, z1), Vector3f(z1, x2), Vector3f(0.0f, -1.0f, 0.0f),
        Vector3f(x2, y1, z2), Vector3f(z2, x2), Vector3f(0.0f, -1.0f, 0.0f),
        Vector3f(x1, y1, z2), Vector3f(z2, x1), Vector3f(0.0f, -1.0f, 0.0f),

        Vector3f(x1, y1, z2), Vector3f(z2, y1), Vector3f(-1.0f, 0.0f, 0.0f),
        Vector3f(x1, y1, z1), Vector3f(z1, y1), Vector3f(-1.0f, 0.0f, 0.0f),
        Vector3f(x1, y2, z1), Vector3f(z1, y2), Vector3f(-1.0f, 0.0f, 0.0f),
        Vector3f(x1, y2, z2), Vector3f(z2, y2), Vector3f(-1.0f, 0.0f, 0.0f),

        Vector3f(x2, y1, z2), Vector3f(z2, y1), Vector3f(1.0f, 0.0f, 0.0f),
        Vector3f(x2, y1, z1), Vector3f(z1, y1), Vector3f(1.0f, 0.0f, 0.0f),
        Vector3f(x2, y2, z1), Vector3f(z1, y2), Vector3f(1.0f, 0.0f, 0.0f),
        Vector3f(x2, y2, z2), Vector3f(z2, y2), Vector3f(1.0f, 0.0f, 0.0f),

        Vector3f(x1, y1, z1), Vector3f(x1, y1), Vector3f(0.0f, 0.0f, -1.0f),
        Vector3f(x2, y1, z1), Vector3f(x2, y1), Vector3f(0.0f, 0.0f, -1.0f),
        Vector3f(x2, y2, z1), Vector3f(x2, y2), Vector3f(0.0f, 0.0f, -1.0f),
        Vector3f(x1, y2, z1), Vector3f(x1, y2), Vector3f(0.0f, 0.0f, -1.0f),

        Vector3f(x1, y1, z2), Vector3f(x1, y1), Vector3f(0.0f, 0.0f, 1.0f),
        Vector3f(x2, y1, z2), Vector3f(x2, y1), Vector3f(0.0f, 0.0f, 1.0f),
        Vector3f(x2, y2, z2), Vector3f(x2, y2), Vector3f(0.0f, 0.0f, 1.0f),
        Vector3f(x1, y2, z2), Vector3f(x1, y2), Vector3f(0.0f, 0.0f, 1.0f)
    };


    UInt16 startIndex = GetNextVertexIndex();

    enum
    {
        CubeVertexCount = sizeof(CubeVertices) / sizeof(CubeVertices[0]),
        CubeIndexCount  = sizeof(CubeIndices) / sizeof(CubeIndices[0])
    };

    for(int v = 0; v < CubeVertexCount; v++)
    {
        AddVertex(Vertex(CubeVertices[v][0], c, CubeVertices[v][1].x, CubeVertices[v][1].y, CubeVertices[v][2]));
    }

    // Renumber indices
    for(int i = 0; i < CubeIndexCount / 3; i++)
    {
        AddTriangle(CubeIndices[i * 3] + startIndex,
                    CubeIndices[i * 3 + 1] + startIndex,
                    CubeIndices[i * 3 + 2] + startIndex);
    }
}


//-------------------------------------------------------------------------------------


void ShaderFill::Set(PrimitiveType prim) const
{
    Shaders->Set(prim);
    for(int i = 0; i < 8; i++)
    {
        if(Textures[i])
        {
            Textures[i]->Set(i);
        }
    }
}



//-------------------------------------------------------------------------------------
// ***** Rendering


RenderDevice::RenderDevice()
    : CurPostProcess(PostProcess_None),
      SceneColorTexW(0), SceneColorTexH(0),
      SceneRenderScale(1),      
      Distortion(1.0f, 0.18f, 0.115f),
      PostProcessShaderActive(PostProcessShader_DistortionAndChromAb)
{
    PostProcessShaderRequested = PostProcessShaderActive;
}

ShaderFill* RenderDevice::CreateTextureFill(RenderTiny::Texture* t)
{
    ShaderSet* shaders = CreateShaderSet();
    shaders->SetShader(LoadBuiltinShader(Shader_Vertex, VShader_MVP));
    shaders->SetShader(LoadBuiltinShader(Shader_Fragment, FShader_Texture));
    ShaderFill* f = new ShaderFill(*shaders);
    f->SetTexture(0, t);
    return f;
}

void RenderDevice::SetLighting(const LightingParams* lt)
{
    if (!LightingBuffer)
        LightingBuffer = *CreateBuffer();

    LightingBuffer->Data(Buffer_Uniform, lt, sizeof(LightingParams));
    SetCommonUniformBuffer(1, LightingBuffer);
}



void RenderDevice::SetSceneRenderScale(float ss)
{
    SceneRenderScale = ss;
    pSceneColorTex = NULL;
}

void RenderDevice::SetViewport(const Viewport& vp)
{
    VP = vp;

    if (CurPostProcess == PostProcess_Distortion)
    {
        Viewport svp = vp;
        svp.w = (int)ceil(SceneRenderScale * vp.w);
        svp.h = (int)ceil(SceneRenderScale * vp.h);
        svp.x = (int)ceil(SceneRenderScale * vp.x);
        svp.y = (int)ceil(SceneRenderScale * vp.y);
        SetRealViewport(svp);
    }
    else
    {
        SetRealViewport(vp);
    }
}


bool RenderDevice::initPostProcessSupport(PostProcessType pptype)
{
    if (pptype != PostProcess_Distortion)
        return true;


    if (PostProcessShaderRequested !=  PostProcessShaderActive)
    {
        pPostProcessShader.Clear();
        PostProcessShaderActive = PostProcessShaderRequested;
    }

    if (!pPostProcessShader)
    {
        Shader *vs   = LoadBuiltinShader(Shader_Vertex, VShader_PostProcess);

        Shader *ppfs = NULL;
        if (PostProcessShaderActive == PostProcessShader_Distortion)
        {
            ppfs = LoadBuiltinShader(Shader_Fragment, FShader_PostProcess);
        }
        else if (PostProcessShaderActive == PostProcessShader_DistortionAndChromAb)
        {
            ppfs = LoadBuiltinShader(Shader_Fragment, FShader_PostProcessWithChromAb);
        }
        else
            OVR_ASSERT(false);
    
        pPostProcessShader = *CreateShaderSet();
        pPostProcessShader->SetShader(vs);
        pPostProcessShader->SetShader(ppfs);
    }


    int texw = (int)ceil(SceneRenderScale * WindowWidth),
        texh = (int)ceil(SceneRenderScale * WindowHeight);

    // If pSceneColorTex is already created and is of correct size, we are done.
    // It's important to check width/height in case window size changed.
    if (pSceneColorTex && (texw == SceneColorTexW) && (texh == SceneColorTexH))
    {
        return true;
    }

    pSceneColorTex = *CreateTexture(Texture_RGBA | Texture_RenderTarget | Params.Multisample,
                                    texw, texh, NULL);
    if (!pSceneColorTex)
    {
        return false;
    }
    SceneColorTexW = texw;
    SceneColorTexH = texh;
    pSceneColorTex->SetSampleMode(Sample_ClampBorder | Sample_Linear);


    if (!pFullScreenVertexBuffer)
    {
        pFullScreenVertexBuffer = *CreateBuffer();
        const RenderTiny::Vertex QuadVertices[] =
        {
            Vertex(Vector3f(0, 1, 0), Color(1, 1, 1, 1), 0, 0),
            Vertex(Vector3f(1, 1, 0), Color(1, 1, 1, 1), 1, 0),
            Vertex(Vector3f(0, 0, 0), Color(1, 1, 1, 1), 0, 1),
            Vertex(Vector3f(1, 0, 0), Color(1, 1, 1, 1), 1, 1)
        };
        pFullScreenVertexBuffer->Data(Buffer_Vertex, QuadVertices, sizeof(QuadVertices));
    }
    return true;
}

void RenderDevice::SetProjection(const Matrix4f& proj)
{
    Proj = proj;
    SetWorldUniforms(proj);
}

void RenderDevice::BeginScene(PostProcessType pptype)
{
    BeginRendering();

    if ((pptype != PostProcess_None) && initPostProcessSupport(pptype))
    {
        CurPostProcess = pptype;
    }
    else
    {
        CurPostProcess = PostProcess_None;
    }

    if (CurPostProcess == PostProcess_Distortion)
    {
        SetRenderTarget(pSceneColorTex);
        SetViewport(VP);
    }
    else
    {
        SetRenderTarget(0);
    }

    SetWorldUniforms(Proj);    
}

void RenderDevice::FinishScene()
{    
    if (CurPostProcess == PostProcess_None)
        return;
    
    SetRenderTarget(0);
    SetRealViewport(VP);
    FinishScene1();

    CurPostProcess = PostProcess_None;
}



void RenderDevice::FinishScene1()
{
    // Clear with black
    Clear(0.0f, 0.0f, 0.0f, 1.0f);

    float w = float(VP.w) / float(WindowWidth),
          h = float(VP.h) / float(WindowHeight),
          x = float(VP.x) / float(WindowWidth),
          y = float(VP.y) / float(WindowHeight);

    float as = float(VP.w) / float(VP.h);

    // We are using 1/4 of DistortionCenter offset value here, since it is
    // relative to [-1,1] range that gets mapped to [0, 0.5].
    pPostProcessShader->SetUniform2f("LensCenter",
                                     x + (w + Distortion.XCenterOffset * 0.5f)*0.5f, y + h*0.5f);
    pPostProcessShader->SetUniform2f("ScreenCenter", x + w*0.5f, y + h*0.5f);

    // MA: This is more correct but we would need higher-res texture vertically; we should adopt this
    // once we have asymmetric input texture scale.
    float scaleFactor = 1.0f / Distortion.Scale;

    pPostProcessShader->SetUniform2f("Scale",   (w/2) * scaleFactor, (h/2) * scaleFactor * as);
    pPostProcessShader->SetUniform2f("ScaleIn", (2/w),               (2/h) / as);

    pPostProcessShader->SetUniform4f("HmdWarpParam",
                                     Distortion.K[0], Distortion.K[1], Distortion.K[2], Distortion.K[3]);

    if (PostProcessShaderRequested == PostProcessShader_DistortionAndChromAb)
    {
        pPostProcessShader->SetUniform4f("ChromAbParam",
                                        Distortion.ChromaticAberration[0], 
                                        Distortion.ChromaticAberration[1],
                                        Distortion.ChromaticAberration[2],
                                        Distortion.ChromaticAberration[3]);
    }

    Matrix4f texm(w, 0, 0, x,
                  0, h, 0, y,
                  0, 0, 0, 0,
                  0, 0, 0, 1);
    pPostProcessShader->SetUniform4x4f("Texm", texm);

    Matrix4f view(2, 0, 0, -1,
                  0, 2, 0, -1,
                  0, 0, 0, 0,
                  0, 0, 0, 1);

    ShaderFill fill(pPostProcessShader);
    fill.SetTexture(0, pSceneColorTex);
    Render(&fill, pFullScreenVertexBuffer, NULL, view, 0, 4, Prim_TriangleStrip);
}


int GetNumMipLevels(int w, int h)
{
    int n = 1;
    while(w > 1 || h > 1)
    {
        w >>= 1;
        h >>= 1;
        n++;
    }
    return n;
}

void FilterRgba2x2(const UByte* src, int w, int h, UByte* dest)
{
    for(int j = 0; j < (h & ~1); j += 2)
    {
        const UByte* psrc = src + (w * j * 4);
        UByte*       pdest = dest + ((w >> 1) * (j >> 1) * 4);

        for(int i = 0; i < w >> 1; i++, psrc += 8, pdest += 4)
        {
            pdest[0] = (((int)psrc[0]) + psrc[4] + psrc[w * 4 + 0] + psrc[w * 4 + 4]) >> 2;
            pdest[1] = (((int)psrc[1]) + psrc[5] + psrc[w * 4 + 1] + psrc[w * 4 + 5]) >> 2;
            pdest[2] = (((int)psrc[2]) + psrc[6] + psrc[w * 4 + 2] + psrc[w * 4 + 6]) >> 2;
            pdest[3] = (((int)psrc[3]) + psrc[7] + psrc[w * 4 + 3] + psrc[w * 4 + 7]) >> 2;
        }
    }
}


}}
