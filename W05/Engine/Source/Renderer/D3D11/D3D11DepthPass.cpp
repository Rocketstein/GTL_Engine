#include <cstring>
#include <cmath>
#include <algorithm>

#include "D3D11DepthPass.h"
#include "Renderer/D3D11/D3D11Device.h"
#include "Renderer/D3D11/Types/D3D11VertexTypes.h"
#include "Renderer/SceneView.h"
#include "Scene/Scene.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Asset/StaticMesh.h"
#include "Core/Geometry/Primitives/AABB.h"
using namespace std;

// ---------------------------------------------------------------------------
//  Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    // Minimum projected screen-size fraction (relative to viewport height) for
    // a mesh to be included in the depth pre-pass.  Objects whose bounding
    // sphere projects to less than this fraction are skipped entirely.
    static constexpr float MinScreenSizeFraction = 0.005;

    // Returns the approximate screen-space size of an AABB as a fraction of
    // the viewport height.  A sphere that circumscribes the AABB half-diagonal
    // is projected through the VP matrix.
    //
    // Axis convention: this engine stores view-space depth along X, right along Y,
    // up along Z.  The perspective projection matrix therefore has:
    //   Proj.M[0][3] = 1  (depth → clip.w)
    //   Proj.M[1][0] = XScale  (right → clip.x)
    //   Proj.M[2][1] = YScale  (up → clip.y)  ← cot(fovY/2)
    float ComputeScreenSizeFraction(const Geometry::FAABB &AABB,
                                    const FMatrix         &VP,
                                    const FMatrix         &Proj)
    {
        const FVector3 Center = AABB.GetCenter();
        const FVector3 Extent = AABB.GetExtent();

        // clip.w = view-space depth = dot(Center - Eye, Forward).
        // In VP (row-vector), clip.w comes from column 3, which equals V's column 0
        // scaled by P[0][3]=1 — i.e. VP.M[row][3] = V.M[row][0].
        const float ClipW = Center.X * VP.M[0][3] + Center.Y * VP.M[1][3]
                          + Center.Z * VP.M[2][3] + VP.M[3][3];
        if (ClipW <= 0.0f) return 1.0f;  // behind or at camera – keep

        // Circumscribed sphere radius (AABB half-diagonal)
        const float R = std::sqrt(Extent.X * Extent.X
                                + Extent.Y * Extent.Y
                                + Extent.Z * Extent.Z);

        // Proj.M[2][1] = YScale = cot(fovY/2): maps view-space Up (Z) → NDC Y.
        // NDC range is [-1,1] (height 2), so multiply by 0.5 for [0,1] fraction.
        return (R * Proj.M[2][1] / ClipW) * 0.5f;
    }

    uint32_t NextPow2(uint32_t V)
    {
        --V; V|=V>>1; V|=V>>2; V|=V>>4; V|=V>>8; V|=V>>16; return ++V;
    }

    uint32_t CalcMipCount(uint32_t W, uint32_t H)
    {
        uint32_t N = 1;
        while (W > 1 || H > 1) { W = max(1u, W>>1); H = max(1u, H>>1); ++N; }
        return N;
    }

    ID3D11Buffer* CreateDynamicCB(ID3D11Device* D, uint32_t Bytes)
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth      = (Bytes + 15) & ~15u;
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ID3D11Buffer* Buf = nullptr;
        D->CreateBuffer(&Desc, nullptr, &Buf);
        return Buf;
    }

    template<typename T>
    void UpdateCB(ID3D11DeviceContext* Ctx, ID3D11Buffer* Buf, const T& Data)
    {
        D3D11_MAPPED_SUBRESOURCE M = {};
        if (SUCCEEDED(Ctx->Map(Buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &M)))
        {
            std::memcpy(M.pData, &Data, sizeof(T));
            Ctx->Unmap(Buf, 0);
        }
    }
}

