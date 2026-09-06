/*
    UberLit.hlsl는 legacy view-mode lighting entry입니다.
    tiled light culling이 필요한 lit view mode에서 공유 helper를 사용합니다.
*/

#ifndef ENABLE_LIGHT_EVAL_COUNTER
#define ENABLE_LIGHT_EVAL_COUNTER 0
#endif

Texture2D g_BaseColorTex : register(t0);
Texture2D g_Surface1Tex : register(t1);
Texture2D g_Surface2Tex : register(t2);

Texture2D g_ModifiedBaseColorTex : register(t3);
Texture2D g_ModifiedSurface1Tex : register(t4);
Texture2D g_ModifiedSurface2Tex : register(t5);

#if ENABLE_LIGHT_EVAL_COUNTER
RWBuffer<uint> GlobalLightEvalCounter : register(u1);
#endif

#define USE_LIGHT_CULLING 1
#include "../Shared/ViewModeCommon.hlsli"

float4 ResolveBaseColor(float2 UV)
{
    return ResolveViewModeSurfaceValue(g_BaseColorTex, g_ModifiedBaseColorTex, UV);
}

float4 ResolveSurface1(float2 UV)
{
    return ResolveViewModeSurfaceValue(g_Surface1Tex, g_ModifiedSurface1Tex, UV);
}

float4 ResolveSurface2(float2 UV)
{
    return ResolveViewModeSurfaceValue(g_Surface2Tex, g_ModifiedSurface2Tex, UV);
}

PS_Input_UV VS_Fullscreen(uint VertexID : SV_VertexID)
{
    return FullscreenTriangleVS(VertexID);
}

float4 PS_UberLit(PS_Input_UV Input) : SV_TARGET0
{
    float2 UV = Input.uv;
    float4 BaseColor = ResolveBaseColor(UV);
    float3 Normal = normalize(DecodeNormal(ResolveSurface1(UV)));

#if defined(LIGHTING_MODEL_UNLIT)
    return BaseColor;
#endif

#if defined(LIGHTING_MODEL_WORLDNORMAL)
    return float4(Normal * 0.5f + 0.5f, 1.0f);
#endif

    float3 WorldPos = ReconstructWorldPositionFromSceneDepth(UV);

#if defined(LIGHTING_MODEL_BLINNPHONG)
    float4 MaterialParam = DecodeMaterialParam(ResolveSurface2(UV));
    float3 ViewDir = normalize(CameraWorldPos - WorldPos);
    return ComputeForwardTiledBlinnPhongLighting(BaseColor, Normal, MaterialParam, WorldPos, ViewDir, Input.position);
#elif defined(LIGHTING_MODEL_LAMBERT)
    return ComputeForwardTiledLambertLighting(BaseColor, Normal, WorldPos, Input.position);
#else
    return BaseColor;
#endif
}
