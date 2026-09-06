#pragma once
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/D3D11/Resources/D3D11Buffer.h"
#include "Renderer/D3D11/Resources/D3D11InputLayout.h"
#include "Renderer/D3D11/Resources/D3D11Shader.h"
#include "Renderer/D3D11/Types/D3D11Types.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include <vector>

class FScene;
class FSceneView;

struct alignas(16) FHZBBuildCB
{
    uint32_t SrcSizeX;
    uint32_t SrcSizeY;
    uint32_t PassIndex;
    uint32_t Pad;
};

struct alignas(16) FHZBCullCB
{
    FMatrix  ViewProjection;
    uint32_t HZBSizeX;
    uint32_t HZBSizeY;
    uint32_t ObjectCount;
    uint32_t MipCount;
};

struct FObjectBoundsGPU
{
    FVector3 Center;
    float    PadA;
    FVector3 Extent;
    float    PadB;
};

struct FDepthCullStats
{
    uint64_t TotalObjects = 0;
    uint64_t VisibleObjects = 0;
    uint64_t OccludedObjects = 0;
};

class FD3D11DepthPass
{
public:
    void Initialize(FD3D11Device *InDevice);
    void CreatePipeline();
    void Shutdown();
    void ReleasePipeline();

    // Rasterize all objects depth-only into the device DSV
    void Render(FScene *Scene, const FSceneView *InSceneView);

    // Build HZB pyramid from the filled depth buffer
    void BuildHZB();

    // Dispatch cull compute, readback visibility results
    void CullWithHZB(FScene *Scene, const FSceneView *InSceneView);
    void SetAllVisible(FScene *Scene);

    bool IsVisible(uint32 ComponentIndex) const;
    const FDepthCullStats &GetLastCullStats() const { return LastCullStats; }

    void OnWindowResized();

private:
    void CreateHZBResources();
    void CreateComputeResources();
    void CreateAABBBox();
    bool EnsurePerObjectConstantBufferCapacity(uint32 RequiredObjectCount);
    bool UploadPerObjectConstantsBatch(
        const FD3D11PerObjectConstantsAligned *BatchData, uint32 BatchCount);
    void EnsureBoundsBuffer(uint32 Count);
    void EnsureVisibilityBuffer(uint32 Count);

private:
    FD3D11Device      *Device     = nullptr;
    ID3D11DeviceContext1 *DeviceContext1 = nullptr;
    FD3D11Shader       Shader;
    FD3D11InputLayout  InputLayout;
    FD3D11Buffer       PerObjectConstantBuffer;
    uint32_t           PerObjectBufferCapacityObjects = 0;

    ID3D11Buffer      *AABBVertexBuffer = nullptr;
    ID3D11Buffer      *AABBIndexBuffer  = nullptr;

    // HZB pyramid (R32_FLOAT, full mip chain)
    ID3D11Texture2D          *HZBTexture = nullptr;
    ID3D11ShaderResourceView *HZBSRV     = nullptr;  // all mips, for cull shader
    std::vector<ID3D11UnorderedAccessView *> HZBMipUAVs; // one per mip
    std::vector<ID3D11ShaderResourceView  *> HZBMipSRVs; // one per mip
    uint32_t HZBWidth = 0;
    uint32_t HZBHeight = 0;
    uint32_t HZBMips  = 0;

    // Compute shaders
    ID3D11ComputeShader *HZBBuildCS = nullptr;
    ID3D11ComputeShader *HZBCullCS  = nullptr;

    // Constant buffers for compute passes
    ID3D11Buffer *HZBBuildCBuf = nullptr;
    ID3D11Buffer *HZBCullCBuf  = nullptr;

    // Per-object AABB structured buffer (GPU)
    ID3D11Buffer             *BoundsBuffer    = nullptr;
    ID3D11ShaderResourceView *BoundsSRV       = nullptr;
    uint32_t                  BoundsBufferCount = 0;

    // Visibility UAV buffer + CPU staging readback
    static const uint16_t      Safeguard = 60;
    static const uint32_t     NumBuffers = 3;
    uint32_t                  FrameCount = 0;
    ID3D11Buffer              *VisibilityUAVBuffer;
    ID3D11UnorderedAccessView*VisibilityUAV           = nullptr;
    ID3D11Buffer              *VisibilityStagingBuffer[NumBuffers];
    uint32_t                  VisibilityBufferCount   = 0;
    std::vector<uint32_t>      CandidateIDs[NumBuffers];
    std::vector<uint32_t> VisibilityResults; // 1 = visible, 0 = occluded
    std::vector<uint32_t>      RenderFeed;
    FDepthCullStats       LastCullStats;

    float tmp = -1;
};
