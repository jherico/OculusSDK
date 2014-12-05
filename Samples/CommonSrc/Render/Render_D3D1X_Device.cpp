/************************************************************************************

Filename    :   Renderer_D3D1x.cpp
Content     :   RenderDevice implementation  for D3DX10/11.
Created     :   September 10, 2012
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

#define GPU_PROFILING 0

#include "Kernel/OVR_Log.h"
#include "Kernel/OVR_Std.h"

#define WIN32_LEAN_AND_MEAN
#include <comdef.h>

#include "Render_D3D1X_Device.h"
#include "Util/Util_ImageWindow.h"
#include "Kernel/OVR_Log.h"

#include "OVR_CAPI_D3D.h"

#include <d3dcompiler.h>

#include <d3d9.h>   // for GPU markers

#if (OVR_D3D_VERSION == 10)
namespace OVR { namespace Render { namespace D3D10 {
#else
namespace OVR { namespace Render { namespace D3D11 {
#endif

static D3D1x_(INPUT_ELEMENT_DESC) ModelVertexDesc[] =
{
    {"Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos),   D3D1x_(INPUT_PER_VERTEX_DATA), 0},
    {"Color",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, offsetof(Vertex, C),     D3D1x_(INPUT_PER_VERTEX_DATA), 0},
    {"TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, U),     D3D1x_(INPUT_PER_VERTEX_DATA), 0},
    {"TexCoord", 1, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, U2),	 D3D1x_(INPUT_PER_VERTEX_DATA), 0},
    {"Normal",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Norm),  D3D1x_(INPUT_PER_VERTEX_DATA), 0},
};

#pragma region Geometry shaders
static const char* StdVertexShaderSrc =
    "float4x4 Proj;\n"
    "float4x4 View;\n"
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "   float2 TexCoord1 : TEXCOORD1;\n"
    "   float3 Normal   : NORMAL;\n"
    "   float3 VPos     : TEXCOORD4;\n"
    "};\n"
    "void main(in float4 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord : TEXCOORD0, in float2 TexCoord1 : TEXCOORD1, in float3 Normal : NORMAL,\n"
    "          out Varyings ov)\n"
    "{\n"
    "   ov.Position = mul(Proj, mul(View, Position));\n"
    "   ov.Normal = mul(View, Normal);\n"
    "   ov.VPos = mul(View, Position);\n"
    "   ov.TexCoord = TexCoord;\n"
    "   ov.TexCoord1 = TexCoord1;\n"
    "   ov.Color = Color;\n"
    "}\n";

static const char* DirectVertexShaderSrc =
    "float4x4 View : register(c4);\n"
    "void main(in float4 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord : TEXCOORD0, in float2 TexCoord1 : TEXCOORD1, in float3 Normal : NORMAL,\n"
    "          out float4 oPosition : SV_Position, out float4 oColor : COLOR, out float2 oTexCoord : TEXCOORD0, out float2 oTexCoord1 : TEXCOORD1, out float3 oNormal : NORMAL)\n"
    "{\n"
    "   oPosition = mul(View, Position);\n"
    "   oTexCoord = TexCoord;\n"
    "   oTexCoord1 = TexCoord1;\n"
    "   oColor = Color;\n"
    "   oNormal = mul(View, Normal);\n"
    "}\n";

static const char* SolidPixelShaderSrc =
    "float4 Color;\n"
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "};\n"
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "   float4 finalColor = ov.Color;"
    // blend state expects premultiplied alpha
	"	finalColor.rgb *= finalColor.a;\n"
    "   return finalColor;\n"
    "}\n";

static const char* GouraudPixelShaderSrc =
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "};\n"
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "   float4 finalColor = ov.Color;"
    // blend state expects premultiplied alpha
	"	finalColor.rgb *= finalColor.a;\n"
    "   return finalColor;\n"
    "}\n";

static const char* TexturePixelShaderSrc =
    "Texture2D Texture : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "};\n"
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "	float4 color2 = ov.Color * Texture.Sample(Linear, ov.TexCoord);\n"
    "   if (color2.a <= 0.4)\n"
    "		discard;\n"
    "   return color2;\n"
    "}\n";

static const char* MultiTexturePixelShaderSrc =
    "Texture2D Texture[2] : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "   float2 TexCoord1 : TEXCOORD1;\n"
    "};\n"
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "float4 color1;\n"
    "float4 color2;\n"
	"	color1 = Texture[0].Sample(Linear, ov.TexCoord);\n"
    "	color2 = Texture[1].Sample(Linear, ov.TexCoord1);\n"
    "	color2.rgb = color2.rgb * lerp(1.9, 1.2, saturate(length(color2.rgb)));\n"
    "	color2 = color1 * color2;\n"
    "   if (color2.a <= 0.4)\n"
    "		discard;\n"
	"	return float4(color2.rgb / color2.a, 1);\n"
    "}\n";

#define LIGHTING_COMMON                 \
    "cbuffer Lighting : register(b1)\n" \
    "{\n"                               \
    "    float3 Ambient;\n"             \
    "    float3 LightPos[8];\n"         \
    "    float4 LightColor[8];\n"       \
    "    float  LightCount;\n"          \
    "};\n"                              \
    "struct Varyings\n"                 \
    "{\n"                                       \
    "   float4 Position : SV_Position;\n"       \
    "   float4 Color    : COLOR0;\n"            \
    "   float2 TexCoord : TEXCOORD0;\n"         \
    "   float3 Normal   : NORMAL;\n"            \
    "   float3 VPos     : TEXCOORD4;\n"         \
    "};\n"                                      \
    "float4 DoLight(Varyings v)\n"              \
    "{\n"                                       \
    "   float3 norm = normalize(v.Normal);\n"   \
    "   float3 light = Ambient;\n"              \
    "   for (uint i = 0; i < LightCount; i++)\n"\
    "   {\n"                                        \
    "       float3 ltp = (LightPos[i] - v.VPos);\n" \
    "       float  ldist = dot(ltp,ltp);\n"         \
    "       ltp = normalize(ltp);\n"                \
    "       light += saturate(LightColor[i] * v.Color.rgb * dot(norm, ltp) / sqrt(ldist));\n"\
    "   }\n"                                        \
    "   return float4(light, v.Color.a);\n"         \
    "}\n"

static const char* LitSolidPixelShaderSrc =
    LIGHTING_COMMON
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "   return DoLight(ov) * ov.Color;\n"
    "}\n";

static const char* LitTexturePixelShaderSrc =
    "Texture2D Texture : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
    LIGHTING_COMMON
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
    "   return DoLight(ov) * Texture.Sample(Linear, ov.TexCoord);\n"
    "}\n";

static const char* AlphaTexturePixelShaderSrc =
    "Texture2D Texture : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
    "struct Varyings\n"
    "{\n"
    "   float4 Position : SV_Position;\n"
    "   float4 Color    : COLOR0;\n"
    "   float2 TexCoord : TEXCOORD0;\n"
    "};\n"
    "float4 main(in Varyings ov) : SV_Target\n"
    "{\n"
	"	float4 finalColor = ov.Color;\n"
    "	finalColor.a *= Texture.Sample(Linear, ov.TexCoord).r;\n"
    // blend state expects premultiplied alpha
	"	finalColor.rgb *= finalColor.a;\n"
	"	return finalColor;\n"
    "}\n";

static const char* AlphaBlendedTexturePixelShaderSrc =
	"Texture2D Texture : register(t0);\n"
	"SamplerState Linear : register(s0);\n"
	"struct Varyings\n"
	"{\n"
	"   float4 Position : SV_Position;\n"
	"   float4 Color    : COLOR0;\n"
	"   float2 TexCoord : TEXCOORD0;\n"
	"};\n"
	"float4 main(in Varyings ov) : SV_Target\n"
	"{\n"
	"	float4 finalColor = ov.Color;\n"
	"	finalColor *= Texture.Sample(Linear, ov.TexCoord);\n"
	// blend state expects premultiplied alpha
	"	finalColor.rgb *= finalColor.a;\n"
	"	return finalColor;\n"
	"}\n";
#pragma endregion

#pragma region Distortion shaders
// ***** PostProcess Shader

static const char* PostProcessVertexShaderSrc =
    "float4x4 View : register(c4);\n"
    "float4x4 Texm : register(c8);\n"
    "void main(in float4 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord : TEXCOORD0, in float2 TexCoord1 : TEXCOORD1,\n"
    "          out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0)\n"
    "{\n"
    "   oPosition = mul(View, Position);\n"
    "   oTexCoord = mul(Texm, float4(TexCoord,0,1));\n"
    "}\n";


// Shader with lens distortion and chromatic aberration correction.
static const char* PostProcessPixelShaderWithChromAbSrc =
    "Texture2D Texture : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
    "float3 DistortionClearColor;\n"
    "float EdgeFadeScale;\n"
    "float2 EyeToSourceUVScale;\n"
    "float2 EyeToSourceUVOffset;\n"
    "float2 EyeToSourceNDCScale;\n"
    "float2 EyeToSourceNDCOffset;\n"
    "float2 TanEyeAngleScale;\n"
    "float2 TanEyeAngleOffset;\n"
    "float4 HmdWarpParam;\n"
    "float4 ChromAbParam;\n"
    "\n"

    "float4 main(in float4 oPosition : SV_Position,\n"
    "            in float2 oTexCoord : TEXCOORD0) : SV_Target\n"
    "{\n"
    // Input oTexCoord is [-1,1] across the half of the screen used for a single eye.
    "   float2 TanEyeAngleDistorted = oTexCoord * TanEyeAngleScale + TanEyeAngleOffset;\n" // Scales to tan(thetaX),tan(thetaY), but still distorted (i.e. only the center is correct)
    "   float  RadiusSq = TanEyeAngleDistorted.x * TanEyeAngleDistorted.x + TanEyeAngleDistorted.y * TanEyeAngleDistorted.y;\n"
    "   float Distort = rcp ( 1.0 + RadiusSq * ( HmdWarpParam.y + RadiusSq * ( HmdWarpParam.z + RadiusSq * ( HmdWarpParam.w ) ) ) );\n"
    "   float DistortR = Distort * ( ChromAbParam.x + RadiusSq * ChromAbParam.y );\n"
    "   float DistortG = Distort;\n"
    "   float DistortB = Distort * ( ChromAbParam.z + RadiusSq * ChromAbParam.w );\n"
    "   float2 TanEyeAngleR = DistortR * TanEyeAngleDistorted;\n"
    "   float2 TanEyeAngleG = DistortG * TanEyeAngleDistorted;\n"
    "   float2 TanEyeAngleB = DistortB * TanEyeAngleDistorted;\n"

    // These are now in "TanEyeAngle" space.
    // The vectors (TanEyeAngleRGB.x, TanEyeAngleRGB.y, 1.0) are real-world vectors pointing from the eye to where the components of the pixel appear to be.
    // If you had a raytracer, you could just use them directly.

    // Scale them into ([0,0.5],[0,1]) or ([0.5,0],[0,1]) UV lookup space (depending on eye)
    "   float2 SourceCoordR = TanEyeAngleR * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   float2 SourceCoordG = TanEyeAngleG * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   float2 SourceCoordB = TanEyeAngleB * EyeToSourceUVScale + EyeToSourceUVOffset;\n"

    // Find the distance to the nearest edge.
    "   float2 NDCCoord = TanEyeAngleG * EyeToSourceNDCScale + EyeToSourceNDCOffset;\n"
    "   float EdgeFadeIn = EdgeFadeScale * ( 1.0 - max ( abs ( NDCCoord.x ), abs ( NDCCoord.y ) ) );\n"
    "   if ( EdgeFadeIn < 0.0 )\n"
    "   {\n"
    "       return float4(DistortionClearColor.r, DistortionClearColor.g, DistortionClearColor.b, 1.0);\n"
    "   }\n"
    "   EdgeFadeIn = saturate ( EdgeFadeIn );\n"

    // Actually do the lookups.
    "   float4 Result = float4(0,0,0,1);\n"
    "   Result.r = Texture.Sample(Linear, SourceCoordR).r;\n"
    "   Result.g = Texture.Sample(Linear, SourceCoordG).g;\n"
    "   Result.b = Texture.Sample(Linear, SourceCoordB).b;\n"
    "   Result.rgb *= EdgeFadeIn;\n"
    "   return Result;\n"
    "}\n";

//----------------------------------------------------------------------------

// A vertex format used for mesh-based distortion.
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

static D3D1x_(INPUT_ELEMENT_DESC) DistortionVertexDesc[] =
{
    {"Position", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,          D3D1x_(INPUT_PER_VERTEX_DATA), 0},
    {"TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 8,          D3D1x_(INPUT_PER_VERTEX_DATA), 0},
    {"TexCoord", 1, DXGI_FORMAT_R32G32_FLOAT,       0, 8+8,	       D3D1x_(INPUT_PER_VERTEX_DATA), 0},
    {"TexCoord", 2, DXGI_FORMAT_R32G32_FLOAT,       0, 8+8+8,	   D3D1x_(INPUT_PER_VERTEX_DATA), 0},
    {"Color",    0, DXGI_FORMAT_R8G8B8A8_UNORM,     0, 8+8+8+8,    D3D1x_(INPUT_PER_VERTEX_DATA), 0},
};

//----------------------------------------------------------------------------
// Simple distortion shader that does three texture reads.
// Used for mesh-based distortion without timewarp.

static const char* PostProcessMeshVertexShaderSrc =
    "float2 EyeToSourceUVScale;\n"
    "float2 EyeToSourceUVOffset;\n"
    "void main(in float2 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord0 : TEXCOORD0, in float2 TexCoord1 : TEXCOORD1, in float2 TexCoord2 : TEXCOORD2,\n"
    "          out float4 oPosition : SV_Position, out float4 oColor : COLOR, out float2 oTexCoord0 : TEXCOORD0, out float2 oTexCoord1 : TEXCOORD1, out float2 oTexCoord2 : TEXCOORD2)\n"
    "{\n"
    "   oPosition.x = Position.x;\n"
    "   oPosition.y = Position.y;\n"
    "   oPosition.z = 0.5;\n"
    "   oPosition.w = 1.0;\n"
    // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
    // Scale them into the correct [0-1],[0-1] UV lookup space (depending on eye)
    "   oTexCoord0 = TexCoord0 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord1 = TexCoord1 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord2 = TexCoord2 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oColor = Color;\n"              // Used for vignette fade.
    "}\n";
    
static const char* PostProcessMeshPixelShaderSrc =
    "Texture2D HmdSpcTexture : register(t0);\n"
    "Texture2D OverlayTexture : register(t1);\n"
    "SamplerState Linear : register(s0);\n"
    "float  UseOverlay = 1;\n"
    "\n"
    "float4 main(in float4 oPosition : SV_Position, in float4 oColor : COLOR,\n"
    "            in float2 oTexCoord0 : TEXCOORD0, in float2 oTexCoord1 : TEXCOORD1, in float2 oTexCoord2 : TEXCOORD2) : SV_Target\n"
    "{\n"
    "   float4 finalColor = float4(0,0,0,1);\n"
    "   finalColor.r = HmdSpcTexture.Sample(Linear, oTexCoord0).r;\n"
    "   finalColor.g = HmdSpcTexture.Sample(Linear, oTexCoord1).g;\n"
    "   finalColor.b = HmdSpcTexture.Sample(Linear, oTexCoord2).b;\n"

    "   if(UseOverlay > 0)\n"
    "   {\n"
    "       float2 overlayColorR = OverlayTexture.Sample(Linear, oTexCoord0).ra;\n"
    "       float2 overlayColorG = OverlayTexture.Sample(Linear, oTexCoord1).ga;\n"
    "       float2 overlayColorB = OverlayTexture.Sample(Linear, oTexCoord2).ba;\n"

    // do premultiplied alpha blending - overlayColorX.x is color, overlayColorX.y is alpha
    "       finalColor.r = finalColor.r * saturate(1-overlayColorR.y) + overlayColorR.x;\n"
    "       finalColor.g = finalColor.g * saturate(1-overlayColorG.y) + overlayColorG.x;\n"
    "       finalColor.b = finalColor.b * saturate(1-overlayColorB.y) + overlayColorB.x;\n"
    "   }\n"

    "   finalColor.rgb = saturate(finalColor.rgb * oColor.rgb);\n"
    "   return finalColor;\n"
    "}\n";


//----------------------------------------------------------------------------
// Pixel shader is very simple - does three texture reads.
// Vertex shader does all the hard work.
// Used for mesh-based distortion with timewarp.

static const char* PostProcessMeshTimewarpVertexShaderSrc =
    "float2 EyeToSourceUVScale;\n"
    "float2 EyeToSourceUVOffset;\n"
    "float3x3 EyeRotationStart;\n"
    "float3x3 EyeRotationEnd;\n"
    "void main(in float2 Position : POSITION, in float4 Color : COLOR0,\n"
    "          in float2 TexCoord0 : TEXCOORD0, in float2 TexCoord1 : TEXCOORD1, in float2 TexCoord2 : TEXCOORD2,\n"
    "          out float4 oPosition : SV_Position, out float4 oColor : COLOR,\n"
    "          out float2 oHmdSpcTexCoordR : TEXCOORD0, out float2 oHmdSpcTexCoordG : TEXCOORD1, out float2 oHmdSpcTexCoordB : TEXCOORD2,"
    "          out float2 oOverlayTexCoordR : TEXCOORD3, out float2 oOverlayTexCoordG : TEXCOORD4, out float2 oOverlayTexCoordB : TEXCOORD5)\n"
    "{\n"
    "   oPosition.x = Position.x;\n"
    "   oPosition.y = Position.y;\n"
    "   oPosition.z = 0.5;\n"
    "   oPosition.w = 1.0;\n"

    // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
    // These are now "real world" vectors in direction (x,y,1) relative to the eye of the HMD.
    "   float3 TanEyeAngleR = float3 ( TexCoord0.x, TexCoord0.y, 1.0 );\n"
    "   float3 TanEyeAngleG = float3 ( TexCoord1.x, TexCoord1.y, 1.0 );\n"
    "   float3 TanEyeAngleB = float3 ( TexCoord2.x, TexCoord2.y, 1.0 );\n"

    // Accurate time warp lerp vs. faster
#if 1
    // Apply the two 3x3 timewarp rotations to these vectors.
    "   float3 TransformedRStart = mul ( TanEyeAngleR, EyeRotationStart );\n"
    "   float3 TransformedGStart = mul ( TanEyeAngleG, EyeRotationStart );\n"
    "   float3 TransformedBStart = mul ( TanEyeAngleB, EyeRotationStart );\n"
    "   float3 TransformedREnd   = mul ( TanEyeAngleR, EyeRotationEnd );\n"
    "   float3 TransformedGEnd   = mul ( TanEyeAngleG, EyeRotationEnd );\n"
    "   float3 TransformedBEnd   = mul ( TanEyeAngleB, EyeRotationEnd );\n"
    // And blend between them.
    "   float3 TransformedR = lerp ( TransformedRStart, TransformedREnd, Color.a );\n"
    "   float3 TransformedG = lerp ( TransformedGStart, TransformedGEnd, Color.a );\n"
    "   float3 TransformedB = lerp ( TransformedBStart, TransformedBEnd, Color.a );\n"
#else
    "   float3x3 EyeRotation = lerp ( EyeRotationStart, EyeRotationEnd, Color.a );\n"
    "   float3 TransformedR   = mul ( TanEyeAngleR, EyeRotation );\n"
    "   float3 TransformedG   = mul ( TanEyeAngleG, EyeRotation );\n"
    "   float3 TransformedB   = mul ( TanEyeAngleB, EyeRotation );\n"
#endif

    // Project them back onto the Z=1 plane of the rendered images.
    "   float RecipZR = rcp ( TransformedR.z );\n"
    "   float RecipZG = rcp ( TransformedG.z );\n"
    "   float RecipZB = rcp ( TransformedB.z );\n"
    "   float2 FlattenedR = float2 ( TransformedR.x * RecipZR, TransformedR.y * RecipZR );\n"
    "   float2 FlattenedG = float2 ( TransformedG.x * RecipZG, TransformedG.y * RecipZG );\n"
    "   float2 FlattenedB = float2 ( TransformedB.x * RecipZB, TransformedB.y * RecipZB );\n"

    // These are now still in TanEyeAngle space.
    // Scale them into the correct [0-1],[0-1] UV lookup space (depending on eye)
    "   oHmdSpcTexCoordR = FlattenedR * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oHmdSpcTexCoordG = FlattenedG * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oHmdSpcTexCoordB = FlattenedB * EyeToSourceUVScale + EyeToSourceUVOffset;\n"

    // Static layer texcoords don't get any time warp offset
    "   oOverlayTexCoordR = TexCoord0 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oOverlayTexCoordG = TexCoord1 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oOverlayTexCoordB = TexCoord2 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"

    "   oColor = Color.r;\n"              // Used for vignette fade.
    "}\n";
    
static const char* PostProcessMeshTimewarpPixelShaderSrc =
    "Texture2D HmdSpcTexture : register(t0);\n"
    "Texture2D OverlayTexture : register(t1);\n"
    "SamplerState Linear : register(s0);\n"
    "float  UseOverlay = 1;\n"
    "\n"
    "float4 main(in float4 oPosition : SV_Position, in float4 oColor : COLOR,\n"
    "          in float2 oHmdSpcTexCoordR : TEXCOORD0, in float2 oHmdSpcTexCoordG : TEXCOORD1, in float2 oHmdSpcTexCoordB : TEXCOORD2,"
    "          in float2 oOverlayTexCoordR : TEXCOORD3, in float2 oOverlayTexCoordG : TEXCOORD4, in float2 oOverlayTexCoordB : TEXCOORD5) : SV_Target\n"
    "{\n"
    "   float4 finalColor = float4(0,0,0,1);\n"
    "   finalColor.r = HmdSpcTexture.Sample(Linear, oHmdSpcTexCoordR).r;\n"
    "   finalColor.g = HmdSpcTexture.Sample(Linear, oHmdSpcTexCoordG).g;\n"
    "   finalColor.b = HmdSpcTexture.Sample(Linear, oHmdSpcTexCoordB).b;\n"

    "   if(UseOverlay > 0)\n"
    "   {\n"
    "       float2 overlayColorR = OverlayTexture.Sample(Linear, oOverlayTexCoordR).ra;\n"
    "       float2 overlayColorG = OverlayTexture.Sample(Linear, oOverlayTexCoordG).ga;\n"
    "       float2 overlayColorB = OverlayTexture.Sample(Linear, oOverlayTexCoordB).ba;\n"

    // do premultiplied alpha blending - overlayColorX.x is color, overlayColorX.y is alpha
    "       finalColor.r = finalColor.r * saturate(1-overlayColorR.y) + overlayColorR.x;\n"
    "       finalColor.g = finalColor.g * saturate(1-overlayColorG.y) + overlayColorG.x;\n"
    "       finalColor.b = finalColor.b * saturate(1-overlayColorB.y) + overlayColorB.x;\n"
    "   }\n"

    "   finalColor.rgb = saturate(finalColor.rgb * oColor.rgb);\n"
    "   return finalColor;\n"
    "}\n";


//----------------------------------------------------------------------------
// Pixel shader is very simple - does three texture reads.
// Vertex shader does all the hard work.
// Used for mesh-based distortion with positional timewarp.

static const char* PostProcessMeshPositionalTimewarpVertexShaderSrc =
    "Texture2DMS<float,4> DepthTexture : register(t0);\n"
    // Padding because we are uploading "standard uniform buffer" constants
    "float4x4 Padding1;\n"
    "float4x4 Padding2;\n"
    "float2 EyeToSourceUVScale;\n"
    "float2 EyeToSourceUVOffset;\n"
    "float2 DepthProjector;\n"
    "float2 DepthDimSize;\n"
    "float4x4 EyeRotationStart;\n"
    "float4x4 EyeRotationEnd;\n"

    "float4 PositionFromDepth(float2 inTexCoord)\n"
    "{\n"
    "   float2 eyeToSourceTexCoord = inTexCoord * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   float depth = DepthTexture.Load(int2(eyeToSourceTexCoord * DepthDimSize), 0).x;\n"
    "   float linearDepth = DepthProjector.y / (depth - DepthProjector.x);\n"
    "   float4 retVal = float4(inTexCoord, 1, 1);\n"
    "   retVal.xyz *= linearDepth;\n"
    "   return retVal;\n"
    "}\n"

    "float2 TimewarpTexCoordToWarpedPos(float2 inTexCoord, float4x4 rotMat)\n"
    "{\n"
    // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
    // These are now "real world" vectors in direction (x,y,1) relative to the eye of the HMD.	
    // Apply the 4x4 timewarp rotation to these vectors.
    "   float4 inputPos = PositionFromDepth(inTexCoord);\n"
    "   float3 transformed = float3( mul ( rotMat, inputPos ).xyz);\n"
    // Project them back onto the Z=1 plane of the rendered images.
    "   float2 flattened = transformed.xy / transformed.z;\n"
    // Scale them into ([0,0.5],[0,1]) or ([0.5,0],[0,1]) UV lookup space (depending on eye)
    "   float2 noDepthUV = flattened * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    //"   float depth = DepthTexture.SampleLevel(Linear, noDepthUV, 0).r;\n"
    "   return noDepthUV.xy;\n"
    "}\n"

    "void main(in float2 Position    : POSITION,    in float4 Color       : COLOR0,    in float2 TexCoord0 : TEXCOORD0,\n"
    "          in float2 TexCoord1   : TEXCOORD1,   in float2 TexCoord2   : TEXCOORD2,\n"
    "          out float4 oPosition  : SV_Position, out float4 oColor     : COLOR,\n"
    "          out float2 oHmdSpcTexCoordR : TEXCOORD0, out float2 oHmdSpcTexCoordG : TEXCOORD1, out float2 oHmdSpcTexCoordB : TEXCOORD2,"
    "          out float2 oOverlayTexCoordR : TEXCOORD3, out float2 oOverlayTexCoordG : TEXCOORD4, out float2 oOverlayTexCoordB : TEXCOORD5)\n"
    "{\n"
    "   oPosition.x = Position.x;\n"
    "   oPosition.y = Position.y;\n"
    "   oPosition.z = 0.5;\n"
    "   oPosition.w = 1.0;\n"

    "   float timewarpLerpFactor = Color.a;\n"
    "   float4x4 lerpedEyeRot = lerp(EyeRotationStart, EyeRotationEnd, timewarpLerpFactor);\n"
    //"	float4x4 lerpedEyeRot = EyeRotationStart;\n"

    // warped positions are a bit more involved, hence a separate function
    "   oHmdSpcTexCoordR = TimewarpTexCoordToWarpedPos(TexCoord0, lerpedEyeRot);\n"
    "   oHmdSpcTexCoordG = TimewarpTexCoordToWarpedPos(TexCoord1, lerpedEyeRot);\n"
    "   oHmdSpcTexCoordB = TimewarpTexCoordToWarpedPos(TexCoord2, lerpedEyeRot);\n"

    "   oOverlayTexCoordR = TexCoord0 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oOverlayTexCoordG = TexCoord1 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oOverlayTexCoordB = TexCoord2 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"

    "   oColor = Color.r;              // Used for vignette fade.\n"
    "}\n";

static const char* PostProcessMeshPositionalTimewarpPixelShaderSrc =
    "Texture2D HmdSpcTexture : register(t0);\n"
    "Texture2D OverlayTexture : register(t1);\n"
    "SamplerState Linear : register(s0);\n"
    "float2 DepthDimSize;\n"
    "float  UseOverlay = 1;\n"
    "\n"
    "float4 main(in float4 oPosition : SV_Position, in float4 oColor : COLOR,\n"
    "            in float2 oHmdSpcTexCoordR : TEXCOORD0, in float2 oHmdSpcTexCoordG : TEXCOORD1, in float2 oHmdSpcTexCoordB : TEXCOORD2,"
    "            in float2 oOverlayTexCoordR : TEXCOORD3, in float2 oOverlayTexCoordG : TEXCOORD4, in float2 oOverlayTexCoordB : TEXCOORD5) : SV_Target\n"
    "{\n"
    "   float4 finalColor = float4(0,0,0,1);\n"
    "   finalColor.r = HmdSpcTexture.Sample(Linear, oHmdSpcTexCoordR).r;\n"
    "   finalColor.g = HmdSpcTexture.Sample(Linear, oHmdSpcTexCoordG).g;\n"
    "   finalColor.b = HmdSpcTexture.Sample(Linear, oHmdSpcTexCoordB).b;\n"

    "   if(UseOverlay > 0)\n"
    "   {\n"
    "       float2 overlayColorR = OverlayTexture.Sample(Linear, oOverlayTexCoordR).ra;\n"
    "       float2 overlayColorG = OverlayTexture.Sample(Linear, oOverlayTexCoordG).ga;\n"
    "       float2 overlayColorB = OverlayTexture.Sample(Linear, oOverlayTexCoordB).ba;\n"

    // do premultiplied alpha blending - overlayColorX.x is color, overlayColorX.y is alpha
    "       finalColor.r = finalColor.r * saturate(1-overlayColorR.y) + overlayColorR.x;\n"
    "       finalColor.g = finalColor.g * saturate(1-overlayColorG.y) + overlayColorG.x;\n"
    "       finalColor.b = finalColor.b * saturate(1-overlayColorB.y) + overlayColorB.x;\n"
    "   }\n"

    "   finalColor.rgb = saturate(finalColor.rgb * oColor.rgb);\n"
    "   return finalColor;\n"
    "}\n";

//----------------------------------------------------------------------------
// Pixel shader is very simple - does three texture reads.
// Vertex shader does all the hard work.
// Used for mesh-based heightmap reprojection for positional timewarp.

static D3D1x_(INPUT_ELEMENT_DESC) HeightmapVertexDesc[] =
{
    {"Position", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,          D3D1x_(INPUT_PER_VERTEX_DATA), 0},
    {"TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 8,          D3D1x_(INPUT_PER_VERTEX_DATA), 0},
};

static const char* PostProcessHeightmapTimewarpVertexShaderSrc =
	"Texture2DMS<float,4> DepthTexture : register(t0);\n"
    // Padding because we are uploading "standard uniform buffer" constants
    "float4x4 Padding1;\n"
    "float4x4 Padding2;\n"
    "float2 EyeToSourceUVScale;\n"
    "float2 EyeToSourceUVOffset;\n"
	"float2 DepthDimSize;\n"
	"float4x4 EyeXformStart;\n"
    "float4x4 EyeXformEnd;\n"
    //"float4x4 Projection;\n"
    "float4x4 InvProjection;\n"

    "float4 PositionFromDepth(float2 position, float2 inTexCoord)\n"
    "{\n"
    "   float depth = DepthTexture.Load(int2(inTexCoord * DepthDimSize), 0).x;\n"
	"   float4 retVal = float4(position, depth, 1);\n"
    "   return retVal;\n"
    "}\n"

    "float4 TimewarpPos(float2 position, float2 inTexCoord, float4x4 rotMat)\n"
    "{\n"
    // Apply the 4x4 timewarp rotation to these vectors.
    "   float4 transformed = PositionFromDepth(position, inTexCoord);\n"
    // TODO: Precombining InvProjection in rotMat causes loss of precision flickering
    "   transformed = mul ( InvProjection, transformed );\n"
    "   transformed = mul ( rotMat, transformed );\n"
    // Commented out as Projection is currently contained in rotMat
    //"   transformed = mul ( Projection, transformed );\n"
    "   return transformed;\n"
    "}\n"

    "void main( in float2 Position    : POSITION,    in float3 TexCoord0    : TEXCOORD0,\n"
    "           out float4 oPosition  : SV_Position, out float2 oTexCoord0  : TEXCOORD0)\n"
    "{\n"
    "   float2 eyeToSrcTexCoord = TexCoord0.xy * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord0 = eyeToSrcTexCoord;\n"

    "   float timewarpLerpFactor = TexCoord0.z;\n"
    "   float4x4 lerpedEyeRot = lerp(EyeXformStart, EyeXformEnd, timewarpLerpFactor);\n"
    //"	float4x4 lerpedEyeRot = EyeXformStart;\n"

    "   oPosition = TimewarpPos(Position.xy, oTexCoord0, lerpedEyeRot);\n"
    "}\n";

static const char* PostProcessHeightmapTimewarpPixelShaderSrc =
	"Texture2D Texture : register(t0);\n"
    "SamplerState Linear : register(s0);\n"
	"\n"
    "float4 main(in float4 oPosition : SV_Position, in float2 oTexCoord0 : TEXCOORD0) : SV_Target\n"
    "{\n"
    "   float3 result;\n"
	"   result = Texture.Sample(Linear, oTexCoord0);\n"
	"   return float4(result, 1.0);\n"
    "}\n";

#pragma endregion

//----------------------------------------------------------------------------

struct ShaderSource
{
    const char* ShaderModel;
    const char* SourceStr;
};

static ShaderSource VShaderSrcs[VShader_Count] =
{
    {"vs_4_0", DirectVertexShaderSrc},
    {"vs_4_0", StdVertexShaderSrc},
    {"vs_4_0", PostProcessVertexShaderSrc},
    {"vs_4_0", PostProcessMeshVertexShaderSrc},
    {"vs_4_0", PostProcessMeshTimewarpVertexShaderSrc},
    {"vs_4_1", PostProcessMeshPositionalTimewarpVertexShaderSrc},
    {"vs_4_1", PostProcessHeightmapTimewarpVertexShaderSrc},
};
static ShaderSource FShaderSrcs[FShader_Count] =
{
    {"ps_4_0", SolidPixelShaderSrc},
    {"ps_4_0", GouraudPixelShaderSrc},
    {"ps_4_0", TexturePixelShaderSrc},
	{"ps_4_0", AlphaTexturePixelShaderSrc},
	{"ps_4_0", AlphaBlendedTexturePixelShaderSrc},
    {"ps_4_0", PostProcessPixelShaderWithChromAbSrc},
    {"ps_4_0", LitSolidPixelShaderSrc},
    {"ps_4_0", LitTexturePixelShaderSrc},
    {"ps_4_0", MultiTexturePixelShaderSrc},
    {"ps_4_0", PostProcessMeshPixelShaderSrc},
    {"ps_4_0", PostProcessMeshTimewarpPixelShaderSrc},
    {"ps_4_0", PostProcessMeshPositionalTimewarpPixelShaderSrc},
    {"ps_4_0", PostProcessHeightmapTimewarpPixelShaderSrc},
};

#ifdef OVR_BUILD_DEBUG

static void ReportCOMError(HRESULT hr, const char* file, int line)
{
    if (FAILED(hr))
    {
        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();

        if (sizeof(TCHAR) == sizeof(char))
        {
            LogError("{ERR-017w} [D3D] Error in %s on line %d : %s", file, line, errMsg);
        }
        else
        {
#ifdef _UNICODE
            size_t len = wcslen(errMsg);
            char* data = new char[len + 1];
            size_t count = len;
            wcstombs_s(&count, data, len + 1, errMsg, len);
            if (count < len)
            {
                len = count;
            }
            data[len] = '\0';
#else
            const char* data = errMsg;
#endif
            LogError("{ERR-018w} [D3D] Error in %s on line %d : %s", file, line, data);
#ifdef _UNICODE
            delete[] data;
#endif
        }

        OVR_ASSERT(false);
    }
}

#define OVR_LOG_COM_ERROR(hr) \
    ReportCOMError(hr, __FILE__, __LINE__);

#else

#define OVR_LOG_COM_ERROR(hr) ;

#endif


RenderDevice::RenderDevice(const RendererParams& p, HWND window) :
    DXGIFactory(),
    Window(window),
    Device(),
    Context(),
    SwapChain(),
    Adapter(),
    FullscreenOutput(),
    FSDesktopX(-1),
    FSDesktopY(-1),
    PreFullscreenX(0),
    PreFullscreenY(0),
    PreFullscreenW(0),
    PreFullscreenH(0),
    BackBuffer(),
    BackBufferRT(),
#if (OVR_D3D_VERSION>=11)
    BackBufferUAV(),
#endif
    CurRenderTarget(),
    CurDepthBuffer(),
    Rasterizer(),
    BlendState(),
  //DepthStates[]
    CurDepthState(),
    ModelVertexIL(),
    DistortionVertexIL(),
    HeightmapVertexIL(),
  //SamplerStates[]
    StdUniforms(),
  //UniformBuffers[];
  //MaxTextureSet[];
  //VertexShaders[];
  //PixelShaders[];
  //pStereoShaders[];
  //CommonUniforms[];
    ExtraShaders(),
    DefaultFill(),
    QuadVertexBuffer(),
    DepthBuffers()
{
    memset(&D3DViewport, 0, sizeof(D3DViewport));
    memset(MaxTextureSet, 0, sizeof(MaxTextureSet));

    HRESULT hr;

    RECT rc;
    if (p.Resolution == Sizei(0))
    {
        GetClientRect(window, &rc);
        UINT width  = rc.right - rc.left;
        UINT height = rc.bottom - rc.top;
        ::OVR::Render::RenderDevice::SetWindowSize(width, height);
    }
    else
    {
        // TBD: This should be renamed to not be tied to window for App mode.
        ::OVR::Render::RenderDevice::SetWindowSize(p.Resolution.w, p.Resolution.h);
    }
    
    Params = p;
    DXGIFactory = NULL;
    hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&DXGIFactory.GetRawRef()));
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
        return;
    }

    // Find the adapter & output (monitor) to use for fullscreen, based on the reported name of the HMD's monitor.
    if (Params.Display.MonitorName.GetLength() > 0)
    {
        for(UINT AdapterIndex = 0; ; AdapterIndex++)
        {
			Adapter = NULL;
            HRESULT hr = DXGIFactory->EnumAdapters(AdapterIndex, &Adapter.GetRawRef());
            if (hr == DXGI_ERROR_NOT_FOUND)
                break;
            if (FAILED(hr))
            {
                OVR_LOG_COM_ERROR(hr);
            }

            DXGI_ADAPTER_DESC Desc;
            hr = Adapter->GetDesc(&Desc);
            if (FAILED(hr))
            {
                OVR_LOG_COM_ERROR(hr);
            }

            UpdateMonitorOutputs();

            if (FullscreenOutput)
                break;
        }

        if (!FullscreenOutput)
            Adapter = NULL;
    }

    if (!Adapter)
    {
        hr = DXGIFactory->EnumAdapters(0, &Adapter.GetRawRef());
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
        }
        UpdateMonitorOutputs();
    }

    int flags = D3D10_CREATE_DEVICE_BGRA_SUPPORT;

    if(p.DebugEnabled)
        flags |= D3D1x_(CREATE_DEVICE_DEBUG);

#if (OVR_D3D_VERSION == 10)
    Device = NULL;
    hr = D3D10CreateDevice1(Adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, flags, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION,
                           &Device.GetRawRef());
    Context = Device;
    Context->AddRef();
#else //11
    Device = NULL;
    Context = NULL;
    D3D_FEATURE_LEVEL featureLevel; // TODO: Limit certain features based on D3D feature level
    hr = D3D11CreateDevice(Adapter, Adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                           NULL, flags, NULL, 0, D3D1x_(SDK_VERSION),
                           &Device.GetRawRef(), &featureLevel, &Context.GetRawRef());
#endif
	if (FAILED(hr))
	{
        OVR_LOG_COM_ERROR(hr);
        LogError("{ERR-019w} [D3D1X] Unable to create device: %x", hr);
        OVR_ASSERT(false);
		return;
	}

    if (!RecreateSwapChain())
        return;

    if (Params.Fullscreen)
        SwapChain->SetFullscreenState(1, FullscreenOutput);

    CurRenderTarget = NULL;
    for(int i = 0; i < Shader_Count; i++)
    {
        UniformBuffers[i] = *CreateBuffer();
        MaxTextureSet[i] = 0;
    }

    ID3D10Blob* vsData = CompileShader(VShaderSrcs[0].ShaderModel, VShaderSrcs[0].SourceStr);

    VertexShaders[VShader_MV] = *new VertexShader(this, vsData);
    for(int i = 1; i < VShader_Count; i++)
    {
        OVR_ASSERT ( VShaderSrcs[i].SourceStr != NULL );      // You forgot a shader!
        ID3D10Blob *pShader = CompileShader(VShaderSrcs[i].ShaderModel, VShaderSrcs[i].SourceStr);

        VertexShaders[i] = NULL;
        if ( pShader != NULL )
        {
            VertexShaders[i] = *new VertexShader(this, pShader);
        }
    }

    for(int i = 0; i < FShader_Count; i++)
    {
        OVR_ASSERT ( FShaderSrcs[i].SourceStr != NULL );      // You forgot a shader!
        ID3D10Blob *pShader = CompileShader(FShaderSrcs[i].ShaderModel, FShaderSrcs[i].SourceStr);

        PixelShaders[i] = NULL;
        if ( pShader != NULL )
        {
            PixelShaders[i] = *new PixelShader(this, pShader);
        }
    }

    intptr_t bufferSize = vsData->GetBufferSize();
    const void* buffer = vsData->GetBufferPointer();
    ModelVertexIL = NULL;
    ID3D1xInputLayout** objRef = &ModelVertexIL.GetRawRef();
    hr = Device->CreateInputLayout(ModelVertexDesc, sizeof(ModelVertexDesc)/sizeof(ModelVertexDesc[0]), buffer, bufferSize, objRef);
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }

    {
        ID3D10Blob* vsData2 = CompileShader("vs_4_1", PostProcessMeshVertexShaderSrc);
        intptr_t bufferSize2 = vsData2->GetBufferSize();
        const void* buffer2 = vsData2->GetBufferPointer();
        DistortionVertexIL = NULL;
        ID3D1xInputLayout** objRef2 = &DistortionVertexIL.GetRawRef();
        hr = Device->CreateInputLayout(DistortionVertexDesc, sizeof(DistortionVertexDesc)/sizeof(DistortionVertexDesc[0]), buffer2, bufferSize2, objRef2);
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
        }
    }

    {
        ID3D10Blob* vsData2 = CompileShader("vs_4_1", PostProcessHeightmapTimewarpVertexShaderSrc);
        intptr_t bufferSize2 = vsData2->GetBufferSize();
        const void* buffer2 = vsData2->GetBufferPointer();
        HeightmapVertexIL = NULL;
        ID3D1xInputLayout** objRef2 = &HeightmapVertexIL.GetRawRef();
        hr = Device->CreateInputLayout(HeightmapVertexDesc, sizeof(HeightmapVertexDesc)/sizeof(HeightmapVertexDesc[0]), buffer2, bufferSize2, objRef2);
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
        }
    }

    Ptr<ShaderSet> gouraudShaders = *new ShaderSet();
    gouraudShaders->SetShader(VertexShaders[VShader_MVP]);
    gouraudShaders->SetShader(PixelShaders[FShader_Gouraud]);
    DefaultFill = *new ShaderFill(gouraudShaders);

#if (OVR_D3D_VERSION == 10)
    D3D1x_(BLEND_DESC) bm;
    memset(&bm, 0, sizeof(bm));
    bm.BlendEnable[0] = true;
    bm.BlendOp      = bm.BlendOpAlpha   = D3D1x_(BLEND_OP_ADD);
    bm.SrcBlend     = bm.SrcBlendAlpha  = D3D1x_(BLEND_ONE); //premultiplied alpha
    bm.DestBlend    = bm.DestBlendAlpha = D3D1x_(BLEND_INV_SRC_ALPHA);
    bm.RenderTargetWriteMask[0]         = D3D1x_(COLOR_WRITE_ENABLE_ALL);
    BlendState = NULL;
    hr = Device->CreateBlendState(&bm, &BlendState.GetRawRef());
#else
    D3D1x_(BLEND_DESC) bm;
    memset(&bm, 0, sizeof(bm));
    bm.RenderTarget[0].BlendEnable = true;
    bm.RenderTarget[0].BlendOp     = bm.RenderTarget[0].BlendOpAlpha    = D3D1x_(BLEND_OP_ADD);
    bm.RenderTarget[0].SrcBlend    = bm.RenderTarget[0].SrcBlendAlpha   = D3D1x_(BLEND_ONE); //premultiplied alpha
    bm.RenderTarget[0].DestBlend   = bm.RenderTarget[0].DestBlendAlpha  = D3D1x_(BLEND_INV_SRC_ALPHA);
    bm.RenderTarget[0].RenderTargetWriteMask = D3D1x_(COLOR_WRITE_ENABLE_ALL);
    BlendState = NULL;
    hr = Device->CreateBlendState(&bm, &BlendState.GetRawRef());
#endif
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }

    D3D1x_(RASTERIZER_DESC) rs;
    memset(&rs, 0, sizeof(rs));
    rs.AntialiasedLineEnable = false;       // You can't just turn this on - it needs alpha modes etc setting up and doesn't work with Z buffers.
    rs.CullMode = D3D1x_(CULL_BACK);
    // rs.CullMode = D3D1x_(CULL_NONE);
    rs.DepthClipEnable = true;
    rs.FillMode = D3D1x_(FILL_SOLID);
    Rasterizer = NULL;
    hr = Device->CreateRasterizerState(&rs, &Rasterizer.GetRawRef());
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }

    QuadVertexBuffer = *CreateBuffer();
    const Render::Vertex QuadVertices[] =
    { Vertex(Vector3f(0, 1, 0)), Vertex(Vector3f(1, 1, 0)),
      Vertex(Vector3f(0, 0, 0)), Vertex(Vector3f(1, 0, 0)) };
    if (!QuadVertexBuffer->Data(Buffer_Vertex | Buffer_ReadOnly, QuadVertices, sizeof(QuadVertices)))
    {
        OVR_ASSERT(false);
    }

    SetDepthMode(0, 0);
}

RenderDevice::~RenderDevice()
{
    if (SwapChain && Params.Fullscreen)
    {
        HRESULT hr = SwapChain->SetFullscreenState(false, NULL);
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
        }
    }
}


// Implement static initializer function to create this class.
Render::RenderDevice* RenderDevice::CreateDevice(const RendererParams& rp, void* oswnd)
{
#if (OVR_D3D_VERSION == 10)
    Render::D3D10::RenderDevice* render = new RenderDevice(rp, (HWND)oswnd);
#else
    Render::D3D11::RenderDevice* render = new RenderDevice(rp, (HWND)oswnd);
#endif
    // Sanity check to make sure our resources were created.
    // This should stop a lot of driver related crashes we have experienced
    if ((render->DXGIFactory == NULL) || (render->Device == NULL) || (render->SwapChain == NULL))
    {
        OVR_ASSERT(false);
        // TBD: Probabaly other things like shader creation should be verified as well
        render->Shutdown();
        render->Release();
        return NULL;
    }
    else
    {
        return render;
    }
}


// Fallback monitor enumeration in case newly plugged in monitor wasn't detected.
// Added originally for the FactoryTest app.
// New Outputs don't seem to be detected unless adapter is re-created, but that would also
// require us to re-initialize D3D10 (recreating objects, etc). This bypasses that for "fake"
// fullscreen modes.
BOOL CALLBACK MonitorEnumFunc(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData)
{
    RenderDevice* renderer = (RenderDevice*)dwData;

    MONITORINFOEXA monitor;
    monitor.cbSize = sizeof(monitor);

    if (::GetMonitorInfoA(hMonitor, &monitor) && monitor.szDevice[0])
    {
        DISPLAY_DEVICEA dispDev;
        memset(&dispDev, 0, sizeof(dispDev));
        dispDev.cb = sizeof(dispDev);

        if (::EnumDisplayDevicesA(monitor.szDevice, 0, &dispDev, 0))
        {
            if (strstr(String(dispDev.DeviceName).ToCStr(), renderer->GetParams().Display.MonitorName.ToCStr()))
            {
                renderer->FSDesktopX = monitor.rcMonitor.left;
                renderer->FSDesktopY = monitor.rcMonitor.top;
                return FALSE;
            }
        }
    }

    return TRUE;
}


void RenderDevice::UpdateMonitorOutputs(bool needRecreate)
{
    HRESULT hr;

    if (needRecreate)
    {
        // need to recreate DXGIFactory and Adapter in order 
        // to get latest info about monitors.
        if (SwapChain)
        {
            hr = SwapChain->SetFullscreenState(FALSE, NULL);
            if (FAILED(hr))
            {
                OVR_LOG_COM_ERROR(hr);
            }
            SwapChain = NULL;
        }

        DXGIFactory = NULL;
        Adapter = NULL;
        hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&DXGIFactory.GetRawRef()));
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
            return;
        }
        hr = DXGIFactory->EnumAdapters(0, &Adapter.GetRawRef());
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
        }
    }

    bool deviceNameFound = false;

    for(UINT OutputIndex = 0; ; OutputIndex++)
    {
        Ptr<IDXGIOutput> Output;
        hr = Adapter->EnumOutputs(OutputIndex, &Output.GetRawRef());
        if (hr == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
        }

        DXGI_OUTPUT_DESC OutDesc;
        Output->GetDesc(&OutDesc);

        MONITORINFOEXA monitor;
        monitor.cbSize = sizeof(monitor);
        if (::GetMonitorInfoA(OutDesc.Monitor, &monitor) && monitor.szDevice[0])
        {
            DISPLAY_DEVICEA dispDev;
            memset(&dispDev, 0, sizeof(dispDev));
            dispDev.cb = sizeof(dispDev);

            if (::EnumDisplayDevicesA(monitor.szDevice, 0, &dispDev, 0))
            {
                if (strstr(String(dispDev.DeviceName).ToCStr(), Params.Display.MonitorName.ToCStr()))
                {
                    deviceNameFound = true;
                    FullscreenOutput = Output;
                    FSDesktopX = monitor.rcMonitor.left;
                    FSDesktopY = monitor.rcMonitor.top;
                    break;
                }
            }
        }
    }

    if (!deviceNameFound && !Params.Display.MonitorName.IsEmpty())
    {
        if (!EnumDisplayMonitors(0, 0, MonitorEnumFunc, (LPARAM)this))
        {
            OVR_ASSERT(false);
        }
    }
}

bool RenderDevice::RecreateSwapChain()
{
    HRESULT hr;

    DXGI_SWAP_CHAIN_DESC scDesc;
    memset(&scDesc, 0, sizeof(scDesc));
    scDesc.BufferCount = 1;
    scDesc.BufferDesc.Width  = WindowWidth;
    scDesc.BufferDesc.Height = WindowHeight;
    scDesc.BufferDesc.Format = Params.SrgbBackBuffer ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    // Use default refresh rate; switching rate on CC prototype can cause screen lockup.
    scDesc.BufferDesc.RefreshRate.Numerator = 0;
    scDesc.BufferDesc.RefreshRate.Denominator = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
#if (OVR_D3D_VERSION>=11)
    scDesc.BufferUsage |= DXGI_USAGE_UNORDERED_ACCESS;
#endif
    scDesc.OutputWindow = Window;
    scDesc.SampleDesc.Count = Params.Multisample;
    scDesc.SampleDesc.Quality = 0;
    scDesc.Windowed = Params.Fullscreen != Display_Fullscreen;
    scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	if (SwapChain)
	{
		hr = SwapChain->SetFullscreenState(FALSE, NULL);
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
        }
        SwapChain = NULL;
	}

    Ptr<IDXGISwapChain> newSC;
    hr = DXGIFactory->CreateSwapChain(Device, &scDesc, &newSC.GetRawRef());
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
        return false;
    }
    SwapChain = newSC;

    BackBuffer = NULL;
    BackBufferRT = NULL;
#if (OVR_D3D_VERSION>=11)
    BackBufferUAV = NULL;
#endif
    hr = SwapChain->GetBuffer(0, __uuidof(ID3D1xTexture2D), (void**)&BackBuffer.GetRawRef());
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
        return false;
    }

    hr = Device->CreateRenderTargetView(BackBuffer, NULL, &BackBufferRT.GetRawRef());
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
        return false;
    }

#if (OVR_D3D_VERSION>=11)
    hr = Device->CreateUnorderedAccessView(BackBuffer, NULL, &BackBufferUAV.GetRawRef());
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
        return false;
    }
#endif

    Texture* depthBuffer = GetDepthBuffer(WindowWidth, WindowHeight, Params.Multisample);
    CurDepthBuffer = depthBuffer;
	if (CurRenderTarget == NULL && depthBuffer != NULL)
    {
        Context->OMSetRenderTargets(1, &BackBufferRT.GetRawRef(), depthBuffer->TexDsv);
    }
    return true;
}

bool RenderDevice::SetParams(const RendererParams& newParams)
{
    String oldMonitor = Params.Display.MonitorName;

    Params = newParams;
    if (newParams.Display.MonitorName != oldMonitor)
    {
        UpdateMonitorOutputs(true);
    }

    return RecreateSwapChain();
}
	
ovrRenderAPIConfig RenderDevice::Get_ovrRenderAPIConfig() const
{
#if (OVR_D3D_VERSION == 10)
	static ovrD3D10Config cfg;
	cfg.D3D10.Header.API            = ovrRenderAPI_D3D10;
	cfg.D3D10.Header.BackBufferSize = Sizei(WindowWidth, WindowHeight);
	cfg.D3D10.Header.Multisample    = Params.Multisample;
	cfg.D3D10.pDevice               = Device;
	cfg.D3D10.pBackBufferRT         = BackBufferRT;
	cfg.D3D10.pSwapChain            = SwapChain;
#else
	static ovrD3D11Config cfg;
	cfg.D3D11.Header.API            = ovrRenderAPI_D3D11;
	cfg.D3D11.Header.BackBufferSize = Sizei(WindowWidth, WindowHeight);
	cfg.D3D11.Header.Multisample    = Params.Multisample;
	cfg.D3D11.pDevice               = Device;
	cfg.D3D11.pDeviceContext        = Context;
	cfg.D3D11.pBackBufferRT         = BackBufferRT;
	cfg.D3D11.pBackBufferUAV        = BackBufferUAV;
	cfg.D3D11.pSwapChain            = SwapChain;
#endif
	return cfg.Config;
}

ovrTexture Texture::Get_ovrTexture()
{
	ovrTexture tex;

	OVR::Sizei newRTSize(Width, Height);
	
#if (OVR_D3D_VERSION == 10)
    ovrD3D10TextureData* texData = (ovrD3D10TextureData*)&tex;
    texData->Header.API            = ovrRenderAPI_D3D10;
#else
    ovrD3D11TextureData* texData = (ovrD3D11TextureData*)&tex;
    texData->Header.API            = ovrRenderAPI_D3D11;
#endif
    texData->Header.TextureSize    = newRTSize;
    texData->Header.RenderViewport = Recti(newRTSize);
    texData->pTexture              = Tex;
    texData->pSRView               = TexSv;

	return tex;
}

void RenderDevice::SetWindowSize(int w, int h)
{
	// This code is rendered a no-op
	// It interferes with proper driver operation in
	// application mode and doesn't add any value in
	// compatibility mode
	OVR_UNUSED(w);
	OVR_UNUSED(h);
}

bool RenderDevice::SetFullscreen(DisplayMode fullscreen)
{
    if (fullscreen == Params.Fullscreen)
    {
        return true;
    }

    if (Params.Fullscreen == Display_FakeFullscreen)
    {
        SetWindowLong(Window, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS);
        SetWindowPos(Window, NULL, PreFullscreenX, PreFullscreenY,
                     PreFullscreenW, PreFullscreenH, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    if (fullscreen == Display_FakeFullscreen)
    {
        // Get WINDOWPLACEMENT before changing style to get OVERLAPPED coordinates,
        // which we will restore.
        WINDOWPLACEMENT wp;
        wp.length = sizeof(wp);
        GetWindowPlacement(Window, &wp);
        PreFullscreenW = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
        PreFullscreenH = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
        PreFullscreenX = wp.rcNormalPosition.left;
        PreFullscreenY = wp.rcNormalPosition.top;
        // Warning: SetWindowLong sends message computed based on old size (incorrect).
        // A proper work-around would be to mask that message out during window frame change in Platform.
        SetWindowLong(Window, GWL_STYLE, WS_OVERLAPPED | WS_VISIBLE | WS_CLIPSIBLINGS);
        SetWindowPos(Window, NULL, FSDesktopX, FSDesktopY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);

        // Relocate cursor into the window to avoid losing focus on first click.
        POINT oldCursor;
        if (GetCursorPos(&oldCursor) &&
                ((oldCursor.x < FSDesktopX) || (oldCursor.x > (FSDesktopX + PreFullscreenW)) ||
                 (oldCursor.y < FSDesktopY) || (oldCursor.x > (FSDesktopY + PreFullscreenH))))
        {
            // TBD: FullScreen window logic should really be in platform; it causes world rotation
            // in relative mouse mode.
            ::SetCursorPos(FSDesktopX, FSDesktopY);
        }
    }
    else
    {
        HRESULT hr = SwapChain->SetFullscreenState(fullscreen, fullscreen ? FullscreenOutput : NULL);
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
            return false;
        }
    }

    Params.Fullscreen = fullscreen;
    return true;
}

void RenderDevice::SetViewport(const Recti& vp)
{
#if (OVR_D3D_VERSION == 10)
    D3DViewport.Width    = vp.w;
    D3DViewport.Height   = vp.h;
    D3DViewport.MinDepth = 0;
    D3DViewport.MaxDepth = 1;
    D3DViewport.TopLeftX = vp.x;
    D3DViewport.TopLeftY = vp.y;
#else
    D3DViewport.Width    = (float)vp.w;
    D3DViewport.Height   = (float)vp.h;
    D3DViewport.MinDepth = 0;
    D3DViewport.MaxDepth = 1;
    D3DViewport.TopLeftX = (float)vp.x;
    D3DViewport.TopLeftY = (float)vp.y;
#endif
    Context->RSSetViewports(1,&D3DViewport);
}

static int GetDepthStateIndex(bool enable, bool write, RenderDevice::CompareFunc func)
{
    if (!enable)
    {
        return 0;
    }
    return 1 + int(func) * 2 + write;
}

void RenderDevice::SetDepthMode(bool enable, bool write, CompareFunc func)
{
    int index = GetDepthStateIndex(enable, write, func);
    if (DepthStates[index])
    {
        CurDepthState = DepthStates[index];
        Context->OMSetDepthStencilState(DepthStates[index], 0);
        return;
    }

    D3D1x_(DEPTH_STENCIL_DESC) dss;
    memset(&dss, 0, sizeof(dss));
    dss.DepthEnable = enable;
    switch(func)
    {
    case Compare_Always:  dss.DepthFunc = D3D1x_(COMPARISON_ALWAYS);  break;
    case Compare_Less:    dss.DepthFunc = D3D1x_(COMPARISON_LESS);    break;
    case Compare_Greater: dss.DepthFunc = D3D1x_(COMPARISON_GREATER); break;
    default:
        OVR_ASSERT(0);
    }
    dss.DepthWriteMask = write ? D3D1x_(DEPTH_WRITE_MASK_ALL) : D3D1x_(DEPTH_WRITE_MASK_ZERO);
    HRESULT hr = Device->CreateDepthStencilState(&dss, &DepthStates[index].GetRawRef());
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }
    Context->OMSetDepthStencilState(DepthStates[index], 0);
    CurDepthState = DepthStates[index];
}

Texture* RenderDevice::GetDepthBuffer(int w, int h, int ms)
{
    for(unsigned i = 0; i < DepthBuffers.GetSize(); i++)
    {
        if (w == DepthBuffers[i]->Width && h == DepthBuffers[i]->Height &&
            ms == DepthBuffers[i]->Samples)
            return DepthBuffers[i];
    }

    Ptr<Texture> newDepth = *CreateTexture(Texture_Depth | Texture_RenderTarget | ms, w, h, NULL);
    if (newDepth == NULL)
    {
        OVR_DEBUG_LOG(("Failed to get depth buffer."));
        return NULL;
    }

    DepthBuffers.PushBack(newDepth);
    return newDepth.GetPtr();
}

void RenderDevice::Clear(float r /*= 0*/, float g /*= 0*/, float b /*= 0*/, float a /*= 1*/,
                         float depth /*= 1*/,
                         bool clearColor /*= true*/, bool clearDepth /*= true*/)
{
    if ( clearColor )
    {
        const float color[] = {r, g, b, a};
        if ( CurRenderTarget == NULL )
        {
            Context->ClearRenderTargetView ( BackBufferRT.GetRawRef(), color );
        }
        else
        {
            Context->ClearRenderTargetView ( CurRenderTarget->TexRtv, color );
        }
    }

    if ( clearDepth )
    {
        Context->ClearDepthStencilView ( CurDepthBuffer->TexDsv, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, depth, 0 );
    }
}

