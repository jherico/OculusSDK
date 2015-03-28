/************************************************************************************
Filename    :   Win32_RoomTiny_AppRendered.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   October 20th, 2014
Authors     :   Tom Heath
Copyright   :   Copyright 2012 Oculus, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/

// Additional structures needed for app-rendered
Scene      * pLatencyTestScene;
Model      * DistModel[2];

//-----------------------------------------------------------------------------------
void MakeNewDistortionMeshes(float overrideEyeRelief)
{
    for (int eye=0; eye<2; eye++)
    {
		if (DistModel[eye]->VertexBuffer) delete DistModel[eye]->VertexBuffer;
		if (DistModel[eye]->IndexBuffer)  delete DistModel[eye]->IndexBuffer;

        ovrDistortionMesh meshData;
        ovrHmd_CreateDistortionMeshDebug(HMD, (ovrEyeType)eye, EyeRenderDesc[eye].Fov,
                                         ovrDistortionCap_TimeWarp, &meshData, overrideEyeRelief);
        DistModel[eye]->VertexBuffer = new DataBuffer(D3D11_BIND_VERTEX_BUFFER, meshData.pVertexData,
                                                      sizeof(ovrDistortionVertex)*meshData.VertexCount);
        DistModel[eye]->IndexBuffer  = new DataBuffer(D3D11_BIND_INDEX_BUFFER, meshData.pIndexData,
                                                      sizeof(unsigned short)* meshData.IndexCount);
        ovrHmd_DestroyDistortionMesh(&meshData);
    }
}

//-----------------------------------------------------------------------------------------
void APP_RENDER_SetupGeometryAndShaders(int sampleCount)
    {
    D3D11_INPUT_ELEMENT_DESC VertexDesc[] = {
        { "Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "Position", 1, DXGI_FORMAT_R32_FLOAT,    0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "Position", 2, DXGI_FORMAT_R32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TexCoord", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TexCoord", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 } };

    char* vShader =
        "float2   EyeToSourceUVScale, EyeToSourceUVOffset;                                      \n"
        "float4x4 EyeRotationStart,   EyeRotationEnd;                                           \n"
        "float2   TimewarpTexCoord(float2 TexCoord, float4x4 rotMat)                            \n"
        "{                                                                                      \n"
             // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic 
             // aberration and distortion). These are now "real world" vectors in direction (x,y,1) 
             // relative to the eye of the HMD.    Apply the 3x3 timewarp rotation to these vectors.
        "    float3 transformed = float3( mul ( rotMat, float4(TexCoord.xy, 1, 1) ).xyz);       \n"
             // Project them back onto the Z=1 plane of the rendered images.
        "    float2 flattened = (transformed.xy / transformed.z);                               \n"
             // Scale them into ([0,0.5],[0,1]) or ([0.5,0],[0,1]) UV lookup space (depending on eye)
        "    return(EyeToSourceUVScale * flattened + EyeToSourceUVOffset);                      \n"
        "}                                                                                      \n"
        "void main(in float2  Position   : POSITION,  in float timewarpLerpFactor : POSITION1,  \n"
        "          in float   Vignette   : POSITION2, in float2 TexCoord0         : TEXCOORD0,  \n"
        "          in float2  TexCoord1  : TEXCOORD1, in float2 TexCoord2         : TEXCOORD2,  \n"
        "          out float4 oPosition  : SV_Position,                                         \n"
        "          out float2 oTexCoord0 : TEXCOORD0, out float2 oTexCoord1 : TEXCOORD1,        \n"
        "          out float2 oTexCoord2 : TEXCOORD2, out float  oVignette  : TEXCOORD3)        \n"
        "{                                                                                      \n"
        "    float4x4 lerpedEyeRot = lerp(EyeRotationStart, EyeRotationEnd, timewarpLerpFactor);\n"
        "    oTexCoord0  = TimewarpTexCoord(TexCoord0,lerpedEyeRot);                            \n"
        "    oTexCoord1  = TimewarpTexCoord(TexCoord1,lerpedEyeRot);                            \n"
        "    oTexCoord2  = TimewarpTexCoord(TexCoord2,lerpedEyeRot);                            \n"
        "    oPosition = float4(Position.xy, 0.5, 1.0);    oVignette = Vignette;                \n"
        "}";

    char* pShader =
        "Texture2D Texture   : register(t0);                                                    \n"
        "SamplerState Linear : register(s0);                                                    \n"
        "float4 main(in float4 oPosition  : SV_Position,  in float2 oTexCoord0 : TEXCOORD0,     \n"
        "            in float2 oTexCoord1 : TEXCOORD1,    in float2 oTexCoord2 : TEXCOORD2,     \n"
        "            in float  oVignette  : TEXCOORD3)    : SV_Target                           \n"
        "{                                                                                      \n"
             // 3 samples for fixing chromatic aberrations
        "    float R = Texture.Sample(Linear, oTexCoord0.xy).r;                                 \n"
        "    float G = Texture.Sample(Linear, oTexCoord1.xy).g;                                 \n"
        "    float B = Texture.Sample(Linear, oTexCoord2.xy).b;                                 \n"
        "    return (oVignette*float4(R,G,B,1));                                                \n"
        "}";

    // Create eye render descriptions, and distortion models
    for (int eye = 0; eye<2; eye++)
	{
		ShaderFill * DistFill = new ShaderFill(VertexDesc,6,vShader,pShader,pEyeRenderTexture[eye],sizeof(ovrDistortionVertex),false);
		DistModel[eye] = new Model(Vector3f(0,0,0),DistFill);
        EyeRenderDesc[eye] = ovrHmd_GetRenderDesc(HMD, (ovrEyeType)eye, HMD->DefaultEyeFov[eye]);
	}

    MakeNewDistortionMeshes();

    // A model for the latency test colour in the corner
    pLatencyTestScene = new Scene();

    ExampleFeatures3(VertexDesc, 6, vShader, pShader,sampleCount);
    }

//----------------------------------------------------------------------------------
void APP_RENDER_DistortAndPresent()
{
    bool waitForGPU = true;
    
    // Clear screen
    Platform.ClearAndSetRenderTarget(Platform.BackBufferRT,
                                    Platform.MainDepthBuffer, Recti(0,0,Platform.WinSize.w,Platform.WinSize.h));

    // Render latency-tester square
    unsigned char latencyColor[3];
    if (ovrHmd_GetLatencyTest2DrawColor(HMD, latencyColor))
    {
        float       col[] = { latencyColor[0] / 255.0f, latencyColor[1] / 255.0f,
                              latencyColor[2] / 255.0f, 1 };
        Matrix4f    view;
        ovrFovPort  fov = { 1, 1, 1, 1 };
        
        Matrix4f    proj = ovrMatrix4f_Projection(fov, 0.15f, 2, ovrProjection_RightHanded);

        pLatencyTestScene->Models[0]->Fill->VShader->SetUniform("NewCol", 4, col);
        pLatencyTestScene->Render(view, proj);
    }

    // Render distorted eye buffers
    for (int eye=0; eye<2; eye++)  
    {
        Model      * useModel          = DistModel[eye];
        ovrPosef   * useEyePose        = &EyeRenderPose[eye];
        float      * useYaw            = &YawAtRender[eye];
        double       debugTimeAdjuster = 0.0;

        ExampleFeatures4(eye,&useModel,&useEyePose,&useYaw,&debugTimeAdjuster,&waitForGPU);

        // Get and set shader constants
        ovrVector2f UVScaleOffset[2];
        ovrHmd_GetRenderScaleAndOffset(EyeRenderDesc[eye].Fov,
                                       pEyeRenderTexture[eye]->Size, EyeRenderViewport[eye], UVScaleOffset);
		useModel->Fill->VShader->SetUniform("EyeToSourceUVScale", 2, (float*)&UVScaleOffset[0]);
		useModel->Fill->VShader->SetUniform("EyeToSourceUVOffset", 2, (float *)&UVScaleOffset[1]);
		
        ovrMatrix4f    timeWarpMatrices[2];

		// Fold in the extra yaw into timewarp matrices
		Quatf extraFromYawSinceRender = Quatf(Vector3f(0, 1, 0), Yaw - *useYaw);
		ovrPosef tempPose = *useEyePose;
		tempPose.Orientation = (Quatf)tempPose.Orientation  * extraFromYawSinceRender.Inverted(); //The order of multiplication could be the reversed - insufficient use cases to confirm at this stage.

        ovrHmd_GetEyeTimewarpMatricesDebug(HMD, (ovrEyeType)eye, tempPose, Quatf(), timeWarpMatrices, debugTimeAdjuster);
        timeWarpMatrices[0] = ((Matrix4f)timeWarpMatrices[0]).Transposed();
        timeWarpMatrices[1] = ((Matrix4f)timeWarpMatrices[1]).Transposed();
		useModel->Fill->VShader->SetUniform("EyeRotationStart", 16, (float *)&timeWarpMatrices[0]);
		useModel->Fill->VShader->SetUniform("EyeRotationEnd",   16, (float *)&timeWarpMatrices[1]);
 
        // Perform distortion
		useModel->Render(Matrix4f(),Matrix4f());
    }

    Platform.SwapChain->Present(true, 0); // Vsync enabled

    // Only flush GPU for ExtendDesktop; not needed in Direct App Rendering with Oculus driver.
    if (HMD->HmdCaps & ovrHmdCap_ExtendDesktop)
    {
        Platform.Context->Flush();
        if (waitForGPU) 
            Platform.WaitUntilGpuIdle();
    }
    Platform.OutputFrameTime(ovr_GetTimeInSeconds());
    ovrHmd_EndFrameTiming(HMD);
}
