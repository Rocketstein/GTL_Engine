#include "Common/Functions.hlsli"
#include "Common/ConstantBuffers.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"


#define SceneColor SceneColorTexture
#define Sampler LinearClampSampler

cbuffer PostProcessMaterialBuffer : register(b2)
{
    float HitEffectIntensity;
    float3 _Pad;
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 color = SceneColor.Sample(Sampler, input.uv);

    if (HitEffectIntensity <= 0.0f)
    {
        return color;
    }

    float2 dist = (input.uv - 0.5f) * 2.0f;
    float vignette = length(dist);

    // Apply exponential curve for sharp edges and multiply by intensity
    float hitEffect = pow(saturate(vignette * 0.8f), 4.0) * HitEffectIntensity;

    // Blend with deep red
    float3 redColor = float3(0.8f, 0.0f, 0.0f);
    color.rgb = lerp(color.rgb, redColor, hitEffect);

    // Subtle overall red tint based on intensity
    color.rgb += redColor * HitEffectIntensity * 0.05f;

    return color;
}