// Buffers

Buffer* RenderDevice::CreateBuffer()
{
    return new Buffer(this);
}

Buffer::~Buffer()
{
}

bool   Buffer::Data(int use, const void *buffer, size_t size)
{
    if (D3DBuffer && Size >= size)
    {
        if (Dynamic)
        {
            if (!buffer)
                return true;

            void* v = Map(0, size, Map_Discard);
            if (v)
            {
                memcpy(v, buffer, size);
                Unmap(v);
                return true;
            }
        }
        else
        {
            OVR_ASSERT (!(use & Buffer_ReadOnly));
            Ren->Context->UpdateSubresource(D3DBuffer, 0, NULL, buffer, 0, 0);
            return true;
        }
    }
    if (D3DBuffer)
    {
        D3DBuffer = NULL;
        Size = 0;
        Use = 0;
        Dynamic = false;
    }
#if (OVR_D3D_VERSION>=11)
    D3DUav = NULL;
#endif

    D3D1x_(BUFFER_DESC) desc;
    memset(&desc, 0, sizeof(desc));
    if (use & Buffer_ReadOnly)
    {
        desc.Usage = D3D1x_(USAGE_IMMUTABLE);
        desc.CPUAccessFlags = 0;
    }
    else
    {
        desc.Usage = D3D1x_(USAGE_DYNAMIC);
        desc.CPUAccessFlags = D3D1x_(CPU_ACCESS_WRITE);
        Dynamic = true;
    }

    switch(use & Buffer_TypeMask)
    {
    case Buffer_Vertex:  desc.BindFlags = D3D1x_(BIND_VERTEX_BUFFER); break;
    case Buffer_Index:   desc.BindFlags = D3D1x_(BIND_INDEX_BUFFER);  break;
    case Buffer_Uniform:
        desc.BindFlags = D3D1x_(BIND_CONSTANT_BUFFER);
        size = ((size + 15) & ~15);
        break;
    case Buffer_Feedback:
        desc.BindFlags = D3D1x_(BIND_STREAM_OUTPUT);
        desc.Usage     = D3D1x_(USAGE_DEFAULT);
        desc.CPUAccessFlags = 0;
        size = ((size + 15) & ~15);
        break;
    case Buffer_Compute:
#if (OVR_D3D_VERSION >= 11)
        // There's actually a bunch of options for buffers bound to a CS.
        // Right now this is the most appropriate general-purpose one. Add more as needed.

        // NOTE - if you want D3D1x_(CPU_ACCESS_WRITE), it MUST be either D3D1x_(USAGE_DYNAMIC) or D3D1x_(USAGE_STAGING).
        // TODO: we want a resource that is rarely written to, in which case we'd need two surfaces - one a STAGING
        // that the CPU writes to, and one a DEFAULT, and we CopyResource from one to the other. Hassle!
        // Setting it as D3D1x_(USAGE_DYNAMIC) will get the job done for now.
        // Also for fun - you can't have a D3D1x_(USAGE_DYNAMIC) buffer that is also a D3D1x_(BIND_UNORDERED_ACCESS).
        OVR_ASSERT ( !(use & Buffer_ReadOnly) );
        desc.BindFlags = D3D1x_(BIND_SHADER_RESOURCE);
        desc.Usage     = D3D1x_(USAGE_DYNAMIC);
        desc.MiscFlags = D3D1x_(RESOURCE_MISC_BUFFER_STRUCTURED);
        desc.CPUAccessFlags = D3D1x_(CPU_ACCESS_WRITE);
        // SUPERHACKYFIXME
        desc.StructureByteStride = sizeof(DistortionComputePin);

        Dynamic = true;
        size = ((size + 15) & ~15);
#else
        OVR_ASSERT ( false );  // No compute shaders in DX10
#endif
        break;
    default:
        OVR_ASSERT ( !"unknown buffer type" );
        break;
    }

    desc.ByteWidth = (unsigned)size;

    D3D1x_(SUBRESOURCE_DATA) sr;
    sr.pSysMem = buffer;
    sr.SysMemPitch = 0;
    sr.SysMemSlicePitch = 0;

    D3DBuffer = NULL;
    HRESULT hr = Ren->Device->CreateBuffer(&desc, buffer ? &sr : NULL, &D3DBuffer.GetRawRef());
    if (SUCCEEDED(hr))
    {
        Use = use;
        Size = desc.ByteWidth;
    }
    else
    {
        OVR_LOG_COM_ERROR(hr);
        OVR_ASSERT ( false );
        return false;
    }

    if ( ( use & Buffer_TypeMask ) == Buffer_Compute )
    {
        HRESULT hres = Ren->Device->CreateShaderResourceView ( D3DBuffer, NULL, &D3DSrv.GetRawRef() );
        if ( SUCCEEDED(hres) )
        {
#if (OVR_D3D_VERSION >= 11)
#if 0           // Right now we do NOT ask for UAV access (see flags above).
            hres = Ren->Device->CreateUnorderedAccessView ( D3DBuffer, NULL, &D3DUav.GetRawRef() );
            if ( SUCCEEDED(hres) )
            {
                // All went well.
            }
#endif
#endif
        }

        if ( !SUCCEEDED(hres) )
        {
            OVR_LOG_COM_ERROR(hr);
            OVR_ASSERT ( false );
            Use = 0;
            Size = 0;
            return false;
        }
    }

    return true;
}

