#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"

cbuffer GammaCorrectionBuffer : register(b2)
{
    float DisplayGamma;
    float BlendWeight;
    uint bUseSRGBCurve;
    float _Pad;
};

float3 LinearToSRGB(float3 Linear)
{
    Linear = saturate(Linear);
    return lerp(
        Linear * 12.92f,
        1.055f * pow(Linear, 1.0f / 2.4f) - 0.055f,
        step(0.0031308f, Linear)
    );
}

float3 LinearToDisplayGamma(float3 Linear)
{
    Linear = saturate(Linear);
    return pow(Linear, 1.0f / max(DisplayGamma, 0.001f));
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 SceneColor = SceneColorTexture.SampleLevel(LinearClampSampler, input.uv, 0);
    float3 CorrectedColor = bUseSRGBCurve != 0
        ? LinearToSRGB(SceneColor.rgb)
        : LinearToDisplayGamma(SceneColor.rgb);

    float3 OutputColor = lerp(SceneColor.rgb, CorrectedColor, saturate(BlendWeight));
    return float4(OutputColor, SceneColor.a);
}
