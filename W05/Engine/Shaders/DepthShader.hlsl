cbuffer CameraCB : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 WorldViewProjection;
};

struct VSInput
{
    float3 Position : POSITION;
};

struct VSOutput
{
    float4 PositionCS : SV_Position;
};

VSOutput mainVS(VSInput Input)
{
    VSOutput Output;
    Output.PositionCS = mul(float4(Input.Position, 1.0f), WorldViewProjection);
    return Output;
};

float4 mainPS() : SV_TARGET
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}

// No need of colors