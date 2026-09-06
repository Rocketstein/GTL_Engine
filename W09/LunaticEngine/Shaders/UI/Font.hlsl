#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D FontAtlas : register(t0);

float GetFontCoverage(float4 Sampled, float TintAlpha)
{
    if (TintAlpha < 0.0f)
    {
        return Sampled.r;
    }

    // Prefer the atlas alpha channel when present so text fade follows the
    // bitmap font coverage instead of filtered RGB values.
    if (Sampled.a > 0.0f)
    {
        return Sampled.a;
    }

    return max(Sampled.r, max(Sampled.g, Sampled.b));
}

PS_Input_TexColor VS(VS_Input_PTC input)
{
    PS_Input_TexColor output;
    output.position = ApplyVP(input.position);
    output.color = input.color;
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_TexColor input) : SV_TARGET
{
    if (input.texcoord.x < 0.0f || input.texcoord.y < 0.0f)
    {
        return float4(ApplyWireframe(input.color.rgb), 1.0f);
    }

    float4 col = FontAtlas.Sample(LinearClampSampler, input.texcoord);
    const float coverage = GetFontCoverage(col, input.color.a);
    if (!bIsWireframe && ShouldDiscardFontPixel(coverage))
        discard;

    const float3 tinted = input.color.rgb;
    return float4(ApplyWireframe(tinted), bIsWireframe ? 1.0f : (coverage * abs(input.color.a)));
}