void FD3D11DepthPass::OnWindowResized() {
    for (auto *UAV : HZBMipUAVs)
        D3D11Util::SafeRelease(UAV);
    for (auto *SRV : HZBMipSRVs)
        D3D11Util::SafeRelease(SRV);
    HZBMipUAVs.clear();
    HZBMipSRVs.clear();
    D3D11Util::SafeRelease(HZBSRV);
    D3D11Util::SafeRelease(HZBTexture);

    FrameCount = 0;
    // Recreate at the new viewport dimensions
    CreateHZBResources();
}

// ---------------------------------------------------------------------------
//  Init / Shutdown
// ---------------------------------------------------------------------------
void FD3D11DepthPass::Initialize(FD3D11Device *InDevice)
{
    Device = InDevice;
    if (Device != nullptr && Device->GetDeviceContext() != nullptr)
    {
        Device->GetDeviceContext()->QueryInterface(__uuidof(ID3D11DeviceContext1),
                                                   reinterpret_cast<void **>(&DeviceContext1));
    }
    CreatePipeline();
}

void FD3D11DepthPass::CreatePipeline()
{
    if (Device == nullptr) return;

    ID3D11Device *D3DDevice = Device->GetDevice();

    Shader.CompileVertexShader(D3DDevice, L"Engine/Shaders/DepthShader.hlsl", "mainVS", "vs_5_0");

    D3D11_INPUT_ELEMENT_DESC Layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}};

    if (Shader.GetVertexShaderBlob())
    {
        InputLayout.Create(D3DDevice, Layout, 1,
                           Shader.GetVertexShaderBlob()->GetBufferPointer(),
                           Shader.GetVertexShaderBlob()->GetBufferSize());
    }

    PerObjectBufferCapacityObjects = 0;
    EnsurePerObjectConstantBufferCapacity(1);

    CreateAABBBox();
    CreateHZBResources();
    CreateComputeResources();
}

void FD3D11DepthPass::Shutdown()
{
    ReleasePipeline();
    D3D11Util::SafeRelease(DeviceContext1);
    Device = nullptr;
}

void FD3D11DepthPass::ReleasePipeline()
{
    PerObjectConstantBuffer.Release();
    PerObjectBufferCapacityObjects = 0;
    InputLayout.Release();
    Shader.Release();

    D3D11Util::SafeRelease(AABBVertexBuffer);
    D3D11Util::SafeRelease(AABBIndexBuffer);

    for (auto *UAV : HZBMipUAVs) D3D11Util::SafeRelease(UAV);
    for (auto *SRV : HZBMipSRVs) D3D11Util::SafeRelease(SRV);
    HZBMipUAVs.clear();
    HZBMipSRVs.clear();
    D3D11Util::SafeRelease(HZBSRV);
    D3D11Util::SafeRelease(HZBTexture);

    D3D11Util::SafeRelease(HZBBuildCS);
    D3D11Util::SafeRelease(HZBCullCS);
    D3D11Util::SafeRelease(HZBBuildCBuf);
    D3D11Util::SafeRelease(HZBCullCBuf);

    D3D11Util::SafeRelease(BoundsSRV);
    D3D11Util::SafeRelease(BoundsBuffer);
    BoundsBufferCount = 0;

    D3D11Util::SafeRelease(VisibilityUAVBuffer);
    D3D11Util::SafeRelease(VisibilityUAV);
    for (UINT16 i = 0; i < NumBuffers; i++)
    {
        D3D11Util::SafeRelease(VisibilityStagingBuffer[i]);
    }
    VisibilityBufferCount = 0;
    FrameCount = 0;
}