void*  Buffer::Map(size_t start, size_t size, int flags)
{
    OVR_UNUSED(size);

    D3D1x_(MAP) mapFlags = D3D1x_(MAP_WRITE);
    if (flags & Map_Discard)    
        mapFlags = D3D1x_(MAP_WRITE_DISCARD);    
    if (flags & Map_Unsynchronized)    
        mapFlags = D3D1x_(MAP_WRITE_NO_OVERWRITE);    

#if (OVR_D3D_VERSION == 10)
    void* map;
    if (SUCCEEDED(D3DBuffer->Map(mapFlags, 0, &map)))    
        return ((char*)map) + start;    
    else    
        return NULL;
#else
    D3D1x_(MAPPED_SUBRESOURCE) map;
    if (SUCCEEDED(Ren->Context->Map(D3DBuffer, 0, mapFlags, 0, &map)))    
        return ((char*)map.pData) + start;    
    else    
        return NULL;    
#endif
}

bool   Buffer::Unmap(void* m)
{
    OVR_UNUSED(m);

#if (OVR_D3D_VERSION == 10)
    D3DBuffer->Unmap();
#else
    Ren->Context->Unmap(D3DBuffer, 0);
#endif
    return true;
}


// Shaders

#if (OVR_D3D_VERSION == 10)
template<> bool Shader<Render::Shader_Vertex, ID3D10VertexShader>::Load(void* shader, size_t size)
{
    HRESULT hr = Ren->Device->CreateVertexShader(shader, size, &D3DShader);
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }
    return SUCCEEDED(hr);
}
template<> bool Shader<Render::Shader_Pixel, ID3D10PixelShader>::Load(void* shader, size_t size)
{
    HRESULT hr = Ren->Device->CreatePixelShader(shader, size, &D3DShader);
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }
    return SUCCEEDED(hr);
}
template<> bool Shader<Render::Shader_Geometry, ID3D10GeometryShader>::Load(void* shader, size_t size)
{
    HRESULT hr = Ren->Device->CreateGeometryShader(shader, size, &D3DShader);
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }
    return SUCCEEDED(hr);
}

