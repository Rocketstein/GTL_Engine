#include "Renderer/D3D11/D3D11SceneRenderer.h"
#include "Asset/Core/StaticMeshTypes.h"
#include "Asset/Manager/AssetCacheManager.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Math/MathUtility.h"
#include "Engine/Asset/AssetObjectManager.h"
#include "Engine/Asset/Material.h"
#include "Engine/Asset/StaticMesh.h"
#include "Engine/Asset/Texture.h"
#include "Engine/Component/PrimitiveComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Renderer/D3D11/D3D11DepthPass.h"
#include "Renderer/D3D11/D3D11Device.h"
#include "Renderer/SceneView.h"
#include "Scene/Scene.h"
#include "Viewport/Gizmo.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace
{
    constexpr float  GridMajorLineEvery = 10.0f;
    constexpr float  GridAxisExtent = 10000.0f;
    constexpr uint32 CpuGridHalfLineCount = 40;

    FVector4 MakeColor(float R, float G, float B, float A = 1.0f) { return FVector4(R, G, B, A); }
} // namespace

void FD3D11SceneRenderer::Initialize(FD3D11Device *InDevice)
{
    Device = InDevice;
    VertexStride = sizeof(FD3D11VertexPT);
    if (Device != nullptr && Device->GetDeviceContext() != nullptr)
    {
        const HRESULT QueryResult =
            Device->GetDeviceContext()->QueryInterface(__uuidof(ID3D11DeviceContext1),
                                                       reinterpret_cast<void **>(&DeviceContext1));
        if (SUCCEEDED(QueryResult) && DeviceContext1 != nullptr)
        {
            UE_LOG(SceneRenderer, ELogLevel::Info,
                   "ID3D11DeviceContext1 is available. Using constant-buffer offset binding path.");
        }
        else
        {
            UE_LOG(SceneRenderer, ELogLevel::Warning,
                   "ID3D11DeviceContext1 is unavailable. Falling back to per-object CB upload path.");
        }
    }
    CreatePipeline();
    CreateGridPipeline();
}

void FD3D11SceneRenderer::Shutdown()
{
    ReleaseGridPipeline();
    ReleasePipeline();
    FailedStaticMeshPaths.clear();
    CachedGridLineVertices.clear();
    CachedGridVertexCount = 0;
    bGridQuadBufferDirty = true;
    D3D11Util::SafeRelease(DeviceContext1);
    Device = nullptr;
}

void FD3D11SceneRenderer::SetShowWorldAxis(bool bInShowWorldAxis)
{
    if (bShowWorldAxis != bInShowWorldAxis)
    {
        bShowWorldAxis = bInShowWorldAxis;
        InvalidateCpuGridCache();
    }
}

void FD3D11SceneRenderer::SetGridSpacing(float InGridSpacing)
{
    const float NewGridSpacing = FMath::Max(InGridSpacing, 1.0f);
    if (std::abs(GridSpacing - NewGridSpacing) > 1e-4f)
    {
        GridSpacing = NewGridSpacing;
        InvalidateCpuGridCache();
        bGridQuadBufferDirty = true;
    }
}

void FD3D11SceneRenderer::SetGridRenderMode(EGridRenderMode InGridRenderMode)
{
    if (GridRenderMode != InGridRenderMode)
    {
        GridRenderMode = InGridRenderMode;
        bGridQuadBufferDirty = true;
    }
}

void FD3D11SceneRenderer::Render(FScene *Scene, const FSceneView *InSceneView,
                                 const FD3D11DepthPass *DepthPass)
{
    if (Scene == nullptr || InSceneView == nullptr || Device == nullptr)
    {
        return;
    }

    ID3D11DeviceContext *Context = Device->GetDeviceContext();
    if (Context == nullptr)
    {
        return;
    }

    // If a depth prepass ran, use depth-equal so fragments that lost the prepass are discarded
    //ID3D11DepthStencilState *MeshDepthState =
    //    (DepthPass != nullptr) ? Device->GetDepthEqualState() : Device->GetDepthWriteState();

    ID3D11DepthStencilState *MeshDepthState = Device->GetDepthWriteState();

    Context->OMSetDepthStencilState(MeshDepthState, 0);
    RenderStaticMeshes(Scene, *InSceneView, DepthPass);

    Context->OMSetDepthStencilState(Device->GetDepthReadOnlyState(), 0);
    ID3D11RasterizerState *PreviousRasterizerState = nullptr;
    const bool             bHasOverlayPass = (bShowGrid || bShowWorldAxis || bShowSelectedAABB);
    if (bHasOverlayPass)
    {
        Context->RSGetState(&PreviousRasterizerState);
        if (OverlayRasterizerStateNoDepthClip != nullptr)
        {
            Context->RSSetState(OverlayRasterizerStateNoDepthClip);
        }
    }

    if (bShowGrid)
    {
        RenderGrid(*InSceneView);
    }

    if (bShowWorldAxis)
    {
        RenderWorldAxis(*InSceneView);
    }

    if (bShowSelectedAABB)
    {
        RenderSelectedPrimitiveAABB(*InSceneView);
    }

    if (bHasOverlayPass)
    {
        Context->RSSetState(PreviousRasterizerState);
        D3D11Util::SafeRelease(PreviousRasterizerState);
    }

    Context->OMSetDepthStencilState(Device->GetDepthWriteState(), 0);
}