// ---------------------------------------------------------------------------
//  Resource creation
// ---------------------------------------------------------------------------
void FD3D11DepthPass::CreateAABBBox()
{
    if (Device == nullptr) return;

    FVector3 Verts[8] = {
        {-0.5f,-0.5f,-0.5f},{+0.5f,-0.5f,-0.5f},
        {+0.5f,+0.5f,-0.5f},{-0.5f,+0.5f,-0.5f},
        {-0.5f,-0.5f,+0.5f},{+0.5f,-0.5f,+0.5f},
        {+0.5f,+0.5f,+0.5f},{-0.5f,+0.5f,+0.5f},
    };
    uint32_t Indices[36] = {
        0,1,2, 0,2,3,
        4,6,5, 4,7,6,
        0,4,5, 0,5,1,
        2,6,7, 2,7,3,
        0,3,7, 0,7,4,
        1,5,6, 1,6,2,
    };

    ID3D11Device *D = Device->GetDevice();
    D3D11_BUFFER_DESC VBDesc = {};
    VBDesc.ByteWidth = sizeof(Verts);
    VBDesc.Usage     = D3D11_USAGE_DEFAULT;
    VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA VBData = { Verts };
    D->CreateBuffer(&VBDesc, &VBData, &AABBVertexBuffer);

    D3D11_BUFFER_DESC IBDesc = {};
    IBDesc.ByteWidth = sizeof(Indices);
    IBDesc.Usage     = D3D11_USAGE_DEFAULT;
    IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA IBData = { Indices };
    D->CreateBuffer(&IBDesc, &IBData, &AABBIndexBuffer);
}

void FD3D11DepthPass::CreateHZBResources()
{
    if (Device == nullptr) return;

    const D3D11_VIEWPORT &VP = Device->GetViewport();
    HZBWidth  = max(1u, (uint32_t)VP.Width  / 2);
    HZBHeight = max(1u, (uint32_t)VP.Height / 2);
    HZBMips   = CalcMipCount(HZBWidth, HZBHeight);

    ID3D11Device *D = Device->GetDevice();

    D3D11_TEXTURE2D_DESC Desc = {};
    Desc.Width     = HZBWidth;
    Desc.Height    = HZBHeight;
    Desc.MipLevels = HZBMips;
    Desc.ArraySize = 1;
    Desc.Format    = DXGI_FORMAT_R32_FLOAT;
    Desc.SampleDesc.Count = 1;
    Desc.Usage     = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    D->CreateTexture2D(&Desc, nullptr, &HZBTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format                    = DXGI_FORMAT_R32_FLOAT;
    SRVDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels       = HZBMips;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    D->CreateShaderResourceView(HZBTexture, &SRVDesc, &HZBSRV);

    HZBMipUAVs.resize(HZBMips, nullptr);
    HZBMipSRVs.resize(HZBMips, nullptr);
    for (uint32_t M = 0; M < HZBMips; ++M)
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.Format             = DXGI_FORMAT_R32_FLOAT;
        UAVDesc.ViewDimension      = D3D11_UAV_DIMENSION_TEXTURE2D;
        UAVDesc.Texture2D.MipSlice = M;
        D->CreateUnorderedAccessView(HZBTexture, &UAVDesc, &HZBMipUAVs[M]);

        D3D11_SHADER_RESOURCE_VIEW_DESC MipSRVDesc = {};
        MipSRVDesc.Format                    = DXGI_FORMAT_R32_FLOAT;
        MipSRVDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        MipSRVDesc.Texture2D.MostDetailedMip = M;
        MipSRVDesc.Texture2D.MipLevels       = 1;
        D->CreateShaderResourceView(HZBTexture, &MipSRVDesc, &HZBMipSRVs[M]);
    }
}

void FD3D11DepthPass::CreateComputeResources()
{
    if (Device == nullptr) return;
    ID3D11Device *D = Device->GetDevice();

    ID3DBlob *Blob = nullptr, *Err = nullptr;
    if (SUCCEEDED(D3DCompileFromFile(L"Engine/Shaders/HZBBuild.hlsl", nullptr, nullptr,
                                     "CSMain", "cs_5_0", 0, 0, &Blob, &Err)))
    {
        D->CreateComputeShader(Blob->GetBufferPointer(), Blob->GetBufferSize(), nullptr, &HZBBuildCS);
        Blob->Release();
    }
    if (Err) Err->Release();

    Err = nullptr;
    if (SUCCEEDED(D3DCompileFromFile(L"Engine/Shaders/HZBCull.hlsl", nullptr, nullptr,
                                     "CSMain", "cs_5_0", 0, 0, &Blob, &Err)))
    {
        D->CreateComputeShader(Blob->GetBufferPointer(), Blob->GetBufferSize(), nullptr, &HZBCullCS);
        Blob->Release();
    }
    if (Err) Err->Release();

    HZBBuildCBuf = CreateDynamicCB(D, sizeof(FHZBBuildCB));
    HZBCullCBuf  = CreateDynamicCB(D, sizeof(FHZBCullCB));
}

