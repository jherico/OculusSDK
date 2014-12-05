/************************************************************************************

Filename    :   DistortionCS2x2Pentile.vsh

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


// Note - the only difference between the various Distortion Compute Shaders is these #defines.
// The code is otherwise identical, so if you change one, rememeber to change the others!
#define PENTILE_MODE 0
#define ENABLE_OVERLAY 0
#define ENABLE_TIMEWARP 1


#define GRID_SIZE_IN_PIXELS 16
#define PINS_PER_EDGE 128
#define NXN_BLOCK_SIZE_PIXELS 2
#define SIMD_SQUARE_SIZE 16


struct DistortionComputePin
{
	float2      TanEyeAnglesR;  
	float2      TanEyeAnglesG;  
	float2      TanEyeAnglesB;  
	int         Color;          
	int         Padding[1];        
};
struct DistortionComputePinUnpacked
{
	float2      TanEyeAnglesR;  
	float2      TanEyeAnglesG;  
	float2      TanEyeAnglesB;  
	float       TimewarpLerp;   
	float       Fade;           
};
struct DistortionComputePinTimewarped
{
	float2      HmdSpcTexCoordR;  
	float2      HmdSpcTexCoordG;  
	float2      HmdSpcTexCoordB;
#if ENABLE_OVERLAY
	float2      OverlayTexCoordR;
	float2      OverlayTexCoordG;
	float2      OverlayTexCoordB;
#endif
};

// Cut'n'pasted from D3DX_DXGIFormatConvert.inl. Obviously we should have #included it, but...
typedef float4 XMFLOAT4;                                                   
typedef uint UINT;                                                         
#define D3DX11INLINE                                                       
#define hlsl_precise precise                                               
D3DX11INLINE XMFLOAT4 D3DX_R8G8B8A8_UNORM_to_FLOAT4(UINT packedInput)      
{                                                                          
	hlsl_precise XMFLOAT4 unpackedOutput;                                  
	unpackedOutput.x = (FLOAT)  (packedInput      & 0x000000ff)  / 255;    
	unpackedOutput.y = (FLOAT)(((packedInput>> 8) & 0x000000ff)) / 255;    
	unpackedOutput.z = (FLOAT)(((packedInput>>16) & 0x000000ff)) / 255;    
	unpackedOutput.w = (FLOAT)  (packedInput>>24)                / 255;    
	return unpackedOutput;                                                 
}                                                                          

DistortionComputePinUnpacked UnpackPin ( DistortionComputePin src )       
{                                                                                      
	DistortionComputePinUnpacked result;                                         
	result.TanEyeAnglesR = src.TanEyeAnglesR;                                          
	result.TanEyeAnglesG = src.TanEyeAnglesG;                                          
	result.TanEyeAnglesB = src.TanEyeAnglesB;                                          
	float4 tempColor = D3DX_R8G8B8A8_UNORM_to_FLOAT4 ( src.Color );                    
	result.Fade = tempColor.r * 2.0 - 1.0;                                             
	result.TimewarpLerp = tempColor.a;                                                 
	return result;                                                                     
}                                                                                      


float4x4 Padding1;
float4x4 Padding2;
float2 EyeToSourceUVScale;
float2 EyeToSourceUVOffset;
float3x3 EyeRotationStart;
float3x3 EyeRotationEnd;
float  UseOverlay = 1;
float  RightEye = 1;
float  FbSizePixelsX;


RWTexture2D<float4> Framebuffer : register(u0);
SamplerState Linear : register(s0);
// Subtlety - fill->Set stops at the first NULL texture, so make sure you order them by priority!
Texture2D HmdSpcTexture : register(t0);
Texture2D OverlayTexture : register(t1);
// t1, t2, t3 for layers in future.
// This is set by other calls, so no problem putting it in t4.
StructuredBuffer<DistortionComputePin> UntransformedGridPins : register(t4);

// Each eye has a grid of "pins" - spaced every gridSizeInPixels apart in a square grid.
// You can think of them as vertices in a mesh, but they are regularly
// distributed in screen space, not pre-distorted.
// Pins are laid out in a vertical scanline pattern,
// scanning right to left, then within each scan going top to bottom, like DK2.
// If we move to a different panel orientation, we may need to flip this around.
// pinsPerEdge is the pitch of the buffer, and is fixed whatver the resolution
// - it just needs to be large enough for the largest res we support.

// The grid size remains fixed, but now each shader invocation does an NxN "tile" of output pixels.
// This allows it to read, timewarp & project the input pins just once, then interpolate final UV over a number of pixels.
// The "SIMD square size" is how large the square of dispatched pixels is - it's the
// thing set in numthreads(N,N,1). For a SIMD size of 64-wide, it needs to be more than 8,
// otherwise we'll starve the machine.

// Summary:
// Each SIMD lane does a tileBlockSizePixels^2 of pixels.
// Each "CS group" (i.e. a virtualized SIMD thread) will cover a (simdSquareSize*tileBlockSizePixels)^2 block of pixels.
// The pin grid is unaffected by any of these (except it has to be larger than tileBlockSizePixels).
static const int gridSizeInPixels = GRID_SIZE_IN_PIXELS;
static const int pinsPerEdge = PINS_PER_EDGE;
static const int tileBlockSizePixels = NXN_BLOCK_SIZE_PIXELS;
static const int simdSquareSize = SIMD_SQUARE_SIZE;
static const int tilesPerGridSide = gridSizeInPixels / tileBlockSizePixels;

DistortionComputePinTimewarped WarpAndDistort ( DistortionComputePinUnpacked inp )
{
	// Pin inputs are in TanEyeAngle space for the R,G,B channels (i.e. after chromatic aberration and distortion).
#if ENABLE_TIMEWARP
	// These are now "real world" vectors in direction (x,y,1) relative to the eye of the HMD.
	float3 TanEyeAngle3R = float3 ( inp.TanEyeAnglesR.x, inp.TanEyeAnglesR.y, 1.0 );
	float3 TanEyeAngle3G = float3 ( inp.TanEyeAnglesG.x, inp.TanEyeAnglesG.y, 1.0 );
	float3 TanEyeAngle3B = float3 ( inp.TanEyeAnglesB.x, inp.TanEyeAnglesB.y, 1.0 );

	// Apply the two 3x3 timewarp rotations to these vectors.
	float3 TransformedRStart = mul ( EyeRotationStart, TanEyeAngle3R );
	float3 TransformedGStart = mul ( EyeRotationStart, TanEyeAngle3G );
	float3 TransformedBStart = mul ( EyeRotationStart, TanEyeAngle3B );
	float3 TransformedREnd   = mul ( EyeRotationEnd,   TanEyeAngle3R );
	float3 TransformedGEnd   = mul ( EyeRotationEnd,   TanEyeAngle3G );
	float3 TransformedBEnd   = mul ( EyeRotationEnd,   TanEyeAngle3B );
	// And blend between them.
	float3 TransformedR = lerp ( TransformedRStart, TransformedREnd, inp.TimewarpLerp );
	float3 TransformedG = lerp ( TransformedGStart, TransformedGEnd, inp.TimewarpLerp );
	float3 TransformedB = lerp ( TransformedBStart, TransformedBEnd, inp.TimewarpLerp );

	// Project them back onto the Z=1 plane of the rendered images.
	float RecipZR = rcp ( TransformedR.z );
	float RecipZG = rcp ( TransformedG.z );
	float RecipZB = rcp ( TransformedB.z );
	float2 FlattenedR = float2 ( TransformedR.x * RecipZR, TransformedR.y * RecipZR );
	float2 FlattenedG = float2 ( TransformedG.x * RecipZG, TransformedG.y * RecipZG );
	float2 FlattenedB = float2 ( TransformedB.x * RecipZB, TransformedB.y * RecipZB );
#else
    float2 FlattenedR = inp.TanEyeAnglesR;
    float2 FlattenedG = inp.TanEyeAnglesG;
    float2 FlattenedB = inp.TanEyeAnglesB;
#endif

	DistortionComputePinTimewarped result;

	// These are now still in TanEyeAngle space.
	// Scale them into the correct [0-1],[0-1] UV lookup space (depending on eye)
	result.HmdSpcTexCoordR = FlattenedR * EyeToSourceUVScale + EyeToSourceUVOffset;
	result.HmdSpcTexCoordG = FlattenedG * EyeToSourceUVScale + EyeToSourceUVOffset;
	result.HmdSpcTexCoordB = FlattenedB * EyeToSourceUVScale + EyeToSourceUVOffset;

#if ENABLE_OVERLAY
	// Static layer texcoords don't get any time warp offset
	result.OverlayTexCoordR = inp.TanEyeAnglesR * EyeToSourceUVScale + EyeToSourceUVOffset;
	result.OverlayTexCoordG = inp.TanEyeAnglesG * EyeToSourceUVScale + EyeToSourceUVOffset;
	result.OverlayTexCoordB = inp.TanEyeAnglesB * EyeToSourceUVScale + EyeToSourceUVOffset;
#endif
	return result;
}

float3 FindPixelColour ( float2 pinFrac,
                         DistortionComputePinUnpacked PinTL,
                         DistortionComputePinUnpacked PinTR,
                         DistortionComputePinUnpacked PinBL,
                         DistortionComputePinUnpacked PinBR,
                         DistortionComputePinTimewarped PinWarpTL,
                         DistortionComputePinTimewarped PinWarpTR,
                         DistortionComputePinTimewarped PinWarpBL,
                         DistortionComputePinTimewarped PinWarpBR)
{
	float pinWeightTL = (1.0-pinFrac.x) * (1.0-pinFrac.y);                                 
	float pinWeightTR = (    pinFrac.x) * (1.0-pinFrac.y);                                 
	float pinWeightBL = (1.0-pinFrac.x) * (    pinFrac.y);                                 
	float pinWeightBR = (    pinFrac.x) * (    pinFrac.y);                                 
                                                                                                               
	float Fade = ( PinTL.Fade * pinWeightTL ) +                                                 
				 ( PinTR.Fade * pinWeightTR ) +                                                 
				 ( PinBL.Fade * pinWeightBL ) +                                                 
				 ( PinBR.Fade * pinWeightBR );                                                  
	float2 HmdSpcTexCoordR = ( PinWarpTL.HmdSpcTexCoordR * pinWeightTL ) +                      
							 ( PinWarpTR.HmdSpcTexCoordR * pinWeightTR ) +                      
							 ( PinWarpBL.HmdSpcTexCoordR * pinWeightBL ) +                      
							 ( PinWarpBR.HmdSpcTexCoordR * pinWeightBR );                       
	float2 HmdSpcTexCoordG = ( PinWarpTL.HmdSpcTexCoordG * pinWeightTL ) +                      
							 ( PinWarpTR.HmdSpcTexCoordG * pinWeightTR ) +                      
							 ( PinWarpBL.HmdSpcTexCoordG * pinWeightBL ) +                      
							 ( PinWarpBR.HmdSpcTexCoordG * pinWeightBR );                       
	float2 HmdSpcTexCoordB = ( PinWarpTL.HmdSpcTexCoordB * pinWeightTL ) +                      
							 ( PinWarpTR.HmdSpcTexCoordB * pinWeightTR ) +                      
							 ( PinWarpBL.HmdSpcTexCoordB * pinWeightBL ) +                      
							 ( PinWarpBR.HmdSpcTexCoordB * pinWeightBR );                       

	float3 finalColor;

#if PENTILE_MODE > 0
    // R & B channels have a 0.5 bias because of fewer pels.
    const float mipBiasRB = 0.5;
#else
    const float mipBiasRB = 0.0;
#endif
	finalColor.r = HmdSpcTexture.SampleLevel(Linear, HmdSpcTexCoordR, mipBiasRB).r;
	finalColor.g = HmdSpcTexture.SampleLevel(Linear, HmdSpcTexCoordG, 0        ).g;
	finalColor.b = HmdSpcTexture.SampleLevel(Linear, HmdSpcTexCoordB, mipBiasRB).b;
#if ENABLE_OVERLAY
	if(UseOverlay > 0)
	{
		float2 OverlayTexCoordR = ( PinWarpTL.OverlayTexCoordR * pinWeightTL ) +                
								  ( PinWarpTR.OverlayTexCoordR * pinWeightTR ) +                
								  ( PinWarpBL.OverlayTexCoordR * pinWeightBL ) +                
								  ( PinWarpBR.OverlayTexCoordR * pinWeightBR );                 
		float2 OverlayTexCoordG = ( PinWarpTL.OverlayTexCoordG * pinWeightTL ) +                
								  ( PinWarpTR.OverlayTexCoordG * pinWeightTR ) +                
								  ( PinWarpBL.OverlayTexCoordG * pinWeightBL ) +                
								  ( PinWarpBR.OverlayTexCoordG * pinWeightBR );                 
		float2 OverlayTexCoordB = ( PinWarpTL.OverlayTexCoordB * pinWeightTL ) +                
								  ( PinWarpTR.OverlayTexCoordB * pinWeightTR ) +                
								  ( PinWarpBL.OverlayTexCoordB * pinWeightBL ) +                
								  ( PinWarpBR.OverlayTexCoordB * pinWeightBR );                 
		float2 overlayColorR = OverlayTexture.SampleLevel(Linear, OverlayTexCoordR, mipBiasRB).ra;
		float2 overlayColorG = OverlayTexture.SampleLevel(Linear, OverlayTexCoordG, 0        ).ga;
		float2 overlayColorB = OverlayTexture.SampleLevel(Linear, OverlayTexCoordB, mipBiasRB).ba;
		// do premultiplied alpha blending - overlayColorX.x is color, overlayColorX.y is alpha
		finalColor.r = finalColor.r * saturate(1-overlayColorR.y) + overlayColorR.x;
		finalColor.g = finalColor.g * saturate(1-overlayColorG.y) + overlayColorG.x;
		finalColor.b = finalColor.b * saturate(1-overlayColorB.y) + overlayColorB.x;
	}
#endif
	finalColor.rgb = saturate(finalColor.rgb * Fade);
	return finalColor;
}


[numthreads(simdSquareSize, simdSquareSize, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
// Reminder:
// GroupThreadID.xy will range from 0 to (simdSquareSize-1).
// GroupID.xy will range from 0 to (screen_size.xy)/(simdSquareSize*tileBlockSizePixels)
// DispatchThreadID.xy = GroupID.xy * simdSquareSize + GroupThreadID.xy
{
    int2 PixelPosTile = DTid.xy * tileBlockSizePixels;                                                          
    float2 PixelPosFloat = (float2)PixelPosTile;                                                                
    float2 pinFracTileStart = (float2)PixelPosTile * ( 1.0 / gridSizeInPixels );                             
    float2 pinWholeTileStart = floor ( pinFracTileStart );                                                
    pinFracTileStart -= pinWholeTileStart;                                                                
    int2 pinInt = (int2)pinWholeTileStart;                                                                
    pinInt.x = (0.5*FbSizePixelsX/gridSizeInPixels - 1) - pinInt.x;                                           
    pinFracTileStart.x = 1.0 - pinFracTileStart.x;                                                        
    if ( RightEye > 0.5 )
    {
        PixelPosTile.x += 0.5*FbSizePixelsX;
    }

    int pinIndexTL = pinInt.x*pinsPerEdge + pinInt.y;                                                 
    int pinIndexTR = pinIndexTL + pinsPerEdge;                                                           
    int pinIndexBL = pinIndexTL + 1;                                                                      
    int pinIndexBR = pinIndexTR + 1;                                                                      
    DistortionComputePinUnpacked PinTL = UnpackPin ( UntransformedGridPins[pinIndexTL] );        
    DistortionComputePinUnpacked PinTR = UnpackPin ( UntransformedGridPins[pinIndexTR] );        
    DistortionComputePinUnpacked PinBL = UnpackPin ( UntransformedGridPins[pinIndexBL] );        
    DistortionComputePinUnpacked PinBR = UnpackPin ( UntransformedGridPins[pinIndexBR] );        
    if ( ( PinTL.Fade > 0.0 ) ||                                                                               
         ( PinTR.Fade > 0.0 ) ||                                                                               
         ( PinBL.Fade > 0.0 ) ||                                                                               
         ( PinBR.Fade > 0.0 ) )                                                                                
    {
        DistortionComputePinTimewarped PinWarpTL = WarpAndDistort ( PinTL );                            
        DistortionComputePinTimewarped PinWarpTR = WarpAndDistort ( PinTR );                            
        DistortionComputePinTimewarped PinWarpBL = WarpAndDistort ( PinBL );                            
        DistortionComputePinTimewarped PinWarpBR = WarpAndDistort ( PinBR );                            

        float2 pinFrac;                                                                          
        int2 PixelPos;
        pinFrac.x = pinFracTileStart.x;                                        
        pinFrac.y = pinFracTileStart.y;                                        
        float3 finalColor00 = FindPixelColour ( pinFrac,                                               
                                                PinTL,
                                                PinTR,
                                                PinBL,
                                                PinBR,
                                                PinWarpTL,
                                                PinWarpTR,
                                                PinWarpBL,
                                                PinWarpBR);
        pinFrac.x = pinFracTileStart.x - (1.0 / gridSizeInPixels);                                        
        pinFrac.y = pinFracTileStart.y;                                        
        float3 finalColor01 = FindPixelColour ( pinFrac,                                               
                                                PinTL,
                                                PinTR,
                                                PinBL,
                                                PinBR,
                                                PinWarpTL,
                                                PinWarpTR,
                                                PinWarpBL,
                                                PinWarpBR);
        pinFrac.x = pinFracTileStart.x;                                        
        pinFrac.y = pinFracTileStart.y + (1.0 / gridSizeInPixels);                                        
        float3 finalColor10 = FindPixelColour ( pinFrac,                                               
                                                PinTL,
                                                PinTR,
                                                PinBL,
                                                PinBR,
                                                PinWarpTL,
                                                PinWarpTR,
                                                PinWarpBL,
                                                PinWarpBR);
        pinFrac.x = pinFracTileStart.x - (1.0 / gridSizeInPixels);                                        
        pinFrac.y = pinFracTileStart.y + (1.0 / gridSizeInPixels);                                        
        float3 finalColor11 = FindPixelColour ( pinFrac,                                               
                                                PinTL,
                                                PinTR,
                                                PinBL,
                                                PinBR,
                                                PinWarpTL,
                                                PinWarpTR,
                                                PinWarpBL,
                                                PinWarpBR);

        float3 finalOut00;
        float3 finalOut01;
        float3 finalOut10;
        float3 finalOut11;
#if PENTILE_MODE==0
        // No pentile, so it's easy.
        finalOut00 = finalColor00;
        finalOut01 = finalColor01;
        finalOut10 = finalColor10;
        finalOut11 = finalColor11;
#elif PENTILE_MODE==1
        // Now the DK2 pentile swizzle. Don't try to understand it; just rope, throw and brand it.
        finalOut00.g = finalColor10.g;
        finalOut01.g = finalColor01.g;
        finalOut10.g = finalColor00.g;
        finalOut11.g = finalColor11.g;
        finalOut00.r = finalColor10.r;
        finalOut01.r = finalColor01.r;
        finalOut10.r = finalColor00.b;
        finalOut11.r = finalColor11.b;
        finalOut00.b = 0.0;
        finalOut01.b = 0.0;
        finalOut10.b = 0.0;
        finalOut11.b = 0.0;
#endif

        PixelPos.x = PixelPosTile.x;
        PixelPos.y = PixelPosTile.y;
        Framebuffer[PixelPos.xy] = float4 ( finalOut00, 0.0 );
        PixelPos.x = PixelPosTile.x + 1;
        PixelPos.y = PixelPosTile.y;
        Framebuffer[PixelPos.xy] = float4 ( finalOut01, 0.0 );
        PixelPos.x = PixelPosTile.x;
        PixelPos.y = PixelPosTile.y + 1;
        Framebuffer[PixelPos.xy] = float4 ( finalOut10, 0.0 );
        PixelPos.x = PixelPosTile.x + 1;
        PixelPos.y = PixelPosTile.y + 1;
        Framebuffer[PixelPos.xy] = float4 ( finalOut11, 0.0 );
    }
};



