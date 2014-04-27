/************************************************************************************

Filename    :   Render_GL_Device.cpp
Content     :   RenderDevice implementation for OpenGL
Created     :   September 10, 2012
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

#include "../Render/Render_GL_Device.h"
#include "Kernel/OVR_Log.h"
#include "OVR_CAPI_GL.h"

namespace OVR { namespace Render { namespace GL {

void InitGLExtensions()
{
  glewExperimental = GL_TRUE;
  glewInit();
}


static const char* StdVertexShaderSrc =
    "uniform mat4 Proj;\n"
    "uniform mat4 View;\n"
    "attribute vec4 Position;\n"
    "attribute vec4 Color;\n"
    "attribute vec2 TexCoord;\n"
    "attribute vec2 TexCoord1;\n"
    "attribute vec3 Normal;\n"
    "varying  vec4 oColor;\n"
    "varying  vec2 oTexCoord;\n"
    "varying  vec2 oTexCoord1;\n"
    "varying  vec3 oNormal;\n"
    "varying  vec3 oVPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = Proj * (View * Position);\n"
    "   oNormal = vec3(View * vec4(Normal,0));\n"
    "   oVPos = vec3(View * Position);\n"
    "   oTexCoord = TexCoord;\n"
    "   oTexCoord1 = TexCoord1;\n"
    "   oColor = Color;\n"
    "}\n";

static const char* DirectVertexShaderSrc =
    "uniform mat4 View;\n"
    "attribute vec4 Position;\n"
    "attribute vec4 Color;\n"
    "attribute vec2 TexCoord;\n"
    "attribute vec3 Normal;\n"
    "varying  vec4 oColor;\n"
    "varying  vec2 oTexCoord;\n"
    "varying  vec3 oNormal;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = View * Position;\n"
    "   oTexCoord = TexCoord;\n"
    "   oColor = Color;\n"
    "   oNormal = vec3(View * vec4(Normal,0));\n"
    "}\n";

static const char* SolidFragShaderSrc =
    "uniform vec4 Color;\n"
    "void main()\n"
    "{\n"
    "   gl_FragColor = Color;\n"
    "}\n";

static const char* GouraudFragShaderSrc =
    "varying vec4 oColor;\n"
    "void main()\n"
    "{\n"
    "   gl_FragColor = oColor;\n"
    "}\n";

static const char* TextureFragShaderSrc =
    "uniform sampler2D Texture0;\n"
    "varying vec4 oColor;\n"
    "varying vec2 oTexCoord;\n"
    "void main()\n"
    "{\n"
    "   gl_FragColor = oColor * texture2D(Texture0, oTexCoord);\n"
    "   if (gl_FragColor.a < 0.4)\n"
    "       discard;\n"
    "}\n";

#define LIGHTING_COMMON                                                 \
    "uniform   vec3 Ambient;\n"                                               \
    "uniform   vec4 LightPos[8];\n"                                           \
    "uniform   vec4 LightColor[8];\n"                                         \
    "uniform   float LightCount;\n"                                          \
    "varying   vec4 oColor;\n"                                                  \
    "varying   vec2 oTexCoord;\n"                                               \
    "varying   vec3 oNormal;\n"                                                 \
    "varying   vec3 oVPos;\n"                                                   \
    "vec4 DoLight()\n"                                        \
    "{\n"                                                               \
    "   vec3 norm = normalize(oNormal);\n"                             \
    "   vec3 light = Ambient;\n"                                        \
    "   for (int i = 0; i < int(LightCount); i++)\n"                \
    "   {\n"                                                            \
    "       vec3 ltp = (LightPos[i].xyz - oVPos);\n"              \
    "       float  ldist = length(ltp);\n"                             \
    "       ltp = normalize(ltp);\n"                             \
    "       light += clamp(LightColor[i].rgb * oColor.rgb * (dot(norm, ltp) / ldist), 0.0,1.0);\n" \
    "   }\n"                                                            \
    "   return vec4(light, oColor.a);\n"                               \
    "}\n"

static const char* LitSolidFragShaderSrc =
    LIGHTING_COMMON
    "void main()\n"
    "{\n"
    "   gl_FragColor = DoLight() * oColor;\n"
    "}\n";

static const char* LitTextureFragShaderSrc =
    "uniform sampler2D Texture0;\n"
    LIGHTING_COMMON
    "void main()\n"
    "{\n"
    "   gl_FragColor = DoLight() * texture2D(Texture0, oTexCoord);\n"
    "}\n";

static const char* AlphaTextureFragShaderSrc =
    "uniform sampler2D Texture0;\n"
    "varying vec4 oColor;\n"
    "varying vec2 oTexCoord;\n"
    "void main()\n"
    "{\n"
    "   gl_FragColor = oColor * vec4(1,1,1,texture2D(Texture0, oTexCoord).a);\n"
    "}\n";

static const char* MultiTextureFragShaderSrc =
    "uniform sampler2D Texture0;\n"
    "uniform sampler2D Texture1;\n"
    "varying vec4 oColor;\n"
    "varying vec2 oTexCoord;\n"
    "varying vec2 oTexCoord1;\n"
    "void main()\n"
    "{\n"
	"	vec4 color1 = texture2D(Texture0, oTexCoord);\n"
	"	vec4 color2 = texture2D(Texture1, oTexCoord1);\n"
	"	color2.rgb = color2.rgb * mix(1.9, 1.2, clamp(length(color2.rgb),0.0,1.0));\n"
	"	color2 = color1 * color2;\n"
	"   if (color2.a <= 0.6)\n"
	"		discard;\n"
	"	gl_FragColor = color2;\n"
    "}\n";

static const char* PostProcessMeshFragShaderSrc =
    "uniform sampler2D Texture;\n"
    "varying vec4 oColor;\n"
    "varying vec2 oTexCoord0;\n"
    "varying vec2 oTexCoord1;\n"
    "varying vec2 oTexCoord2;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   float ResultR = texture2D(Texture, oTexCoord0).r;\n"
    "   float ResultG = texture2D(Texture, oTexCoord1).g;\n"
    "   float ResultB = texture2D(Texture, oTexCoord2).b;\n"
    "   gl_FragColor = vec4(ResultR * oColor.r, ResultG * oColor.g, ResultB * oColor.b, 1.0);\n"
    "}\n";

static const char* PostProcessMeshTimewarpFragShaderSrc =
    "uniform sampler2D Texture;\n"
    "varying vec4 oColor;\n"
    "varying vec2 oTexCoord0;\n"
    "varying vec2 oTexCoord1;\n"
    "varying vec2 oTexCoord2;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   float ResultR = texture2D(Texture, oTexCoord0).r;\n"
    "   float ResultG = texture2D(Texture, oTexCoord1).g;\n"
    "   float ResultB = texture2D(Texture, oTexCoord2).b;\n"
    "   gl_FragColor = vec4(ResultR * oColor.r, ResultG * oColor.g, ResultB * oColor.b, 1.0);\n"
    "}\n";

static const char* PostProcessMeshPositionalTimewarpFragShaderSrc =
    "uniform sampler2D Texture0;\n"
    "uniform sampler2D Texture1;\n"
    "varying vec4 oColor;\n"
    "varying vec2 oTexCoord0;\n"
    "varying vec2 oTexCoord1;\n"
    "varying vec2 oTexCoord2;\n"
    "\n"
    "void main()\n"
    "{\n"
	"   gl_FragColor.r = oColor.r * texture2D(Texture1, oTexCoord0).r;\n"
    "   gl_FragColor.g = oColor.g * texture2D(Texture1, oTexCoord1).g;\n"
    "   gl_FragColor.b = oColor.b * texture2D(Texture1, oTexCoord2).b;\n"
    "   gl_FragColor.a = 1.0;\n"
    "}\n";


static const char* PostProcessVertexShaderSrc =
    "uniform mat4 View;\n"
    "uniform mat4 Texm;\n"
    "attribute vec4 Position;\n"
    "attribute vec2 TexCoord;\n"
    "varying  vec2 oTexCoord;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = View * Position;\n"
    "   oTexCoord = vec2(Texm * vec4(TexCoord,0,1));\n"
    "}\n";

static const char* PostProcessMeshVertexShaderSrc =
    "uniform vec2 EyeToSourceUVScale;\n"
    "uniform vec2 EyeToSourceUVOffset;\n"

    "attribute vec2 Position;\n"
    "attribute vec4 Color;\n"
    "attribute vec2 TexCoord0;\n"
    "attribute vec2 TexCoord1;\n"
    "attribute vec2 TexCoord2;\n"

    "varying vec4 oColor;\n"
    "varying vec2 oTexCoord0;\n"
    "varying vec2 oTexCoord1;\n"
    "varying vec2 oTexCoord2;\n"

    "void main()\n"
    "{\n"
    "   gl_Position.x = Position.x;\n"
    "   gl_Position.y = Position.y;\n"
    "   gl_Position.z = 0.5;\n"
    "   gl_Position.w = 1.0;\n"
    // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
    // Scale them into the correct [0-1],[0-1] UV lookup space (depending on eye)
    "   oTexCoord0 = TexCoord0 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord0.y = 1-oTexCoord0.y;\n"
    "   oTexCoord1 = TexCoord1 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord1.y = 1-oTexCoord1.y;\n"
    "   oTexCoord2 = TexCoord2 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord2.y = 1-oTexCoord2.y;\n"
    "   oColor = Color;\n"              // Used for vignette fade.
    "}\n";

static const char* PostProcessMeshTimewarpVertexShaderSrc =
    "uniform vec2 EyeToSourceUVScale;\n"
    "uniform vec2 EyeToSourceUVOffset;\n"
    "uniform mat4 EyeRotationStart;\n"
    "uniform mat4 EyeRotationEnd;\n"

    "attribute vec2 Position;\n"
    "attribute vec4 Color;\n"
    "attribute vec2 TexCoord0;\n"
    "attribute vec2 TexCoord1;\n"
    "attribute vec2 TexCoord2;\n"

    "varying vec4 oColor;\n"
    "varying vec2 oTexCoord0;\n"
    "varying vec2 oTexCoord1;\n"
    "varying vec2 oTexCoord2;\n"

    "void main()\n"
    "{\n"
    "   gl_Position.x = Position.x;\n"
    "   gl_Position.y = Position.y;\n"
    "   gl_Position.z = 0.0;\n"
    "   gl_Position.w = 1.0;\n"

    // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
    // These are now "real world" vectors in direction (x,y,1) relative to the eye of the HMD.
    "   vec3 TanEyeAngleR = vec3 ( TexCoord0.x, TexCoord0.y, 1.0 );\n"
    "   vec3 TanEyeAngleG = vec3 ( TexCoord1.x, TexCoord1.y, 1.0 );\n"
    "   vec3 TanEyeAngleB = vec3 ( TexCoord2.x, TexCoord2.y, 1.0 );\n"

    // Accurate time warp lerp vs. faster
#if 1
    // Apply the two 3x3 timewarp rotations to these vectors.
	"   vec3 TransformedRStart = (EyeRotationStart * vec4(TanEyeAngleR, 0)).xyz;\n"
	"   vec3 TransformedGStart = (EyeRotationStart * vec4(TanEyeAngleG, 0)).xyz;\n"
	"   vec3 TransformedBStart = (EyeRotationStart * vec4(TanEyeAngleB, 0)).xyz;\n"
	"   vec3 TransformedREnd   = (EyeRotationEnd * vec4(TanEyeAngleR, 0)).xyz;\n"
	"   vec3 TransformedGEnd   = (EyeRotationEnd * vec4(TanEyeAngleG, 0)).xyz;\n"
	"   vec3 TransformedBEnd   = (EyeRotationEnd * vec4(TanEyeAngleB, 0)).xyz;\n"
    // And blend between them.
    "   vec3 TransformedR = mix ( TransformedRStart, TransformedREnd, Color.a );\n"
    "   vec3 TransformedG = mix ( TransformedGStart, TransformedGEnd, Color.a );\n"
    "   vec3 TransformedB = mix ( TransformedBStart, TransformedBEnd, Color.a );\n"
#else
    "   mat3 EyeRotation = mix ( EyeRotationStart, EyeRotationEnd, Color.a );\n"
    "   vec3 TransformedR   = EyeRotation * TanEyeAngleR;\n"
    "   vec3 TransformedG   = EyeRotation * TanEyeAngleG;\n"
    "   vec3 TransformedB   = EyeRotation * TanEyeAngleB;\n"
#endif

    // Project them back onto the Z=1 plane of the rendered images.
    "   float RecipZR = 1.0 / TransformedR.z;\n"
    "   float RecipZG = 1.0 / TransformedG.z;\n"
    "   float RecipZB = 1.0 / TransformedB.z;\n"
    "   vec2 FlattenedR = vec2 ( TransformedR.x * RecipZR, TransformedR.y * RecipZR );\n"
    "   vec2 FlattenedG = vec2 ( TransformedG.x * RecipZG, TransformedG.y * RecipZG );\n"
    "   vec2 FlattenedB = vec2 ( TransformedB.x * RecipZB, TransformedB.y * RecipZB );\n"

    // These are now still in TanEyeAngle space.
    // Scale them into the correct [0-1],[0-1] UV lookup space (depending on eye)
    "   vec2 SrcCoordR = FlattenedR * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   vec2 SrcCoordG = FlattenedG * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   vec2 SrcCoordB = FlattenedB * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord0 = SrcCoordR;\n"
    "   oTexCoord0.y = 1-oTexCoord0.y;\n"
    "   oTexCoord1 = SrcCoordG;\n"
    "   oTexCoord1.y = 1-oTexCoord1.y;\n"
    "   oTexCoord2 = SrcCoordB;\n"
    "   oTexCoord2.y = 1-oTexCoord2.y;\n"
    "   oColor = Color.r;\n"              // Used for vignette fade.
    "}\n";

static const char* PostProcessMeshPositionalTimewarpVertexShaderSrc =
    "#version 150\n"
	"uniform sampler2D Texture0;\n"
    "uniform vec2 EyeToSourceUVScale;\n"
    "uniform vec2 EyeToSourceUVOffset;\n"
	"uniform vec2 DepthProjector;\n"
	"uniform vec2 DepthDimSize;\n"
	"uniform mat4 EyeRotationStart;\n"
    "uniform mat4 EyeRotationEnd;\n"

    "in vec2 Position;\n"
    "in vec4 Color;\n"
    "in vec2 TexCoord0;\n"
    "in vec2 TexCoord1;\n"
    "in vec2 TexCoord2;\n"

    "out vec4 oColor;\n"
    "out vec2 oTexCoord0;\n"
    "out vec2 oTexCoord1;\n"
    "out vec2 oTexCoord2;\n"

    "vec4 PositionFromDepth(vec2 inTexCoord)\n"
    "{\n"
    "   vec2 eyeToSourceTexCoord = inTexCoord * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   eyeToSourceTexCoord.y = 1 - eyeToSourceTexCoord.y;\n"
	"   float depth = texelFetch(Texture0, ivec2(eyeToSourceTexCoord * DepthDimSize), 0).x;\n"
	"   float linearDepth = DepthProjector.y / (depth - DepthProjector.x);\n"
	"   vec4 retVal = vec4(inTexCoord, 1, 1);\n"
    "   retVal.xyz *= linearDepth;\n"
    "   return retVal;\n"
    "}\n"

    "vec2 TimewarpTexCoordToWarpedPos(vec2 inTexCoord, float a)\n"
    "{\n"
    // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
    // These are now "real world" vectors in direction (x,y,1) relative to the eye of the HMD.	
    // Apply the 4x4 timewarp rotation to these vectors.
    "   vec4 inputPos = PositionFromDepth(inTexCoord);\n"
    "   vec3 transformed = mix ( EyeRotationStart * inputPos,  EyeRotationEnd * inputPos, a ).xyz;\n"
    // Project them back onto the Z=1 plane of the rendered images.
    "   vec2 flattened = transformed.xy / transformed.z;\n"
    // Scale them into ([0,0.5],[0,1]) or ([0.5,0],[0,1]) UV lookup space (depending on eye)
    "   vec2 noDepthUV = flattened * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    //"   float depth = texture2DLod(Texture0, noDepthUV, 0).r;\n"
    "   return noDepthUV.xy;\n"
    "}\n"

    "void main()\n"
    "{\n"
    "   gl_Position.x = Position.x;\n"
    "   gl_Position.y = Position.y;\n"
    "   gl_Position.z = 0.0;\n"
    "   gl_Position.w = 1.0;\n"

    // warped positions are a bit more involved, hence a separate function
    "   oTexCoord0 = TimewarpTexCoordToWarpedPos(TexCoord0, Color.a);\n"
    "   oTexCoord0.y = 1-oTexCoord0.y;\n"
    "   oTexCoord1 = TimewarpTexCoordToWarpedPos(TexCoord1, Color.a);\n"
    "   oTexCoord1.y = 1-oTexCoord1.y;\n"
    "   oTexCoord2 = TimewarpTexCoordToWarpedPos(TexCoord2, Color.a);\n"
    "   oTexCoord2.y = 1-oTexCoord2.y;\n"

    "   oColor = vec4(Color.r);              // Used for vignette fade.\n"
    "}\n";

static const char* PostProcessFragShaderSrc =
    "uniform vec2 LensCenter;\n"
    "uniform vec2 ScreenCenter;\n"
    "uniform vec2 EyeToSourceUVScale;\n"
    "uniform vec2 EyeToSourceNDCScale;\n"
    "uniform vec4 HmdWarpParam;\n"
    "uniform sampler2D Texture1;\n"

    "varying vec2 oTexCoord;\n"

    "vec2 HmdWarp(vec2 in01)\n"
    "{\n"
    "   vec2  theta = (in01 - LensCenter) * EyeToSourceNDCScale;\n" // Scales to [-1, 1]
    "   float rSq = theta.x * theta.x + theta.y * theta.y;\n"
    "   vec2  theta1 = theta * (HmdWarpParam.x + HmdWarpParam.y * rSq + "
    "                           HmdWarpParam.z * rSq * rSq + HmdWarpParam.w * rSq * rSq * rSq);\n"
    "   return LensCenter + EyeToSourceUVScale * theta1;\n"
    "}\n"

    "void main()\n"
    "{\n"
    "   vec2 tc = HmdWarp(oTexCoord);\n"
    "   if (!all(equal(clamp(tc, ScreenCenter-vec2(0.25,0.5), ScreenCenter+vec2(0.25,0.5)), tc)))\n"
    "       gl_FragColor = vec4(0);\n"
    "   else\n"
    "       gl_FragColor = texture2D(Texture1, tc);\n"
    "}\n";

// Shader with lens distortion and chromatic aberration correction.
static const char* PostProcessFragShaderWithChromAbSrc =
    "uniform sampler2D Texture;\n"
    "uniform vec3 DistortionClearColor;\n"
    "uniform float EdgeFadeScale;\n"
    "uniform vec2 EyeToSourceUVScale;\n"
    "uniform vec2 EyeToSourceUVOffset;\n"
    "uniform vec2 EyeToSourceNDCScale;\n"
    "uniform vec2 EyeToSourceNDCOffset;\n"
    "uniform vec2 TanEyeAngleScale;\n"
    "uniform vec2 TanEyeAngleOffset;\n"
    "uniform vec4 HmdWarpParam;\n"
    "uniform vec4 ChromAbParam;\n"

    "varying vec4 oPosition;\n"
    "varying vec2 oTexCoord;\n"

	"void main()\n"
    "{\n"
    // Input oTexCoord is [-1,1] across the half of the screen used for a single eye.
    "   vec2 TanEyeAngleDistorted = oTexCoord * TanEyeAngleScale + TanEyeAngleOffset;\n" // Scales to tan(thetaX),tan(thetaY), but still distorted (i.e. only the center is correct)
    "   float  RadiusSq = TanEyeAngleDistorted.x * TanEyeAngleDistorted.x + TanEyeAngleDistorted.y * TanEyeAngleDistorted.y;\n"
    "   float Distort = 1.0 / ( 1.0 + RadiusSq * ( HmdWarpParam.y + RadiusSq * ( HmdWarpParam.z + RadiusSq * ( HmdWarpParam.w ) ) ) );\n"
    "   float DistortR = Distort * ( ChromAbParam.x + RadiusSq * ChromAbParam.y );\n"
    "   float DistortG = Distort;\n"
    "   float DistortB = Distort * ( ChromAbParam.z + RadiusSq * ChromAbParam.w );\n"
    "   vec2 TanEyeAngleR = DistortR * TanEyeAngleDistorted;\n"
    "   vec2 TanEyeAngleG = DistortG * TanEyeAngleDistorted;\n"
    "   vec2 TanEyeAngleB = DistortB * TanEyeAngleDistorted;\n"

    // These are now in "TanEyeAngle" space.
    // The vectors (TanEyeAngleRGB.x, TanEyeAngleRGB.y, 1.0) are real-world vectors pointing from the eye to where the components of the pixel appear to be.
    // If you had a raytracer, you could just use them directly.

    // Scale them into ([0,0.5],[0,1]) or ([0.5,0],[0,1]) UV lookup space (depending on eye)
    "   vec2 SourceCoordR = TanEyeAngleR * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
	"	SourceCoordR.y = 1 - SourceCoordR.y;\n"
    "   vec2 SourceCoordG = TanEyeAngleG * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
	"	SourceCoordG.y = 1 - SourceCoordG.y;\n"
    "   vec2 SourceCoordB = TanEyeAngleB * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
	"	SourceCoordB.y = 1 - SourceCoordB.y;\n"

    // Find the distance to the nearest edge.
    "   vec2 NDCCoord = TanEyeAngleG * EyeToSourceNDCScale + EyeToSourceNDCOffset;\n"
	"   float EdgeFadeIn = clamp ( EdgeFadeScale, 0, 1e5 ) * ( 1.0 - max ( abs ( NDCCoord.x ), abs ( NDCCoord.y ) ) );\n"
    "   if ( EdgeFadeIn < 0.0 )\n"
    "   {\n"
    "       gl_FragColor = vec4(DistortionClearColor.r, DistortionClearColor.g, DistortionClearColor.b, 1.0);\n"
	"       return;\n"
    "   }\n"
    "   EdgeFadeIn = clamp ( EdgeFadeIn, 0.0, 1.0 );\n"

    // Actually do the lookups.
    "   float ResultR = texture2D(Texture, SourceCoordR).r;\n"
    "   float ResultG = texture2D(Texture, SourceCoordG).g;\n"
    "   float ResultB = texture2D(Texture, SourceCoordB).b;\n"

    "   gl_FragColor =  vec4(ResultR * EdgeFadeIn, ResultG * EdgeFadeIn, ResultB * EdgeFadeIn, 1.0);\n"
    "}\n";



static const char* VShaderSrcs[VShader_Count] =
{
    DirectVertexShaderSrc,
    StdVertexShaderSrc,
    PostProcessVertexShaderSrc,
	PostProcessMeshVertexShaderSrc,
    PostProcessMeshTimewarpVertexShaderSrc,
    PostProcessMeshPositionalTimewarpVertexShaderSrc
};
static const char* FShaderSrcs[FShader_Count] =
{
    SolidFragShaderSrc,
    GouraudFragShaderSrc,
    TextureFragShaderSrc,
    AlphaTextureFragShaderSrc,
    PostProcessFragShaderWithChromAbSrc,
    LitSolidFragShaderSrc,
    LitTextureFragShaderSrc,
    MultiTextureFragShaderSrc,
    PostProcessMeshFragShaderSrc,
    PostProcessMeshTimewarpFragShaderSrc,
    PostProcessMeshPositionalTimewarpFragShaderSrc
};


RenderDevice::RenderDevice(const RendererParams&)
{
    for (int i = 0; i < VShader_Count; i++)
        VertexShaders[i] = *new Shader(this, Shader_Vertex, VShaderSrcs[i]);

    for (int i = 0; i < FShader_Count; i++)
        FragShaders[i] = *new Shader(this, Shader_Fragment, FShaderSrcs[i]);

    Ptr<ShaderSet> gouraudShaders = *new ShaderSet();
    gouraudShaders->SetShader(VertexShaders[VShader_MVP]);
    gouraudShaders->SetShader(FragShaders[FShader_Gouraud]);
    DefaultFill = *new ShaderFill(gouraudShaders);

    glGenFramebuffersEXT(1, &CurrentFbo);
}

RenderDevice::~RenderDevice()
{
    Shutdown();
}

void RenderDevice::Shutdown()
{
    // Release any other resources first.
    OVR::Render::RenderDevice::Shutdown();
    
    // This runs before the subclass's Shutdown(), where the context, etc, may be deleted.

    glDeleteFramebuffersEXT(1, &CurrentFbo);
    
    for (int i = 0; i < VShader_Count; ++i)
        VertexShaders[i].Clear();

    for (int i = 0; i < FShader_Count; ++i)
        FragShaders[i].Clear();

    DefaultFill.Clear();
    DepthBuffers.Clear();
}


void RenderDevice::FillTexturedRect(float left, float top, float right, float bottom, float ul, float vt, float ur, float vb, Color c, Ptr<OVR::Render::Texture> tex)
{
	Render::RenderDevice::FillTexturedRect(left, top, right, bottom, ul, vb, ur, vt, c, tex);
}


Shader *RenderDevice::LoadBuiltinShader(ShaderStage stage, int shader)
{
    switch (stage)
    {
    case Shader_Vertex: return VertexShaders[shader];
    case Shader_Fragment: return FragShaders[shader];
    default:
        return NULL;
    }
}


void RenderDevice::BeginRendering()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);

    glLineWidth(3.0f);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void RenderDevice::SetDepthMode(bool enable, bool write, CompareFunc func)
{
    if (enable)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(write);
        switch (func)
        {
        case Compare_Always:  glDepthFunc(GL_ALWAYS); break;
        case Compare_Less:    glDepthFunc(GL_LESS); break;
        case Compare_Greater: glDepthFunc(GL_GREATER); break;
        default: assert(0);
        }
    }
    else
        glDisable(GL_DEPTH_TEST);
}

void RenderDevice::SetViewport(const Recti& vp)
{
    int wh;
    if (CurRenderTarget)
        wh = CurRenderTarget->Height;
    else
        wh = WindowHeight;
    glViewport(vp.x, wh-vp.y-vp.h, vp.w, vp.h);

    //glEnable(GL_SCISSOR_TEST);
    //glScissor(vp.x, wh-vp.y-vp.h, vp.w, vp.h);
}

void RenderDevice::WaitUntilGpuIdle()
{
	glFlush();
	glFinish();
}

void RenderDevice::Clear(float r, float g, float b, float a, float depth, bool clearColor /*= true*/, bool clearDepth /*= true*/)
{
	glClearColor(r,g,b,a);
	glClearDepth(depth);
    glClear(
        ( clearColor ? ( GL_COLOR_BUFFER_BIT ) : 0 ) |
        ( clearDepth ? ( GL_DEPTH_BUFFER_BIT ) : 0 )
        );
}