template<> void Shader<Render::Shader_Vertex, ID3D10VertexShader>::Set(PrimitiveType) const
{
    Ren->Context->VSSetShader(D3DShader);
}
template<> void Shader<Render::Shader_Pixel, ID3D10PixelShader>::Set(PrimitiveType) const
{
    Ren->Context->PSSetShader(D3DShader);
}
template<> void Shader<Render::Shader_Geometry, ID3D10GeometryShader>::Set(PrimitiveType) const
{
    Ren->Context->GSSetShader(D3DShader);
}

#else // 11
template<> bool Shader<Render::Shader_Vertex, ID3D11VertexShader>::Load(void* shader, size_t size)
{
    HRESULT hr = Ren->Device->CreateVertexShader(shader, size, NULL, &D3DShader);
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }
    return SUCCEEDED(hr);
}
template<> bool Shader<Render::Shader_Pixel, ID3D11PixelShader>::Load(void* shader, size_t size)
{
    HRESULT hr = Ren->Device->CreatePixelShader(shader, size, NULL, &D3DShader);
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }
    return SUCCEEDED(hr);
}
template<> bool Shader<Render::Shader_Geometry, ID3D11GeometryShader>::Load(void* shader, size_t size)
{
    HRESULT hr = Ren->Device->CreateGeometryShader(shader, size, NULL, &D3DShader);
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }
    return SUCCEEDED(hr);
}

