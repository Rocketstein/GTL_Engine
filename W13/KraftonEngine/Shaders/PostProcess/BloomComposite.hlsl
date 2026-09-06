#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D SceneTexture : register(t0);
Texture2D BloomTexture0 : register(t1);
Texture2D BloomTexture1 : register(t2);
Texture2D BloomTexture2 : register(t3);
Texture2D BloomTexture3 : register(t4);
Texture2D BloomTexture4 : register(t5);

cbuffer BloomCompositeCB : register(b2)
{
    float Intensity;
    float3 _Pad;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 scene = SceneTexture.SampleLevel(LinearClampSampler, input.uv, 0);

    float3 bloom = 0.0f;
    bloom += BloomTexture0.SampleLevel(LinearClampSampler, input.uv, 0).rgb * 0.45f;
    bloom += BloomTexture1.SampleLevel(LinearClampSampler, input.uv, 0).rgb * 0.25f;
    bloom += BloomTexture2.SampleLevel(LinearClampSampler, input.uv, 0).rgb * 0.15f;
    bloom += BloomTexture3.SampleLevel(LinearClampSampler, input.uv, 0).rgb * 0.10f;
    bloom += BloomTexture4.SampleLevel(LinearClampSampler, input.uv, 0).rgb * 0.05f;

    return float4(scene.rgb + bloom * Intensity, scene.a);
}
