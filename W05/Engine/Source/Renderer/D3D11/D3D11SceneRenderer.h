#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/D3D11/Resources/D3D11Buffer.h"
#include "Renderer/D3D11/Resources/D3D11InputLayout.h"
#include "Renderer/D3D11/Resources/D3D11Shader.h"
#include "Renderer/D3D11/Types/D3D11Types.h"
#include "Renderer/D3D11/Types/D3D11VertexTypes.h"
#include <unordered_set>

class FD3D11Device;
class FScene;
class FSceneView;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UMaterial;
class FD3D11DepthPass;

enum class EGridRenderMode : uint8
{
    CpuLineBatch = 0,
    GpuProcedural = 1,
    ScreenSpaceInfinite = 2,
};

class FD3D11SceneRenderer
{
  public:
    void Initialize(FD3D11Device *InDevice);
    void Shutdown();

    void Render(FScene *Scene, const FSceneView *InSceneView,
                const FD3D11DepthPass *DepthPass = nullptr);

    void SetShowGrid(bool bInShowGrid) { bShowGrid = bInShowGrid; }
    void SetShowWorldAxis(bool bInShowWorldAxis);
    void SetShowSelectedAABB(bool bInShowSelectedAABB) { bShowSelectedAABB = bInShowSelectedAABB; }
    void SetGridSpacing(float InGridSpacing);
    void SetGridRenderMode(EGridRenderMode InGridRenderMode);

  private:
    void CreatePipeline();
    void ReleasePipeline();

    void CreateGridPipeline();
    void ReleaseGridPipeline();

    void RenderGrid(const FSceneView &SceneView);
    void RenderWorldAxis(const FSceneView &SceneView);
    void RenderSelectedPrimitiveAABB(const FSceneView &SceneView);
    void RenderCpuGrid(const FSceneView &SceneView, UINT VertexOffset, UINT VertexCount);
    void RenderGpuGrid(const FSceneView &SceneView);
    void RenderStaticMeshes(FScene *Scene, const FSceneView &SceneView,
                            const FD3D11DepthPass *DepthPass);

    void BuildCpuGridLineVertices(const FSceneView &SceneView, TArray<FD3D11VertexPC> &OutVertices) const;
    void AppendWorldAxisVertices(TArray<FD3D11VertexPC> &InOutVertices) const;
    void InvalidateCpuGridCache();
    void UpdateCpuGridCacheIfNeeded(const FSceneView &SceneView);
    bool EnsureLineVertexBufferCapacity(UINT RequiredVertexCount);
    bool EnsureQuadVertexBufferCapacity(UINT RequiredVertexCount);

    void DrawStaticMeshComponent(UStaticMeshComponent *Component, const FSceneView &SceneView);
    void BindMaterial(UMaterial *Material, UStaticMeshComponent *Component, const FSceneView &SceneView);
    bool EnsureLinearWrapSampler();
    bool EnsurePerObjectConstantBufferCapacity(uint32 RequiredObjectCount);
    bool UploadPerObjectConstantsBatch(
        const FD3D11PerObjectConstantsAligned *BatchData, uint32 BatchCount);

  private:
    FD3D11Device               *Device = nullptr;
    FD3D11Shader                Shader;
    FD3D11InputLayout           InputLayout;
    FD3D11Buffer                PerObjectConstantBuffer;
    ID3D11SamplerState         *LinearWrapSampler = nullptr;
    ID3D11DeviceContext1       *DeviceContext1 = nullptr;
    uint32                      PerObjectBufferCapacityObjects = 0;
    UINT                        VertexStride = 0;
    std::unordered_set<FString> FailedStaticMeshPaths;

    FD3D11Shader      GridLineShader;
    FD3D11InputLayout GridLineInputLayout;
    ID3D11Buffer     *GridLineVertexBuffer = nullptr;
    ID3D11Buffer     *GridLineConstantBuffer = nullptr;
    UINT              GridLineVertexCapacity = 0;

    FD3D11Shader      GridQuadShader;
    FD3D11InputLayout GridQuadInputLayout;
    ID3D11Buffer     *GridQuadVertexBuffer = nullptr;
    ID3D11Buffer     *GridQuadConstantBuffer = nullptr;
    ID3D11RasterizerState *OverlayRasterizerStateNoDepthClip = nullptr;
    UINT              GridQuadVertexCapacity = 0;

    bool            bShowGrid = true;
    bool            bShowWorldAxis = true;
    bool            bShowSelectedAABB = false;
    float           GridSpacing = 100.0f;
    EGridRenderMode GridRenderMode = EGridRenderMode::CpuLineBatch;

    TArray<FD3D11VertexPC> CachedGridLineVertices;
    float                  CachedGridSpacing = -1.0f;
    float                  CachedGridCenterX = 0.0f;
    float                  CachedGridCenterY = 0.0f;
    bool                   CachedShowGrid = false;
    bool                   CachedShowWorldAxis = false;
    bool                   bGridCacheDirty = true;
    bool                   bGridBufferDirty = true;
    UINT                   CachedGridVertexCount = 0;
    UINT                   CachedGridLineVertexCount = 0;
    UINT                   CachedWorldAxisVertexOffset = 0;
    UINT                   CachedWorldAxisVertexCount = 0;

    bool  bGridQuadBufferDirty = true;
    bool  CachedGridQuadScreenSpaceInfinite = false;
    float CachedGridQuadSpacing = -1.0f;
    float CachedGridQuadCenterX = 0.0f;
    float CachedGridQuadCenterY = 0.0f;
};
