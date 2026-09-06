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
    float4 Position[256]; // xyz = center position
    float4 Size[256]; // x = halfWidth, y = halfHeight
    float4 Angle[256]; // x = rotation
    int Count;
    float3 Pad;
};

PS_INPUT mainVS(VS_INPUT input, uint instanceId : SV_InstanceID)
{
    PS_INPUT output;

    float angle = Angle[instanceId].x;
    float s = sin(angle);
    float c = cos(angle);

    float2 p = input.position.xy;

    // 먼저 가로/세로 크기 적용
    float2 scaled;
    scaled.x = p.x * Size[instanceId].x;
    scaled.y = p.y * Size[instanceId].y;

    // 그 다음 회전
    float2 rotated;
    rotated.x = scaled.x * c - scaled.y * s;
    rotated.y = scaled.x * s + scaled.y * c;

    output.position = float4(rotated.x + Position[instanceId].x,
                             rotated.y + Position[instanceId].y,
                             Position[instanceId].z,
                             1.0f);

    output.uv = input.uv;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    float4 color = DiffuseTexture.SampleLevel(LinearSampler, input.uv, 0);
    clip(color.a - 0.01f);
    return color;
}