#ifndef LIGHTING_EVALUATION_HLSLI
#define LIGHTING_EVALUATION_HLSLI

#include "DirectLighting.hlsli"

float3 AccumulateForwardLambertLocalLight(float3 Normal, float3 WorldPosition, float4 PixelPos)
{
    float3 TotalLocalLight = 0.0f;

    [loop]
    for (int LightIndex = 0; LightIndex < NumLocalLights; ++LightIndex)
    {
        TotalLocalLight += LocalLightLambertTerm(g_LightBuffer[LightIndex], Normal, WorldPosition, PixelPos);
    }

    return TotalLocalLight;
}

FLocalBlinnPhongTerm AccumulateForwardBlinnPhongLocalLight(
    float3 Normal,
    float3 WorldPosition,
    float3 ViewDirection,
    float Shininess,
    float SpecularStrength,
    float4 PixelPos)
{
    FLocalBlinnPhongTerm TotalLocalLight = (FLocalBlinnPhongTerm)0;

    [loop]
    for (int LightIndex = 0; LightIndex < NumLocalLights; ++LightIndex)
    {
        FLocalBlinnPhongTerm LocalTerm = LocalLightBlinnPhongTerm(
            g_LightBuffer[LightIndex],
            Normal,
            WorldPosition,
            ViewDirection,
            Shininess,
            SpecularStrength,
            PixelPos);
        TotalLocalLight.Diffuse += LocalTerm.Diffuse;
        TotalLocalLight.Specular += LocalTerm.Specular;
    }

    return TotalLocalLight;
}

#ifdef USE_LIGHT_CULLING
uint GetLightTileIndex(uint2 PixelCoord)
{
    uint2 TileCoord = PixelCoord / TileSize;
    uint TilesX = (ScreenSize.x + TileSize.x - 1) / TileSize.x;
    return TileCoord.x + TileCoord.y * TilesX;
}

float3 AccumulateTiledLambertLocalLight(float3 Normal, float3 WorldPosition, float4 PixelPos)
{
    float3 TotalLocalLight = 0.0f;
    uint FlatTileIndex = GetLightTileIndex(uint2(PixelPos.xy));
    int StartIndex = FlatTileIndex * SHADER_ENTITY_TILE_BUCKET_COUNT;

    [loop]
    for (int Bucket = 0; Bucket < SHADER_ENTITY_TILE_BUCKET_COUNT; ++Bucket)
    {
        uint TileMask = PerTileLightMask[StartIndex + Bucket];

        [loop]
        for (int BitIndex = 0; BitIndex < 32; ++BitIndex)
        {
            if ((TileMask & (1u << BitIndex)) == 0)
            {
                continue;
            }

            int LightIndex = Bucket * 32 + BitIndex;
            if (LightIndex >= NumLocalLights)
            {
                continue;
            }

            TotalLocalLight += LocalLightLambertTerm(g_LightBuffer[LightIndex], Normal, WorldPosition, PixelPos);

#if ENABLE_LIGHT_EVAL_COUNTER
            InterlockedAdd(GlobalLightEvalCounter[0], 1);
#endif
        }
    }

    return TotalLocalLight;
}

