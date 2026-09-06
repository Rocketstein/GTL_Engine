#include "Common/Functions.hlsli"
#include "Common/ConstantBuffers.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"

cbuffer PostProcessMaterialBuffer : register(b2)
{
    float HitEffectIntensity;
    float ChromaticAberration;
    float2 _Pad;
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float2 uv = input.uv;

    if (ChromaticAberration > 0.0f)
    {
        float2 dir = uv - 0.5f;
        float amount = ChromaticAberration * 0.01f;
        float r = SceneColorTexture.Sample(LinearClampSampler, uv + dir * amount).r;
        float g = SceneColorTexture.Sample(LinearClampSampler, uv).g;
        float b = SceneColorTexture.Sample(LinearClampSampler, uv - dir * amount).b;
        float a = SceneColorTexture.Sample(LinearClampSampler, uv).a;
        float4 color = float4(r, g, b, a);

        float vignette = pow(saturate(length(dir * 2.0f) * 0.8f), 4.0f);
        float hitEffect = vignette * HitEffectIntensity;
        float3 hitColor = float3(0.8f, 0.0f, 0.0f);
        color.rgb = lerp(color.rgb, hitColor, saturate(hitEffect));
        color.rgb += hitColor * HitEffectIntensity * 0.05f;
        return color;
    }

    float4 baseColor = SceneColorTexture.Sample(LinearClampSampler, uv);
    if (HitEffectIntensity <= 0.0f)
    {
        return baseColor;
    }

    float2 dist = (uv - 0.5f) * 2.0f;
    float vignetteOnly = pow(saturate(length(dist) * 0.8f), 4.0f);
    float hitOnly = vignetteOnly * HitEffectIntensity;
    float3 redColor = float3(0.8f, 0.0f, 0.0f);
    baseColor.rgb = lerp(baseColor.rgb, redColor, saturate(hitOnly));
    baseColor.rgb += redColor * HitEffectIntensity * 0.05f;
    return baseColor;
}
