#define CS_SHADER
#include "Common.hlsl"

Texture2D<float> DepthTexture : register(t0);
RWStructuredBuffer<uint> TileDepthMask : register(u0);

groupshared uint ActiveMask;

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint threadIndex : SV_GroupIndex)
{
    if (threadIndex == 0)
        ActiveMask = 0;

    GroupMemoryBarrierWithGroupSync();

    uint2 pixel = groupID.xy * TILE_SIZE + groupThreadID.xy;
    if (pixel.x < uint(ViewportSize.x) && pixel.y < uint(ViewportSize.y))
    {
        float depth = DepthTexture.Load(uint3(pixel, 0));
        if (depth > 0.0f && depth < 1.0f)
        {
            float viewDepth = LinearizeDepth(depth);
            uint sliceIndex = clamp(
                uint(log(viewDepth / NearZ) / log(FarZ / NearZ) * NUM_SLICE),
                0,
                NUM_SLICE - 1);
            InterlockedOr(ActiveMask, 1u << sliceIndex);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (threadIndex == 0)
    {
        uint numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
        uint tileIndex = groupID.y * numTilesX + groupID.x;
        TileDepthMask[tileIndex] = ActiveMask;
    }
}
