/************************************************************************************

Filename    :   Win32_DistortionMesh.cpp
Content     :   Manual creation and rendering of a distortion mesh
Created     :   March 5, 2014
Authors     :   Tom Heath, Volga Aksoy
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

//-----------------------------------------------------------------------------------
// If we decide to do our own rendering, then we need to make sure that
// we are creating the distortion mesh manually using the data provided by the LibOVR SDK

#include "OVR_CAPI.h"
#include "RenderTiny_D3D11_Device.h"

//-----------------------------------------------------------------------------------

// Contains render data required to render the distortion mesh with the proper shaders
// NOTE: For *demostration purposes*, the C-style functions in Win32_OculusRoomTiny.cpp 
// actually render the distortion mesh, while this struct only stores the data in a logical group
struct DistortionRenderData
{ 
 	ShaderSet *            Shaders;  
	ID3D11InputLayout    * VertexIL;
	Vector2f               UVScaleOffset[2][2];
	Ptr<Buffer>            MeshVBs[2];
	Ptr<Buffer>            MeshIBs[2]; 
} DistortionData;

//Format for mesh and shaders
struct DistortionVertex
{
    Vector2f Pos;
    Vector2f TexR;
    Vector2f TexG;
    Vector2f TexB;
    Color Col;
};
static D3D11_INPUT_ELEMENT_DESC DistortionMeshVertexDesc[] =
{
    {"Position", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,   D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 8,   D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TexCoord", 1, DXGI_FORMAT_R32G32_FLOAT,   0, 16,	D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TexCoord", 2, DXGI_FORMAT_R32G32_FLOAT,   0, 24,	D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"Color",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 32,  D3D11_INPUT_PER_VERTEX_DATA, 0},
};



void DistortionMeshInit(unsigned distortionCaps, ovrHmd HMD,
                        ovrEyeRenderDesc eyeRenderDesc[2],
                        ovrSizei textureSize, ovrRecti viewports[2],
                        RenderDevice* pRender)
{
    //Generate distortion mesh for each eye
    for ( int eyeNum = 0; eyeNum < 2; eyeNum++ )
    {
        // Allocate & generate distortion mesh vertices.
        ovrDistortionMesh meshData;
        ovrHmd_CreateDistortionMesh(HMD,
                                    eyeRenderDesc[eyeNum].Eye, eyeRenderDesc[eyeNum].Fov,
                                    distortionCaps, &meshData);

        ovrHmd_GetRenderScaleAndOffset(eyeRenderDesc[eyeNum].Fov,
                                       textureSize, viewports[eyeNum],
                                       (ovrVector2f*) DistortionData.UVScaleOffset[eyeNum]);

        // Now parse the vertex data and create a render ready vertex buffer from it
        DistortionVertex * pVBVerts = (DistortionVertex*)OVR_ALLOC( 
                                       sizeof(DistortionVertex) * meshData.VertexCount );
        DistortionVertex * v        = pVBVerts;
        ovrDistortionVertex * ov    = meshData.pVertexData;
        for ( unsigned vertNum = 0; vertNum < meshData.VertexCount; vertNum++ )
        {
            v->Pos.x = ov->Pos.x;
            v->Pos.y = ov->Pos.y;
            v->TexR  = (*(Vector2f*)&ov->TexR);
            v->TexG  = (*(Vector2f*)&ov->TexG);
            v->TexB  = (*(Vector2f*)&ov->TexB);
            v->Col.R = v->Col.G = v->Col.B = (OVR::UByte)( ov->VignetteFactor * 255.99f );
            v->Col.A = (OVR::UByte)( ov->TimeWarpFactor * 255.99f );
            v++; ov++;
        }
        //Register this mesh with the renderer
        DistortionData.MeshVBs[eyeNum] = *pRender->CreateBuffer();
        DistortionData.MeshVBs[eyeNum]->Data ( Buffer_Vertex, pVBVerts,
                                               sizeof(DistortionVertex) * meshData.VertexCount );
        DistortionData.MeshIBs[eyeNum] = *pRender->CreateBuffer();
        DistortionData.MeshIBs[eyeNum]->Data ( Buffer_Index, meshData.pIndexData,
                                                sizeof(unsigned short) * meshData.IndexCount );

        OVR_FREE ( pVBVerts );
        ovrHmd_DestroyDistortionMesh( &meshData );
    }

	// Pixel shader for the mesh
	//-------------------------------------------------------------------------------------------
    const char* pixelShader =
		"Texture2D Texture   : register(t0);                                                    \n"
		"SamplerState Linear : register(s0);                                                    \n"

		"float4 main(in float4 oPosition  : SV_Position, in float4 oColor : COLOR,              \n"
		"            in float2 oTexCoord0 : TEXCOORD0,   in float2 oTexCoord1 : TEXCOORD1,      \n"
		"            in float2 oTexCoord2 : TEXCOORD2)   : SV_Target                            \n"
		"{                                                                                      \n"
		// 3 samples for fixing chromatic aberrations
		"    float ResultR = Texture.Sample(Linear, oTexCoord0.xy).r;                           \n"
		"    float ResultG = Texture.Sample(Linear, oTexCoord1.xy).g;                           \n"
		"    float ResultB = Texture.Sample(Linear, oTexCoord2.xy).b;                           \n"
		"    return float4(ResultR * oColor.r, ResultG * oColor.g, ResultB * oColor.b, 1.0);    \n"
		"}";


    // Choose the vertex shader, according to if you have timewarp enabled
	if (distortionCaps & ovrDistortionCap_TimeWarp)
	{   // TIMEWARP
		//--------------------------------------------------------------------------------------------
		const char* vertexShader = 
			"float2 EyeToSourceUVScale;                                                                \n"
			"float2 EyeToSourceUVOffset;                                                               \n"
			"float4x4 EyeRotationStart;                                                             \n"
			"float4x4 EyeRotationEnd;                                                               \n"
			"float2 TimewarpTexCoord(float2 TexCoord, float4x4 rotMat)                              \n"
			"{                                                                                      \n"
			// Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic 
			// aberration and distortion). These are now "real world" vectors in direction (x,y,1) 
			// relative to the eye of the HMD.	Apply the 3x3 timewarp rotation to these vectors.
			"    float3 transformed = float3( mul ( rotMat, float4(TexCoord.xy, 1, 1) ).xyz);       \n"
			// Project them back onto the Z=1 plane of the rendered images.
			"    float2 flattened = (transformed.xy / transformed.z);                               \n"
			// Scale them into ([0,0.5],[0,1]) or ([0.5,0],[0,1]) UV lookup space (depending on eye)
			"    return(EyeToSourceUVScale * flattened + EyeToSourceUVOffset);                             \n"
			"}                                                                                      \n"
			"void main(in float2 Position    : POSITION,    in float4 Color       : COLOR0,         \n"
			"          in float2 TexCoord0   : TEXCOORD0,   in float2 TexCoord1   : TEXCOORD1,      \n"
			"          in float2 TexCoord2   : TEXCOORD2,                                           \n"
			"          out float4 oPosition  : SV_Position, out float4 oColor     : COLOR,          \n"
			"          out float2 oTexCoord0 : TEXCOORD0,   out float2 oTexCoord1 : TEXCOORD1,      \n"
			"          out float2 oTexCoord2 : TEXCOORD2)                                           \n"
			"{                                                                                      \n"
			"    float timewarpLerpFactor = Color.a;                                                \n"
			"    float4x4 lerpedEyeRot = lerp(EyeRotationStart, EyeRotationEnd, timewarpLerpFactor);\n"
			"    oTexCoord0  = TimewarpTexCoord(TexCoord0,lerpedEyeRot);                            \n"
			"    oTexCoord1  = TimewarpTexCoord(TexCoord1,lerpedEyeRot);                            \n"
			"    oTexCoord2  = TimewarpTexCoord(TexCoord2,lerpedEyeRot);                            \n"
			"    oPosition = float4(Position.xy, 0.5, 1.0);                                         \n"
			"    oColor = Color.r;  /*For vignette fade*/                                           \n"
			"}";
		
		pRender->InitShaders(vertexShader, pixelShader, &DistortionData.Shaders,
                             &DistortionData.VertexIL,DistortionMeshVertexDesc,5);
	}
	else
	{
		//-------------------------------------------------------------------------------------------
		const char* vertexShader = 
			"float2 EyeToSourceUVScale;                                                                \n"
			"float2 EyeToSourceUVOffset;                                                               \n"
			"void main(in float2 Position    : POSITION,    in float4 Color       : COLOR0,         \n"
			"          in float2 TexCoord0   : TEXCOORD0,   in float2 TexCoord1   : TEXCOORD1,      \n"
			"          in float2 TexCoord2   : TEXCOORD2,                                           \n"
			"          out float4 oPosition  : SV_Position, out float4 oColor     : COLOR,          \n"
			"          out float2 oTexCoord0 : TEXCOORD0,   out float2 oTexCoord1 : TEXCOORD1,      \n"
			"          out float2 oTexCoord2 : TEXCOORD2)                                           \n"
			"{                                                                                      \n"
			// Scale them into ([0,0.5],[0,1]) or ([0.5,0],[0,1]) UV lookup space (depending on eye)
			"    oTexCoord0  = EyeToSourceUVScale * TexCoord0 + EyeToSourceUVOffset;                      \n"
			"    oTexCoord1  = EyeToSourceUVScale * TexCoord1 + EyeToSourceUVOffset;                      \n"
			"    oTexCoord2  = EyeToSourceUVScale * TexCoord2 + EyeToSourceUVOffset;                      \n"
			"    oPosition = float4(Position.xy, 0.5, 1.0);                                         \n"
			"    oColor = Color.r;  /*For vignette fade*/                                           \n"
			"}";
		
		pRender->InitShaders(vertexShader, pixelShader, &DistortionData.Shaders,
                             &DistortionData.VertexIL,DistortionMeshVertexDesc,5);
	}
}


