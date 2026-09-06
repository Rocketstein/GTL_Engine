// HZBBuild.hlsl
// Pass 0: copy depth SRV (R24) into mip 0 of the HZB (R32F)
// Pass N: downsample mip N-1 -> mip N taking the max of 2x2

Texture2D<float>   DepthSRV   : register(t0); // only bound on pass 0
Texture2D<float>   SrcMip     : register(t1); // bound on passes 1+
RWTexture2D<float> DstMip     : register(u0);

cbuffer HZBBuildCB : register(b0)
{
    uint2 SrcSize;   // dimensions of the source mip
    uint  PassIndex; // 0 = from depth, 1+ = from previous mip
    uint  _Pad;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint2 Dst = DTid.xy;
    uint2 Src = Dst * 2;

    if (PassIndex == 0)
    {
        // Sample 2x2 from the full-res depth buffer, take max (furthest = least visible)
        float d00 = DepthSRV.Load(int3(Src + uint2(0, 0), 0));
        float d10 = DepthSRV.Load(int3(Src + uint2(1, 0), 0));
        float d01 = DepthSRV.Load(int3(Src + uint2(0, 1), 0));
        float d11 = DepthSRV.Load(int3(Src + uint2(1, 1), 0));
        DstMip[Dst] = max(max(d00, d10), max(d01, d11));
    }
    else
    {
        float d00 = SrcMip.Load(int3(Src + uint2(0, 0), 0));
        float d10 = SrcMip.Load(int3(Src + uint2(1, 0), 0));
        float d01 = SrcMip.Load(int3(Src + uint2(0, 1), 0));
        float d11 = SrcMip.Load(int3(Src + uint2(1, 1), 0));
        DstMip[Dst] = max(max(d00, d10), max(d01, d11));
    }
}
