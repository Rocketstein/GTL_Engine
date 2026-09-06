#ifndef SURFACE_EVALUATION_HLSLI
#define SURFACE_EVALUATION_HLSLI

#include "../Opaque/OpaquePassTypes.hlsli"
#include "MaterialSampling.hlsli"

float3 ResolveStaticMeshSurfaceNormal(float3 WorldNormal, float4 WorldTangent, float2 UV, Texture2D NormalMap)
{
    float3 N = normalize(WorldNormal);

#if defined(USE_NORMAL_MAP)
    if (StaticMeshHasNormalTexture())
    {
        float3 T = normalize(WorldTangent.xyz);
        T = normalize(T - dot(T, N) * N);
        float3 B = cross(N, T) * WorldTangent.w;
        float3x3 TBN = float3x3(T, B, N);
        float3 NormalSample = NormalMap.Sample(LinearWrapSampler, UV).rgb;
        float3 TangentNormal = NormalSample * 2.0f - 1.0f;
        return normalize(mul(TangentNormal, TBN));
    }
#endif

    return N;
}

float2 ResolveStaticMeshMaterialParam(float2 UV, Texture2D SpecularMap)
{
    float Shininess = MaterialParam.x > 0.0f ? MaterialParam.x : 32.0f;
    float SpecularStrength = MaterialParam.y > 0.0f ? MaterialParam.y : 0.3f;

    if (StaticMeshHasSpecularTexture())
    {
        SpecularStrength *= SpecularMap.Sample(LinearWrapSampler, UV).r;
    }

    return float2(Shininess, SpecularStrength);
}

#endif