Texture* RenderDevice::GetDepthBuffer(int w, int h, int ms)
{
    for (unsigned i = 0; i < DepthBuffers.GetSize(); i++)
        if (w == DepthBuffers[i]->Width && h == DepthBuffers[i]->Height && ms == DepthBuffers[i]->GetSamples())
            return DepthBuffers[i];

    Ptr<Texture> newDepth = *CreateTexture(Texture_Depth|Texture_RenderTarget|ms, w, h, NULL);
    DepthBuffers.PushBack(newDepth);
    return newDepth.GetPtr();
}

void RenderDevice::SetRenderTarget(Render::Texture* color, Render::Texture* depth, Render::Texture* stencil)
{
    OVR_UNUSED(stencil);

    CurRenderTarget = (Texture*)color;
    if (color == NULL)
    {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        return;
    }
	
	if (depth == NULL)
		depth = GetDepthBuffer(color->GetWidth(), color->GetHeight(), CurRenderTarget->GetSamples());

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, CurrentFbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, ((Texture*)color)->TexId, 0);
    if (depth)
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, ((Texture*)depth)->TexId, 0);
    else
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);

    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
        OVR_DEBUG_LOG(("framebuffer not complete: %x", status));
}


void RenderDevice::SetWorldUniforms(const Matrix4f& proj)
{
    Proj = proj.Transposed();
}

