#include "PostProcess/DOF/DOFCommon.hlsli"

Texture2D SceneTexture : register(t0);
Texture2D DepthTexture : register(t1);


// rgb = scene color, a = signed CoC.
float4 PS(PS_Input_UV input) : SV_TARGET
{
    float3 color = SceneTexture.SampleLevel(LinearClampSampler, input.uv, 0).rgb;

    // Offsets to the four full-res texel centres inside this half-res texel.
    float2 o = SourceTexelSize * 0.5f;
    float d0 = DepthTexture.SampleLevel(PointClampSampler, input.uv + float2(-o.x, -o.y), 0).r;
    float d1 = DepthTexture.SampleLevel(PointClampSampler, input.uv + float2( o.x, -o.y), 0).r;
    float d2 = DepthTexture.SampleLevel(PointClampSampler, input.uv + float2(-o.x,  o.y), 0).r;
    float d3 = DepthTexture.SampleLevel(PointClampSampler, input.uv + float2( o.x,  o.y), 0).r;

    float coc = ComputeCoC(LinearizeDepth(d0));
    float c1 = ComputeCoC(LinearizeDepth(d1));
    float c2 = ComputeCoC(LinearizeDepth(d2));
    float c3 = ComputeCoC(LinearizeDepth(d3));
    if (abs(c1) > abs(coc)) coc = c1;
    if (abs(c2) > abs(coc)) coc = c2;
    if (abs(c3) > abs(coc)) coc = c3;

    return float4(color, coc);
}
