#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D FontAtlas : register(t0);

float GetFontCoverage(float4 Sampled, float TintAlpha)
{
    if (TintAlpha < 0.0f)
    {
        return Sampled.r;
    }

    // Prefer the atlas alpha channel when present so UI text fade uses the
    // intended glyph coverage instead of any RGB bleed from filtering.
    if (Sampled.a > 0.0f)
    {
        return Sampled.a;
    }

    return max(Sampled.r, max(Sampled.g, Sampled.b));
}

PS_Input_TexColor VS(VS_Input_PTC input)
{
    PS_Input_TexColor output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_TexColor input) : SV_TARGET
{
    if (input.texcoord.x < 0.0f || input.texcoord.y < 0.0f)
    {
        return float4(input.color.rgb, 1.0f);
    }

    float4 col = FontAtlas.Sample(LinearClampSampler, input.texcoord);
    const float coverage = GetFontCoverage(col, input.color.a);

    if (coverage < 0.1f)
        discard;

    return float4(input.color.rgb, coverage * abs(input.color.a));
}