void FD3D11DepthPass::EnsureBoundsBuffer(uint32 Count)
{
    if (Count <= BoundsBufferCount) return;

    D3D11Util::SafeRelease(BoundsSRV);
    D3D11Util::SafeRelease(BoundsBuffer);

    ID3D11Device *D = Device->GetDevice();
    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth           = sizeof(FObjectBoundsGPU) * Count;
    Desc.Usage               = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
    Desc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    Desc.StructureByteStride = sizeof(FObjectBoundsGPU);
    D->CreateBuffer(&Desc, nullptr, &BoundsBuffer);

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format              = DXGI_FORMAT_UNKNOWN;
    SRVDesc.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.FirstElement = 0;
    SRVDesc.Buffer.NumElements  = Count;
    D->CreateShaderResourceView(BoundsBuffer, &SRVDesc, &BoundsSRV);

    BoundsBufferCount = Count;
}

void FD3D11DepthPass::EnsureVisibilityBuffer(uint32 Count)
{
    if (Count <= VisibilityBufferCount) return;

    D3D11Util::SafeRelease(VisibilityUAVBuffer);
    D3D11Util::SafeRelease(VisibilityUAV);
    for (UINT16 i = 0; i < NumBuffers; i++)
    {
        D3D11Util::SafeRelease(VisibilityStagingBuffer[i]);
    }

    ID3D11Device *D = Device->GetDevice();

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth  = sizeof(uint32_t) * Count;
    Desc.Usage      = D3D11_USAGE_DEFAULT;
    Desc.BindFlags  = D3D11_BIND_UNORDERED_ACCESS;
    Desc.MiscFlags  = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    D->CreateBuffer(&Desc, nullptr, &VisibilityUAVBuffer);

    D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    UAVDesc.Format              = DXGI_FORMAT_R32_TYPELESS;
    UAVDesc.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
    UAVDesc.Buffer.FirstElement = 0;
    UAVDesc.Buffer.NumElements  = Count;
    UAVDesc.Buffer.Flags        = D3D11_BUFFER_UAV_FLAG_RAW;
    D->CreateUnorderedAccessView(VisibilityUAVBuffer, &UAVDesc, &VisibilityUAV);


    Desc.Usage          = D3D11_USAGE_STAGING;
    Desc.BindFlags      = 0;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Desc.MiscFlags      = 0;
    for (uint32_t i = 0; i < NumBuffers; i++)
    {
        D->CreateBuffer(&Desc, nullptr, &VisibilityStagingBuffer[i]);
    }

    VisibilityBufferCount = Count;
    VisibilityResults.resize(Count, 1u);
    
}

