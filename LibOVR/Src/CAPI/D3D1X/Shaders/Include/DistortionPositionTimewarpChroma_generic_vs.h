/************************************************************************************

Filename    :   DistortionPositionTimewarpChroma_generic_vs.vsh

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
// ENABLE_DEPTH 0,1

#define ENABLE_LATE_LATCHING 1

#if ENABLE_LATE_LATCHING==1

float  EyeIndex = 0.0;

struct RingElement
{
    // Index represents eye index. Should be renamed to EyeStart, EyeEnd.
    float4x4 Start[2];
    float4x4 End[2];
    float4 Debug;
};

// Constant buffers must be aligned to 128 bits. This is why RingIndex is padded.
cbuffer RingStruct0 : register (b1)
{
    uint4 RingIndex0;
    RingElement RingData0[3]; // 3 Must match RingElementCount
};

#if NUM_LAYERS >= 2
cbuffer RingStruct1 : register (b2)
{
    uint4 RingIndex1;
    RingElement RingData1[3]; // 3 Must match RingElementCount
};
#if NUM_LAYERS >= 3
cbuffer RingStruct2 : register (b3)
{
    uint4 RingIndex2;
    RingElement RingData2[3]; // 3 Must match RingElementCount
};
#if NUM_LAYERS >= 4
cbuffer RingStruct3 : register (b4)
{
    uint4 RingIndex3;
    RingElement RingData3[3]; // 3 Must match RingElementCount
};
#endif // NUM_LAYERS >= 4
#endif // NUM_LAYERS >= 3
#endif // NUM_LAYERS >= 2

#endif // ENABLE_LATE_LATCHING==1

#if ENABLE_DEPTH==1

Texture2D             DepthTexture1x : register(t0);
Texture2DMS<float,2>  DepthTexture2x : register(t1);
Texture2DMS<float,4>  DepthTexture4x : register(t2);

// Only layer 0 may have a depth.
// DepthProjector values can also be calculated as:
// float DepthProjectorX = FarClip / (FarClip - NearClip);
// float DepthProjectorY = (-FarClip * NearClip) / (FarClip - NearClip) / ModelViewScale;
float2 DepthProjector;
// These are only used for the depth buffer.
float2 DepthEyeToSourceUVScale;
float2 DepthEyeToSourceUVOffset;
float DepthMsaaSamples = 1.0;       // set to zero to disable depth.
float4 DepthClipRect;

#endif //ENABLE_DEPTH==1

// One start and end per layer.
float4x4 EyeRotationStart[NUM_LAYERS];
float4x4 EyeRotationEnd[NUM_LAYERS];

float2 EyeToSourceUVScale;
float2 EyeToSourceUVOffset;


struct Coord32
{
    float2 R;
    float2 G;
    float2 B;
};

struct Coord33
{
    float3 R;
    float3 G;
    float3 B;
};

#if ENABLE_DEPTH==1
float4 PositionFromDepth(float2 inTexCoord, float4x4 rotMat)
{
    // Rotationally timewarp the depth buffer before reading it.
    float3 transformed = float3( mul ( rotMat, float4(inTexCoord,1,1) ).xyz);
    float2 depthTexCoord = transformed.xy / transformed.z;
    float2 eyeToSourceTexCoord = depthTexCoord * DepthEyeToSourceUVScale + DepthEyeToSourceUVOffset;
    
    // clamp to edge of the eye buffer
    eyeToSourceTexCoord = clamp(eyeToSourceTexCoord, DepthClipRect.xz, DepthClipRect.yw);

    float depth;
         if(DepthMsaaSamples <= 1.5)    depth = DepthTexture1x.Load(int3(eyeToSourceTexCoord, 0)).x;
    else if(DepthMsaaSamples <= 2.5)    depth = DepthTexture2x.Load(int2(eyeToSourceTexCoord), 0).x;
    else                                depth = DepthTexture4x.Load(int2(eyeToSourceTexCoord), 0).x;

    float linearDepth = DepthProjector.y / (depth - DepthProjector.x);
    float4 retVal = float4(inTexCoord, 1, 1);
    retVal.xyz *= linearDepth;
    return retVal;
}
#endif //ENABLE_DEPTH==1

float3 TimewarpTexCoordToWarpedPos ( float2 inTexCoord, float4x4 rotMat, int layerNum, bool layerHasDepth )
{
    // Vertex inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
    // These are now "real world" vectors in direction (x,y,1) relative to the eye of the HMD.
#if ENABLE_DEPTH==1
    if ( layerHasDepth )
    {
        float4 inputPos = PositionFromDepth(inTexCoord, rotMat);
        float3 transformed = float3( mul ( rotMat, inputPos ).xyz);
#if 0
        // Leaving this code in because you DON'T want to do it (and if I remove it, someone will try it again).
        // It looks mathematically identical to the enabled path, but the problem is that the transform we're
        // doing is NOT a linear one. So lerp-then-project gives a completely different result to project-then-lerp.
        // DON'T DO THIS, EVEN THOUGH IT LOOKS RIGHT.
        transformed.xy = transformed.xy * EyeToSourceUVScale.xy + EyeToSourceUVOffset.xy * transformed.z;
#else
        transformed.xy = ( transformed.xy / transformed.z );
        transformed.xy = transformed.xy * EyeToSourceUVScale.xy + EyeToSourceUVOffset.xy;
        // Make it into a dumb 2D non-projected lerp.
        transformed.z = 1.0f;
#endif
        // The divide of .xy by .z will be done in the pixel shader.
        return transformed;
    }
    else
#endif //ENABLE_DEPTH==1
    {
        float4 inputPos = float4(inTexCoord.xy, 1, 0);
        float4 transformed = mul ( rotMat, inputPos );
        // The divide of .xy by .z will be done in the pixel shader.
        return transformed.xyz;
    }
}

Coord33 TransformLayer ( int layerNum, float4x4 eyeStart, float4x4 eyeEnd, float timewarpLerpFactor, Coord32 inCoords, bool layerHasDepth )
{

    float4x4 lerpedEyeRot = lerp(eyeStart, eyeEnd, timewarpLerpFactor);

    Coord33 result;
    result.R = TimewarpTexCoordToWarpedPos ( inCoords.R, lerpedEyeRot, layerNum, layerHasDepth );
    result.G = TimewarpTexCoordToWarpedPos ( inCoords.G, lerpedEyeRot, layerNum, layerHasDepth );
    result.B = TimewarpTexCoordToWarpedPos ( inCoords.B, lerpedEyeRot, layerNum, layerHasDepth );
    return result;
}


void main(in float2 Position     : POSITION,
          in float2 iTexCoordR   : TEXCOORD0,
          in float2 iTexCoordG   : TEXCOORD1,
          in float2 iTexCoordB   : TEXCOORD2,
          in float2 VigetteTimewarp : TEXCOORD3,
          out float4 oPosition   : SV_Position,
          out float1 oColor      : COLOR,
          out float3 oTexCoord0R : TEXCOORD0,
          out float3 oTexCoord0G : TEXCOORD1,
          out float3 oTexCoord0B : TEXCOORD2
#if NUM_LAYERS >= 2
          ,
          out float3 oTexCoord1R : TEXCOORD3,
          out float3 oTexCoord1G : TEXCOORD4,
          out float3 oTexCoord1B : TEXCOORD5
#if NUM_LAYERS >= 3
          ,
          out float3 oTexCoord2R : TEXCOORD6,
          out float3 oTexCoord2G : TEXCOORD7,
          out float3 oTexCoord2B : TEXCOORD8
#if NUM_LAYERS >= 4
          ,
          out float3 oTexCoord3R : TEXCOORD9,
          out float3 oTexCoord3G : TEXCOORD10,
          out float3 oTexCoord3B : TEXCOORD11
#endif
#endif
#endif
          )
{
    oPosition.x = Position.x;
    oPosition.y = Position.y;
    oPosition.z = 0.5;
    oPosition.w = 1.0;
             
    float timewarpLerpFactor = VigetteTimewarp.y;

    Coord32 coordsIn;
    coordsIn.R = iTexCoordR;
    coordsIn.G = iTexCoordG;
    coordsIn.B = iTexCoordB;

#if ENABLE_DEPTH==1
    bool layer0HasDepth = ( DepthMsaaSamples > 0.5f );
#else
    bool layer0HasDepth = false;
#endif

#if ENABLE_LATE_LATCHING==1
    uint eyeIndexInt = uint(EyeIndex + 0.1);

    uint ringIndex   = RingIndex0[0];  // Only the first 4 bytes are used. The rest are for padding.
    float4x4 l0Begin = RingData0[ringIndex].Start[eyeIndexInt];
    float4x4 l0End   = RingData0[ringIndex].End[eyeIndexInt];
#else
    float4x4 l0Begin = EyeRotationStart[0];
    float4x4 l0End   = EyeRotationEnd[0];
#endif

    Coord33 layer0res = TransformLayer ( 0, l0Begin, l0End, timewarpLerpFactor, coordsIn, layer0HasDepth );
    oTexCoord0R = layer0res.R;
    oTexCoord0G = layer0res.G;
    oTexCoord0B = layer0res.B;

#if NUM_LAYERS >= 2

#if ENABLE_LATE_LATCHING==1
    ringIndex        = RingIndex1[0];  // Only the first 4 bytes are used. The rest are for padding.
    float4x4 l1Begin = RingData1[ringIndex].Start[eyeIndexInt];
    float4x4 l1End   = RingData1[ringIndex].End[eyeIndexInt];
#else
    float4x4 l1Begin = EyeRotationStart[1];
    float4x4 l1End   = EyeRotationEnd[1];
#endif

    Coord33 layer1res = TransformLayer ( 1, l1Begin, l1End, timewarpLerpFactor, coordsIn, false );
    oTexCoord1R = layer1res.R;
    oTexCoord1G = layer1res.G;
    oTexCoord1B = layer1res.B;

#if NUM_LAYERS >= 3

#if ENABLE_LATE_LATCHING==1
    ringIndex        = RingIndex2[0];  // Only the first 4 bytes are used. The rest are for padding.
    float4x4 l2Begin = RingData2[ringIndex].Start[eyeIndexInt];
    float4x4 l2End   = RingData2[ringIndex].End[eyeIndexInt];
#else
    float4x4 l2Begin = EyeRotationStart[2];
    float4x4 l2End   = EyeRotationEnd[2];
#endif

    Coord33 layer2res = TransformLayer ( 2, l2Begin, l2End, timewarpLerpFactor, coordsIn, false );
    oTexCoord2R = layer2res.R;
    oTexCoord2G = layer2res.G;
    oTexCoord2B = layer2res.B;

#if NUM_LAYERS >= 4

#if ENABLE_LATE_LATCHING==1
    ringIndex        = RingIndex3[0];  // Only the first 4 bytes are used. The rest are for padding.
    float4x4 l3Begin = RingData3[ringIndex].Start[eyeIndexInt];
    float4x4 l3End   = RingData3[ringIndex].End[eyeIndexInt];
#else
    float4x4 l3Begin = EyeRotationStart[3];
    float4x4 l3End   = EyeRotationEnd[3];
#endif

    Coord33 layer3res = TransformLayer ( 3, l3Begin, l3End, timewarpLerpFactor, coordsIn, false );
    oTexCoord3R = layer3res.R;
    oTexCoord3G = layer3res.G;
    oTexCoord3B = layer3res.B;
#endif // NUM_LAYERS >= 4
#endif // NUM_LAYERS >= 3
#endif // NUM_LAYERS >= 2

    oColor = VigetteTimewarp.x;              // Used for vignette fade.
}


