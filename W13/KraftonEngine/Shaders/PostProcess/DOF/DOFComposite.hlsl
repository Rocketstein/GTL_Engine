#include "PostProcess/DOF/DOFCommon.hlsli"

Texture2D SharpTexture : register(t0);   // SceneColorCopy
Texture2D BlurTexture  : register(t1);   // blurred result (a = coverage)

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 sharp = SharpTexture.SampleLevel(LinearClampSampler, input.uv, 0);
    float4 blur  = BlurTexture.SampleLevel(LinearClampSampler, input.uv, 0);

    // blur.a is the blurred coverage (includes foreground spill onto sharp pixels).
    float3 color = lerp(sharp.rgb, blur.rgb, saturate(blur.a));
    return float4(color, sharp.a);
}