// ---------------------------------------------------------------------------
//  Depth prepass
// ---------------------------------------------------------------------------
void FD3D11DepthPass::Render(FScene *Scene, const FSceneView *InSceneView)
{
    if (Scene == nullptr || InSceneView == nullptr || Device == nullptr) return;

    ID3D11DeviceContext *Context = Device->GetDeviceContext();
    if (Context == nullptr) return;

    const TArray<UStaticMeshComponent *> &Components = Scene->GetStaticMeshComponents();
    TArray<UStaticMeshComponent *>         DrawComponents;
    TArray<FD3D11PerObjectConstantsAligned> PerObjectData;
    DrawComponents.reserve(Scene->GetDrawItems().size());
    PerObjectData.reserve(Scene->GetDrawItems().size());

    const FMatrix &VP   = InSceneView->GetViewProjectionMatrix();
    const FMatrix &Proj = InSceneView->GetProjectionMatrix();

    if (FrameCount >= Safeguard && !RenderFeed.empty())
    {
        for (uint32_t i = 0; i < RenderFeed.size(); i++)
        {
            if (RenderFeed[i])
            {
                auto *Component = Components[i];
                if (!Component)
                    continue;

                UStaticMesh *Mesh = Component->GetStaticMesh();
                if (!Mesh || !Mesh->GetRenderResource())
                    continue;

                const auto &RR = Mesh->GetRenderResource();
                if (!RR->IsValid() || !RR->VertexBuffer || !RR->IndexBuffer || RR->IndexCount == 0)
                    continue;


                tmp = ComputeScreenSizeFraction(Component->GetCachedWorldAABB(), VP, Proj);
                if (ComputeScreenSizeFraction(Component->GetCachedWorldAABB(), VP, Proj)
                        < MinScreenSizeFraction)
                    continue;

                FD3D11PerObjectConstantsAligned Constants = {};
                Constants.World = Component->GetRelativeMatrix();
                Constants.WorldViewProjection = Constants.World * VP;
                Constants.SelectionTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

                DrawComponents.push_back(Component);
                PerObjectData.push_back(Constants);
            }
        }
    }
    else
    {
        for (const FSceneDrawItem &Item : Scene->GetDrawItems())
        {
            if (Item.ComponentIndex >= Components.size())
                continue;
            UStaticMeshComponent *Component = Components[Item.ComponentIndex];
            if (!Component)
                continue;

            UStaticMesh *Mesh = Component->GetStaticMesh();
            if (!Mesh || !Mesh->GetRenderResource())
                continue;

            const auto &RR = Mesh->GetRenderResource();
            if (!RR->IsValid() || !RR->VertexBuffer || !RR->IndexBuffer || RR->IndexCount == 0)
                continue;

            if (ComputeScreenSizeFraction(Component->GetCachedWorldAABB(), VP, Proj)
                    < MinScreenSizeFraction)
                continue;

            FD3D11PerObjectConstantsAligned Constants = {};
            Constants.World = Component->GetRelativeMatrix();
            Constants.WorldViewProjection = Constants.World * VP;
            Constants.SelectionTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

            DrawComponents.push_back(Component);
            PerObjectData.push_back(Constants);
        }
    }

    if (DrawComponents.size())
    {
    }

    if (DrawComponents.empty()) return;

    const uint32_t DrawCount = static_cast<uint32_t>(DrawComponents.size());
    if (!EnsurePerObjectConstantBufferCapacity(DrawCount)) return;

    ID3D11Buffer *CB = PerObjectConstantBuffer.Get();
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->OMSetRenderTargets(0, nullptr, Device->GetDepthStencilView());
    Context->OMSetDepthStencilState(Device->GetDepthWriteState(), 0);
    Context->IASetInputLayout(InputLayout.Get());
    Context->VSSetShader(Shader.GetVertexShader(), nullptr, 0);
    Context->PSSetShader(nullptr, nullptr, 0);
    Context->VSSetConstantBuffers(0, 1, &CB);

    constexpr UINT NumConstants = sizeof(FD3D11PerObjectConstantsAligned) / 16;
    if (!UploadPerObjectConstantsBatch(PerObjectData.data(), DrawCount)) return;

    for (uint32_t DrawIndex = 0; DrawIndex < DrawCount; ++DrawIndex)
    {
        UStaticMeshComponent *Component = DrawComponents[DrawIndex];
        UStaticMesh *Mesh = Component->GetStaticMesh();
        if (!Mesh || !Mesh->GetRenderResource())
        {
            continue;
        }
        const auto &RR = Mesh->GetRenderResource();
        if (!RR->IsValid() || !RR->VertexBuffer || !RR->IndexBuffer || RR->IndexCount == 0)
        {
            continue;
        }

        ID3D11Buffer *VB = RR->VertexBuffer->Get();
        ID3D11Buffer *IB = RR->IndexBuffer->Get();
        if (!VB || !IB)
        {
            continue;
        }

        const UINT Stride = RR->VertexStride != 0 ? RR->VertexStride : sizeof(FD3D11VertexPT);
        const UINT Offset = 0;
        Context->IASetVertexBuffers(0, 1, &VB, &Stride, &Offset);
        Context->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);

        if (DeviceContext1 != nullptr)
        {
            const UINT FirstConstant = DrawIndex * NumConstants;
            DeviceContext1->VSSetConstantBuffers1(0, 1, &CB, &FirstConstant, &NumConstants);
        }
        else
        {
            UploadPerObjectConstantsBatch(PerObjectData.data() + DrawIndex, 1);
        }

        Context->DrawIndexed(RR->IndexCount, 0, 0);
    }

    ID3D11RenderTargetView *RTV = Device->GetRenderTargetView();
    Context->OMSetRenderTargets(1, &RTV, Device->GetDepthStencilView());
}