template<> void Shader<Render::Shader_Vertex, ID3D11VertexShader>::Set(PrimitiveType) const
{
    Ren->Context->VSSetShader(D3DShader, NULL, 0);
}
template<> void Shader<Render::Shader_Pixel, ID3D11PixelShader>::Set(PrimitiveType) const
{
    Ren->Context->PSSetShader(D3DShader, NULL, 0);
}
template<> void Shader<Render::Shader_Geometry, ID3D11GeometryShader>::Set(PrimitiveType) const
{
    Ren->Context->GSSetShader(D3DShader, NULL, 0);
}
#endif

template<> void Shader<Render::Shader_Vertex, ID3D1xVertexShader>::SetUniformBuffer(Render::Buffer* buffer, int i)
{
    Ren->Context->VSSetConstantBuffers(i, 1, &((Buffer*)buffer)->D3DBuffer.GetRawRef());
}
template<> void Shader<Render::Shader_Pixel, ID3D1xPixelShader>::SetUniformBuffer(Render::Buffer* buffer, int i)
{
    Ren->Context->PSSetConstantBuffers(i, 1, &((Buffer*)buffer)->D3DBuffer.GetRawRef());
}
template<> void Shader<Render::Shader_Geometry, ID3D1xGeometryShader>::SetUniformBuffer(Render::Buffer* buffer, int i)
{
    Ren->Context->GSSetConstantBuffers(i, 1, &((Buffer*)buffer)->D3DBuffer.GetRawRef());
}

