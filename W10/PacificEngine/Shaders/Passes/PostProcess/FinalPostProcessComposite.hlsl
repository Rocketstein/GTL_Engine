#include "../../Utils/Functions.hlsl"
#include "../../Resources/SystemResources.hlsl"
#include "../../Resources/SystemSamplers.hlsl"

cbuffer FinalPostProcessCompositeParams : register(b2)
{
    float bVignettingEnabled;
    float VignetteIntensity;
    float VignetteRadius;
    float VignetteSoftness;

    float3 VignetteColor;
    float bGammaCorrectionEnabled;

    float DisplayGamma;
    float bFadeEnabled;
    float FadeAlpha;
    float bLetterboxEnabled;

    float3 FadeColor;
    float TargetAspectRatio;

    float ViewportAspectRatio;
    float LetterboxOpacity;
    float Padding0;
    float Padding1;

    float3 LetterboxColor;
    float Padding2;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float3 ApplyVignetting(float3 sceneColor, float2 uv)
{
    if (bVignettingEnabled <= 0.0f)
    {
        return sceneColor;
    }

    float2 centerUV = uv - float2(0.5f, 0.5f);
    float distanceFromCenter = length(centerUV) / length(float2(0.5f, 0.5f));
    float vignetteMask = smoothstep(
        VignetteRadius,
        VignetteRadius + max(VignetteSoftness, 0.0001f),
        distanceFromCenter);
    vignetteMask *= VignetteIntensity;

    return lerp(sceneColor, VignetteColor, saturate(vignetteMask));
}

float3 ApplyGammaCorrection(float3 sceneColor)
{
    if (bGammaCorrectionEnabled <= 0.0f)
    {
        return sceneColor;
    }

    return pow(saturate(sceneColor), 1.0f / max(DisplayGamma, 0.0001f));
}

float3 ApplyFade(float3 sceneColor)
{
    if (bFadeEnabled <= 0.0f)
    {
        return sceneColor;
    }

    return lerp(sceneColor, FadeColor, saturate(FadeAlpha));
}

float ComputeLetterboxMask(float2 uv)
{
    if (bLetterboxEnabled <= 0.0f)
    {
        return 0.0f;
    }

    float currentAspect = max(ViewportAspectRatio, 0.0001f);
    float targetAspect = max(TargetAspectRatio, 0.0001f);

    if (currentAspect < targetAspect)
    {
        float activeHeight = saturate(currentAspect / targetAspect);
        float barSize = (1.0f - activeHeight) * 0.5f;
        return saturate(step(uv.y, barSize) + step(1.0f - barSize, uv.y));
    }

    if (currentAspect > targetAspect)
    {
        float activeWidth = saturate(targetAspect / currentAspect);
        float barSize = (1.0f - activeWidth) * 0.5f;
        return saturate(step(uv.x, barSize) + step(1.0f - barSize, uv.x));
    }

    return 0.0f;
}

float3 ApplyLetterbox(float3 sceneColor, float2 uv)
{
    float mask = ComputeLetterboxMask(uv) * saturate(LetterboxOpacity);
    return lerp(sceneColor, LetterboxColor, mask);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 sceneColor = SceneColor.SampleLevel(LinearClampSampler, input.uv, 0);
    sceneColor.rgb = ApplyVignetting(sceneColor.rgb, input.uv);
    sceneColor.rgb = ApplyGammaCorrection(sceneColor.rgb);
    sceneColor.rgb = ApplyFade(sceneColor.rgb);
    sceneColor.rgb = ApplyLetterbox(sceneColor.rgb, input.uv);

    return sceneColor;
}