bool FD3D11DepthPass::EnsurePerObjectConstantBufferCapacity(uint32 RequiredObjectCount)
{
    if (Device == nullptr || RequiredObjectCount == 0)
    {
        return false;
    }

    if (PerObjectConstantBuffer.Get() != nullptr &&
        PerObjectBufferCapacityObjects >= RequiredObjectCount)
    {
        return true;
    }

    const uint32 NewCapacity = std::max<uint32>(RequiredObjectCount, PerObjectBufferCapacityObjects * 2u);
    PerObjectConstantBuffer.Release();
    if (!PerObjectConstantBuffer.CreateConstantBuffer(
            Device, sizeof(FD3D11PerObjectConstantsAligned) * NewCapacity))
    {
        PerObjectBufferCapacityObjects = 0;
        return false;
    }

    PerObjectBufferCapacityObjects = NewCapacity;
    return true;
}

bool FD3D11DepthPass::UploadPerObjectConstantsBatch(
    const FD3D11PerObjectConstantsAligned *BatchData, uint32 BatchCount)
{
    if (BatchData == nullptr || BatchCount == 0 || Device == nullptr ||
        Device->GetDeviceContext() == nullptr || PerObjectConstantBuffer.Get() == nullptr)
    {
        return false;
    }

    ID3D11DeviceContext *Context = Device->GetDeviceContext();
    D3D11_MAPPED_SUBRESOURCE MappedResource = {};
    if (FAILED(Context->Map(PerObjectConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                            &MappedResource)))
    {
        return false;
    }

    std::memcpy(MappedResource.pData, BatchData, sizeof(FD3D11PerObjectConstantsAligned) * BatchCount);
    Context->Unmap(PerObjectConstantBuffer.Get(), 0);
    return true;
}

// ---------------------------------------------------------------------------
//  HZB build
// ---------------------------------------------------------------------------
void FD3D11DepthPass::BuildHZB()
{
    if (Device == nullptr || HZBTexture == nullptr || HZBBuildCS == nullptr) return;

    ID3D11DeviceContext *Ctx = Device->GetDeviceContext();

    Ctx->OMSetRenderTargets(0, nullptr, nullptr);

    Ctx->CSSetShader(HZBBuildCS, nullptr, 0);
    Ctx->CSSetConstantBuffers(0, 1, &HZBBuildCBuf);

    uint32_t W = HZBWidth, H = HZBHeight;
    for (uint32_t M = 0; M < HZBMips; ++M)
    {
        FHZBBuildCB CB = {};
        CB.SrcSizeX  = (M == 0) ? (uint32_t)Device->GetViewport().Width  : (W << 1);
        CB.SrcSizeY  = (M == 0) ? (uint32_t)Device->GetViewport().Height : (H << 1);
        CB.PassIndex = M;
        UpdateCB(Ctx, HZBBuildCBuf, CB);

        if (M == 0)
        {
            ID3D11ShaderResourceView *DepthSRV = Device->GetDepthSRV();
            Ctx->CSSetShaderResources(0, 1, &DepthSRV);
            ID3D11ShaderResourceView *Null1 = nullptr;
            Ctx->CSSetShaderResources(1, 1, &Null1);
        }
        else
        {
            ID3D11ShaderResourceView *Null0 = nullptr;
            Ctx->CSSetShaderResources(0, 1, &Null0);
            Ctx->CSSetShaderResources(1, 1, &HZBMipSRVs[M - 1]);
        }

        Ctx->CSSetUnorderedAccessViews(0, 1, &HZBMipUAVs[M], nullptr);

        const uint32_t GX = max(1u, (W + 7) / 8);
        const uint32_t GY = max(1u, (H + 7) / 8);
        Ctx->Dispatch(GX, GY, 1);

        // Unbind UAV before it becomes SRV input next iteration
        ID3D11UnorderedAccessView *NullUAV = nullptr;
        Ctx->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);

        W = max(1u, W >> 1);
        H = max(1u, H >> 1);
    }

    ID3D11ShaderResourceView *NullSRVs[2] = { nullptr, nullptr };
    Ctx->CSSetShaderResources(0, 2, NullSRVs);
    Ctx->CSSetShader(nullptr, nullptr, 0);

    // Restore RTV + DSV for subsequent passes
    ID3D11RenderTargetView *RTV = Device->GetRenderTargetView();
    Ctx->OMSetRenderTargets(1, &RTV, Device->GetDepthStencilView());
}