ID3D10Blob* RenderDevice::CompileShader(const char* profile, const char* src, const char* mainName)
{
    ID3D10Blob* shader;
    ID3D10Blob* errors;
    HRESULT hr = D3DCompile(src, strlen(src), NULL, NULL, NULL, mainName, profile,
                            0, 0, &shader, &errors);
    if (FAILED(hr))
    {
        OVR_DEBUG_LOG(("Compiling D3D shader for %s failed\n%s\n\n%s",
                       profile, src, errors->GetBufferPointer()));
        OutputDebugStringA((char*)errors->GetBufferPointer());
        OVR_LOG_COM_ERROR(hr);
        return NULL;
    }
    if (errors)
    {
        errors->Release();
    }
    return shader;
}


ShaderBase::ShaderBase(RenderDevice* r, ShaderStage stage) :
    Render::Shader(stage),
    Ren(r),
    UniformData(NULL),
    UniformsSize(-1)
{
}

ShaderBase::~ShaderBase()
{
    if (UniformData)
    {
        OVR_FREE(UniformData);
        UniformData = NULL;
    }
}

bool ShaderBase::SetUniform(const char* name, int n, const float* v)
{
    for (int i = 0; i < UniformInfo.GetSizeI(); i++)
    {
        if (UniformInfo[i].Name == name)
        {
            memcpy(UniformData + UniformInfo[i].Offset, v, n * sizeof(float));
            return true;
        }
    }

    return false;
}

void ShaderBase::InitUniforms(ID3D10Blob* s)
{
    ID3D10ShaderReflection* ref = NULL;
    HRESULT hr = D3D10ReflectShader(s->GetBufferPointer(), s->GetBufferSize(), &ref);
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }
    ID3D10ShaderReflectionConstantBuffer* buf = ref->GetConstantBufferByIndex(0);
    D3D10_SHADER_BUFFER_DESC bufd;
    hr = buf->GetDesc(&bufd);
    if (FAILED(hr))
    {
        //OVR_LOG_COM_ERROR(hr); - Seems to happen normally
        UniformsSize = 0;
        if (UniformData)
        {
            OVR_FREE(UniformData);
            UniformData = 0;
        }
        return;
    }

    for(unsigned i = 0; i < bufd.Variables; i++)
    {
        ID3D10ShaderReflectionVariable* var = buf->GetVariableByIndex(i);
        if (var)
        {
            D3D10_SHADER_VARIABLE_DESC vd;
            hr = var->GetDesc(&vd);
            if (SUCCEEDED(hr))
            {
                Uniform u;
                u.Name = vd.Name;
                u.Offset = vd.StartOffset;
                u.Size = vd.Size;
                UniformInfo.PushBack(u);
            }
            if (FAILED(hr))
            {
                OVR_LOG_COM_ERROR(hr);
            }
        }
    }

    UniformsSize = bufd.Size;
    UniformData = (unsigned char*)OVR_ALLOC(bufd.Size);
}

void ShaderBase::UpdateBuffer(Buffer* buf)
{
    if (UniformsSize)
    {
        if (!buf->Data(Buffer_Uniform, UniformData, UniformsSize))
        {
            OVR_ASSERT(false);
        }
    }
}

void RenderDevice::SetCommonUniformBuffer(int i, Render::Buffer* buffer)
{
    CommonUniforms[i] = (Buffer*)buffer;

    Context->PSSetConstantBuffers(1, 1, &CommonUniforms[1]->D3DBuffer.GetRawRef());
    Context->VSSetConstantBuffers(1, 1, &CommonUniforms[1]->D3DBuffer.GetRawRef());
}

Render::Shader *RenderDevice::LoadBuiltinShader(ShaderStage stage, int shader)
{
    switch(stage)
    {
    case Shader_Vertex:
        return VertexShaders[shader];
    case Shader_Pixel:
        return PixelShaders[shader];
    default:
        OVR_ASSERT ( false );
        return NULL;
    }
}

ShaderBase* RenderDevice::CreateStereoShader(PrimitiveType prim, Render::Shader* vs)
{
    if (pStereoShaders[prim])
    {
        return pStereoShaders[prim];
    }

    OVR_UNUSED(vs);
    const char* varyings =
        "   float4 Position : SV_Position;\n"
        "   float4 Color    : COLOR0;\n"
        "   float2 TexCoord : TEXCOORD0;\n"
        "   float3 Normal   : NORMAL;\n";
    const char* copyVaryings =
        "       o.Color = iv[i].Color;\n"
        "       o.Normal = iv[i].Normal;\n"
        "       o.TexCoord = iv[i].TexCoord;\n";

    StringBuffer src =
        "float4x4 Proj[2]     : register(c0);\n"
        "float4   ViewOffset  : register(c8);\n"
        "struct Varyings\n"
        "{\n";
    src += varyings;
    src += "};\n"
           "struct OutVaryings\n"
           "{\n";
    src += varyings;
    src +=
        "   float3 VPos     : TEXCOORD4;\n"
        "   uint   Viewport : SV_ViewportArrayIndex;\n"
        "};\n";

    if (prim == Prim_Lines)
        src +=
            "[maxvertexcount(4)]\n"
            "void main(line Varyings iv[2], inout LineStream<OutVaryings> v)\n";
    else
        src +=
            "[maxvertexcount(6)]\n"
            "void main(triangle Varyings iv[3], inout TriangleStream<OutVaryings> v)\n";

    char ivsize[6];
    OVR_sprintf(ivsize, 6, "%d", (prim == Prim_Lines) ? 2 : 3);

    src +=
        "{\n"
        "   OutVaryings o;\n"
        "   for (uint i = 0; i < ";
    src += ivsize;
    src += "; i++)\n"
           "   {\n"
           "       o.Position = mul(Proj[0], iv[i].Position - ViewOffset);\n"
           "       o.VPos = iv[i].Position;\n"
           "       o.Viewport = 0;\n";
    src += copyVaryings;
    src +=
        "       v.Append(o);\n"
        "   }\n"
        "   v.RestartStrip();\n"
        "   for (uint i = 0; i < ";
    src += ivsize;
    src += "; i++)\n"
           "   {\n"
           "       o.Position = mul(Proj[1], iv[i].Position + ViewOffset);\n"
           "       o.VPos = iv[i].Position;\n"
           "       o.Viewport = 1;\n";
    src += copyVaryings;
    src +=
        "       v.Append(o);\n"
        "   }\n"
        "   v.RestartStrip();\n"
        "}\n";

    pStereoShaders[prim] = *new GeomShader(this, CompileShader("gs_4_0", src.ToCStr()));
    return pStereoShaders[prim];
}

Fill* RenderDevice::CreateSimpleFill(int flags)
{
    OVR_UNUSED(flags);
    return DefaultFill;
}

// Textures

ID3D1xSamplerState* RenderDevice::GetSamplerState(int sm)
{
    if (SamplerStates[sm])    
        return SamplerStates[sm];

    D3D1x_(SAMPLER_DESC) ss;
    memset(&ss, 0, sizeof(ss));
    if (sm & Sample_Clamp)    
        ss.AddressU = ss.AddressV = ss.AddressW = D3D1x_(TEXTURE_ADDRESS_CLAMP);    
    else if (sm & Sample_ClampBorder)    
        ss.AddressU = ss.AddressV = ss.AddressW = D3D1x_(TEXTURE_ADDRESS_BORDER);    
    else    
        ss.AddressU = ss.AddressV = ss.AddressW = D3D1x_(TEXTURE_ADDRESS_WRAP);
    
    if (sm & Sample_Nearest)
    {
        ss.Filter = D3D1x_(FILTER_MIN_MAG_MIP_POINT);
    }
    else if (sm & Sample_Anisotropic)
    {
        ss.Filter = D3D1x_(FILTER_ANISOTROPIC);
        ss.MaxAnisotropy = 4;
    }
    else
    {
        ss.Filter = D3D1x_(FILTER_MIN_MAG_MIP_LINEAR);
    }
    ss.MaxLOD = 15;
    HRESULT hr = Device->CreateSamplerState(&ss, &SamplerStates[sm].GetRawRef());
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }
    return SamplerStates[sm];
}

Texture::Texture(RenderDevice* ren, int fmt, int w, int h) :
    Ren(ren),
    Tex(NULL),
    TexSv(NULL),
    TexRtv(NULL),
    TexDsv(NULL),
    TexStaging(NULL),
    Sampler(NULL),
    Format(fmt),
    Width(w),
    Height(h),
    Samples(0)
{
    Sampler = Ren->GetSamplerState(0);
}

void* Texture::GetInternalImplementation() 
{ 
	return Tex;
}

Texture::~Texture()
{
}

void Texture::Set(int slot, Render::ShaderStage stage) const
{
    Ren->SetTexture(stage, slot, this);
}

void Texture::SetSampleMode(int sm)
{
    Sampler = Ren->GetSamplerState(sm);
}

void RenderDevice::SetTexture(Render::ShaderStage stage, int slot, const Texture* t)
{
    if (MaxTextureSet[stage] <= slot)
        MaxTextureSet[stage] = slot + 1;    

    ID3D1xShaderResourceView* sv = t ? t->TexSv : NULL;
    switch(stage)
    {
    case Shader_Pixel:
        Context->PSSetShaderResources(slot, 1, &sv);
        if (t)
        {
            Context->PSSetSamplers(slot, 1, &t->Sampler.GetRawRef());
        }
        break;

    case Shader_Vertex:
        Context->VSSetShaderResources(slot, 1, &sv);
        if (t)
        {
            Context->VSSetSamplers(slot, 1, &t->Sampler.GetRawRef());
        }
        break;

#if (OVR_D3D_VERSION >= 11)
    case Shader_Compute:
        Context->CSSetShaderResources(slot, 1, &sv);
        if (t)
        {
            Context->CSSetSamplers(slot, 1, &t->Sampler.GetRawRef());
        }
        break;
#endif

    default:
        OVR_ASSERT ( false );
        break;

    }
}

