/* Constant Buffers */

struct VS_INPUT
{
    // vertex buffer
    float3 Position : POSITION;
    float2 LocalUV : TEXCOORD0;
    
    // instance buffer
    row_major float4x4 MVP : MVP;
    float2 PenPos : PEN_POS;
    float2 CellSize : CELL_SIZE;
    float2 StartUV : START_UV;
    float2 UVSize : UV_SIZE;
    float4 Color : COLOR;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR;
};

PS_INPUT VS_Font(VS_INPUT input)
{
    float3 pos;
    pos.x = 0;
    pos.y = input.PenPos.x + input.Position.y * input.CellSize.x;
    pos.z = input.PenPos.y + input.Position.z * input.CellSize.y;

    PS_INPUT output;
    output.Position = mul(float4(pos, 1.0f), input.MVP);
    output.TexCoord = input.LocalUV * input.UVSize + input.StartUV;
    output.Color = input.Color;
    return output;
}

Texture2D FontAtlas : register(t0);
SamplerState FontSampler : register(s0);

float4 PS_Font(PS_INPUT input) : SV_TARGET
{
    float4 sample = FontAtlas.Sample(FontSampler, input.TexCoord);
    sample.a = sample.r;
    return sample * input.Color;
}
