float2   EyeToSourceUVScale;
float2   EyeToSourceUVOffset;

void main(in float2 Position   : POSITION,    in float4 Color : COLOR0,  in float2 TexCoord0 : TEXCOORD0,
          out float4 oPosition : SV_Position, out float4 oColor : COLOR, out float3 oTexCoord0 : TEXCOORD0)
{
   oPosition.x = Position.x;
   oPosition.y = Position.y;
   oPosition.z = 0.5;
   oPosition.w = 1.0;   
   oTexCoord0  = float3(EyeToSourceUVScale * TexCoord0 + EyeToSourceUVOffset, 1);
   oColor      = Color; // Used for vignette fade.
}