void RenderDevice::GenerateSubresourceData(
    unsigned imageWidth, unsigned imageHeight, int format, unsigned imageDimUpperLimit,
    const void* rawBytes, D3D1x_(SUBRESOURCE_DATA)* subresData,
    unsigned& largestMipWidth, unsigned& largestMipHeight, unsigned& byteSize, unsigned& effectiveMipCount)
{
    largestMipWidth  = 0;
    largestMipHeight = 0;

    unsigned sliceLen   = 0;
    unsigned rowLen     = 0;
    unsigned numRows    = 0;
    const byte* mipBytes = static_cast<const byte*>(rawBytes);

    unsigned index        = 0;
    unsigned subresWidth  = imageWidth;
    unsigned subresHeight = imageHeight;
    unsigned numMips      = effectiveMipCount;

    unsigned bytesPerBlock = 0;
    switch(format)
    {
    case DXGI_FORMAT_BC1_UNORM_SRGB: // fall thru
    case DXGI_FORMAT_BC1_UNORM: bytesPerBlock = 8;  break;

    case DXGI_FORMAT_BC2_UNORM_SRGB: // fall thru
    case DXGI_FORMAT_BC2_UNORM: bytesPerBlock = 16;  break;

    case DXGI_FORMAT_BC3_UNORM_SRGB: // fall thru
    case DXGI_FORMAT_BC3_UNORM: bytesPerBlock = 16;  break;

    default:    OVR_ASSERT(false);
    }

    for(unsigned i = 0; i < numMips; i++)
    {

        unsigned blockWidth = 0;
        blockWidth = (subresWidth + 3) / 4;
        if (blockWidth < 1)
        {
            blockWidth = 1;
        }

        unsigned blockHeight = 0;
        blockHeight = (subresHeight + 3) / 4;
        if (blockHeight < 1)
        {
            blockHeight = 1;
        }

        rowLen = blockWidth * bytesPerBlock;
        numRows = blockHeight;
        sliceLen = rowLen * numRows;

        if (imageDimUpperLimit == 0 || (effectiveMipCount == 1) ||
            (subresWidth <= imageDimUpperLimit && subresHeight <= imageDimUpperLimit))
        {
            if(!largestMipWidth)
            {
                largestMipWidth = subresWidth;
                largestMipHeight = subresHeight;
            }

            subresData[index].pSysMem = (const void*)mipBytes;
            subresData[index].SysMemPitch = static_cast<UINT>(rowLen);
            subresData[index].SysMemSlicePitch = static_cast<UINT>(sliceLen);
            byteSize += sliceLen;
            ++index;
        }
        else
        {
            effectiveMipCount--;
        }

        mipBytes += sliceLen;

        subresWidth = subresWidth >> 1;
        subresHeight = subresHeight >> 1;
        if (subresWidth <= 0)
        {
            subresWidth = 1;
        }
        if (subresHeight <= 0)
        {
            subresHeight = 1;
        }
    }
}

#define _256Megabytes 268435456
#define _512Megabytes 536870912

Texture* RenderDevice::CreateTexture(int format, int width, int height, const void* data, int mipcount)
{
    OVR_ASSERT(Device != NULL);
    
    size_t gpuMemorySize = 0;
    {
        IDXGIDevice* pDXGIDevice;
        HRESULT hr = Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
        }
        IDXGIAdapter * pDXGIAdapter;
        hr = pDXGIDevice->GetAdapter(&pDXGIAdapter);
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
        }
        DXGI_ADAPTER_DESC adapterDesc;
        hr = pDXGIAdapter->GetDesc(&adapterDesc);
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
        }
        gpuMemorySize = adapterDesc.DedicatedVideoMemory;
        pDXGIAdapter->Release();
        pDXGIDevice->Release();
    }
 
    unsigned imageDimUpperLimit = 0;
    if (gpuMemorySize <= _256Megabytes)
    {
        imageDimUpperLimit = 512;
    }    
    else if (gpuMemorySize <= _512Megabytes)
    {
        imageDimUpperLimit = 1024;
    } 

	if ((format & Texture_TypeMask) == Texture_DXT1 ||
        (format & Texture_TypeMask) == Texture_DXT3 ||
        (format & Texture_TypeMask) == Texture_DXT5)
    {
		int convertedFormat;
        if((format & Texture_TypeMask) == Texture_DXT1)
        {
            convertedFormat = (format & Texture_SRGB) ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM;
        }
        else if((format & Texture_TypeMask) == Texture_DXT3)
        {
            convertedFormat = (format & Texture_SRGB) ? DXGI_FORMAT_BC2_UNORM_SRGB : DXGI_FORMAT_BC2_UNORM;
        }
		else if((format & Texture_TypeMask) == Texture_DXT5)
        {
            convertedFormat = (format & Texture_SRGB) ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;
        }
        else
        {
            OVR_ASSERT(false);  return NULL;
        }

        unsigned largestMipWidth   = 0;
        unsigned largestMipHeight  = 0;
        unsigned effectiveMipCount = mipcount;
        unsigned textureSize       = 0;

        D3D1x_(SUBRESOURCE_DATA)* subresData = (D3D1x_(SUBRESOURCE_DATA)*)
                                                OVR_ALLOC(sizeof(D3D1x_(SUBRESOURCE_DATA)) * mipcount);
        GenerateSubresourceData(width, height, convertedFormat, imageDimUpperLimit, data, subresData, largestMipWidth,
                                largestMipHeight, textureSize, effectiveMipCount);
        TotalTextureMemoryUsage += textureSize;

        if (!Device || !subresData)
        {
            return NULL;
        }

        Texture* NewTex = new Texture(this, format, largestMipWidth, largestMipHeight);
        // BCn/DXTn - no AA.
        NewTex->Samples = 1;

        D3D1x_(TEXTURE2D_DESC) desc;
        desc.Width      = largestMipWidth;
        desc.Height     = largestMipHeight;
        desc.MipLevels  = effectiveMipCount;
        desc.ArraySize  = 1;
        desc.Format     = static_cast<DXGI_FORMAT>(convertedFormat);
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage      = D3D1x_(USAGE_DEFAULT);
        desc.BindFlags  = D3D1x_(BIND_SHADER_RESOURCE);
        desc.CPUAccessFlags = 0;
        desc.MiscFlags  = 0;

        NewTex->Tex = NULL;
        HRESULT hr = Device->CreateTexture2D(&desc, static_cast<D3D1x_(SUBRESOURCE_DATA)*>(subresData),
                                             &NewTex->Tex.GetRawRef());
        OVR_FREE(subresData);
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
        }

        if (SUCCEEDED(hr) && NewTex != 0)
        {
            D3D1x_(SHADER_RESOURCE_VIEW_DESC) SRVDesc;
            memset(&SRVDesc, 0, sizeof(SRVDesc));
            SRVDesc.Format = static_cast<DXGI_FORMAT>(format);
            SRVDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = desc.MipLevels;

            NewTex->TexSv = NULL;
            hr = Device->CreateShaderResourceView(NewTex->Tex, NULL, &NewTex->TexSv.GetRawRef());

            if (FAILED(hr))
            {
                OVR_LOG_COM_ERROR(hr);
                NewTex->Release();
                return NULL;
            }
            return NewTex;
        }

        return NULL;
    }
    else
    {
        int samples = (format & Texture_SamplesMask);
        if (samples < 1)
        {
            samples = 1;
        }

        bool createDepthSrv = (format & Texture_SampleDepth) > 0;

        DXGI_FORMAT d3dformat;
        int         bpp;
        switch(format & Texture_TypeMask)
        {
		case Texture_BGRA:
			bpp = 4;
			d3dformat = (format & Texture_SRGB) ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
			break;
        case Texture_RGBA:
            bpp = 4;
			d3dformat = (format & Texture_SRGB) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        case Texture_R:
            bpp = 1;
            d3dformat = DXGI_FORMAT_R8_UNORM;
            break;
		case Texture_A:
			bpp = 1;
			d3dformat = DXGI_FORMAT_A8_UNORM;
			break;
        case Texture_Depth:
            bpp = 0;
            d3dformat = createDepthSrv ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_D32_FLOAT;
            break;
        default:
            OVR_ASSERT(false);
            return NULL;
        }

        Texture* NewTex = new Texture(this, format, width, height);
        NewTex->Samples = samples;

        D3D1x_(TEXTURE2D_DESC) dsDesc;
		dsDesc.Width     = width;
		dsDesc.Height    = height;
        dsDesc.MipLevels = (format == (Texture_RGBA | Texture_GenMipmaps) && data) ? GetNumMipLevels(width, height) : 1;
        dsDesc.ArraySize = 1;
        dsDesc.Format    = d3dformat;
		dsDesc.SampleDesc.Count = samples;
        dsDesc.SampleDesc.Quality = 0;
        dsDesc.Usage     = D3D1x_(USAGE_DEFAULT);
        dsDesc.BindFlags = D3D1x_(BIND_SHADER_RESOURCE);
        dsDesc.CPUAccessFlags = 0;
        dsDesc.MiscFlags      = 0;

        if (format & Texture_RenderTarget)
        {
            if ((format & Texture_TypeMask) == Texture_Depth)
            {
                dsDesc.BindFlags = createDepthSrv ? (dsDesc.BindFlags | D3D1x_(BIND_DEPTH_STENCIL)) : D3D1x_(BIND_DEPTH_STENCIL);
            }
            else
            {
                dsDesc.BindFlags |= D3D1x_(BIND_RENDER_TARGET);
            }
        }

        NewTex->Tex = NULL;
        HRESULT hr = Device->CreateTexture2D(&dsDesc, NULL, &NewTex->Tex.GetRawRef());
        if (FAILED(hr))
        {
            OVR_LOG_COM_ERROR(hr);
            OVR_DEBUG_LOG_TEXT(("Failed to create 2D D3D texture."));
            NewTex->Release();
            return NULL;
        }
        if (dsDesc.BindFlags & D3D1x_(BIND_SHADER_RESOURCE))
        {
            if((dsDesc.BindFlags & D3D1x_(BIND_DEPTH_STENCIL)) > 0 && createDepthSrv)
            {
                D3D1x_(SHADER_RESOURCE_VIEW_DESC) depthSrv;
                depthSrv.Format = DXGI_FORMAT_R32_FLOAT;
				depthSrv.ViewDimension = samples > 1 ? D3D1x_(SRV_DIMENSION_TEXTURE2DMS) : D3D1x_(SRV_DIMENSION_TEXTURE2D);
                depthSrv.Texture2D.MostDetailedMip = 0;
                depthSrv.Texture2D.MipLevels = dsDesc.MipLevels;
                NewTex->TexSv = NULL;
                hr = Device->CreateShaderResourceView(NewTex->Tex, &depthSrv, &NewTex->TexSv.GetRawRef());
                if (FAILED(hr))
                {
                    OVR_LOG_COM_ERROR(hr);
                }
            }
            else
            {
                NewTex->TexSv = NULL;
                hr = Device->CreateShaderResourceView(NewTex->Tex, NULL, &NewTex->TexSv.GetRawRef());
                if (FAILED(hr))
                {
                    OVR_LOG_COM_ERROR(hr);
                }
            }
        }

        if (data)
        {
            Context->UpdateSubresource(NewTex->Tex, 0, NULL, data, width * bpp, width * height * bpp);
            if (format == (Texture_RGBA | Texture_GenMipmaps))
            {
                int srcw = width, srch = height;
                int level = 0;
                uint8_t* mipmaps = NULL;
                do
                {
                    level++;
                    int mipw = srcw >> 1;
                    if (mipw < 1)
                    {
                        mipw = 1;
                    }
                    int miph = srch >> 1;
                    if (miph < 1)
                    {
                        miph = 1;
                    }
                    if (mipmaps == NULL)
                    {
                        mipmaps = (uint8_t*)OVR_ALLOC(mipw * miph * 4);
                    }
                    FilterRgba2x2(level == 1 ? (const uint8_t*)data : mipmaps, srcw, srch, mipmaps);
                    Context->UpdateSubresource(NewTex->Tex, level, NULL, mipmaps, mipw * bpp, miph * bpp);
                    srcw = mipw;
                    srch = miph;
                }
                while(srcw > 1 || srch > 1);

                if (mipmaps != NULL)
                {
                    OVR_FREE(mipmaps);
                }
            }
        }

        if (format & Texture_RenderTarget)
        {
            if ((format & Texture_TypeMask) == Texture_Depth)
            {
                D3D1x_(DEPTH_STENCIL_VIEW_DESC) depthDsv;
                ZeroMemory(&depthDsv, sizeof(depthDsv));
                depthDsv.Format = DXGI_FORMAT_D32_FLOAT;
                depthDsv.ViewDimension = samples > 1 ? D3D1x_(DSV_DIMENSION_TEXTURE2DMS) : D3D1x_(DSV_DIMENSION_TEXTURE2D);
                depthDsv.Texture2D.MipSlice = 0;
                NewTex->TexDsv = NULL;
                hr = Device->CreateDepthStencilView(NewTex->Tex, createDepthSrv ? &depthDsv : NULL, &NewTex->TexDsv.GetRawRef());
                if (FAILED(hr))
                {
                    OVR_LOG_COM_ERROR(hr);
                }
            }
            else
            {
                NewTex->TexRtv = NULL;
                hr = Device->CreateRenderTargetView(NewTex->Tex, NULL, &NewTex->TexRtv.GetRawRef());
                if (FAILED(hr))
                {
                    OVR_LOG_COM_ERROR(hr);
                }
            }
        }

        return NewTex;
    }
}

