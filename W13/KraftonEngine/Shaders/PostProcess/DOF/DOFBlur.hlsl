#include "PostProcess/DOF/DOFCommon.hlsli"

// t0 = CoC target (rgb = color, a = signed CoC).
Texture2D CoCTexture : register(t0);

static const float DOF_MAX_RADIUS = 16.0f;     // max bokeh radius in pixels
static const float GOLDEN_ANGLE   = 2.39996323f;

// rgb = blurred color, a = blurred coverage (for the composite blend).
float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 center = CoCTexture.SampleLevel(LinearClampSampler, input.uv, 0);
    float centerCoC = abs(center.a) * DOF_MAX_RADIUS;   // pixels

    float3 sum = center.rgb;
    float weight = 1.0f;

    int taps = max(DOFSample, 1);
    [loop]
    for (int i = 0; i < taps; ++i)
    {
        float t = (i + 0.5f) / taps;
        float r = sqrt(t) * DOF_MAX_RADIUS;             // fixed-radius disk, pixels
        float a = i * GOLDEN_ANGLE;
        float2 offset = float2(cos(a), sin(a)) * r * SourceTexelSize;

        float4 s = CoCTexture.SampleLevel(LinearClampSampler, input.uv + offset, 0);
        float sampleCoC = abs(s.a) * DOF_MAX_RADIUS;
        float effectiveCoC = (s.a < 0.0f) ? sampleCoC : min(sampleCoC, centerCoC);
        float w = saturate(effectiveCoC - r + 1.0f);

        sum += s.rgb * w;
        weight += w;
    }

    return float4(sum / weight, saturate(1.0f - 1.0f / weight));
}
