struct VS_INPUT
{
    float4 position : POSITION;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

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

    float3 local = float3(pr, input.position.z);

    output.position = float4(local * PosRadius.w + PosRadius.xyz, 1.0f);
    if (useConstantColor)
        output.color = Color;
    else
        output.color = input.color;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    return input.color;
}