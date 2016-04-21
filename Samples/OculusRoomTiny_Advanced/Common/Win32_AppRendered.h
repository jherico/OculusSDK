

/************************************************************************************
Filename    :   Win32_AppRendered.h
Content     :   App rendered specific code, for basic VR
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
#ifndef OVR_Win32_AppRendered_h
#define OVR_Win32_AppRendered_h

struct AppRenderVR : BasicVR
{
    // Additional structures needed
    Model * pLatencyTestModel;
    Model * DistModel[2];

    //--------------------------------------------------------------------------
    AppRenderVR(HINSTANCE hinst) : BasicVR(hinst)
    {
    }

    //-----------------------------------------------------------------------------------
    void MakeNewDistortionMeshes(float overrideEyeRelief=0)
    {
        for (int eye=0; eye<2; eye++)
        {
            if (DistModel[eye]->VertexBuffer) delete DistModel[eye]->VertexBuffer;
            if (DistModel[eye]->IndexBuffer)  delete DistModel[eye]->IndexBuffer;

            ovrDistortionMesh meshData;
            ovr_CreateDistortionMeshDebug(HMD, (ovrEyeType)eye, EyeRenderDesc[eye].Fov,
                                             ovrDistortionCap_TimeWarp, &meshData, overrideEyeRelief);
            DistModel[eye]->VertexBuffer = new DataBuffer(DIRECTX.Device,D3D11_BIND_VERTEX_BUFFER, meshData.pVertexData,
                                                          sizeof(ovrDistortionVertex)*meshData.VertexCount);
            DistModel[eye]->IndexBuffer  = new DataBuffer(DIRECTX.Device,D3D11_BIND_INDEX_BUFFER, meshData.pIndexData,
                                                          sizeof(unsigned short)* meshData.IndexCount);
            ovr_DestroyDistortionMesh(&meshData);
        }
    }

    //-----------------------------------------------------------------------------------------
    void ConfigureRendering()
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
            Material * DistFill = new Material(pEyeRenderTexture[eye],0,VertexDesc,6,vShader,pShader,sizeof(ovrDistortionVertex));
            DistModel[eye]      = new Model(NULL,Vector3f(0,0,0),DistFill);
            EyeRenderDesc[eye]  = ovr_GetRenderDesc(HMD, (ovrEyeType)eye, HMD->DefaultEyeFov[eye]);
        }

        MakeNewDistortionMeshes();

        // A model for the latency test color in the corner
        pLatencyTestModel = new Model(new Texture(false, Sizei(256, 256),Texture::AUTO_WHITE),0.975f,0.95f,1,1);

        }

    //------------------------------------------------------
    void BeginFrame()
    {
        // Start timing
        ovr_BeginFrameTiming(HMD, 0); 
    }

    //----------------------------------------------------------------------------------
    void DistortAndPresent(Texture * leftEyeTexture = 0, ovrPosef * leftEyePose = 0,
                            double debugTimeAdjuster = 0, Quatf * extraQuat = 0, bool waitForGPU = true)
    {
        // Use defaults where none specified
        Texture * useEyeTexture[2]    = {pEyeRenderTexture[0],pEyeRenderTexture[1]};
        ovrPosef  useEyeRenderPose[2] = {EyeRenderPose[0],    EyeRenderPose[1]};
        if (leftEyeTexture) useEyeTexture[0]    = leftEyeTexture;
        if (leftEyePose)    useEyeRenderPose[0] = *leftEyePose;

        // Clear screen
        DIRECTX.SetAndClearRenderTarget(DIRECTX.BackBufferRT,  DIRECTX.MainDepthBuffer);
        DIRECTX.SetViewport(Recti(0,0,DIRECTX.WinSize.w,DIRECTX.WinSize.h));

        // Render latency-tester square
        unsigned char latencyColor[3];
        if (ovr_GetLatencyTest2DrawColor(HMD, latencyColor))
        {
            pLatencyTestModel->Render(Matrix4f(),latencyColor[0]/255.0f, latencyColor[1]/ 255.0f, latencyColor[2]/255.0f, 1,true);
        }
        // Render distorted eye buffers
        for (int eye=0; eye<2; eye++)  
        {
            // Get and set shader constants
            ovrVector2f UVScaleOffset[2];
            ovr_GetRenderScaleAndOffset(EyeRenderDesc[eye].Fov,
                                           useEyeTexture[eye]->Size, EyeRenderViewport[eye], UVScaleOffset);
            memcpy(DIRECTX.UniformData + 0,  UVScaleOffset, 16); // EyeToSourceUVScale + EyeToSourceUVOffset
        
            ovrMatrix4f timeWarpMatrices[2];
            Quatf extraFromYawSinceRender = extraQuat ? extraQuat[eye] : Quatf();

            // With timewarp matrices, we are folding in the extra quaternion rotation,
            // from user control typically.
            ovrPosef tempPose = useEyeRenderPose[eye];
            tempPose.Orientation = (Quatf)tempPose.Orientation * extraFromYawSinceRender.Inverted(); // The order of multiplication could be the reversed - insufficient use cases to confirm at this stage.

            ovr_GetEyeTimewarpMatricesDebug(HMD, (ovrEyeType)eye, tempPose, Quatf(), timeWarpMatrices, debugTimeAdjuster);
            timeWarpMatrices[0] = ((Matrix4f)timeWarpMatrices[0]).Transposed();
            timeWarpMatrices[1] = ((Matrix4f)timeWarpMatrices[1]).Transposed();
            memcpy(DIRECTX.UniformData + 16,  timeWarpMatrices, 128); // Set Timewarp matrices shader constants

            // Perform distortion, putting the right texture in the model
            DistModel[eye]->Fill->Tex = useEyeTexture[eye];
            DistModel[eye]->Render(Matrix4f(),1,1,1,1,false);
        }

        DIRECTX.SwapChain->Present(true, 0); // Vsync enabled

        // Only flush GPU for ExtendDesktop; not needed in Direct App Rendering with Oculus driver.
        if (HMD->HmdCaps & ovrHmdCap_ExtendDesktop)
        {
            DIRECTX.Context->Flush();
            if (waitForGPU) 
                Util.WaitUntilGpuIdle();
        }
        Util.OutputFrameTime(ovr_GetTimeInSeconds());
        ovr_EndFrameTiming(HMD);
    }
};

#endif // OVR_Win32_AppRendered_h