void RenderDevice::SetTexture(Render::ShaderStage, int slot, const Texture* t)
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, ((Texture*)t)->TexId);
    glActiveTexture(GL_TEXTURE0);
}

Buffer* RenderDevice::CreateBuffer()
{
    return new Buffer(this);
}

Fill* RenderDevice::CreateSimpleFill(int flags)
{
    OVR_UNUSED(flags);
    return DefaultFill;
}
    
void RenderDevice::Render(const Matrix4f& matrix, Model* model)
{
    // Store data in buffers if not already
    if (!model->VertexBuffer)
    {
        Ptr<Render::Buffer> vb = *CreateBuffer();
        vb->Data(Buffer_Vertex, &model->Vertices[0], model->Vertices.GetSize() * sizeof(Vertex));
        model->VertexBuffer = vb;
    }
    if (!model->IndexBuffer)
    {
        Ptr<Render::Buffer> ib = *CreateBuffer();
        ib->Data(Buffer_Index, &model->Indices[0], model->Indices.GetSize() * 2);
        model->IndexBuffer = ib;
    }

    Render(model->Fill ? (const Fill*)model->Fill : (const Fill*)DefaultFill,
           model->VertexBuffer, model->IndexBuffer,
           matrix, 0, (int)model->Indices.GetSize(), model->GetPrimType());
}

