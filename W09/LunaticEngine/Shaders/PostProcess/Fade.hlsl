#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"

cbuffer FadeBuffer : register(b2)
{
    float4 FadeColor;
    float FadeAmount;
    float3 _Pad;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 sceneColor = SceneColorTexture.SampleLevel(LinearClampSampler, input.uv, 0);
    float amount = saturate(FadeAmount);
    return float4(lerp(sceneColor.rgb, FadeColor.rgb, amount), sceneColor.a);
}