void DistortionMeshRender(unsigned distortionCaps, ovrHmd HMD,
                          double timwarpTimePoint, ovrPosef eyeRenderPoses[2],
                          RenderDevice* pRender, Texture* pRendertargetTexture)
{
 	if (distortionCaps & ovrDistortionCap_TimeWarp)
	{   // TIMEWARP
        // Wait till time-warp to reduce latency.
	    ovr_WaitTillTime(timwarpTimePoint);
	}

	// Clear screen
    pRender->SetDefaultRenderTarget();
    pRender->SetFullViewport();
    pRender->Clear(0.0f, 0.0f, 0.0f, 0.0f);

	// Setup shader
	ShaderFill distortionShaderFill(DistortionData.Shaders);
    distortionShaderFill.SetTexture(0, pRendertargetTexture);
    distortionShaderFill.SetInputLayout(DistortionData.VertexIL);

    for(int eyeNum = 0; eyeNum < 2; eyeNum++)
    {
		// Setup shader constants
		DistortionData.Shaders->SetUniform2f("EyeToSourceUVScale",
            DistortionData.UVScaleOffset[eyeNum][0].x, DistortionData.UVScaleOffset[eyeNum][0].y);
        DistortionData.Shaders->SetUniform2f("EyeToSourceUVOffset",
            DistortionData.UVScaleOffset[eyeNum][1].x, DistortionData.UVScaleOffset[eyeNum][1].y);

 		if (distortionCaps & ovrDistortionCap_TimeWarp)
		{   // TIMEWARP - Additional shader constants required
			ovrMatrix4f timeWarpMatrices[2];
			ovrHmd_GetEyeTimewarpMatrices(HMD, (ovrEyeType)eyeNum, eyeRenderPoses[eyeNum], timeWarpMatrices);
			//WARNING!!! These matrices are transposed in SetUniform4x4f, before being used by the shader.
			DistortionData.Shaders->SetUniform4x4f("EyeRotationStart", Matrix4f(timeWarpMatrices[0]));
			DistortionData.Shaders->SetUniform4x4f("EyeRotationEnd",   Matrix4f(timeWarpMatrices[1]));
		}
		// Perform distortion
		pRender->Render(&distortionShaderFill,
                        DistortionData.MeshVBs[eyeNum], DistortionData.MeshIBs[eyeNum]);
    }

    pRender->SetDefaultRenderTarget();
}


void DistortionMeshRelease(void)  
{
	for(int eyeNum = 0; eyeNum < 2; eyeNum++)
	{
		DistortionData.MeshVBs[eyeNum].Clear();
		DistortionData.MeshIBs[eyeNum].Clear();
	}
    if (DistortionData.Shaders)
    {
	    DistortionData.Shaders->UnsetShader(Shader_Vertex);
	    DistortionData.Shaders->UnsetShader(Shader_Pixel);
    }	
}