void RenderDevice::Render(const Fill* fill, Render::Buffer* vertices, Render::Buffer* indices,
                      const Matrix4f& matrix, int offset, int count, PrimitiveType rprim, bool useDistortionVertex/* = false*/)
{
    ShaderSet* shaders = (ShaderSet*) ((ShaderFill*)fill)->GetShaders();

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
        assert(0);
        return;
    }

    fill->Set();
    if (shaders->ProjLoc >= 0)
        glUniformMatrix4fv(shaders->ProjLoc, 1, 0, &Proj.M[0][0]);
    if (shaders->ViewLoc >= 0)
        glUniformMatrix4fv(shaders->ViewLoc, 1, 0, &matrix.Transposed().M[0][0]);

    if (shaders->UsesLighting && Lighting->Version != shaders->LightingVer)
    {
        shaders->LightingVer = Lighting->Version;
        Lighting->Set(shaders);
    }

	glBindBuffer(GL_ARRAY_BUFFER, ((Buffer*)vertices)->GLBuffer);
	for (int i = 0; i < 5; i++)
		glEnableVertexAttribArray(i);

	if (useDistortionVertex)
	{		
		glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(DistortionVertex), (char*)offset + offsetof(DistortionVertex, Pos));
		glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, true, sizeof(DistortionVertex), (char*)offset + offsetof(DistortionVertex, Col));
		glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(DistortionVertex), (char*)offset + offsetof(DistortionVertex, TexR));
		glVertexAttribPointer(3, 2, GL_FLOAT, false, sizeof(DistortionVertex), (char*)offset + offsetof(DistortionVertex, TexG));
		glVertexAttribPointer(4, 2, GL_FLOAT, false, sizeof(DistortionVertex), (char*)offset + offsetof(DistortionVertex, TexB));
	}
	else
	{
		glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vertex), (char*)offset + offsetof(Vertex, Pos));
		glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, true, sizeof(Vertex), (char*)offset + offsetof(Vertex, C));
		glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(Vertex), (char*)offset + offsetof(Vertex, U));
		glVertexAttribPointer(3, 2, GL_FLOAT, false, sizeof(Vertex), (char*)offset + offsetof(Vertex, U2));
		glVertexAttribPointer(4, 3, GL_FLOAT, false, sizeof(Vertex), (char*)offset + offsetof(Vertex, Norm));
	}

    if (indices)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ((Buffer*)indices)->GLBuffer);
        glDrawElements(prim, count, GL_UNSIGNED_SHORT, NULL);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    else
    {
        glDrawArrays(prim, 0, count);
    }

	for (int i = 0; i < 5; i++)
		glDisableVertexAttribArray(i);
}

