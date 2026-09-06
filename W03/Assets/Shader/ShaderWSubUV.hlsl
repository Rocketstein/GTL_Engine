cbuffer TransformBuffer : register(b0)
{
    row_major float4x4 Model;
    row_major float4x4 View;
    row_major float4x4 Projection;
};

cbuffer SubUVConstantBuffer : register(b1)
{
    float startX, startY;
    float cellSizeWidth, cellSizeheight;
};


struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 ApplyMVP(float3 pos)
{
    float4 world = mul(float4(pos, 1.0f), Model);
    float4 view = mul(world, View);
    return mul(view, Projection);
}

PS_INPUT VS_Sprite(VS_INPUT input)
{
    PS_INPUT output;
    output.Position = ApplyMVP(input.Position);
    output.TexCoord = input.TexCoord;
    output.TexCoord = float2(
        startX + input.TexCoord.x * cellSizeWidth,
        startY + input.TexCoord.y * cellSizeheight
    );
    return output;
}

Texture2D SpriteAtlas : register(t0);
SamplerState SpriteSampler : register(s0);

float4 PS_Sprite(PS_INPUT input) : SV_TARGET
{
    float4 color = SpriteAtlas.Sample(SpriteSampler, input.TexCoord);
    float minColor = min(min(color.r, color.b), color.g);
    float maxColor = max(max(color.r, color.b), color.g);
    if (maxColor < 0.1f)
        discard;
    //color.rgb *= color.a;
    return color;

}