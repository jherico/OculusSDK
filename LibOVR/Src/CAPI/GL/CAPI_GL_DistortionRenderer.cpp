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

#include "../../OVR_CAPI_GL.h"

namespace OVR { namespace CAPI { namespace GL {


static const char SimpleQuad_vs[] =
    "uniform vec2 PositionOffset;\n"
    "uniform vec2 Scale;\n"

    "attribute vec3 Position;\n"

	"void main()\n"
	"{\n"
	"	gl_Position = vec4(Position.xy * Scale + PositionOffset, 0.5, 1.0);\n"
	"}\n";

const OVR::CAPI::GL::ShaderBase::Uniform SimpleQuad_vs_refl[] =
{
	{ "PositionOffset", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 8 },
	{ "Scale", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 8, 8 },
};

static const char SimpleQuad_fs[] =
    "uniform vec4 Color;\n"

	"void main()\n"
	"{\n"
	"	gl_FragColor = Color;\n"
	"}\n";

const OVR::CAPI::GL::ShaderBase::Uniform SimpleQuad_fs_refl[] =
{
	{ "Color", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 16 },
};


static const char Distortion_vs[] =
    "uniform vec2 EyeToSourceUVScale;\n"
    "uniform vec2 EyeToSourceUVOffset;\n"

    "attribute vec2 Position;\n"
    "attribute vec4 Color;\n"
    "attribute vec2 TexCoord0;\n"

    "varying vec4 oColor;\n"
    "varying vec2 oTexCoord0;\n"

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
    "   oColor = Color;\n"              // Used for vignette fade.
    "}\n";

const OVR::CAPI::GL::ShaderBase::Uniform Distortion_vs_refl[] =
{
	{ "EyeToSourceUVScale", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 8 },
	{ "EyeToSourceUVOffset", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 8, 8 },
};

static const char Distortion_fs[] =
    "uniform sampler2D Texture0;\n"

    "varying vec4 oColor;\n"
    "varying vec2 oTexCoord0;\n"

    "void main()\n"
    "{\n"
    "   gl_FragColor = texture2D(Texture0, oTexCoord0);\n"
    "   gl_FragColor.a = 1.0;\n"
    "}\n";


static const char DistortionTimewarp_vs[] =
    "uniform vec2 EyeToSourceUVScale;\n"
    "uniform vec2 EyeToSourceUVOffset;\n"
    "uniform mat4 EyeRotationStart;\n"
    "uniform mat4 EyeRotationEnd;\n"

    "attribute vec2 Position;\n"
    "attribute vec4 Color;\n"
    "attribute vec2 TexCoord0;\n"

    "varying vec4 oColor;\n"
    "varying vec2 oTexCoord0;\n"

    "void main()\n"
    "{\n"
    "   gl_Position.x = Position.x;\n"
    "   gl_Position.y = Position.y;\n"
    "   gl_Position.z = 0.0;\n"
    "   gl_Position.w = 1.0;\n"

    // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
    // These are now "real world" vectors in direction (x,y,1) relative to the eye of the HMD.
    "   vec3 TanEyeAngle = vec3 ( TexCoord0.x, TexCoord0.y, 1.0 );\n"

    // Accurate time warp lerp vs. faster
#if 1
    // Apply the two 3x3 timewarp rotations to these vectors.
	"   vec3 TransformedStart = (EyeRotationStart * vec4(TanEyeAngle, 0)).xyz;\n"
	"   vec3 TransformedEnd   = (EyeRotationEnd * vec4(TanEyeAngle, 0)).xyz;\n"
    // And blend between them.
    "   vec3 Transformed = mix ( TransformedStart, TransformedEnd, Color.a );\n"
#else
    "   mat3 EyeRotation = mix ( EyeRotationStart, EyeRotationEnd, Color.a );\n"
    "   vec3 Transformed   = EyeRotation * TanEyeAngle;\n"
#endif

    // Project them back onto the Z=1 plane of the rendered images.
    "   float RecipZ = 1.0 / Transformed.z;\n"
    "   vec2 Flattened = vec2 ( Transformed.x * RecipZ, Transformed.y * RecipZ );\n"

    // These are now still in TanEyeAngle space.
    // Scale them into the correct [0-1],[0-1] UV lookup space (depending on eye)
    "   vec2 SrcCoord = Flattened * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord0 = SrcCoord;\n"
    "   oTexCoord0.y = 1-oTexCoord0.y;\n"
    "   oColor = Color.r;\n"              // Used for vignette fade.
    "}\n";

const OVR::CAPI::GL::ShaderBase::Uniform DistortionTimewarp_vs_refl[] =
{
	{ "EyeToSourceUVScale", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 8 },
	{ "EyeToSourceUVOffset", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 8, 8 },
};


static const char DistortionPositionalTimewarp_vs[] =
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