// ---------------------------------------------------------------------------
//  HZB cull
// ---------------------------------------------------------------------------
void FD3D11DepthPass::CullWithHZB(FScene *Scene, const FSceneView *InSceneView)
{
    LastCullStats = {};
    if (Scene == nullptr || InSceneView == nullptr || Device == nullptr) return;
    if (HZBCullCS == nullptr || HZBSRV == nullptr) return;

    const TArray<UStaticMeshComponent *> &Components = Scene->GetStaticMeshComponents();
    const TArray<FSceneDrawItem> &DrawItems = Scene->GetDrawItems();
    const uint32_t ComponentCount = static_cast<uint32_t>(Components.size());
    if (ComponentCount == 0 || DrawItems.empty()) return;

    std::vector<uint32_t> CandidateComponentIndices;
    CandidateComponentIndices.reserve(DrawItems.size());
    for (const FSceneDrawItem &Item : DrawItems)
    {
        if (Item.ComponentIndex >= ComponentCount) continue;
        if (Components[Item.ComponentIndex] == nullptr) continue;
        CandidateComponentIndices.push_back(Item.ComponentIndex);
    }

    const uint32_t CandidateCount = static_cast<uint32_t>(CandidateComponentIndices.size());
    if (CandidateCount == 0) return;
    LastCullStats.TotalObjects = CandidateCount;

    EnsureBoundsBuffer(CandidateCount);
    EnsureVisibilityBuffer(CandidateCount);
    VisibilityResults.assign(ComponentCount, 1u);
    RenderFeed.resize(VisibilityResults.size());

    ID3D11DeviceContext *Ctx = Device->GetDeviceContext();

    // Upload AABB bounds
    {
        D3D11_MAPPED_SUBRESOURCE M = {};
        if (SUCCEEDED(Ctx->Map(BoundsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &M)))
        {
            FObjectBoundsGPU *Dst = reinterpret_cast<FObjectBoundsGPU *>(M.pData);
            for (uint32_t i = 0; i < CandidateCount; ++i)
            {
                UStaticMeshComponent *Component = Components[CandidateComponentIndices[i]];
                if (Component)
                {
                    const Geometry::FAABB &AABB = Component->GetCachedWorldAABB();
                    Dst[i].Center = AABB.GetCenter();
                    Dst[i].PadA   = 0.f;
                    Dst[i].Extent = AABB.GetExtent();
                    Dst[i].PadB   = 0.f;
                }
            }
            Ctx->Unmap(BoundsBuffer, 0);
        }
    }

    // Upload cull CB
    {
        FHZBCullCB CB = {};
        CB.ViewProjection = InSceneView->GetViewProjectionMatrix();
        CB.HZBSizeX       = HZBWidth;
        CB.HZBSizeY       = HZBHeight;
        CB.ObjectCount    = CandidateCount;
        CB.MipCount       = HZBMips;
        UpdateCB(Ctx, HZBCullCBuf, CB);
    }

    Ctx->CSSetShader(HZBCullCS, nullptr, 0);
    Ctx->CSSetConstantBuffers(0, 1, &HZBCullCBuf);
    Ctx->CSSetShaderResources(0, 1, &BoundsSRV);
    Ctx->CSSetShaderResources(1, 1, &HZBSRV);
    Ctx->CSSetUnorderedAccessViews(0, 1, &VisibilityUAV, nullptr);

    const uint32_t Groups = (CandidateCount + 63) / 64;
    Ctx->Dispatch(Groups, 1, 1);

    // Unbind
    ID3D11ShaderResourceView  *NullSRVs[2] = { nullptr, nullptr };
    ID3D11UnorderedAccessView *NullUAV     = nullptr;
    Ctx->CSSetShaderResources(0, 2, NullSRVs);
    Ctx->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);
    Ctx->CSSetShader(nullptr, nullptr, 0);

    // Ring-buffer readback: write this frame's GPU results into WriteIdx,
    // then read from ReadIdx which was written (NumBuffers-1) frames ago.
    // 3 buffers guarantees the GPU is done 2 frames later.
    const uint32_t WriteIdx = FrameCount % NumBuffers;
    const uint32_t ReadIdx  = (FrameCount + 1) % NumBuffers;

    // Save the candidate mapping for this slot so we can correctly interpret
    // the GPU results when this slot is read back 2 frames from now.
    CandidateIDs[WriteIdx] = CandidateComponentIndices;

    Ctx->CopyResource(VisibilityStagingBuffer[WriteIdx], VisibilityUAVBuffer);

    uint64_t VisibleCount = 0;
    if (FrameCount >= NumBuffers - 1)
    {
        D3D11_MAPPED_SUBRESOURCE M = {};
        if (SUCCEEDED(Ctx->Map(VisibilityStagingBuffer[ReadIdx], 0,
                               D3D11_MAP_READ, 0, &M)))
        {
            // Use the indices that were current when this slot was written,
            // not the current frame's indices — they may differ after camera movement.
            const std::vector<uint32_t>& ReadIndices = CandidateIDs[ReadIdx];
            const uint32_t ReadCount = static_cast<uint32_t>(ReadIndices.size());
            const uint32_t* Src = static_cast<const uint32_t*>(M.pData);

            for (uint32_t i = 0; i < ReadCount; ++i)
            {
                const uint32_t CompIdx = ReadIndices[i];
                if (CompIdx < static_cast<uint32_t>(VisibilityResults.size()))
                {
                    VisibilityResults[CompIdx] = Src[i];
                    RenderFeed[CompIdx] = Src[i];
                    VisibleCount += (Src[i] != 0u) ? 1ull : 0ull;
                }
            }
            Ctx->Unmap(VisibilityStagingBuffer[ReadIdx], 0);
        }
    }
    ++FrameCount;

    LastCullStats.VisibleObjects = VisibleCount;
    LastCullStats.OccludedObjects =
        (LastCullStats.TotalObjects > LastCullStats.VisibleObjects)
            ? (LastCullStats.TotalObjects - LastCullStats.VisibleObjects)
            : 0;
}

