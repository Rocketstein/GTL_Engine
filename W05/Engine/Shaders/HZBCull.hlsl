// HZBCull.hlsl
// For each object, project its AABB, pick the right HZB mip,
// and write 1 (visible) or 0 (occluded) into the visibility buffer.

struct FObjectBounds
{
    float3 Center;
    float  PadA;
    float3 Extent;
    float  PadB;
};

StructuredBuffer<FObjectBounds> BoundsBuffer : register(t0);
Texture2D<float>                HZBTexture   : register(t1);
RWBuffer<uint>                  VisibleBuffer : register(u0);

cbuffer HZBCullCB : register(b0)
{
    row_major float4x4 ViewProjection;
    uint2              HZBSize;     // dimensions of mip 0 of the HZB
    uint               ObjectCount;
    uint               MipCount;
};

// Project a single clip-space position to UV [0,1]
float3 ClipToUVDepth(float4 Clip)
{
    float3 NDC = Clip.xyz / Clip.w;
    return float3(NDC.x * 0.5f + 0.5f, -NDC.y * 0.5f + 0.5f, NDC.z);
}

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint Idx = DTid.x;
    if (Idx >= ObjectCount)
        return;

    FObjectBounds B = BoundsBuffer[Idx];

    // Build 8 AABB corners
    float3 C = B.Center;
    float3 E = B.Extent;
    float3 Corners[8] = {
        C + float3(-E.x, -E.y, -E.z),
        C + float3(+E.x, -E.y, -E.z),
        C + float3(-E.x, +E.y, -E.z),
        C + float3(+E.x, +E.y, -E.z),
        C + float3(-E.x, -E.y, +E.z),
        C + float3(+E.x, -E.y, +E.z),
        C + float3(-E.x, +E.y, +E.z),
        C + float3(+E.x, +E.y, +E.z),
    };

    float MinU = 1, MinV = 1, MaxU = 0, MaxV = 0, MinDepth = 1;
    bool bBehindCamera = true;

    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        float4 Clip = mul(float4(Corners[i], 1.0f), ViewProjection);
        if (Clip.w <= 0.0f)
        {
            bBehindCamera = false; // straddles near plane - cannot be occluded
            MinDepth = 0.0f;
            continue;
        }
        bBehindCamera = false;

        float3 UVD = ClipToUVDepth(Clip);
        MinU     = min(MinU, UVD.x);
        MinV     = min(MinV, UVD.y);
        MaxU     = max(MaxU, UVD.x);
        MaxV     = max(MaxV, UVD.y);
        MinDepth = min(MinDepth, UVD.z);
    }

    // Object entirely behind camera - treat as visible (conservative)
    if (bBehindCamera) { VisibleBuffer[Idx] = 1; return; }

    // Object fully outside frustum - treat as visible
    if (MaxU < 0 || MinU > 1 || MaxV < 0 || MinV > 1)
    { VisibleBuffer[Idx] = 1; return; }

    // Object partially outside screen — MinDepth may be from off-screen back corners,
    // making it artificially large and causing false occlusion. Be conservative.
    if (MinU < 0.0f || MaxU > 1.0f || MinV < 0.0f || MaxV > 1.0f)
    { VisibleBuffer[Idx] = 1; return; }

    // Clamp to [0,1]
    MinU = saturate(MinU); MinV = saturate(MinV);
    MaxU = saturate(MaxU); MaxV = saturate(MaxV);

    // Choose mip level so the footprint covers ~1 texel
    float FootprintU = (MaxU - MinU) * HZBSize.x;
    float FootprintV = (MaxV - MinV) * HZBSize.y;
    float Footprint  = max(FootprintU, FootprintV);
    uint  Mip        = min((uint)ceil(log2(max(Footprint, 1.0f))), MipCount - 1);

    // Sample HZB at chosen mip - Load uses integer coords
    uint2 MipSize  = max(uint2(1, 1), HZBSize >> Mip);
    uint2 TexelMin = uint2(MinU * MipSize.x, MinV * MipSize.y);
    uint2 TexelMax = uint2(MaxU * MipSize.x, MaxV * MipSize.y);
    TexelMax = min(TexelMax, MipSize - 1);

    float HZBDepth = 0;
    HZBDepth = max(HZBDepth, HZBTexture.Load(int3(TexelMin,              Mip)));
    HZBDepth = max(HZBDepth, HZBTexture.Load(int3(uint2(TexelMax.x, TexelMin.y), Mip)));
    HZBDepth = max(HZBDepth, HZBTexture.Load(int3(uint2(TexelMin.x, TexelMax.y), Mip)));
    HZBDepth = max(HZBDepth, HZBTexture.Load(int3(TexelMax,              Mip)));

    // If nearest point of AABB is further than HZB max depth -> fully occluded.
    // Bias accounts for AABB corners being farther than the actual mesh surface
    // (e.g. a sphere fits inside its AABB, so its rendered depth < any corner depth).
    //VisibleBuffer[Idx] = (MinDepth <= HZBDepth) ? 1 : 0;
    static const float DepthBias = (1.0f / 2048.0f);
    VisibleBuffer[Idx] = (MinDepth <= HZBDepth + DepthBias) ? 1 : 0;
}