void FD3D11SceneRenderer::CreatePipeline()
{
    if (Device == nullptr || Device->GetDevice() == nullptr)
    {
        return;
    }

    Shader.CompileVertexShader(Device->GetDevice(), L"Engine/Shaders/ShaderMesh.hlsl", "mainVS",
                               "vs_5_0");
    Shader.CompilePixelShader(Device->GetDevice(), L"Engine/Shaders/ShaderMesh.hlsl", "mainPS",
                              "ps_5_0");

    static_assert(sizeof(Asset::FStaticMeshVertexPT) == sizeof(FD3D11VertexPT),
                  "Static mesh vertex layout size must match renderer layout.");

    D3D11_INPUT_ELEMENT_DESC Layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         static_cast<UINT>(offsetof(FD3D11VertexPT, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         static_cast<UINT>(offsetof(FD3D11VertexPT, UV)), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (Shader.GetVertexShaderBlob() != nullptr)
    {
        InputLayout.Create(Device->GetDevice(), Layout, ARRAYSIZE(Layout),
                           Shader.GetVertexShaderBlob()->GetBufferPointer(),
                           Shader.GetVertexShaderBlob()->GetBufferSize());
    }

    EnsureLinearWrapSampler();
    PerObjectBufferCapacityObjects = 0;
    EnsurePerObjectConstantBufferCapacity(1);
}

void FD3D11SceneRenderer::ReleasePipeline()
{
    D3D11Util::SafeRelease(LinearWrapSampler);
    PerObjectConstantBuffer.Release();
    PerObjectBufferCapacityObjects = 0;
    InputLayout.Release();
    Shader.Release();
}

void FD3D11SceneRenderer::CreateGridPipeline()
{
    if (Device == nullptr || Device->GetDevice() == nullptr)
    {
        return;
    }

    ID3D11Device *D3DDevice = Device->GetDevice();

    GridLineShader.CompileVertexShader(D3DDevice, L"Engine/Shaders/ShaderGrid.hlsl", "GridLineVS",
                                       "vs_5_0");
    GridLineShader.CompilePixelShader(D3DDevice, L"Engine/Shaders/ShaderGrid.hlsl", "GridLinePS",
                                      "ps_5_0");

    D3D11_INPUT_ELEMENT_DESC GridLineLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (GridLineShader.GetVertexShaderBlob() != nullptr)
    {
        GridLineInputLayout.Create(D3DDevice, GridLineLayout, ARRAYSIZE(GridLineLayout),
                                   GridLineShader.GetVertexShaderBlob()->GetBufferPointer(),
                                   GridLineShader.GetVertexShaderBlob()->GetBufferSize());
    }

    GridQuadShader.CompileVertexShader(D3DDevice, L"Engine/Shaders/ShaderGrid.hlsl", "GridQuadVS",
                                       "vs_5_0");
    GridQuadShader.CompilePixelShader(D3DDevice, L"Engine/Shaders/ShaderGrid.hlsl", "GridQuadPS",
                                      "ps_5_0");

    D3D11_INPUT_ELEMENT_DESC GridQuadLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (GridQuadShader.GetVertexShaderBlob() != nullptr)
    {
        GridQuadInputLayout.Create(D3DDevice, GridQuadLayout, ARRAYSIZE(GridQuadLayout),
                                   GridQuadShader.GetVertexShaderBlob()->GetBufferPointer(),
                                   GridQuadShader.GetVertexShaderBlob()->GetBufferSize());
    }

    D3D11_BUFFER_DESC ConstantDesc = {};
    ConstantDesc.Usage = D3D11_USAGE_DYNAMIC;
    ConstantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ConstantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ConstantDesc.ByteWidth = sizeof(FD3D11GridLineConstants);
    D3DDevice->CreateBuffer(&ConstantDesc, nullptr, &GridLineConstantBuffer);

    ConstantDesc.ByteWidth = sizeof(FD3D11GridQuadConstants);
    D3DDevice->CreateBuffer(&ConstantDesc, nullptr, &GridQuadConstantBuffer);

    D3D11_RASTERIZER_DESC OverlayRasterizerDesc = {};
    OverlayRasterizerDesc.FillMode = D3D11_FILL_SOLID;
    OverlayRasterizerDesc.CullMode = D3D11_CULL_NONE;
    OverlayRasterizerDesc.DepthClipEnable = FALSE;
    D3DDevice->CreateRasterizerState(&OverlayRasterizerDesc, &OverlayRasterizerStateNoDepthClip);
}

void FD3D11SceneRenderer::ReleaseGridPipeline()
{
    D3D11Util::SafeRelease(OverlayRasterizerStateNoDepthClip);
    D3D11Util::SafeRelease(GridQuadConstantBuffer);
    D3D11Util::SafeRelease(GridQuadVertexBuffer);
    GridQuadVertexCapacity = 0;
    bGridQuadBufferDirty = true;
    GridQuadInputLayout.Release();
    GridQuadShader.Release();

    D3D11Util::SafeRelease(GridLineConstantBuffer);
    D3D11Util::SafeRelease(GridLineVertexBuffer);
    GridLineVertexCapacity = 0;
    GridLineInputLayout.Release();
    GridLineShader.Release();
}

void FD3D11SceneRenderer::RenderGrid(const FSceneView &SceneView)
{
    if (!bShowGrid)
    {
        return;
    }

    if (GridRenderMode == EGridRenderMode::CpuLineBatch)
    {
        UpdateCpuGridCacheIfNeeded(SceneView);
        RenderCpuGrid(SceneView, 0, CachedGridLineVertexCount);
        return;
    }

    RenderGpuGrid(SceneView);
}

void FD3D11SceneRenderer::RenderWorldAxis(const FSceneView &SceneView)
{
    if (!bShowWorldAxis)
    {
        return;
    }

    UpdateCpuGridCacheIfNeeded(SceneView);
    RenderCpuGrid(SceneView, CachedWorldAxisVertexOffset, CachedWorldAxisVertexCount);
}

void FD3D11SceneRenderer::RenderSelectedPrimitiveAABB(const FSceneView &SceneView)
{
    if (Device == nullptr || Device->GetDeviceContext() == nullptr ||
        GridLineConstantBuffer == nullptr)
    {
        return;
    }

    UPrimitiveComponent *SelectedPrimitive = nullptr;
    if (SceneView.GetGizmo() != nullptr && SceneView.GetGizmo()->HasTarget())
    {
        SelectedPrimitive = SceneView.GetGizmo()->GetTarget();
    }
    if (SelectedPrimitive == nullptr)
    {
        return;
    }

    const Geometry::FAABB &Bounds = SelectedPrimitive->GetWorldAABB();
    if (!Bounds.IsValid())
    {
        return;
    }

    TArray<FD3D11VertexPC> Vertices;
    Vertices.reserve(24);

    const FVector3 Min = Bounds.Min;
    const FVector3 Max = Bounds.Max;
    const FVector4 BoxColor = MakeColor(1.0f, 0.82f, 0.18f);

    const FVector3 P000(Min.X, Min.Y, Min.Z);
    const FVector3 P001(Min.X, Min.Y, Max.Z);
    const FVector3 P010(Min.X, Max.Y, Min.Z);
    const FVector3 P011(Min.X, Max.Y, Max.Z);
    const FVector3 P100(Max.X, Min.Y, Min.Z);
    const FVector3 P101(Max.X, Min.Y, Max.Z);
    const FVector3 P110(Max.X, Max.Y, Min.Z);
    const FVector3 P111(Max.X, Max.Y, Max.Z);

    const auto AddEdge = [&Vertices, &BoxColor](const FVector3 &A, const FVector3 &B)
    {
        Vertices.push_back({A, BoxColor});
        Vertices.push_back({B, BoxColor});
    };

    AddEdge(P000, P001);
    AddEdge(P000, P010);
    AddEdge(P000, P100);
    AddEdge(P001, P011);
    AddEdge(P001, P101);
    AddEdge(P010, P011);
    AddEdge(P010, P110);
    AddEdge(P100, P101);
    AddEdge(P100, P110);
    AddEdge(P011, P111);
    AddEdge(P101, P111);
    AddEdge(P110, P111);

    if (!EnsureLineVertexBufferCapacity(static_cast<UINT>(Vertices.size())))
    {
        return;
    }

    ID3D11DeviceContext *Context = Device->GetDeviceContext();

    D3D11_MAPPED_SUBRESOURCE MappedVertexBuffer = {};
    if (FAILED(
            Context->Map(GridLineVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedVertexBuffer)))
    {
        return;
    }
    std::memcpy(MappedVertexBuffer.pData, Vertices.data(),
                sizeof(FD3D11VertexPC) * Vertices.size());
    Context->Unmap(GridLineVertexBuffer, 0);

    FD3D11GridLineConstants LineConstants = {};
    LineConstants.ViewProjection = SceneView.GetViewProjectionMatrix();

    D3D11_MAPPED_SUBRESOURCE MappedConstantBuffer = {};
    if (FAILED(Context->Map(GridLineConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
                            &MappedConstantBuffer)))
    {
        return;
    }
    std::memcpy(MappedConstantBuffer.pData, &LineConstants, sizeof(LineConstants));
    Context->Unmap(GridLineConstantBuffer, 0);

    const UINT    Stride = sizeof(FD3D11VertexPC);
    const UINT    Offset = 0;
    ID3D11Buffer *VB = GridLineVertexBuffer;
    ID3D11Buffer *CB = GridLineConstantBuffer;

    Context->IASetInputLayout(GridLineInputLayout.Get());
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    Context->IASetVertexBuffers(0, 1, &VB, &Stride, &Offset);
    Context->VSSetShader(GridLineShader.GetVertexShader(), nullptr, 0);
    Context->PSSetShader(GridLineShader.GetPixelShader(), nullptr, 0);
    Context->VSSetConstantBuffers(0, 1, &CB);
    Context->Draw(static_cast<UINT>(Vertices.size()), 0);
}

void FD3D11SceneRenderer::InvalidateCpuGridCache() { bGridCacheDirty = true; }

void FD3D11SceneRenderer::UpdateCpuGridCacheIfNeeded(const FSceneView &SceneView)
{
    const float     Spacing = FMath::Max(GridSpacing, 1.0f);
    const FVector3 &EyePosition = SceneView.GetEyePosition();
    const float     SnappedCenterX = std::floor(EyePosition.X / Spacing) * Spacing;
    const float     SnappedCenterY = std::floor(EyePosition.Y / Spacing) * Spacing;

    const bool bSpacingChanged = std::abs(CachedGridSpacing - Spacing) > 1e-4f;
    const bool bCenterChanged = std::abs(CachedGridCenterX - SnappedCenterX) > 1e-4f ||
                                std::abs(CachedGridCenterY - SnappedCenterY) > 1e-4f;
    const bool bShowGridChanged = CachedShowGrid != bShowGrid;
    const bool bShowWorldAxisChanged = CachedShowWorldAxis != bShowWorldAxis;

    if (!bGridCacheDirty && !bSpacingChanged && !bCenterChanged && !bShowGridChanged &&
        !bShowWorldAxisChanged)
    {
        return;
    }

    BuildCpuGridLineVertices(SceneView, CachedGridLineVertices);
    CachedGridSpacing = Spacing;
    CachedGridCenterX = SnappedCenterX;
    CachedGridCenterY = SnappedCenterY;
    CachedShowGrid = bShowGrid;
    CachedShowWorldAxis = bShowWorldAxis;
    CachedGridVertexCount = static_cast<UINT>(CachedGridLineVertices.size());

    CachedWorldAxisVertexCount = bShowWorldAxis ? 6u : 0u;
    CachedWorldAxisVertexOffset = (CachedGridVertexCount >= CachedWorldAxisVertexCount)
                                      ? (CachedGridVertexCount - CachedWorldAxisVertexCount)
                                      : 0u;
    CachedGridLineVertexCount = CachedGridVertexCount - CachedWorldAxisVertexCount;

    bGridCacheDirty = false;
    bGridBufferDirty = true;
}

void FD3D11SceneRenderer::BuildCpuGridLineVertices(const FSceneView       &SceneView,
                                                   TArray<FD3D11VertexPC> &OutVertices) const
{
    OutVertices.clear();

    const float     Spacing = FMath::Max(GridSpacing, 1.0f);
    const FVector3 &EyePosition = SceneView.GetEyePosition();
    const float     CenterX = std::floor(EyePosition.X / Spacing) * Spacing;
    const float     CenterY = std::floor(EyePosition.Y / Spacing) * Spacing;
    const float     MinX = CenterX - (static_cast<float>(CpuGridHalfLineCount) * Spacing);
    const float     MaxX = CenterX + (static_cast<float>(CpuGridHalfLineCount) * Spacing);
    const float     MinY = CenterY - (static_cast<float>(CpuGridHalfLineCount) * Spacing);
    const float     MaxY = CenterY + (static_cast<float>(CpuGridHalfLineCount) * Spacing);

    OutVertices.reserve((CpuGridHalfLineCount * 4 + 4) * 2 + 6);

    const FVector4 MinorColor = MakeColor(0.18f, 0.18f, 0.18f);
    const FVector4 MajorColor = MakeColor(0.34f, 0.34f, 0.34f);

    if (bShowGrid)
    {
        for (int32 LineIndex = -static_cast<int32>(CpuGridHalfLineCount);
             LineIndex <= static_cast<int32>(CpuGridHalfLineCount); ++LineIndex)
        {
            const float Offset = static_cast<float>(LineIndex) * Spacing;
            const bool  bMajorLine =
                (std::abs(LineIndex) % static_cast<int32>(GridMajorLineEvery)) == 0;
            const FVector4 LineColor = bMajorLine ? MajorColor : MinorColor;

            OutVertices.push_back({FVector3(MinX, CenterY + Offset, 0.0f), LineColor});
            OutVertices.push_back({FVector3(MaxX, CenterY + Offset, 0.0f), LineColor});

            OutVertices.push_back({FVector3(CenterX + Offset, MinY, 0.0f), LineColor});
            OutVertices.push_back({FVector3(CenterX + Offset, MaxY, 0.0f), LineColor});
        }
    }

    if (bShowWorldAxis)
    {
        AppendWorldAxisVertices(OutVertices);
    }
}

void FD3D11SceneRenderer::AppendWorldAxisVertices(TArray<FD3D11VertexPC> &InOutVertices) const
{
    InOutVertices.push_back(
        {FVector3(-GridAxisExtent, 0.0f, 0.0f), MakeColor(0.85f, 0.20f, 0.20f)});
    InOutVertices.push_back({FVector3(GridAxisExtent, 0.0f, 0.0f), MakeColor(0.85f, 0.20f, 0.20f)});

    InOutVertices.push_back(
        {FVector3(0.0f, -GridAxisExtent, 0.0f), MakeColor(0.20f, 0.85f, 0.20f)});
    InOutVertices.push_back({FVector3(0.0f, GridAxisExtent, 0.0f), MakeColor(0.20f, 0.85f, 0.20f)});

    InOutVertices.push_back(
        {FVector3(0.0f, 0.0f, -GridAxisExtent * 0.25f), MakeColor(0.20f, 0.50f, 0.95f)});
    InOutVertices.push_back(
        {FVector3(0.0f, 0.0f, GridAxisExtent * 0.25f), MakeColor(0.20f, 0.50f, 0.95f)});
}

bool FD3D11SceneRenderer::EnsureLineVertexBufferCapacity(UINT RequiredVertexCount)
{
    if (Device == nullptr || Device->GetDevice() == nullptr || RequiredVertexCount == 0)
    {
        return false;
    }

    if (RequiredVertexCount <= GridLineVertexCapacity && GridLineVertexBuffer != nullptr)
    {
        return true;
    }

    D3D11Util::SafeRelease(GridLineVertexBuffer);
    GridLineVertexCapacity = RequiredVertexCount;

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = sizeof(FD3D11VertexPC) * GridLineVertexCapacity;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(Device->GetDevice()->CreateBuffer(&Desc, nullptr, &GridLineVertexBuffer));
}

bool FD3D11SceneRenderer::EnsureQuadVertexBufferCapacity(UINT RequiredVertexCount)
{
    if (Device == nullptr || Device->GetDevice() == nullptr || RequiredVertexCount == 0)
    {
        return false;
    }

    if (RequiredVertexCount <= GridQuadVertexCapacity && GridQuadVertexBuffer != nullptr)
    {
        return true;
    }

    D3D11Util::SafeRelease(GridQuadVertexBuffer);
    GridQuadVertexCapacity = RequiredVertexCount;
    bGridQuadBufferDirty = true;

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = sizeof(FD3D11VertexP) * GridQuadVertexCapacity;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(Device->GetDevice()->CreateBuffer(&Desc, nullptr, &GridQuadVertexBuffer));
}

void FD3D11SceneRenderer::RenderCpuGrid(const FSceneView &SceneView, UINT VertexOffset,
                                        UINT VertexCount)
{
    if (Device == nullptr || Device->GetDeviceContext() == nullptr ||
        GridLineConstantBuffer == nullptr)
    {
        return;
    }

    if (VertexCount == 0)
    {
        return;
    }

    if (!EnsureLineVertexBufferCapacity(CachedGridVertexCount))
    {
        return;
    }

    ID3D11DeviceContext *Context = Device->GetDeviceContext();

    if (bGridBufferDirty)
    {
        D3D11_MAPPED_SUBRESOURCE MappedVertexBuffer = {};
        if (FAILED(Context->Map(GridLineVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
                                &MappedVertexBuffer)))
        {
            return;
        }

        std::memcpy(MappedVertexBuffer.pData, CachedGridLineVertices.data(),
                    sizeof(FD3D11VertexPC) * CachedGridVertexCount);
        Context->Unmap(GridLineVertexBuffer, 0);
        bGridBufferDirty = false;
    }

    FD3D11GridLineConstants LineConstants = {};
    LineConstants.ViewProjection = SceneView.GetViewProjectionMatrix();

    D3D11_MAPPED_SUBRESOURCE MappedConstantBuffer = {};
    if (FAILED(Context->Map(GridLineConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
                            &MappedConstantBuffer)))
    {
        return;
    }

    std::memcpy(MappedConstantBuffer.pData, &LineConstants, sizeof(LineConstants));
    Context->Unmap(GridLineConstantBuffer, 0);

    const UINT    Stride = sizeof(FD3D11VertexPC);
    const UINT    Offset = 0;
    ID3D11Buffer *VB = GridLineVertexBuffer;
    ID3D11Buffer *CB = GridLineConstantBuffer;

    Context->IASetInputLayout(GridLineInputLayout.Get());
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    Context->IASetVertexBuffers(0, 1, &VB, &Stride, &Offset);
    Context->VSSetShader(GridLineShader.GetVertexShader(), nullptr, 0);
    Context->PSSetShader(GridLineShader.GetPixelShader(), nullptr, 0);
    Context->VSSetConstantBuffers(0, 1, &CB);
    Context->Draw(VertexCount, VertexOffset);
}

void FD3D11SceneRenderer::RenderGpuGrid(const FSceneView &SceneView)
{
    if (Device == nullptr || Device->GetDeviceContext() == nullptr ||
        GridQuadConstantBuffer == nullptr)
    {
        return;
    }

    const float     Spacing = FMath::Max(GridSpacing, 1.0f);
    const FVector3 &EyePosition = SceneView.GetEyePosition();
    const bool      bScreenSpaceInfinite = (GridRenderMode == EGridRenderMode::ScreenSpaceInfinite);
    const float     HalfExtent = FMath::Max(Spacing * 400.0f, 4000.0f);

    const float SnappedCenterX = std::floor(EyePosition.X / Spacing) * Spacing;
    const float SnappedCenterY = std::floor(EyePosition.Y / Spacing) * Spacing;

    const bool bModeChanged = (CachedGridQuadScreenSpaceInfinite != bScreenSpaceInfinite);
    const bool bSpacingChanged = std::abs(CachedGridQuadSpacing - Spacing) > 1e-4f;
    const bool bCenterChanged = std::abs(CachedGridQuadCenterX - SnappedCenterX) > 1e-4f ||
                                std::abs(CachedGridQuadCenterY - SnappedCenterY) > 1e-4f;

    const bool bNeedsVertexUpload =
        bGridQuadBufferDirty || bModeChanged ||
        (!bScreenSpaceInfinite && (bSpacingChanged || bCenterChanged));

    FD3D11VertexP QuadVertices[4] = {};
    if (bScreenSpaceInfinite)
    {
        QuadVertices[0] = {FVector3(-1.0f, -1.0f, 0.0f)};
        QuadVertices[1] = {FVector3(-1.0f, 1.0f, 0.0f)};
        QuadVertices[2] = {FVector3(1.0f, -1.0f, 0.0f)};
        QuadVertices[3] = {FVector3(1.0f, 1.0f, 0.0f)};
    }
    else
    {
        QuadVertices[0] = {FVector3(SnappedCenterX - HalfExtent, SnappedCenterY - HalfExtent, 0.0f)};
        QuadVertices[1] = {FVector3(SnappedCenterX - HalfExtent, SnappedCenterY + HalfExtent, 0.0f)};
        QuadVertices[2] = {FVector3(SnappedCenterX + HalfExtent, SnappedCenterY - HalfExtent, 0.0f)};
        QuadVertices[3] = {FVector3(SnappedCenterX + HalfExtent, SnappedCenterY + HalfExtent, 0.0f)};
    }

    if (!EnsureQuadVertexBufferCapacity(4))
    {
        return;
    }

    ID3D11DeviceContext *Context = Device->GetDeviceContext();

    if (bNeedsVertexUpload)
    {
        D3D11_MAPPED_SUBRESOURCE MappedVertexBuffer = {};
        if (FAILED(Context->Map(GridQuadVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
                                &MappedVertexBuffer)))
        {
            return;
        }

        std::memcpy(MappedVertexBuffer.pData, QuadVertices, sizeof(QuadVertices));
        Context->Unmap(GridQuadVertexBuffer, 0);

        bGridQuadBufferDirty = false;
        CachedGridQuadScreenSpaceInfinite = bScreenSpaceInfinite;
        CachedGridQuadSpacing = Spacing;
        CachedGridQuadCenterX = SnappedCenterX;
        CachedGridQuadCenterY = SnappedCenterY;
    }

    FD3D11GridQuadConstants QuadConstants = {};
    QuadConstants.ViewProjection = SceneView.GetViewProjectionMatrix();
    QuadConstants.InverseViewProjection = SceneView.GetViewProjectionMatrix();
    QuadConstants.InverseViewProjection.Inverse();
    QuadConstants.GridParams = FVector4(Spacing, GridMajorLineEvery, 0.85f, 1.35f);
    QuadConstants.EyePositionFade = FVector4(EyePosition, HalfExtent * 0.75f);
    QuadConstants.RenderParams = FVector4(bScreenSpaceInfinite ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

    D3D11_MAPPED_SUBRESOURCE MappedConstantBuffer = {};
    if (FAILED(Context->Map(GridQuadConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
                            &MappedConstantBuffer)))
    {
        return;
    }

    std::memcpy(MappedConstantBuffer.pData, &QuadConstants, sizeof(QuadConstants));
    Context->Unmap(GridQuadConstantBuffer, 0);

    const UINT    Stride = sizeof(FD3D11VertexP);
    const UINT    Offset = 0;
    ID3D11Buffer *VB = GridQuadVertexBuffer;
    ID3D11Buffer *CB = GridQuadConstantBuffer;

    Context->IASetInputLayout(GridQuadInputLayout.Get());
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    Context->IASetVertexBuffers(0, 1, &VB, &Stride, &Offset);
    Context->VSSetShader(GridQuadShader.GetVertexShader(), nullptr, 0);
    Context->PSSetShader(GridQuadShader.GetPixelShader(), nullptr, 0);
    Context->VSSetConstantBuffers(0, 1, &CB);
    Context->PSSetConstantBuffers(0, 1, &CB);
    Context->Draw(4, 0);
}

void FD3D11SceneRenderer::RenderStaticMeshes(FScene *Scene, const FSceneView &SceneView,
                                             const FD3D11DepthPass *DepthPass)
{
    if (Device == nullptr || Device->GetDeviceContext() == nullptr)
    {
        UE_LOG(SceneRenderer, ELogLevel::Warning,
               "RenderStaticMeshes skipped: device/context is not ready.");
        return;
    }

    ID3D11DeviceContext *Context = Device->GetDeviceContext();
    Context->IASetInputLayout(InputLayout.Get());
    Context->VSSetShader(Shader.GetVertexShader(), nullptr, 0);
    Context->PSSetShader(Shader.GetPixelShader(), nullptr, 0);

    if (EnsureLinearWrapSampler())
    {
        Context->PSSetSamplers(0, 1, &LinearWrapSampler);
    }

    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UPrimitiveComponent *SelectedPrimitive = nullptr;
    if (SceneView.GetGizmo() != nullptr && SceneView.GetGizmo()->HasTarget())
    {
        SelectedPrimitive = SceneView.GetGizmo()->GetTarget();
    }

    TArray<UStaticMeshComponent *> DrawComponents;
    TArray<FD3D11PerObjectConstantsAligned> PerObjectData;
    const auto &StaticMeshComponents = Scene->GetStaticMeshComponents();
    DrawComponents.reserve(Scene->GetDrawItems().size());
    PerObjectData.reserve(Scene->GetDrawItems().size());

    for (const FSceneDrawItem &Item : Scene->GetDrawItems())
    {
        if (DepthPass && !DepthPass->IsVisible(Item.ComponentIndex))
        {
            continue;
        }

        if (Item.ComponentIndex >= StaticMeshComponents.size())
        {
            continue;
        }

        UStaticMeshComponent *Component = StaticMeshComponents[Item.ComponentIndex];
        if (Component == nullptr)
        {
            continue;
        }

        FD3D11PerObjectConstantsAligned Constants = {};
        Constants.World = Component->GetRelativeMatrix();
        Constants.WorldViewProjection = Constants.World * SceneView.GetViewProjectionMatrix();
        Constants.SelectionTint = (Component == SelectedPrimitive)
                                      ? FVector4(0.4f, 0.4f, 0.4f, 1.0f)
                                      : FVector4(1.0f, 1.0f, 1.0f, 1.0f);

        DrawComponents.push_back(Component);
        PerObjectData.push_back(Constants);
    }

    if (DrawComponents.empty())
    {
        return;
    }

    const uint32 DrawCount = static_cast<uint32>(DrawComponents.size());
    if (!EnsurePerObjectConstantBufferCapacity(DrawCount))
    {
        return;
    }

    ID3D11Buffer *CB = PerObjectConstantBuffer.Get();
    Context->VSSetConstantBuffers(0, 1, &CB);
    Context->PSSetConstantBuffers(0, 1, &CB);

    constexpr UINT NumConstants = sizeof(FD3D11PerObjectConstantsAligned) / 16; // 16-byte unit
    if (!UploadPerObjectConstantsBatch(PerObjectData.data(), DrawCount))
    {
        return;
    }

    for (uint32 DrawIndex = 0; DrawIndex < DrawCount; ++DrawIndex)
    {
        if (DeviceContext1 != nullptr)
        {
            const UINT FirstConstant = DrawIndex * NumConstants;
            DeviceContext1->VSSetConstantBuffers1(0, 1, &CB, &FirstConstant, &NumConstants);
            DeviceContext1->PSSetConstantBuffers1(0, 1, &CB, &FirstConstant, &NumConstants);
        }
        else
        {
            UploadPerObjectConstantsBatch(PerObjectData.data() + DrawIndex, 1);
        }

        DrawStaticMeshComponent(DrawComponents[DrawIndex], SceneView);
    }
}

void FD3D11SceneRenderer::DrawStaticMeshComponent(UStaticMeshComponent *Component,
                                                  const FSceneView      &SceneView)
{
    if (Component == nullptr || Device == nullptr || Device->GetDeviceContext() == nullptr)
    {
        return;
    }

    UStaticMesh *StaticMesh = Component->GetStaticMesh();
    if (StaticMesh == nullptr || StaticMesh->GetRenderResource() == nullptr)
    {
        return;
    }

    const auto &RenderResource = StaticMesh->GetRenderResource();
    if (!RenderResource->IsValid() || RenderResource->VertexBuffer == nullptr ||
        RenderResource->IndexBuffer == nullptr || RenderResource->IndexCount == 0)
    {
        return;
    }

    ID3D11Buffer *VB = RenderResource->VertexBuffer->Get();
    ID3D11Buffer *IB = RenderResource->IndexBuffer->Get();
    if (VB == nullptr || IB == nullptr)
    {
        return;
    }

    ID3D11DeviceContext *Context = Device->GetDeviceContext();

    const UINT MeshVertexStride =
        RenderResource->VertexStride != 0 ? RenderResource->VertexStride : VertexStride;
    const UINT VertexOffset = 0;

    Context->IASetVertexBuffers(0, 1, &VB, &MeshVertexStride, &VertexOffset);
    Context->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);

    const TArray<FStaticMeshSection> &Sections = StaticMesh->GetSections();
    if (Sections.empty())
    {
        BindMaterial(Component->GetMaterial(0), Component, SceneView);
        Context->DrawIndexed(RenderResource->IndexCount, 0, 0);
        return;
    }

    for (const FStaticMeshSection &Section : Sections)
    {
        if (Section.IndexCount == 0 || Section.FirstIndex >= RenderResource->IndexCount)
        {
            continue;
        }

        const uint32 SafeIndexCount =
            (Section.FirstIndex + Section.IndexCount <= RenderResource->IndexCount)
                ? Section.IndexCount
                : (RenderResource->IndexCount - Section.FirstIndex);

        if (SafeIndexCount == 0)
        {
            continue;
        }

        BindMaterial(Component->GetMaterial(static_cast<int32>(Section.MaterialIndex)), Component,
                     SceneView);
        Context->DrawIndexed(SafeIndexCount, Section.FirstIndex, 0);
    }
}

bool FD3D11SceneRenderer::EnsurePerObjectConstantBufferCapacity(uint32 RequiredObjectCount)
{
    if (Device == nullptr || RequiredObjectCount == 0)
    {
        return false;
    }

    if (PerObjectConstantBuffer.Get() != nullptr &&
        PerObjectBufferCapacityObjects == RequiredObjectCount)
    {
        return true;
    }

    PerObjectConstantBuffer.Release();
    if (!PerObjectConstantBuffer.CreateConstantBuffer(
            Device, sizeof(FD3D11PerObjectConstantsAligned) * RequiredObjectCount))
    {
        UE_LOG(SceneRenderer, ELogLevel::Error,
               "Failed to create per-object constant buffer. ObjectCount=%u", RequiredObjectCount);
        PerObjectBufferCapacityObjects = 0;
        return false;
    }

    PerObjectBufferCapacityObjects = RequiredObjectCount;
    return true;
}

bool FD3D11SceneRenderer::UploadPerObjectConstantsBatch(
    const FD3D11PerObjectConstantsAligned *BatchData, uint32 BatchCount)
{
    if (BatchData == nullptr || BatchCount == 0 || Device == nullptr ||
        Device->GetDeviceContext() == nullptr || PerObjectConstantBuffer.Get() == nullptr)
    {
        return false;
    }

    ID3D11DeviceContext *Context = Device->GetDeviceContext();

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    if (FAILED(Context->Map(PerObjectConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                            &MappedResource)))
    {
        UE_LOG(SceneRenderer, ELogLevel::Warning,
               "Failed to map per-object constant buffer batch. BatchCount=%u", BatchCount);
        return false;
    }

    std::memcpy(MappedResource.pData, BatchData, sizeof(FD3D11PerObjectConstantsAligned) * BatchCount);
    Context->Unmap(PerObjectConstantBuffer.Get(), 0);
    return true;
}

void FD3D11SceneRenderer::BindMaterial(UMaterial *Material, UStaticMeshComponent *Component,
                                       const FSceneView &SceneView)
{
    if (Device == nullptr || Device->GetDeviceContext() == nullptr)
    {
        return;
    }

    ID3D11DeviceContext      *Context = Device->GetDeviceContext();
    ID3D11ShaderResourceView *BaseColorSRV = nullptr;

    if (Material != nullptr && Material->GetRenderResource() != nullptr)
    {
        UTexture *BaseColorTexture = Material->GetBaseColorTexture();
        if (BaseColorTexture != nullptr && BaseColorTexture->GetRenderResource() != nullptr)
        {
            float DistanceToCamera = 0.0f;
            float BoundsRadius = 1.0f;
            if (Component != nullptr)
            {
                const Geometry::FAABB &Bounds = Component->GetCachedWorldAABB();
                const FVector3 Center((Bounds.Min.X + Bounds.Max.X) * 0.5f,
                                      (Bounds.Min.Y + Bounds.Max.Y) * 0.5f,
                                      (Bounds.Min.Z + Bounds.Max.Z) * 0.5f);
                const FVector3 Extent((Bounds.Max.X - Bounds.Min.X) * 0.5f,
                                      (Bounds.Max.Y - Bounds.Min.Y) * 0.5f,
                                      (Bounds.Max.Z - Bounds.Min.Z) * 0.5f);
                DistanceToCamera = (Center - SceneView.GetEyePosition()).Size();
                BoundsRadius = std::max(Extent.Size(), 1.0f);
            }

            const std::shared_ptr<FTextureRenderResource> &LodTexture =
                BaseColorTexture->GetRenderResourceForDistance(DistanceToCamera, BoundsRadius,
                                                               *Device);
            BaseColorSRV = LodTexture ? LodTexture->GetSRV() : nullptr;
        }
    }

    static ID3D11ShaderResourceView *LastBoundSRV = nullptr;
    if (LastBoundSRV != BaseColorSRV)
    {
        Context->PSSetShaderResources(0, 1, &BaseColorSRV);
        LastBoundSRV = BaseColorSRV;
    }
}

bool FD3D11SceneRenderer::EnsureLinearWrapSampler()
{
    if (LinearWrapSampler != nullptr)
    {
        return true;
    }

    if (Device == nullptr || Device->GetDevice() == nullptr)
    {
        return false;
    }

    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    const HRESULT Hr = Device->GetDevice()->CreateSamplerState(&SamplerDesc, &LinearWrapSampler);
    if (FAILED(Hr) || LinearWrapSampler == nullptr)
    {
        UE_LOG(SceneRenderer, ELogLevel::Error, "Failed to create linear wrap sampler. hr=0x%08X",
               static_cast<unsigned>(Hr));
        return false;
    }
    return true;
}
