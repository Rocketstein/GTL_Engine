cbuffer BatchedLineBuffer : register(b0)
{
    row_major float4x4 Model;
    row_major float4x4 View;
    row_major float4x4 Projection;
    float4             CameraPosition;
};

float4 ApplyMVP(float3 pos)
{
    float4 world = mul(float4(pos, 1.0f), Model);
    float4 view = mul(world, View);
    return mul(view, Projection);

}

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float3 worldPos : TEXCOORD0;
};

PSInput VS_BatchLine(VSInput input)
{
    PSInput output;
    output.worldPos = mul(float4(input.position, 1.f), Model).xyz;
    output.position = ApplyMVP(input.position);
    output.color    = input.color;
    return output;
}

float4 PS_BatchLine(PSInput input) : SV_TARGET
{
    float FadeStart = 10.f;
    float FadeEnd   = 180.f;

    float dist  = distance(input.worldPos, CameraPosition.xyz);
    float alpha = 1.f - saturate((dist - FadeStart) / (FadeEnd - FadeStart));
    return float4(input.color.rgb, input.color.a * alpha);
}
