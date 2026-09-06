#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D UIImageTex : register(t0);

struct PS_Input_UI
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float2 texcoord : TEXCOORD0;
};

PS_Input_UI VS(VS_Input_PNCT input)
{
    PS_Input_UI output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_UI input) : SV_TARGET
{
    if (input.color.a < -1.0f)
    {
        float4 texColor = UIImageTex.Sample(LinearClampSampler, input.texcoord);
        return float4(input.color.rgb, (-input.color.a - 1.0f) * texColor.a);
    }

    if (input.color.a < 0.0f)
    {
        return float4(input.color.rgb, -input.color.a);
    }

    float4 texColor = UIImageTex.Sample(LinearClampSampler, input.texcoord);
    return texColor * input.color;
}