void FD3D11DepthPass::SetAllVisible(FScene *Scene)
{
    LastCullStats = {};
    if (Scene == nullptr)
    {
        return;
    }

    const TArray<UStaticMeshComponent *> &Components = Scene->GetStaticMeshComponents();
    const TArray<FSceneDrawItem>         &DrawItems = Scene->GetDrawItems();
    const uint32_t ComponentCount = static_cast<uint32_t>(Components.size());
    VisibilityResults.assign(ComponentCount, 1u);

    uint64_t TotalCandidates = 0;
    for (const FSceneDrawItem &Item : DrawItems)
    {
        if (Item.ComponentIndex >= ComponentCount) continue;
        if (Components[Item.ComponentIndex] == nullptr) continue;
        ++TotalCandidates;
    }

    LastCullStats.TotalObjects = TotalCandidates;
    LastCullStats.VisibleObjects = TotalCandidates;
    LastCullStats.OccludedObjects = 0;
}

// ---------------------------------------------------------------------------
//  Visibility query
// ---------------------------------------------------------------------------
bool FD3D11DepthPass::IsVisible(uint32 ComponentIndex) const
{
    if (ComponentIndex >= VisibilityResults.size() || FrameCount <= 60) return true;
    return VisibilityResults[ComponentIndex] != 0u;
}
