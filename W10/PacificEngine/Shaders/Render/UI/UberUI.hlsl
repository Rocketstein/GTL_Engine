#include "../../Utils/Functions.hlsl"
#include "../../Resources/SystemSamplers.hlsl"

struct VS_Input_UI
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR;
};

struct PS_Input_UI
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR;
};

cbuffer UIParams : register(b2)
{
    float4 UIFlags;
};

Texture2D UITexture : register(t0);

PS_Input_UI VS(VS_Input_UI input)
{
    PS_Input_UI output;
    output.position = UIFlags.z > 0.5f
        ? ApplyVP(input.position)
        : float4(input.position, 1.0f);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 PS(PS_Input_UI input) : SV_TARGET
{
    if (UIFlags.x > 0.5f)
    {
        if (UIFlags.y > 0.5f)
        {
            float4 texColor = UITexture.SampleLevel(PointClampSampler, input.uv, 0.0f);
            float coverage = texColor.r;
            if (coverage < 0.1f)
            {
                discard;
            }
            return float4(input.color.rgb, input.color.a * coverage);
        }

        float4 texColor = UITexture.Sample(LinearClampSampler, input.uv);
        return texColor * input.color;
    }

    return input.color;
}
