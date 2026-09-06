cbuffer FPerObjectConstants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 WorldViewProjection;
    float4 SelectionTint;
};

Texture2D gBaseColorTexture : register(t0);
SamplerState gLinearSampler : register(s0);

struct VSInput
{
    float3 Position : POSITION;
    float2 UV       : TEXCOORD0;
};

struct VSOutput
{
    float4 PositionCS : SV_POSITION;
    float2 UV         : TEXCOORD0;
};

VSOutput mainVS(VSInput Input)
{
    VSOutput Output;
    Output.PositionCS = mul(float4(Input.Position, 1.0f), WorldViewProjection);
    Output.UV = Input.UV;
    return Output;
}

float4 mainPS(VSOutput Input) : SV_TARGET
{
    const float4 Sampled = gBaseColorTexture.Sample(gLinearSampler, Input.UV);
    const float3 Fallback = float3(Input.UV.x, Input.UV.y, 1.0f - Input.UV.x);
    const float SampleEnergy = abs(Sampled.r) + abs(Sampled.g) + abs(Sampled.b) + abs(Sampled.a);
    const float InvalidMask = 1.0f - step(1e-6f, SampleEnergy);
    const float4 BaseColor = lerp(Sampled, float4(Fallback, 1.0f), InvalidMask);
    return float4(BaseColor.rgb * SelectionTint.rgb, BaseColor.a);
}
