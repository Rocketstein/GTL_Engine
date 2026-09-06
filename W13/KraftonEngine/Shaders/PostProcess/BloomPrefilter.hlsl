#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D SourceTexture : register(t0);

cbuffer BloomPrefilterCB : register(b2)
{
    float Threshold;
    float SoftKnee;
    float2 SourceTexelSize;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float3 ApplyBloomThreshold(float3 color)
{
    float brightness = max(max(color.r, color.g), color.b);
    float knee = max(Threshold * SoftKnee, 0.00001f);
    float soft = clamp(brightness - Threshold + knee, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee);

    float contribution = max(soft, brightness - Threshold) / max(brightness, 0.00001f);
    return color * saturate(contribution);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float2 texel = SourceTexelSize;
    float3 bloom = ApplyBloomThreshold(SourceTexture.SampleLevel(LinearClampSampler, input.uv, 0).rgb) * 4.0f;
    bloom += ApplyBloomThreshold(SourceTexture.SampleLevel(LinearClampSampler, input.uv + float2(-texel.x, -texel.y), 0).rgb);
    bloom += ApplyBloomThreshold(SourceTexture.SampleLevel(LinearClampSampler, input.uv + float2( texel.x, -texel.y), 0).rgb);
    bloom += ApplyBloomThreshold(SourceTexture.SampleLevel(LinearClampSampler, input.uv + float2(-texel.x,  texel.y), 0).rgb);
    bloom += ApplyBloomThreshold(SourceTexture.SampleLevel(LinearClampSampler, input.uv + float2( texel.x,  texel.y), 0).rgb);
    return float4(bloom * 0.125f, 1.0f);
}
