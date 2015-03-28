/************************************************************************************
 
 Filename    :   CAPI_GL_Shaders.h
 Content     :   Distortion shader header for GL
 Created     :   November 11, 2013
 Authors     :   David Borel, Volga Aksoy
 
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


#ifndef OVR_CAPI_GL_Shaders_h
#define OVR_CAPI_GL_Shaders_h


#include "CAPI_GL_Util.h"

namespace OVR { namespace CAPI { namespace GL {
    
    static const char glsl2Prefix[] =
    "#version 110\n"
    "#extension GL_ARB_shader_texture_lod : enable\n"
    "#extension GL_ARB_draw_buffers : enable\n"
    "#extension GL_EXT_gpu_shader4 : enable\n"
    "#define _FRAGCOLOR_DECLARATION\n"
    "#define _MRTFRAGCOLOR0_DECLARATION\n"
    "#define _MRTFRAGCOLOR1_DECLARATION\n"
    "#define _GLFRAGCOORD_DECLARATION\n"
    "#define _VS_IN attribute\n"
    "#define _VS_OUT varying\n"
    "#define _FS_IN varying\n"
    "#define _TEXTURELOD texture2DLod\n"
    "#define _TEXTURE texture2D\n"
    "#define _FRAGCOLOR gl_FragColor\n"
    "#define _MRTFRAGCOLOR0 gl_FragData[0]\n"
    "#define _MRTFRAGCOLOR1 gl_FragData[1]\n"       // The texture coordinate [0.0,1.0] for texel i of a texture of size N is: (2i + 1)/2N
    "#ifdef GL_EXT_gpu_shader4\n"
    "  #define _TEXELFETCHDECL vec4 texelFetch(sampler2D tex, ivec2 coord, int lod){ ivec2 size = textureSize2D(tex, lod); return texture2D(tex, vec2(float((coord.x * 2) + 1) / float(size.x * 2), float((coord.y * 2) + 1) / float(size.y * 2))); }\n"
    "#endif\n";
    
    static const char glsl3Prefix[] =
    "#version 150\n"
    "#define _FRAGCOLOR_DECLARATION out vec4 FragColor;\n"
    "#define _MRTFRAGCOLOR0_DECLARATION out vec4 FragData0;\n"
    "#define _MRTFRAGCOLOR1_DECLARATION out vec4 FragData1;\n"
    "#define _GLFRAGCOORD_DECLARATION in vec4 gl_FragCoord;\n"
    "#define _VS_IN in\n"
    "#define _VS_OUT out\n"
    "#define _FS_IN in\n"
    "#define _TEXTURELOD textureLod\n"
    "#define _TEXTURE texture\n"
    "#define _FRAGCOLOR FragColor\n"
    "#define _MRTFRAGCOLOR0 FragData0\n"
    "#define _MRTFRAGCOLOR1 FragData1\n"
    "#define _TEXELFETCHDECL\n";
    
    static const char SimpleQuad_vs[] =
    "uniform vec2 PositionOffset;\n"
    "uniform vec2 Scale;\n"
    
    "_VS_IN vec3 Position;\n"
    
	"void main()\n"
	"{\n"
	"	gl_Position = vec4(Position.xy * Scale + PositionOffset, 0.5, 1.0);\n"
	"}\n";
    
    const OVR::CAPI::GL::ShaderBase::Uniform SimpleQuad_vs_refl[] =
    {
        { "PositionOffset", OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 8 },
        { "Scale",          OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 8, 8 },
    };
    
    static const char SimpleQuad_fs[] =
    "uniform vec4 Color;\n"
    
    "_FRAGCOLOR_DECLARATION\n"
    
	"void main()\n"
	"{\n"
	"    _FRAGCOLOR = Color;\n"
	"}\n";
    
    const OVR::CAPI::GL::ShaderBase::Uniform SimpleQuad_fs_refl[] =
    {
        { "Color", OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 16 },
    };

    static const char SimpleQuadGamma_fs[] =
        "uniform vec4 Color;\n"

        "_FRAGCOLOR_DECLARATION\n"

        "void main()\n"
        "{\n"
        "    _FRAGCOLOR.rgb = pow(Color.rgb, vec3(2.2));\n"
        "    _FRAGCOLOR.a = Color.a;\n"
        "}\n";

    const OVR::CAPI::GL::ShaderBase::Uniform SimpleQuadGamma_fs_refl[] =
    {
        { "Color", OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 16 },
    };

    // This must be prefixed with glsl2Prefix or glsl3Prefix before being compiled.
    static const char SimpleTexturedQuad_vs[] =
        "uniform vec2 PositionOffset;\n"
        "uniform vec2 Scale;\n"

        "_VS_IN vec3 Position;\n"
        "_VS_IN vec4 Color;\n"
        "_VS_IN vec2 TexCoord;\n"
  
        "_VS_OUT vec4 oColor;\n"
        "_VS_OUT vec2 oTexCoord;\n"

        "void main()\n"
        "{\n"
	    "	gl_Position = vec4(Position.xy * Scale + PositionOffset, 0.5, 1.0);\n"
        "   oColor = Color;\n"
        "   oTexCoord = TexCoord;\n"
        "}\n";

    // The following declaration is copied from the generated D3D SimpleTexturedQuad_vs_refl.h file, with D3D_NS renamed to GL.
    const OVR::CAPI::GL::ShaderBase::Uniform SimpleTexturedQuad_vs_refl[] =
    {
	    { "PositionOffset", OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 8 },
	    { "Scale",          OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 8, 8 },
    };


    // This must be prefixed with glsl2Prefix or glsl3Prefix before being compiled.
    static const char SimpleTexturedQuad_ps[] =
        "uniform sampler2D Texture0;\n"
    
        "_FS_IN vec4 oColor;\n"
        "_FS_IN vec2 oTexCoord;\n"
    
        "_FRAGCOLOR_DECLARATION\n"

        "void main()\n"
        "{\n"
        "   _FRAGCOLOR = oColor * _TEXTURE(Texture0, oTexCoord);\n"
        "}\n";

    // The following is copied from the generated D3D SimpleTexturedQuad_ps_refl.h file, with D3D_NS renamed to GL.
    const OVR::CAPI::GL::ShaderBase::Uniform SimpleTexturedQuad_ps_refl[] =
    {
	    { "Color", 	OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 16 },
    };
    
    static const char DistortionChroma_vs[] =
    "uniform vec2 EyeToSourceUVScale;\n"
    "uniform vec2 EyeToSourceUVOffset;\n"
    
    "_VS_IN vec2 Position;\n"
    "_VS_IN vec4 Color;\n"
    "_VS_IN vec2 TexCoord0;\n"
    "_VS_IN vec2 TexCoord1;\n"
    "_VS_IN vec2 TexCoord2;\n"
    
    "_VS_OUT vec4 oColor;\n"
    "_VS_OUT vec2 oTexCoord0;\n"
    "_VS_OUT vec2 oTexCoord1;\n"
    "_VS_OUT vec2 oTexCoord2;\n"
    
    "void main()\n"
    "{\n"
    "   gl_Position.x = Position.x;\n"
    "   gl_Position.y = Position.y;\n"
    "   gl_Position.z = 0.5;\n"
    "   gl_Position.w = 1.0;\n"
    
    // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
    // Scale them into the correct [0-1],[0-1] UV lookup space (depending on eye)
    "   oTexCoord0 = TexCoord0 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord1 = TexCoord1 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    "   oTexCoord2 = TexCoord2 * EyeToSourceUVScale + EyeToSourceUVOffset;\n"
    
    "   oColor = Color;\n" // Used for vignette fade.
    "}\n";
    
    const OVR::CAPI::GL::ShaderBase::Uniform DistortionChroma_vs_refl[] =
    {
        { "EyeToSourceUVScale",  OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 8 },
        { "EyeToSourceUVOffset", OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 8, 8 },
    };
    
    static const char DistortionChroma_fs[] =
    "uniform sampler2D Texture0;\n"
    "uniform sampler2D Texture1;\n"
    "uniform vec3 OverdriveScales_IsSrgb;\n"

    "_FS_IN vec4 oColor;\n"
    "_FS_IN vec2 oTexCoord0;\n"
    "_FS_IN vec2 oTexCoord1;\n"
    "_FS_IN vec2 oTexCoord2;\n"
    
    "_MRTFRAGCOLOR0_DECLARATION\n"   // Desired color (next frame's "PrevTexture")
    "_MRTFRAGCOLOR1_DECLARATION\n"   // Overdriven color (Back-buffer)
    "_GLFRAGCOORD_DECLARATION\n"

    "#ifdef _TEXELFETCHDECL\n"
    "_TEXELFETCHDECL\n"
    "#endif\n"
    
    "void main()\n"
    "{\n"
    "   float ResultR = _TEXTURE(Texture0, oTexCoord0, 0.0).r;\n"
    "   float ResultG = _TEXTURE(Texture0, oTexCoord1, 0.0).g;\n"
    "   float ResultB = _TEXTURE(Texture0, oTexCoord2, 0.0).b;\n"
    "   vec3 newColor = vec3(ResultR * oColor.r, ResultG * oColor.g, ResultB * oColor.b);\n"

    "   _MRTFRAGCOLOR0 = vec4(newColor, 1);\n"
    "   _MRTFRAGCOLOR1 = _MRTFRAGCOLOR0;\n"

    "   #ifdef _TEXELFETCHDECL\n"
    // pixel luminance overdrive
    "   if(OverdriveScales_IsSrgb.x > 0.0)\n"
    "   {\n"
    "       ivec2 pixelCoord = ivec2(gl_FragCoord.x, gl_FragCoord.y);\n"
    "       vec3 oldColor = texelFetch(Texture1, pixelCoord, 0).rgb;\n"

    "       vec3 adjustedScales;\n"
    "       adjustedScales.x = newColor.x > oldColor.x ? OverdriveScales_IsSrgb.x : OverdriveScales_IsSrgb.y;\n"
    "       adjustedScales.y = newColor.y > oldColor.y ? OverdriveScales_IsSrgb.x : OverdriveScales_IsSrgb.y;\n"
    "       adjustedScales.z = newColor.z > oldColor.z ? OverdriveScales_IsSrgb.x : OverdriveScales_IsSrgb.y;\n"

	// overdrive is tuned for gamma space so if we're in linear space fix gamma before doing the calculation
	"		vec3 overdriveColor;\n"
	"       if(OverdriveScales_IsSrgb.z > 0.0)\n"
	"		{\n"
	"           oldColor = pow(oldColor, vec3(1.0/2.2, 1.0/2.2, 1.0/2.2));\n"
	"			newColor = pow(newColor, vec3(1.0/2.2, 1.0/2.2, 1.0/2.2));\n"
    "			overdriveColor = clamp(newColor + (newColor - oldColor) * adjustedScales, 0.0, 1.0);\n"
    "           overdriveColor = pow(overdriveColor, vec3(2.2, 2.2, 2.2));\n"
	"		}\n"
	"		else\n"
	"			overdriveColor = clamp(newColor + (newColor - oldColor) * adjustedScales, 0.0, 1.0);\n"

    "       _MRTFRAGCOLOR1 = vec4(overdriveColor, 1.0);\n"
    "   }\n"
    "   #else\n"
    // If statement to keep OverdriveScales_IsSrgb from being optimized out.
    "   if(OverdriveScales_IsSrgb.x > 0.0)\n"
    "     _MRTFRAGCOLOR1 = vec4(newColor, 1);\n"
    "   #endif\n"
    "}\n";

    const OVR::CAPI::GL::ShaderBase::Uniform DistortionChroma_ps_refl[] =
    {
        { "OverdriveScales_IsSrgb", OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 12 },
    };
    
    static const char DistortionTimewarpChroma_vs[] =
    "uniform vec2 EyeToSourceUVScale;\n"
    "uniform vec2 EyeToSourceUVOffset;\n"
    "uniform mat4 EyeRotationStart;\n"
    "uniform mat4 EyeRotationEnd;\n"
    
    "_VS_IN vec2 Position;\n"
    "_VS_IN vec4 Color;\n"
    "_VS_IN vec2 TexCoord0;\n"
    "_VS_IN vec2 TexCoord1;\n"
    "_VS_IN vec2 TexCoord2;\n"
    
    "_VS_OUT vec4 oColor;\n"
    "_VS_OUT vec2 oTexCoord0;\n"
    "_VS_OUT vec2 oTexCoord1;\n"
    "_VS_OUT vec2 oTexCoord2;\n"
    
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
    "   mat3 EyeRotation;\n"
    "   EyeRotation[0] = mix ( EyeRotationStart[0], EyeRotationEnd[0], Color.a ).xyz;\n"
    "   EyeRotation[1] = mix ( EyeRotationStart[1], EyeRotationEnd[1], Color.a ).xyz;\n"
    "   EyeRotation[2] = mix ( EyeRotationStart[2], EyeRotationEnd[2], Color.a ).xyz;\n"
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
    "   oTexCoord1 = SrcCoordG;\n"
    "   oTexCoord2 = SrcCoordB;\n"
    
    "   oColor = vec4(Color.r, Color.r, Color.r, Color.r);\n"              // Used for vignette fade.
    "}\n";
    

    const OVR::CAPI::GL::ShaderBase::Uniform DistortionTimewarpChroma_vs_refl[] =
    {
        { "EyeToSourceUVScale",  OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 0, 8 },
        { "EyeToSourceUVOffset", OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 8, 8 },
        { "EyeRotationStart",    OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 16, 64 },
        { "EyeRotationEnd",      OVR::CAPI::GL::ShaderBase::VARTYPE_FLOAT, 80, 64 },
    };
    
}}} // OVR::CAPI::GL

#endif // OVR_CAPI_GL_Shaders_h