    "   oColor = vec4(Color.r);              // Used for vignette fade.\n"
    "}\n";

const OVR::CAPI::GL::ShaderBase::Uniform DistortionPositionalTimewarp_vs_refl[] =
{
	{ "EyeToSourceUVScale", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 8 },
	{ "EyeToSourceUVOffset", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 8, 8 },
};


static const char DistortionChroma_vs[] =
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

const OVR::CAPI::GL::ShaderBase::Uniform DistortionChroma_vs_refl[] =
{
	{ "EyeToSourceUVScale", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 8 },
	{ "EyeToSourceUVOffset", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 8, 8 },
};

static const char DistortionChroma_fs[] =
    "uniform sampler2D Texture0;\n"

    "varying vec4 oColor;\n"
    "varying vec2 oTexCoord0;\n"
    "varying vec2 oTexCoord1;\n"
    "varying vec2 oTexCoord2;\n"

    "void main()\n"
    "{\n"
    "   float ResultR = texture2D(Texture0, oTexCoord0).r;\n"
    "   float ResultG = texture2D(Texture0, oTexCoord1).g;\n"
    "   float ResultB = texture2D(Texture0, oTexCoord2).b;\n"

    "   gl_FragColor = vec4(ResultR * oColor.r, ResultG * oColor.g, ResultB * oColor.b, 1.0);\n"
    "}\n";


static const char DistortionTimewarpChroma_vs[] =
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

const OVR::CAPI::GL::ShaderBase::Uniform DistortionTimewarpChroma_vs_refl[] =
{
	{ "EyeToSourceUVScale", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 8 },
	{ "EyeToSourceUVOffset", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 8, 8 },
	{ "EyeRotationStart", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 16, 64 },
	{ "EyeRotationEnd", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 80, 64 },
};


static const char DistortionPositionalTimewarpChroma_vs[] =
    "#version 150\n"
	"uniform sampler2D Texture0;\n"
	"uniform sampler2D Texture1;\n"
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
	"   float depth = texelFetch(Texture1, ivec2(eyeToSourceTexCoord * DepthDimSize), 0).x;\n"
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
    //"   float depth = texture2DLod(Texture1, noDepthUV, 0).r;\n"
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

const OVR::CAPI::GL::ShaderBase::Uniform DistortionPositionalTimewarpChroma_vs_refl[] =
{
	{ "EyeToSourceUVScale", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 8 },
	{ "EyeToSourceUVOffset", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 8, 8 },
};


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
    //SI_REFL__(DistortionPositionalTimewarp_vs),
    //SI_REFL__(DistortionPositionalTimewarpChroma_vs)
};

static ShaderInfo DistortionPixelShaderLookup[DistortionPixelShaderCount] =
{
    SI_NOREFL(Distortion_fs),
    SI_NOREFL(DistortionChroma_fs)
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
    InitGLExtensions();

    return new DistortionRenderer(hmd, timeManager, renderState);
}


bool DistortionRenderer::Initialize(const ovrRenderAPIConfig* apiConfig,
									unsigned hmdCaps, unsigned distortionCaps)
{
    // TBD: Decide if hmdCaps are needed here or are a part of RenderState
    OVR_UNUSED(hmdCaps);

    const ovrGLConfig* config = (const ovrGLConfig*)apiConfig;

    if (!config)
    {
        // Cleanup
        pEyeTextures[0].Clear();
        pEyeTextures[1].Clear();
        memset(&RParams, 0, sizeof(RParams));
        return true;
    }
	
    if (!config->OGL.WglContext || !config->OGL.GdiDc)
        return false;

	RParams.GdiDc       = config->OGL.GdiDc;
	RParams.Multisample = config->OGL.Header.Multisample;
	RParams.RTSize      = config->OGL.Header.RTSize;
	RParams.WglContext  = config->OGL.WglContext;
	RParams.Window      = config->OGL.Window;
	
    DistortionCaps = distortionCaps;
	
    //DistortionWarper.SetVsync((hmdCaps & ovrHmdCap_NoVSync) ? false : true);

    pEyeTextures[0] = *new Texture(&RParams, 0, 0);
    pEyeTextures[1] = *new Texture(&RParams, 0, 0);

    initBuffersAndShaders();

    return true;
}


void DistortionRenderer::SubmitEye(int eyeId, ovrTexture* eyeTexture)
{
	//Doesn't do a lot in here??
	const ovrGLTexture* tex = (const ovrGLTexture*)eyeTexture;

	//Write in values
    eachEye[eyeId].texture = tex->OGL.TexId;

	if (tex)
	{
		//Its only at this point we discover what the viewport of the texture is.
		//because presumably we allow users to realtime adjust the resolution.
		//Which begs the question - why did we ask them what viewport they were
		//using before, which gave them a set of UV offsets.   In fact, our 
		//asking for eye mesh must be entirely independed of these viewports,
		//presumably only to get the parameters.

		ovrEyeDesc     ed = RState.EyeRenderDesc[eyeId].Desc;
		ed.TextureSize    = tex->OGL.Header.TextureSize;
		ed.RenderViewport = tex->OGL.Header.RenderViewport;

		ovrHmd_GetRenderScaleAndOffset(HMD, ed, DistortionCaps, eachEye[eyeId].UVScaleOffset);
				
        pEyeTextures[eyeId]->UpdatePlaceholderTexture(tex->OGL.TexId,
                                                      tex->OGL.Header.TextureSize);
	}
}

void DistortionRenderer::EndFrame(bool swapBuffers, unsigned char* latencyTesterDrawColor, unsigned char* latencyTester2DrawColor)
{	    
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
		bool useVsync = ((RState.HMDCaps & ovrHmdCap_NoVSync) == 0);
		BOOL success;
		int swapInterval = (useVsync) ? 1 : 0;
		if (wglGetSwapIntervalEXT() != swapInterval)
			wglSwapIntervalEXT(swapInterval);

		success = SwapBuffers(RParams.GdiDc);
		OVR_ASSERT(success);

        // Force GPU to flush the scene, resulting in the lowest possible latency.
        // It's critical that this flush is *after* present.
        WaitUntilGpuIdle();
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

    initShaders();
}

void DistortionRenderer::renderDistortion(Texture* leftEyeTexture, Texture* rightEyeTexture)
{    
    setViewport( Recti(0,0, RParams.RTSize.w, RParams.RTSize.h) );
        
	glClearColor(
		RState.ClearColor[0],
		RState.ClearColor[1],
		RState.ClearColor[2],
		RState.ClearColor[3] );

	glClearDepth(0);

    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    for (int eyeNum = 0; eyeNum < 2; eyeNum++)
    {        
		ShaderFill distortionShaderFill(DistortionShader);
        distortionShaderFill.SetTexture(0, eyeNum == 0 ? leftEyeTexture : rightEyeTexture);

        DistortionShader->SetUniform2f("EyeToSourceUVScale",  UVScaleOffset[eyeNum][0].x, UVScaleOffset[eyeNum][0].y);
        DistortionShader->SetUniform2f("EyeToSourceUVOffset", UVScaleOffset[eyeNum][1].x, UVScaleOffset[eyeNum][1].y);
        
		if (DistortionCaps & ovrDistortion_TimeWarp)
		{                       
            ovrMatrix4f timeWarpMatrices[2];            
            ovrHmd_GetEyeTimewarpMatrices(HMD, (ovrEyeType)eyeNum,
                                          RState.EyeRenderPoses[eyeNum], timeWarpMatrices);

            // Feed identity like matrices in until we get proper timewarp calculation going on
			DistortionShader->SetUniform4x4f("EyeRotationStart", Matrix4f(timeWarpMatrices[0]).Transposed());
			DistortionShader->SetUniform4x4f("EyeRotationEnd",   Matrix4f(timeWarpMatrices[1]).Transposed());

            renderPrimitives(&distortionShaderFill, DistortionMeshVBs[eyeNum], DistortionMeshIBs[eyeNum],
                            NULL, 0, (int)DistortionMeshVBs[eyeNum]->GetSize(), Prim_Triangles, true);
		}
        else
        {
            renderPrimitives(&distortionShaderFill, DistortionMeshVBs[eyeNum], DistortionMeshIBs[eyeNum],
                            NULL, 0, (int)DistortionMeshVBs[eyeNum]->GetSize(), Prim_Triangles, true);
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
        renderPrimitives(&quadFill, LatencyTesterQuadVB, NULL, NULL, 0, numQuadVerts, Prim_TriangleStrip, false);
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
    renderPrimitives(&quadFill, LatencyTesterQuadVB, NULL, NULL, 0, numQuadVerts, Prim_TriangleStrip, false);
}

void DistortionRenderer::renderPrimitives(
                          const ShaderFill* fill,
                          Buffer* vertices, Buffer* indices,
                          Matrix4f* viewMatrix, int offset, int count,
                          PrimitiveType rprim, bool useDistortionVertex)
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
        glUniformMatrix4fv(shaders->ProjLoc, 1, 0, &StdUniforms.Proj.M[0][0]);
    if (shaders->ViewLoc >= 0 && viewMatrix != NULL)
        glUniformMatrix4fv(shaders->ViewLoc, 1, 0, &viewMatrix->Transposed().M[0][0]);

    //if (shaders->UsesLighting && Lighting->Version != shaders->LightingVer)
    //{
    //    shaders->LightingVer = Lighting->Version;
    //    Lighting->Set(shaders);
    //}

	glBindBuffer(GL_ARRAY_BUFFER, ((Buffer*)vertices)->GLBuffer);
	for (int i = 0; i < 5; i++)
		glEnableVertexAttribArray(i);
    
    GLuint prog = fill->GetShaders()->Prog;

	if (useDistortionVertex)
	{
        GLint posLoc = glGetAttribLocation(prog, "Position");
        GLint colLoc = glGetAttribLocation(prog, "Color");
        GLint tc0Loc = glGetAttribLocation(prog, "TexCoord0");
        GLint tc1Loc = glGetAttribLocation(prog, "TexCoord1");
        GLint tc2Loc = glGetAttribLocation(prog, "TexCoord2");

		glVertexAttribPointer(posLoc, 2, GL_FLOAT, false, sizeof(DistortionVertex), (char*)offset + offsetof(DistortionVertex, Pos));                
		glVertexAttribPointer(colLoc, 4, GL_UNSIGNED_BYTE, true, sizeof(DistortionVertex), (char*)offset + offsetof(DistortionVertex, Col));        
		glVertexAttribPointer(tc0Loc, 2, GL_FLOAT, false, sizeof(DistortionVertex), (char*)offset + offsetof(DistortionVertex, TexR));
		glVertexAttribPointer(tc1Loc, 2, GL_FLOAT, false, sizeof(DistortionVertex), (char*)offset + offsetof(DistortionVertex, TexG));
		glVertexAttribPointer(tc2Loc, 2, GL_FLOAT, false, sizeof(DistortionVertex), (char*)offset + offsetof(DistortionVertex, TexB));
	}
	else
	{
        GLint posLoc = glGetAttribLocation(prog, "Position");

		glVertexAttribPointer(posLoc, 3, GL_FLOAT, false, sizeof(LatencyVertex), (char*)offset + offsetof(LatencyVertex, Pos));
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

void DistortionRenderer::setViewport(const Recti& vp)
{
    int wh;
    if (CurRenderTarget)
        wh = CurRenderTarget->Height;
    else
	{
		RECT rect;
		BOOL success = GetWindowRect(RParams.Window, &rect);
		OVR_ASSERT(success);
        OVR_UNUSED(success);
		wh = rect.bottom - rect.top;
	}
    glViewport(vp.x, wh-vp.y-vp.h, vp.w, vp.h);

    //glEnable(GL_SCISSOR_TEST);
    //glScissor(vp.x, wh-vp.y-vp.h, vp.w, vp.h);
}


void DistortionRenderer::initShaders()
{
    {
        ShaderInfo vsShaderByteCode = DistortionVertexShaderLookup[DistortionVertexShaderBitMask & DistortionCaps];
        Ptr<GL::VertexShader> vtxShader = *new GL::VertexShader(
            &RParams,
            (void*)vsShaderByteCode.ShaderData, vsShaderByteCode.ShaderSize,
            vsShaderByteCode.ReflectionData, vsShaderByteCode.ReflectionSize);

        DistortionShader = *new ShaderSet;
        DistortionShader->SetShader(vtxShader);

        ShaderInfo psShaderByteCode = DistortionPixelShaderLookup[DistortionPixelShaderBitMask & DistortionCaps];

        Ptr<GL::FragmentShader> ps  = *new GL::FragmentShader(
            &RParams,
            (void*)psShaderByteCode.ShaderData, psShaderByteCode.ShaderSize,
            psShaderByteCode.ReflectionData, psShaderByteCode.ReflectionSize);

        DistortionShader->SetShader(ps);
    }
    {
        Ptr<GL::VertexShader> vtxShader = *new GL::VertexShader(
            &RParams,
            (void*)SimpleQuad_vs, sizeof(SimpleQuad_vs),
            SimpleQuad_vs_refl, sizeof(SimpleQuad_vs_refl) / sizeof(SimpleQuad_vs_refl[0]));

        SimpleQuadShader = *new ShaderSet;
        SimpleQuadShader->SetShader(vtxShader);

        Ptr<GL::FragmentShader> ps  = *new GL::FragmentShader(
            &RParams,
            (void*)SimpleQuad_fs, sizeof(SimpleQuad_fs),
            SimpleQuad_fs_refl, sizeof(SimpleQuad_fs_refl) / sizeof(SimpleQuad_fs_refl[0]));

        SimpleQuadShader->SetShader(ps);
	}
}


void DistortionRenderer::destroy()
{
	for(int eyeNum = 0; eyeNum < 2; eyeNum++)
	{
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
}

}}} // OVR::CAPI::GL