// Rendering

void RenderDevice::ResolveMsaa(OVR::Render::Texture* msaaTex, OVR::Render::Texture* outputTex)
{
    int isSrgb = ((Texture*)msaaTex)->Format & Texture_SRGB;

    Context->ResolveSubresource(((Texture*)outputTex)->Tex, 0, ((Texture*)msaaTex)->Tex, 0, isSrgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM);
}

void RenderDevice::BeginRendering()
{
    Context->RSSetState(Rasterizer);
}

void RenderDevice::SetRenderTarget(Render::Texture* color, Render::Texture* depth, Render::Texture* stencil)
{
    OVR_UNUSED(stencil);

    CurRenderTarget = (Texture*)color;
    if (color == NULL)
    {
        Texture* newDepthBuffer = GetDepthBuffer(WindowWidth, WindowHeight, Params.Multisample);
        if (newDepthBuffer == NULL)
        {
            OVR_DEBUG_LOG(("New depth buffer creation failed."));
        }
        if (newDepthBuffer != NULL)
        {
            CurDepthBuffer = GetDepthBuffer(WindowWidth, WindowHeight, Params.Multisample);
            Context->OMSetRenderTargets(1, &BackBufferRT.GetRawRef(), CurDepthBuffer->TexDsv);
        }
        return;
    }
    if (depth == NULL)
    {
        depth = GetDepthBuffer(color->GetWidth(), color->GetHeight(), CurRenderTarget->Samples);
    }

    ID3D1xShaderResourceView* sv[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    if (MaxTextureSet[Shader_Fragment])
    {
        Context->PSSetShaderResources(0, MaxTextureSet[Shader_Fragment], sv);
    }
    memset(MaxTextureSet, 0, sizeof(MaxTextureSet));

    CurDepthBuffer = (Texture*)depth;
    Context->OMSetRenderTargets(1, &((Texture*)color)->TexRtv.GetRawRef(), ((Texture*)depth)->TexDsv);
}

void RenderDevice::SetWorldUniforms(const Matrix4f& proj)
{
    StdUniforms.Proj = proj.Transposed();
    // Shader constant buffers cannot be partially updated.
}


void RenderDevice::Render(const Matrix4f& matrix, Model* model)
{
    // Store data in buffers if not already
    if (!model->VertexBuffer)
    {
        Ptr<Buffer> vb = *CreateBuffer();
        if (!vb->Data(Buffer_Vertex | Buffer_ReadOnly, &model->Vertices[0], model->Vertices.GetSize() * sizeof(Vertex)))
        {
            OVR_ASSERT(false);
        }
        model->VertexBuffer = vb;
    }
    if (!model->IndexBuffer)
    {
        Ptr<Buffer> ib = *CreateBuffer();
        if (!ib->Data(Buffer_Index | Buffer_ReadOnly, &model->Indices[0], model->Indices.GetSize() * 2))
        {
            OVR_ASSERT(false);
        }
        model->IndexBuffer = ib;
    }

    Render(model->Fill ? model->Fill : DefaultFill,
           model->VertexBuffer, model->IndexBuffer,
           matrix, 0, (unsigned)model->Indices.GetSize(), model->GetPrimType());
}

void RenderDevice::RenderWithAlpha(	const Fill* fill, Render::Buffer* vertices, Render::Buffer* indices,
									const Matrix4f& matrix, int offset, int count, PrimitiveType rprim)
{
	Context->OMSetBlendState(BlendState, NULL, 0xffffffff);
    Render(fill, vertices, indices, matrix, offset, count, rprim);
    Context->OMSetBlendState(NULL, NULL, 0xffffffff);
}

void RenderDevice::Render(const Fill* fill, Render::Buffer* vertices, Render::Buffer* indices,
                          const Matrix4f& matrix, int offset, int count, PrimitiveType rprim, MeshType meshType/* = Mesh_Scene*/)
{
    ID3D1xBuffer* vertexBuffer = ((Buffer*)vertices)->GetBuffer();
    UINT vertexOffset = offset;
    UINT vertexStride = sizeof(Vertex);
    switch(meshType)
    {
    case Mesh_Scene:
        Context->IASetInputLayout(ModelVertexIL);
        vertexStride = sizeof(Vertex);
        break;
    case Mesh_Distortion:
        Context->IASetInputLayout(DistortionVertexIL);
        vertexStride = sizeof(DistortionVertex);
        break;
    case Mesh_Heightmap:
        Context->IASetInputLayout(HeightmapVertexIL);
        vertexStride = sizeof(HeightmapVertex);
        break;
    default: OVR_ASSERT(false);
    }

    Context->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);

    if (indices)
    {
        Context->IASetIndexBuffer(((Buffer*)indices)->GetBuffer(), DXGI_FORMAT_R16_UINT, 0);
    }

    ShaderSet* shaders = ((ShaderFill*)fill)->GetShaders();

    ShaderBase* vshader = ((ShaderBase*)shaders->GetShader(Shader_Vertex));
    unsigned char* vertexData = vshader->UniformData;
    if ( vertexData != NULL )
    {
        // TODO: some VSes don't start with StandardUniformData!
        if ( vshader->UniformsSize >= sizeof(StandardUniformData) )
        {
            StandardUniformData* stdUniforms = (StandardUniformData*) vertexData;
            stdUniforms->View = matrix.Transposed();
            stdUniforms->Proj = StdUniforms.Proj;
        }

        if (!UniformBuffers[Shader_Vertex]->Data(Buffer_Uniform, vertexData, vshader->UniformsSize))
        {
            OVR_ASSERT(false);
        }
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

    D3D1x_(PRIMITIVE_TOPOLOGY) prim;
    switch(rprim)
    {
    case Prim_Triangles:
        prim = D3D1x_(PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        break;
    case Prim_Lines:
        prim = D3D1x_(PRIMITIVE_TOPOLOGY_LINELIST);
        break;
    case Prim_TriangleStrip:
        prim = D3D1x_(PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        break;
    default:
        OVR_ASSERT(0);
        return;
    }
    Context->IASetPrimitiveTopology(prim);

    fill->Set(rprim);
    if (ExtraShaders)
    {
        ExtraShaders->Set(rprim);
    }

    if (indices)
    {
        Context->DrawIndexed(count, 0, 0);
    }
    else
    {
        Context->Draw(count, 0);
    }
}


// This is far less generic than the name suggests - very hard-coded to the distortion CSes.
void RenderDevice::RenderCompute(const Fill* fill, Render::Buffer* buffer, int invocationSizeInPixels )
{
#if (OVR_D3D_VERSION >= 11)
    //Context->CSCSSetShaderResources
    //Context->CSSetUnorderedAccessViews
    //Context->CSSetShader
    //Context->CSSetSamplers
    //Context->CSSetConstantBuffers

    ShaderSet* shaders = ((ShaderFill*)fill)->GetShaders();
    ShaderBase* cshader = ((ShaderBase*)shaders->GetShader(Shader_Compute));

    ID3D1xUnorderedAccessView *uavRendertarget = BackBufferUAV.GetRawRef();
    int SizeX = WindowWidth/2;
    int SizeY = WindowHeight;
    if (CurRenderTarget != NULL)
    {
        OVR_ASSERT ( !"write me" );
        uavRendertarget = NULL; //CurRenderTarget->TexUav.GetRawRef();
        SizeX = CurRenderTarget->GetWidth() / 2;
        SizeY = CurRenderTarget->GetHeight()   ;
    }

    int TileNumX = ( SizeX + (invocationSizeInPixels-1) ) / invocationSizeInPixels;
    int TileNumY = ( SizeY + (invocationSizeInPixels-1) ) / invocationSizeInPixels;

    Context->CSSetUnorderedAccessViews ( 0, 1, &uavRendertarget, NULL );
    if ( buffer != NULL )
    {
        // Incoming eye-buffer textures start at t0 onwards, so set this in slot #4
        // Subtlety - can't put this in slot 0 because fill->Set stops at the first NULL texture.
        ID3D1xShaderResourceView *d3dSrv = ((Buffer*)buffer)->GetSrv();
        Context->CSSetShaderResources ( 4, 1, &d3dSrv );
    }

    // TODO: uniform/constant buffers
    cshader->UpdateBuffer(UniformBuffers[Shader_Compute]);
    cshader->SetUniformBuffer(UniformBuffers[Shader_Compute]);

    // Primitive type is ignored for CS.
    // This call actually sets the textures and does Context->CSSetShader(). Primitive type is ignored.
    fill->Set ( Prim_Unknown );

    Context->Dispatch ( TileNumX, TileNumY, 1 );
#else
    OVR_ASSERT ( !"No compute shaders on DX10" );
    OVR_UNUSED ( fill );
    OVR_UNUSED ( buffer );
    OVR_UNUSED ( invocationSizeInPixels );
#endif
}


size_t RenderDevice::QueryGPUMemorySize()
{
	IDXGIDevice* pDXGIDevice;
	HRESULT hr = Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }
	IDXGIAdapter * pDXGIAdapter;
	hr = pDXGIDevice->GetAdapter(&pDXGIAdapter);
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }
    DXGI_ADAPTER_DESC adapterDesc;
	hr = pDXGIAdapter->GetDesc(&adapterDesc);
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }

	pDXGIAdapter->Release();
	pDXGIDevice->Release();
    return adapterDesc.DedicatedVideoMemory;
}


void RenderDevice::Present ( bool withVsync )
{
	for( int i = 0; i < 4; ++i )
	{
		if( OVR::Util::ImageWindow::GlobalWindow( i ) )
		{
			OVR::Util::ImageWindow::GlobalWindow( i )->Process();
		}
	}

    HRESULT hr;
    if ( withVsync )
    {
        hr = SwapChain->Present(1, 0);
    }
    else
    {
        // Immediate present
        hr = SwapChain->Present(0, 0);
    }
    if (FAILED(hr))
    {
        OVR_LOG_COM_ERROR(hr);
    }
}

void RenderDevice::Flush()
{
    Context->Flush();
}

void RenderDevice::WaitUntilGpuIdle()
{
#if 0
	// If enabling this option and using an NVIDIA GPU,
	// then make sure your "max pre-rendered frames" is set to 1 under the NVIDIA GPU settings.

	// Flush GPU data and don't stall CPU waiting for GPU to complete
	Context->Flush();
#else
	// Flush and Stall CPU while waiting for GPU to complete rendering all of the queued draw calls
    D3D1x_QUERY_DESC queryDesc = { D3D1x_(QUERY_EVENT), 0 };
    Ptr<ID3D1xQuery> query;
    BOOL             done = FALSE;

    if (Device->CreateQuery(&queryDesc, &query.GetRawRef()) == S_OK)
    {
#if (OVR_D3D_VERSION == 10)
        // Begin() not used for EVENT query.
        query->End();
        // GetData will returns S_OK for both done == TRUE or FALSE.
        // Exit on failure to avoid infinite loop.
        do { }
        while(!done && !FAILED(query->GetData(&done, sizeof(BOOL), 0)));
#else
        Context->End(query);
        do { }
        while(!done && !FAILED(Context->GetData(query, &done, sizeof(BOOL), 0)));
#endif
    }

#endif
}

void RenderDevice::FillRect(float left, float top, float right, float bottom, Color c, const Matrix4f* view)
{
    Context->OMSetBlendState(BlendState, NULL, 0xffffffff);
    OVR::Render::RenderDevice::FillRect(left, top, right, bottom, c, view);
    Context->OMSetBlendState(NULL, NULL, 0xffffffff);
}

void RenderDevice::FillGradientRect(float left, float top, float right, float bottom, Color col_top, Color col_btm, const Matrix4f* view)
{
    Context->OMSetBlendState(BlendState, NULL, 0xffffffff);
    OVR::Render::RenderDevice::FillGradientRect(left, top, right, bottom, col_top, col_btm, view);
    Context->OMSetBlendState(NULL, NULL, 0xffffffff);
}

void RenderDevice::RenderText(const struct Font* font, const char* str, float x, float y, float size, Color c, const Matrix4f* view)
{
	Context->OMSetBlendState(BlendState, NULL, 0xffffffff);
	OVR::Render::RenderDevice::RenderText(font, str, x, y, size, c, view);
	Context->OMSetBlendState(NULL, NULL, 0xffffffff);
}

void RenderDevice::RenderImage(float left, float top, float right, float bottom, ShaderFill* image, unsigned char alpha, const Matrix4f* view)
{
    Context->OMSetBlendState(BlendState, NULL, 0xffffffff);
    OVR::Render::RenderDevice::RenderImage(left, top, right, bottom, image, alpha, view);
    Context->OMSetBlendState(NULL, NULL, 0xffffffff);
}

void RenderDevice::BeginGpuEvent(const char* markerText, uint32_t markerColor)
{
#if GPU_PROFILING
    WCHAR wStr[255];
    size_t newStrLen = 0;
    mbstowcs_s(&newStrLen, wStr, markerText, 255);
    LPCWSTR pwStr = wStr;

    D3DPERF_BeginEvent(markerColor, pwStr);
#else
    OVR_UNUSED(markerText);
    OVR_UNUSED(markerColor);
#endif
}

void RenderDevice::EndGpuEvent()
{
#if GPU_PROFILING
    D3DPERF_EndEvent();
#endif
}

}}}