FLocalBlinnPhongTerm AccumulateTiledBlinnPhongLocalLight(
    float3 Normal,
    float3 WorldPosition,
    float3 ViewDirection,
    float Shininess,
    float SpecularStrength,
    float4 PixelPos)
{
    FLocalBlinnPhongTerm TotalLocalLight = (FLocalBlinnPhongTerm)0;
    uint FlatTileIndex = GetLightTileIndex(uint2(PixelPos.xy));
    int StartIndex = FlatTileIndex * SHADER_ENTITY_TILE_BUCKET_COUNT;

    [loop]
    for (int Bucket = 0; Bucket < SHADER_ENTITY_TILE_BUCKET_COUNT; ++Bucket)
    {
        uint TileMask = PerTileLightMask[StartIndex + Bucket];

        [loop]
        for (int BitIndex = 0; BitIndex < 32; ++BitIndex)
        {
            if ((TileMask & (1u << BitIndex)) == 0)
            {
                continue;
            }

            int LightIndex = Bucket * 32 + BitIndex;
            if (LightIndex >= NumLocalLights)
            {
                continue;
            }

            FLocalBlinnPhongTerm LocalTerm = LocalLightBlinnPhongTerm(
                g_LightBuffer[LightIndex],
                Normal,
                WorldPosition,
                ViewDirection,
                Shininess,
                SpecularStrength,
                PixelPos);
            TotalLocalLight.Diffuse += LocalTerm.Diffuse;
            TotalLocalLight.Specular += LocalTerm.Specular;

#if ENABLE_LIGHT_EVAL_COUNTER
            InterlockedAdd(GlobalLightEvalCounter[0], 1);
#endif
        }
    }

    return TotalLocalLight;
}
#endif

float4 EvaluateLambertLighting(float4 BaseColor, float3 GlobalLight, float3 LocalLight)
{
    return float4(BaseColor.rgb * saturate(GlobalLight + LocalLight), BaseColor.a);
}

float4 EvaluateBlinnPhongLighting(float4 BaseColor, FLocalBlinnPhongTerm GlobalLight, FLocalBlinnPhongTerm LocalLight)
{
    return float4(
        BaseColor.rgb * saturate(GlobalLight.Diffuse + LocalLight.Diffuse)
        + (GlobalLight.Specular + LocalLight.Specular) * 0.2f,
        BaseColor.a);
}

float4 ComputeForwardLambertLighting(float4 BaseColor, float3 Normal, float3 WorldPosition, float4 PixelPos)
{
    return EvaluateLambertLighting(
        BaseColor,
        ComputeLambertGlobalLight(Normal, WorldPosition, PixelPos),
        AccumulateForwardLambertLocalLight(Normal, WorldPosition, PixelPos));
}

float4 ComputeForwardBlinnPhongLighting(
    float4 BaseColor,
    float3 Normal,
    float4 MaterialParam,
    float3 WorldPosition,
    float3 ViewDirection,
    float4 PixelPos)
{
    float Shininess = max(MaterialParam.x, 1.0f);
    float SpecularStrength = max(MaterialParam.y, 0.0f);

    return EvaluateBlinnPhongLighting(
        BaseColor,
        ComputeBlinnPhongGlobalLight(Normal, MaterialParam, ViewDirection, WorldPosition, PixelPos),
        AccumulateForwardBlinnPhongLocalLight(
            Normal,
            WorldPosition,
            ViewDirection,
            Shininess,
            SpecularStrength,
            PixelPos));
}

#ifdef USE_LIGHT_CULLING
float4 ComputeForwardTiledLambertLighting(float4 BaseColor, float3 Normal, float3 WorldPosition, float4 PixelPos)
{
    return EvaluateLambertLighting(
        BaseColor,
        ComputeLambertGlobalLight(Normal, WorldPosition, PixelPos),
        AccumulateTiledLambertLocalLight(Normal, WorldPosition, PixelPos));
}

float4 ComputeForwardTiledBlinnPhongLighting(
    float4 BaseColor,
    float3 Normal,
    float4 MaterialParam,
    float3 WorldPosition,
    float3 ViewDirection,
    float4 PixelPos)
{
    float Shininess = max(MaterialParam.x, 1.0f);
    float SpecularStrength = max(MaterialParam.y, 0.0f);

    return EvaluateBlinnPhongLighting(
        BaseColor,
        ComputeBlinnPhongGlobalLight(Normal, MaterialParam, ViewDirection, WorldPosition, PixelPos),
        AccumulateTiledBlinnPhongLocalLight(
            Normal,
            WorldPosition,
            ViewDirection,
            Shininess,
            SpecularStrength,
            PixelPos));
}
#endif

#endif
