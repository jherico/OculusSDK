float2   EyeToSourceUVScale;
float2   EyeToSourceUVOffset;

void main(in float2 Position    : POSITION,    in float4 Color       : COLOR0,
          in float2 TexCoord0   : TEXCOORD0,   in float2 TexCoord1   : TEXCOORD1, in float2 TexCoord2 : TEXCOORD2,
          out float4 oPosition  : SV_Position, out float4 oColor     : COLOR,     out float3 oTexCoord0 : TEXCOORD0,
          out float3 oTexCoord1 : TEXCOORD1,   out float3 oTexCoord2 : TEXCOORD2)
{
   oPosition.x = Position.x;
   oPosition.y = Position.y;
   oPosition.z = 0.5;
   oPosition.w = 1.0;

   // Scale them into  UV lookup space
   float2 tc0scaled = EyeToSourceUVScale * TexCoord0 + EyeToSourceUVOffset;
   float2 tc1scaled = EyeToSourceUVScale * TexCoord1 + EyeToSourceUVOffset;
   float2 tc2scaled = EyeToSourceUVScale * TexCoord2 + EyeToSourceUVOffset;

   oTexCoord0  = float3(tc0scaled, 1);      // R sample.
   oTexCoord1  = float3(tc1scaled, 1);      // G sample.
   oTexCoord2  = float3(tc2scaled, 1);      // B sample.
   oColor      = Color;                    // Used for vignette fade.
}

