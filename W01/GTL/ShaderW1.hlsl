
struct VS_INPUT
{
    float4 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

Texture2D DiffuseTexture : register(t0);
SamplerState LinearSampler : register(s0);

cbuffer constants : register(b0)
{
    float4 PosRadius;
    float4 Angle;
    float4 Color;
    int useConstantColor;
};

PS_INPUT mainVS(VS_INPUT input, uint instanceId : SV_InstanceID)
{
    PS_INPUT output;

    float angle = Angle.x;
    float s = sin(angle);
    float c = cos(angle);

    float2 p = input.position.xy;

    float2 pr;
    pr.x = p.x * c - p.y * s;
    pr.y = p.x * s + p.y * c;

    // float3 local = float3(pr, input.position.z);
    float aspectScaleX = Angle.y;
    float3 local = float3(pr.x * aspectScaleX, pr.y, input.position.z);
    
    output.position = float4(local * PosRadius.w + PosRadius.xyz, 1.0f);
    output.uv = input.uv;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    float2 centered = input.uv - float2(0.5f, 0.5f);
    float dist = length(centered);

    clip(0.5f - dist);

    float4 color = DiffuseTexture.SampleLevel(LinearSampler, input.uv, 0);
    clip(color.a - 0.01f);

    return color;
}