void RenderDevice::RenderWithAlpha(const Fill* fill, Render::Buffer* vertices, Render::Buffer* indices,
                                   const Matrix4f& matrix, int offset, int count, PrimitiveType rprim)
{
    //glEnable(GL_BLEND);
    Render(fill, vertices, indices, matrix, offset, count, rprim);
    //glDisable(GL_BLEND);
}

void RenderDevice::SetLighting(const LightingParams* lt)
{
    Lighting = lt;
}

Buffer::~Buffer()
{
    if (GLBuffer)
        glDeleteBuffers(1, &GLBuffer);
}

bool Buffer::Data(int use, const void* buffer, size_t size)
{
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

Shader::~Shader()
{
    if (GLShader)
        glDeleteShader(GLShader);
}

bool Shader::Compile(const char* src)
{
    if (!GLShader)
        GLShader = glCreateShader(GLStage());

    glShaderSource(GLShader, 1, &src, 0);
    glCompileShader(GLShader);
    GLint r;
    glGetShaderiv(GLShader, GL_COMPILE_STATUS, &r);
    if (!r)
    {
        GLchar msg[1024];
        glGetShaderInfoLog(GLShader, sizeof(msg), 0, msg);
        if (msg[0])
            OVR_DEBUG_LOG(("Compiling shader\n%s\nfailed: %s\n", src, msg));
        if (!r)
            return 0;
    }
    return 1;
}

ShaderSet::ShaderSet()
{
    Prog = glCreateProgram();
}
ShaderSet::~ShaderSet()
{
    glDeleteProgram(Prog);
}

void ShaderSet::SetShader(Render::Shader *s)
{
    Shaders[s->GetStage()] = s;
    Shader* gls = (Shader*)s;
    glAttachShader(Prog, gls->GLShader);
    if (Shaders[Shader_Vertex] && Shaders[Shader_Fragment])
        Link();
}

void ShaderSet::UnsetShader(int stage)
{
    Shader* gls = (Shader*)(Render::Shader*)Shaders[stage];
    if (gls)
        glDetachShader(Prog, gls->GLShader);
    Shaders[stage] = NULL;
    Link();
}

bool ShaderSet::Link()
{
    glBindAttribLocation(Prog, 0, "Position");
    glBindAttribLocation(Prog, 1, "Color");
    glBindAttribLocation(Prog, 2, "TexCoord");
    glBindAttribLocation(Prog, 3, "TexCoord1");
    glBindAttribLocation(Prog, 4, "Normal");

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

void ShaderSet::Set(PrimitiveType) const
{
    glUseProgram(Prog);
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

bool ShaderSet::SetUniform4x4f(const char* name, const Matrix4f& m)
{
    for (unsigned int i = 0; i < UniformInfo.GetSize(); i++)
        if (!strcmp(UniformInfo[i].Name.ToCStr(), name))
        {
            glUseProgram(Prog);
            glUniformMatrix4fv(UniformInfo[i].Location, 1, 1, &m.M[0][0]);
            return 1;
        }

    OVR_DEBUG_LOG(("Warning: uniform %s not present in selected shader", name));
    return 0;
}

Texture::Texture(RenderDevice* r, int w, int h) : Ren(r), Width(w), Height(h)
{
    glGenTextures(1, &TexId);
}

Texture::~Texture()
{
    if (TexId)
        glDeleteTextures(1, &TexId);
}

void Texture::Set(int slot, Render::ShaderStage stage) const
{
    Ren->SetTexture(stage, slot, this);
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

ovrTexture Texture::Get_ovrTexture()
{
	ovrTexture tex;
	OVR::Sizei newRTSize(Width, Height);

    ovrGLTextureData* texData = (ovrGLTextureData*)&tex;
    texData->Header.API            = ovrRenderAPI_OpenGL;
    texData->Header.TextureSize    = newRTSize;
    texData->Header.RenderViewport = Recti(newRTSize);
    texData->TexId                 = TexId;

	return tex;
}

Texture* RenderDevice::CreateTexture(int format, int width, int height, const void* data, int mipcount)
{
    GLenum   glformat, gltype = GL_UNSIGNED_BYTE;
    switch(format & Texture_TypeMask)
    {
    case Texture_RGBA:  glformat = GL_RGBA; break;
    case Texture_R:     glformat = GL_ALPHA; break;
    case Texture_Depth: glformat = GL_DEPTH_COMPONENT32F; gltype = GL_FLOAT; break;
    case Texture_DXT1:  glformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
    case Texture_DXT3:  glformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
    case Texture_DXT5:  glformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
    default:
        return NULL;
    }
    Texture* NewTex = new Texture(this, width, height);
    glBindTexture(GL_TEXTURE_2D, NewTex->TexId);
	OVR_ASSERT(!glGetError());
    
    if (format & Texture_Compressed)
    {
        const unsigned char* level = (const unsigned char*)data;
        int w = width, h = height;
        for (int i = 0; i < mipcount; i++)
        {
            int mipsize = GetTextureSize(format, w, h);
            glCompressedTexImage2D(GL_TEXTURE_2D, i, glformat, w, h, 0, mipsize, level);

            level += mipsize;
            w >>= 1;
            h >>= 1;
            if (w < 1) w = 1;
            if (h < 1) h = 1;
        }
    }
    else if (format & Texture_Depth)
        glTexImage2D(GL_TEXTURE_2D, 0, glformat, width, height, 0, GL_DEPTH_COMPONENT, gltype, data);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, glformat, width, height, 0, glformat, gltype, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (format == (Texture_RGBA|Texture_GenMipmaps)) // not render target
    {
        int srcw = width, srch = height;
        int level = 0;
        UByte* mipmaps = NULL;
        do
        {
            level++;
            int mipw = srcw >> 1; if (mipw < 1) mipw = 1;
            int miph = srch >> 1; if (miph < 1) miph = 1;
            if (mipmaps == NULL)
                mipmaps = (UByte*)OVR_ALLOC(mipw * miph * 4);
            FilterRgba2x2(level == 1 ? (const UByte*)data : mipmaps, srcw, srch, mipmaps);
            glTexImage2D(GL_TEXTURE_2D, level, glformat, mipw, miph, 0, glformat, gltype, mipmaps);
            srcw = mipw;
            srch = miph;
        } while (srcw > 1 || srch > 1);
        if (mipmaps)
            OVR_FREE(mipmaps);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipcount-1);
    }

    OVR_ASSERT(!glGetError());
    glBindTexture(GL_TEXTURE_2D, 0);
    return NewTex;
}

RBuffer::RBuffer(GLenum format, GLint w, GLint h)
{
    Width = w;
    Height = h;
    glGenRenderbuffersEXT(1, &BufId);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, BufId);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, format, w, h);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
}

RBuffer::~RBuffer()
{
    if (BufId)
        glDeleteRenderbuffersEXT(1, &BufId);
}

}}}
