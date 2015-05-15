/************************************************************************************

Filename    :   DistortionChroma_generic_ps.psh

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

// #define the following symbols to these allowed values, then #include this file
// NUM_LAYERS 1,2,3,4
// ENABLE_HIGH_QUALITY 0,1
// OVERDRIVE_NOT_SECOND_ALPHA 0,1

Texture2D Texture0 : register(t0);
SamplerState Sampler0 : register(s0);
#if NUM_LAYERS >= 2
Texture2D Texture1 : register(t1);
SamplerState Sampler1 : register(s1);
#if NUM_LAYERS >= 3
Texture2D Texture2 : register(t2);
SamplerState Sampler2 : register(s2);
#if NUM_LAYERS >= 4
Texture2D Texture3 : register(t3);
SamplerState Sampler3 : register(s3);
#endif
#endif
#endif

#if OVERDRIVE_NOT_SECOND_ALPHA
// For overdrive
Texture2D LastTexture : register(t4);
//SamplerState LastSampler : register(s4);  // not needed, we use a Load()
Texture2D OverdriveLut : register(t5);
SamplerState OverdriveSampler : register(s5);
float3 OverdriveScales;
#endif

#if ENABLE_HIGH_QUALITY==1
float AaDerivativeMult[NUM_LAYERS];      // One per layer.
#endif

// Specified as left, right, top, bottom in UV coords.
// Set left>right to disable layer.
float4 ClipRect[NUM_LAYERS];

struct ColorAlpha
{
    // Each channel needs its own independent alpha value because of CA.
    float3 color;
    float3 alpha;
};

#if ENABLE_HIGH_QUALITY==1

// Fast approximate gamma to linear conversion when averaging colors
float3 ToLinear(float3 inColor) { return inColor * inColor; }
float3 ToGamma(float3 inColor)	{ return sqrt(inColor); }

void SampleStep(float2 texCoordR, float2 texCoordG, float2 texCoordB, float colorWeight, float2 texOffset,
				inout ColorAlpha totalColor, inout float totalWeight, Texture2D tex, SamplerState samp, float4 clipRect)
{
    ColorAlpha newColor;
    texCoordR += texOffset;
    texCoordG += texOffset;
    texCoordB += texOffset;

    // TODO: Change SampleLevel to Sample when we support mips for eye buffers
    // Always sample, otherwise you get moans about conditional gradients.
    float2 resultRA = tex.SampleLevel(samp, texCoordR, 0).ra;
    float2 resultGA = tex.SampleLevel(samp, texCoordG, 0).ga;
    float2 resultBA = tex.SampleLevel(samp, texCoordB, 0).ba;
    // ...but then maybe mask.
    // TODO: can we early-out by checking against comboMin like we do for low-qual?
    // clipRect is left, right, top, bottom.
    if ( ( texCoordR.x < clipRect.x ) || ( texCoordR.x > clipRect.y ) ||
         ( texCoordR.y < clipRect.z ) || ( texCoordR.y > clipRect.w ) )
    {
        resultRA = float2 ( 0.0, 0.0 );
    }
    if ( ( texCoordG.x < clipRect.x ) || ( texCoordG.x > clipRect.y ) ||
         ( texCoordG.y < clipRect.z ) || ( texCoordG.y > clipRect.w ) )
    {
        resultGA = float2 ( 0.0, 0.0 );
    }
    if ( ( texCoordB.x < clipRect.x ) || ( texCoordB.x > clipRect.y ) ||
         ( texCoordB.y < clipRect.z ) || ( texCoordB.y > clipRect.w ) )
    {
        resultBA = float2 ( 0.0, 0.0 );
    }

    newColor.color = ToLinear ( float3 ( resultRA.x, resultGA.x, resultBA.x ) );
    newColor.alpha =            float3 ( resultRA.y, resultGA.y, resultBA.y );

	totalColor.color += newColor.color * colorWeight;
	totalColor.alpha += newColor.alpha * colorWeight;
	totalWeight += colorWeight;
}

#if 0
// Tomf's interesting-but-still-not-right version
ColorAlpha ApplyHqAa(float2 texCoordR, float2 texCoordG, float2 texCoordB,
                     float aaDerivativeMult, Texture2D tex, SamplerState samp, float4 clipRect,
                     ColorAlpha centerSampleColorAlpha, float2 texSampleWidth)
{
	float totalWeight = 4;  // center sample gets 4x weight
	ColorAlpha totalColor;
    totalColor.color = ToLinear( centerSampleColorAlpha.color ) * totalWeight;
    totalColor.alpha =           centerSampleColorAlpha.alpha   * totalWeight;

    float2 texStep;
    
    float2 texStepX = float2(ddx(texCoordG.x),ddx(texCoordG.y));
    float2 texStepY = float2(ddy(texCoordG.x),ddy(texCoordG.y));
    float sizeXsq = dot ( texStepX, texStepX );
    float sizeYsq = dot ( texStepY, texStepY );
    if ( sizeXsq > sizeYsq )
    {
        float howMuch = sqrt(sizeXsq*rcp(sizeYsq)) - 1.0;
        texStep = howMuch * texStepX;
    }
    else
    {
        float howMuch = sqrt(sizeYsq*rcp(sizeXsq)) - 1.0;
        texStep = howMuch * texStepY;
    }

    //texStep *= 10.0;          // for visualisation
	float3 smplWgt = 1.0;
	SampleStep(texCoordR, texCoordG, texCoordB, smplWgt.z,  texStep, totalColor, totalWeight, tex, samp, clipRect);
	SampleStep(texCoordR, texCoordG, texCoordB, smplWgt.z, -texStep, totalColor, totalWeight, tex, samp, clipRect);

    totalColor.color = ToGamma(totalColor.color / totalWeight);
    totalColor.alpha =         totalColor.alpha / totalWeight;

	return totalColor;
}

#else

// Volga's original
ColorAlpha ApplyHqAa(float2 texCoordR, float2 texCoordG, float2 texCoordB,
                     float aaDerivativeMult, Texture2D tex, SamplerState samp, float4 clipRect,
                     ColorAlpha centerSampleColorAlpha, float2 texSampleWidth)
{
	float totalWeight = 4;  // center sample gets 4x weight
	ColorAlpha totalColor;
    totalColor.color = ToLinear( centerSampleColorAlpha.color ) * totalWeight;
    totalColor.alpha =           centerSampleColorAlpha.alpha   * totalWeight;

    float2 texStep = texSampleWidth * aaDerivativeMult;
    
	float3 smplExp = 1.0 / 3.0;
	float3 smplWgt = 1.0;

    //texStep *= 10.0;          // for visualisation
	SampleStep(texCoordR, texCoordG, texCoordB, smplWgt.x, -1.000 * smplExp.x * texStep, totalColor, totalWeight, tex, samp, clipRect);
//	SampleStep(texCoordR, texCoordG, texCoordB, smplWgt.y, -1.250 * smplExp.y * texStep, totalColor, totalWeight, tex, samp, clipRect);
	SampleStep(texCoordR, texCoordG, texCoordB, smplWgt.z, -1.875 * smplExp.z * texStep, totalColor, totalWeight, tex, samp, clipRect);
	SampleStep(texCoordR, texCoordG, texCoordB, smplWgt.z,  1.875 * smplExp.z * texStep, totalColor, totalWeight, tex, samp, clipRect);
//	SampleStep(texCoordR, texCoordG, texCoordB, smplWgt.y,  1.250 * smplExp.y * texStep, totalColor, totalWeight, tex, samp, clipRect);
	SampleStep(texCoordR, texCoordG, texCoordB, smplWgt.x,  1.000 * smplExp.x * texStep, totalColor, totalWeight, tex, samp, clipRect);

    totalColor.color = ToGamma(totalColor.color / totalWeight);
    totalColor.alpha =         totalColor.alpha / totalWeight;

	return totalColor;
}
#endif

#endif //ENABLE_HIGH_QUALITY==1


ColorAlpha SampleLayer (float3 texCoordRh, float3 texCoordGh, float3 texCoordBh, float4 clipRect, float aaDerivativeMult, Texture2D tex, SamplerState samp)
{
    // Transform to standard UV instead from homogenous
    // Perf note - in theory, we could do this inside all the cliprect and Z tests.
    // However, that means that gradients are dependent on conditionals, and
    // that can lead to perf problems. In practice it's better to have the UV coords
    // calculated unconditionally, outside.
    float2 texCoordR = texCoordRh.xy / texCoordRh.z;
    float2 texCoordG = texCoordGh.xy / texCoordGh.z;
    float2 texCoordB = texCoordBh.xy / texCoordBh.z;

    ColorAlpha res;
    res.color = float3(0.0, 0.0, 0.0);
    res.alpha = float3(0.0, 0.0, 0.0);

#if ENABLE_HIGH_QUALITY==1
    float2 texSampleWidth = fwidth(texCoordG);
#else //ENABLE_HIGH_QUALITY==1
    // Here we put this conditional into the #else because the compiler loves to move the fwidth() call
    // into the conditional and then spit out the "gradients in conditionals" warning
    // So we move the conditional below in HQ mode
    if ( clipRect.x < clipRect.y ) // Early-out for disabled layers.
#endif //ENABLE_HIGH_QUALITY==1
    {
        // Ignore reverse projections (backfaces, and surfaces behind the viewer)
        if ( ( texCoordRh.z > 0.01 ) && ( texCoordGh.z > 0.01 ) && ( texCoordBh.z > 0.01 ) )
        {
            // TODO: try various clipping methods for best performance.
            float2 comboMax = max ( max ( texCoordR, texCoordG ), texCoordB );
            float2 comboMin = min ( min ( texCoordR, texCoordG ), texCoordB );
            // clipRect is left, right, top, bottom.
            if ( ( comboMax.x > clipRect.x ) && ( comboMin.x < clipRect.y ) &&
                 ( comboMax.y > clipRect.z ) && ( comboMin.y < clipRect.w ) )
            {
#if ENABLE_HIGH_QUALITY==1
                if (clipRect.x < clipRect.y) // Early-out for disabled layers.
#endif //ENABLE_HIGH_QUALITY==1
                {
                    {
                        // TODO: Change SampleLevel to Sample when we support mips for eye buffers
                        // Always sample, otherwise you get moans about conditional gradients.
                        float2 resultRA = tex.Sample(samp, texCoordR).ra;
                        float2 resultGA = tex.Sample(samp, texCoordG).ga;
                        float2 resultBA = tex.Sample(samp, texCoordB).ba;

                        // ...but then maybe mask.
                        // clipRect is left, right, top, bottom.
                        if ( ( comboMin.x < clipRect.x ) || ( comboMax.x > clipRect.y ) ||
                             ( comboMin.y < clipRect.z ) || ( comboMax.y > clipRect.w ) )
                        {
                            if ( ( texCoordR.x < clipRect.x ) || ( texCoordR.x > clipRect.y ) ||
                                 ( texCoordR.y < clipRect.z ) || ( texCoordR.y > clipRect.w ) )
                            {
                                resultRA = float2 ( 0.0, 0.0 );
                            }
                            if ( ( texCoordG.x < clipRect.x ) || ( texCoordG.x > clipRect.y ) ||
                                 ( texCoordG.y < clipRect.z ) || ( texCoordG.y > clipRect.w ) )
                            {
                                resultGA = float2 ( 0.0, 0.0 );
                            }
                            if ( ( texCoordB.x < clipRect.x ) || ( texCoordB.x > clipRect.y ) ||
                                 ( texCoordB.y < clipRect.z ) || ( texCoordB.y > clipRect.w ) )
                            {
                                resultBA = float2 ( 0.0, 0.0 );
                            }
                        }
	                    res.color = float3(resultRA.x, resultGA.x, resultBA.x);
	                    res.alpha = float3(resultRA.y, resultGA.y, resultBA.y);
                    }

    #if ENABLE_HIGH_QUALITY==1
	                if(aaDerivativeMult > 0)
                    {
                        // directly returning res will result in HLSL compiler spewing incorrect warnings
                        res = ApplyHqAa(texCoordR, texCoordG, texCoordB, aaDerivativeMult, tex, samp, clipRect, res, texSampleWidth);

                        //if ( clipRect.x < clipRect.y ) // Early-out for disabled layers.
                        //{
                        //    res.color = float3(0, 0, 0);
                        //    res.alpha = float3(0, 0, 0);
                        //}
	                }
#endif //ENABLE_HIGH_QUALITY==1
                }
            }
        }
    }

    return res;
}


void   main(in float4 oPosition  : SV_Position,
            in float  oColor     : COLOR,
            in float3 oTexCoord0R : TEXCOORD0,
            in float3 oTexCoord0G : TEXCOORD1,
            in float3 oTexCoord0B : TEXCOORD2,
#if NUM_LAYERS >= 2
            in float3 oTexCoord1R : TEXCOORD3,
            in float3 oTexCoord1G : TEXCOORD4,
            in float3 oTexCoord1B : TEXCOORD5,
#if NUM_LAYERS >= 3
            in float3 oTexCoord2R : TEXCOORD6,
            in float3 oTexCoord2G : TEXCOORD7,
            in float3 oTexCoord2B : TEXCOORD8,
#if NUM_LAYERS >= 4
            in float3 oTexCoord3R : TEXCOORD9,
            in float3 oTexCoord3G : TEXCOORD10,
            in float3 oTexCoord3B : TEXCOORD11,
#endif
#endif
#endif
			out float4 outColor0 : SV_Target0,
			out float4 outColor1 : SV_Target1)
{
    float aaDerivativeMultLayer = -1.0f;
#if ENABLE_HIGH_QUALITY==1
    aaDerivativeMultLayer = AaDerivativeMult[0];
#endif
    ColorAlpha layerColor0 = SampleLayer ( oTexCoord0R, oTexCoord0G, oTexCoord0B, ClipRect[0], aaDerivativeMultLayer, Texture0, Sampler0 );
    float3 newColor = layerColor0.color;
    float3 newAlpha = layerColor0.alpha;

#if NUM_LAYERS >= 2
#if ENABLE_HIGH_QUALITY==1
    aaDerivativeMultLayer = AaDerivativeMult[1];
#endif
    ColorAlpha layerColor1 = SampleLayer ( oTexCoord1R, oTexCoord1G, oTexCoord1B, ClipRect[1], aaDerivativeMultLayer, Texture1, Sampler1 );
    // Premultiplied alpha blending.
    newColor = newColor * (1.0-layerColor1.alpha) + layerColor1.color;
    newAlpha = newAlpha * (1.0-layerColor1.alpha) + layerColor1.alpha;

#if NUM_LAYERS >= 3
#if ENABLE_HIGH_QUALITY==1
    aaDerivativeMultLayer = AaDerivativeMult[2];
#endif
    ColorAlpha layerColor2 = SampleLayer ( oTexCoord2R, oTexCoord2G, oTexCoord2B, ClipRect[2], aaDerivativeMultLayer, Texture2, Sampler2 );
    newAlpha = newAlpha * (1.0-layerColor2.alpha) + layerColor2.alpha;
    newColor = newColor * (1.0-layerColor2.alpha) + layerColor2.color;

#if NUM_LAYERS >= 4
#if ENABLE_HIGH_QUALITY==1
    aaDerivativeMultLayer = AaDerivativeMult[3];
#endif
    ColorAlpha layerColor3 = SampleLayer ( oTexCoord3R, oTexCoord3G, oTexCoord3B, ClipRect[3], aaDerivativeMultLayer, Texture3, Sampler3 );
    newColor = newColor * (1.0-layerColor3.alpha) + layerColor3.color;
    newAlpha = newAlpha * (1.0-layerColor3.alpha) + layerColor3.alpha;
#endif
#endif
#endif

    // Vignette fade.
	newColor = newColor * oColor.xxx;
	outColor0 = float4(newColor, 0.0);

#if OVERDRIVE_NOT_SECOND_ALPHA
	// pixel luminance overdrive output to outColor1
	outColor0 = float4(newColor, newAlpha.g);
	outColor1 = outColor0;
	if(OverdriveScales.x > 0)
	{
		float3 oldColor = LastTexture.Load(int3(oPosition.xy, 0)).rgb;

        // The code in DistortionChroma_generic_ps.psh and JustOverdrive_ps.psh REALLY needs to match!
		
        float3 overdriveColor;

        // x < 1.5 means "use analytical model instead of LUT"
        if(OverdriveScales.x < 1.5)
        {
		    float3 adjustedScales;
		    adjustedScales.x = newColor.x > oldColor.x ? OverdriveScales.y : OverdriveScales.z;
    		adjustedScales.y = newColor.y > oldColor.y ? OverdriveScales.y : OverdriveScales.z;
		    adjustedScales.z = newColor.z > oldColor.z ? OverdriveScales.y : OverdriveScales.z;
		    overdriveColor = saturate(newColor + (newColor - oldColor) * adjustedScales);
        }
		else
        {
            overdriveColor.r = OverdriveLut.Sample(OverdriveSampler, float2(newColor.r, oldColor.r)).r;
		    overdriveColor.g = OverdriveLut.Sample(OverdriveSampler, float2(newColor.g, oldColor.g)).g;
		    overdriveColor.b = OverdriveLut.Sample(OverdriveSampler, float2(newColor.b, oldColor.b)).b;
        }

		outColor1 = float4(overdriveColor, outColor0.a);
	}
#else
	// pixel alpha output to outColor1 for dual-source-color blending
	outColor1 = float4 ( newAlpha, 0.0 );
#endif
}
