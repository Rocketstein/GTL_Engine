#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D SourceTexture : register(t0);

cbuffer BloomDownsampleCB : register(b2)
{
    float2 SourceTexelSize;
    float2 _Pad;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float2 t = SourceTexelSize;
    float3 color = SourceTexture.SampleLevel(LinearClampSampler, input.uv, 0).rgb * 4.0f;
    color += SourceTexture.SampleLevel(LinearClampSampler, input.uv + float2(-t.x, -t.y), 0).rgb;
    color += SourceTexture.SampleLevel(LinearClampSampler, input.uv + float2( t.x, -t.y), 0).rgb;
    color += SourceTexture.SampleLevel(LinearClampSampler, input.uv + float2(-t.x,  t.y), 0).rgb;
    color += SourceTexture.SampleLevel(LinearClampSampler, input.uv + float2( t.x,  t.y), 0).rgb;

    return float4(color * 0.125f, 1.0f);
}
