#include "Renderer/D3D11/D3D11GizmoRenderer.h"
#include "Renderer/D3D11/D3D11Device.h"
#include "Renderer/SceneView.h"
#include "Scene/Scene.h"
#include "Viewport/Gizmo.h"
#include "Core/Containers/Array.h"
#include "Core/Math/MathUtility.h"

#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------------
//  Mesh generation helpers  (file-local, not exposed in the header)
// ---------------------------------------------------------------------------

namespace
{
    static const FVector4 GizmoAxisColors[3] = {
        {1.0f, 0.0f, 0.0f, 1.0f}, // X - red
        {0.0f, 1.0f, 0.0f, 1.0f}, // Y - green
        {0.0f, 0.0f, 1.0f, 1.0f}, // Z - blue
    };

    // Remaps a base shape (built along +Z) to point along each world axis.
    static FVector3 AxisRotate(int Axis, float X, float Y, float Z)
    {
        if (Axis == 0) return {Z, X, Y}; // +Z -> +X
        if (Axis == 1) return {X, Z, Y}; // +Z -> +Y
        return {X, Y, Z};                // +Z -> +Z
    }

    // ---- Translation: arrow (cylinder stem + cone head) × 3 axes ----
    static void BuildTranslationMesh(TArray<FGizmoVertex> &Verts, TArray<uint32> &Indices)
    {
        Verts.clear();
        Indices.clear();

        constexpr int   Segments   = 16;
        constexpr float Radius     = 0.06f;
        constexpr float HeadRadius = 0.12f;
        constexpr float StemLength = 0.8f;
        constexpr float TotalLen   = 1.0f;

        for (int Axis = 0; Axis < 3; ++Axis)
        {
            const FVector4 &Color      = GizmoAxisColors[Axis];
            const int32     AxisStart  = static_cast<int32>(Verts.size());

            // Ring vertices: for each segment, 3 verts (bottom, stem-top, cone-base)
            for (int i = 0; i <= Segments; ++i)
            {
                const float Angle = (FMath::PI * 2.0f * i) / Segments;
                const float C = std::cosf(Angle);
                const float S = std::sinf(Angle);

                Verts.push_back({AxisRotate(Axis, C * Radius,     S * Radius,     0.f),       Color, (float)Axis});
                Verts.push_back({AxisRotate(Axis, C * Radius,     S * Radius,     StemLength), Color, (float)Axis});
                Verts.push_back({AxisRotate(Axis, C * HeadRadius, S * HeadRadius, StemLength), Color, (float)Axis});
            }

            // Tip and cone-base centre
            FVector3 TipPos = (Axis == 0) ? FVector3(TotalLen, 0, 0)
                            : (Axis == 1) ? FVector3(0, TotalLen, 0)
                                          : FVector3(0, 0, TotalLen);
            FVector3 BasePos = (Axis == 0) ? FVector3(StemLength, 0, 0)
                             : (Axis == 1) ? FVector3(0, StemLength, 0)
                                           : FVector3(0, 0, StemLength);

            Verts.push_back({TipPos,  Color, (float)Axis});
            const int32 TipIdx  = static_cast<int32>(Verts.size()) - 1;
            Verts.push_back({BasePos, Color, (float)Axis});
            const int32 BaseIdx = static_cast<int32>(Verts.size()) - 1;

            for (int i = 0; i < Segments; ++i)
            {
                const int32 Curr = AxisStart + i * 3;
                const int32 Next = AxisStart + (i + 1) * 3;

                // Stem side
                Indices.push_back(Curr);     Indices.push_back(Curr + 1); Indices.push_back(Next + 1);
                Indices.push_back(Curr);     Indices.push_back(Next + 1); Indices.push_back(Next);

                // Stem-top → cone-base ring
                Indices.push_back(Curr + 1); Indices.push_back(Next + 2); Indices.push_back(Curr + 2);
                Indices.push_back(Curr + 1); Indices.push_back(Next + 1); Indices.push_back(Next + 2);

                // Cone side
                Indices.push_back(Curr + 2); Indices.push_back(Next + 2); Indices.push_back(TipIdx);

                // Cone base cap
                Indices.push_back(BaseIdx);  Indices.push_back(Next + 2); Indices.push_back(Curr + 2);
            }
        }
    }

    // ---- Rotation: torus × 3 axes ----
    static void BuildRotationMesh(TArray<FGizmoVertex> &Verts, TArray<uint32> &Indices)
    {
        Verts.clear();
        Indices.clear();

        constexpr float Radius       = 1.0f;
        constexpr float Thickness    = 0.03f;
        constexpr int   Segments     = 64;
        constexpr int   TubeSegments = 8;

        for (int Axis = 0; Axis < 3; ++Axis)
        {
            const FVector4 &Color     = GizmoAxisColors[Axis];
            const uint32    StartVIdx = static_cast<uint32>(Verts.size());

            for (int i = 0; i <= Segments; ++i)
            {
                const float Long    = (FMath::PI * 2.0f * i) / Segments;
                const float SinLong = std::sinf(Long);
                const float CosLong = std::cosf(Long);

                for (int j = 0; j < TubeSegments; ++j)
                {
                    const float Lat    = (FMath::PI * 2.0f * j) / TubeSegments;
                    const float SinLat = std::sinf(Lat);
                    const float CosLat = std::cosf(Lat);

                    const float X = (Radius + Thickness * CosLat) * CosLong;
                    const float Y = (Radius + Thickness * CosLat) * SinLong;
                    const float Z = Thickness * SinLat;

                    FVector3 Pos = (Axis == 0) ? FVector3(Z, X, Y)
                                : (Axis == 1) ? FVector3(X, Z, Y)
                                              : FVector3(X, Y, Z);

                    Verts.push_back({Pos, Color, (float)Axis});
                }
            }

            for (int i = 0; i < Segments; ++i)
            {
                for (int j = 0; j < TubeSegments; ++j)
                {
                    const uint32 NextI = i + 1;
                    const uint32 NextJ = (j + 1) % TubeSegments;

                    const uint32 I0 = StartVIdx + i     * TubeSegments + j;
                    const uint32 I1 = StartVIdx + NextI * TubeSegments + j;
                    const uint32 I2 = StartVIdx + NextI * TubeSegments + NextJ;
                    const uint32 I3 = StartVIdx + i     * TubeSegments + NextJ;

                    Indices.push_back(I0); Indices.push_back(I1); Indices.push_back(I2);
                    Indices.push_back(I0); Indices.push_back(I2); Indices.push_back(I3);
                }
            }
        }
    }

    // ---- Scale: thin box stem + cube endpoint × 3 axes ----
    static void BuildScaleMesh(TArray<FGizmoVertex> &Verts, TArray<uint32> &Indices)
    {
        Verts.clear();
        Indices.clear();

        constexpr float LineLength     = 1.0f;
        constexpr float BoxSize        = 0.05f;
        constexpr float StemThickness  = 0.03f;

        static const uint32 BoxFaceIndices[] = {
            0, 2, 1,  0, 3, 2,  // -Z face
            4, 5, 6,  4, 6, 7,  // +Z face
            0, 1, 5,  0, 5, 4,  // -Y face
            2, 3, 7,  2, 7, 6,  // +Y face
            0, 4, 7,  0, 7, 3,  // -X face
            1, 2, 6,  1, 6, 5,  // +X face
        };

        const FVector3 Dirs[3] = {{1,0,0}, {0,1,0}, {0,0,1}};

        auto AddBox = [&](const FVector3 &Center, const FVector3 &Extent,
                          const FVector4 &Color, int Axis)
        {
            const uint32 Start = static_cast<uint32>(Verts.size());
            FVector3 P[8] = {
                Center + FVector3(-Extent.X, -Extent.Y, -Extent.Z),
                Center + FVector3( Extent.X, -Extent.Y, -Extent.Z),
                Center + FVector3( Extent.X,  Extent.Y, -Extent.Z),
                Center + FVector3(-Extent.X,  Extent.Y, -Extent.Z),
                Center + FVector3(-Extent.X, -Extent.Y,  Extent.Z),
                Center + FVector3( Extent.X, -Extent.Y,  Extent.Z),
                Center + FVector3( Extent.X,  Extent.Y,  Extent.Z),
                Center + FVector3(-Extent.X,  Extent.Y,  Extent.Z),
            };
            for (int k = 0; k < 8; ++k)
                Verts.push_back({P[k], Color, (float)Axis});
            for (uint32 Idx : BoxFaceIndices)
                Indices.push_back(Start + Idx);
        };

        for (int i = 0; i < 3; ++i)
        {
            const FVector4 &Color = GizmoAxisColors[i];

            FVector3 StemExtent = (i == 0) ? FVector3(LineLength * 0.5f, StemThickness, StemThickness)
                                : (i == 1) ? FVector3(StemThickness, LineLength * 0.5f, StemThickness)
                                           : FVector3(StemThickness, StemThickness, LineLength * 0.5f);

            AddBox(Dirs[i] * (LineLength * 0.5f),        StemExtent,                         Color, i);
            AddBox(Dirs[i] *  LineLength,                 FVector3(BoxSize, BoxSize, BoxSize), Color, i);
        }
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
//  FD3D11GizmoRenderer
// ---------------------------------------------------------------------------

void FD3D11GizmoRenderer::Initialize(FD3D11Device *InDevice)
{
    Device = InDevice;
    CreatePipeline();
}

void FD3D11GizmoRenderer::Shutdown()
{
    ReleasePipeline();
    Device = nullptr;
}

void FD3D11GizmoRenderer::Render(FScene *Scene, const FSceneView *InSceneView)
{
    if (Scene == nullptr || InSceneView == nullptr) return;
    if (!InSceneView->IsShowGizmo()) return;

    UGizmo *Gizmo = InSceneView->GetGizmo();
    if (Gizmo == nullptr || !Gizmo->IsActive() || !Gizmo->HasTarget()) return;

    DrawGizmo(Gizmo, *InSceneView);
}

void FD3D11GizmoRenderer::CreatePipeline()
{
    if (Device == nullptr) return;

    ID3D11Device *D3DDevice = Device->GetDevice();

    // --- Compile gizmo shader ---
    GizmoShader.CompileVertexShader(D3DDevice, L"Engine/Shaders/ShaderGizmo.hlsl", "mainVS", "vs_5_0");
    GizmoShader.CompilePixelShader(D3DDevice,  L"Engine/Shaders/ShaderGizmo.hlsl", "mainPS", "ps_5_0");

    // --- Input layout matching FGizmoVertex ---
    D3D11_INPUT_ELEMENT_DESC Layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT,           0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    if (GizmoShader.GetVertexShaderBlob())
    {
        GizmoInputLayout.Create(D3DDevice, Layout, 3,
                                GizmoShader.GetVertexShaderBlob()->GetBufferPointer(),
                                GizmoShader.GetVertexShaderBlob()->GetBufferSize());
    }

    // --- Constant buffers ---
    PerObjectCB.CreateConstantBuffer(Device, sizeof(FD3D11PerObjectConstants));
    GizmoParamsCB.CreateConstantBuffer(Device, sizeof(FD3D11GizmoParams));

    // --- Depth stencil: no depth test so gizmo draws on top ---
    {
        D3D11_DEPTH_STENCIL_DESC Desc = {};
        Desc.DepthEnable    = FALSE;
        Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        Desc.DepthFunc      = D3D11_COMPARISON_ALWAYS;
        Desc.StencilEnable  = FALSE;
        D3DDevice->CreateDepthStencilState(&Desc, &NoDepthDSS);
    }

    // --- Build and upload gizmo meshes ---
    TArray<FGizmoVertex> Verts;
    TArray<uint32>       Idxs;

    auto UploadMesh = [&](int ModeIdx)
    {
        if (Verts.empty() || Idxs.empty()) return;
        CpuVerts[ModeIdx]   = Verts;   // keep a CPU copy for raycasting
        CpuIndices[ModeIdx] = Idxs;
        GizmoVB[ModeIdx].CreateVertexBuffer(Device, Verts.data(),
                                            static_cast<UINT>(Verts.size() * sizeof(FGizmoVertex)));
        GizmoIB[ModeIdx].CreateIndexBuffer(Device, Idxs.data(),
                                           static_cast<UINT>(Idxs.size() * sizeof(uint32)), false);
        GizmoIndexCount[ModeIdx] = static_cast<uint32>(Idxs.size());
    };

    BuildTranslationMesh(Verts, Idxs); UploadMesh(0);
    BuildRotationMesh   (Verts, Idxs); UploadMesh(1);
    BuildScaleMesh      (Verts, Idxs); UploadMesh(2);

    bPipelineReady = true;
}

void FD3D11GizmoRenderer::ReleasePipeline()
{
    bPipelineReady = false;

    D3D11Util::SafeRelease(NoDepthDSS);

    GizmoInputLayout.Release();
    GizmoShader.Release();
    PerObjectCB.Release();
    GizmoParamsCB.Release();

    for (int i = 0; i < 3; ++i)
    {
        GizmoVB[i].Release();
        GizmoIB[i].Release();
        GizmoIndexCount[i] = 0;
        CpuVerts[i].clear();
        CpuIndices[i].clear();
    }
}

void FD3D11GizmoRenderer::UploadPickMesh(UGizmo *Gizmo)
{
    if (!bPipelineReady || Gizmo == nullptr) return;

    for (int i = 0; i < 3; ++i)
    {
        TArray<FGizmoPickVertex> PickVerts;
        PickVerts.reserve(CpuVerts[i].size());
        for (const FGizmoVertex &V : CpuVerts[i])
            PickVerts.push_back({V.Position, static_cast<int>(V.AxisIndex)});
        Gizmo->SetPickMesh(i, std::move(PickVerts), CpuIndices[i]);
    }
}

void FD3D11GizmoRenderer::DrawGizmo(UGizmo *Gizmo, const FSceneView &SceneView)
{
    if (!bPipelineReady) return;

    ID3D11DeviceContext *Ctx = Device->GetDeviceContext();
    if (Ctx == nullptr) return;

    const int ModeIdx = static_cast<int>(Gizmo->GetPrimitiveType());
    if (GizmoVB[ModeIdx].Get() == nullptr || GizmoIB[ModeIdx].Get() == nullptr) return;

    // --- Per-object constants ---
    {
        FD3D11PerObjectConstants ObjCB;
        ObjCB.World               = Gizmo->GetWorldMatrix();
        ObjCB.WorldViewProjection = ObjCB.World
                                  * SceneView.GetViewMatrix()
                                  * SceneView.GetProjectionMatrix();
        UpdatePerObjectConstants(ObjCB);
    }

    // --- Gizmo highlight constant ---
    {
        FD3D11GizmoParams GizmoCB;
        GizmoCB.SelectedAxis = static_cast<float>(Gizmo->GetSelectedAxis());
        GizmoCB.Pad[0] = GizmoCB.Pad[1] = GizmoCB.Pad[2] = 0.f;
        D3D11_MAPPED_SUBRESOURCE Mapped = {};
        if (SUCCEEDED(Ctx->Map(GizmoParamsCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
        {
            std::memcpy(Mapped.pData, &GizmoCB, sizeof(GizmoCB));
            Ctx->Unmap(GizmoParamsCB.Get(), 0);
        }
        ID3D11Buffer *ParamBuf = GizmoParamsCB.Get();
        Ctx->VSSetConstantBuffers(1, 1, &ParamBuf);
        Ctx->PSSetConstantBuffers(1, 1, &ParamBuf);
    }

    // --- Bind shader pipeline ---
    Ctx->VSSetShader(GizmoShader.GetVertexShader(), nullptr, 0);
    Ctx->PSSetShader(GizmoShader.GetPixelShader(),  nullptr, 0);
    Ctx->IASetInputLayout(GizmoInputLayout.Get());
    Ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // --- Geometry ---
    ID3D11Buffer *VB     = GizmoVB[ModeIdx].Get();
    const UINT    Stride = GizmoVertexStride;
    const UINT    Offset = 0;
    Ctx->IASetVertexBuffers(0, 1, &VB, &Stride, &Offset);
    Ctx->IASetIndexBuffer(GizmoIB[ModeIdx].Get(), DXGI_FORMAT_R32_UINT, 0);

    // --- Depth state: disable so gizmo always draws on top ---
    Ctx->OMSetDepthStencilState(NoDepthDSS, 0);

    Ctx->DrawIndexed(GizmoIndexCount[ModeIdx], 0, 0);

    // --- Restore default depth state ---
    Ctx->OMSetDepthStencilState(nullptr, 0);
}

void FD3D11GizmoRenderer::UpdatePerObjectConstants(const FD3D11PerObjectConstants &Constants)
{
    ID3D11DeviceContext *Ctx = Device->GetDeviceContext();
    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (SUCCEEDED(Ctx->Map(PerObjectCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        std::memcpy(Mapped.pData, &Constants, sizeof(Constants));
        Ctx->Unmap(PerObjectCB.Get(), 0);
    }
    ID3D11Buffer *Buf = PerObjectCB.Get();
    Ctx->VSSetConstantBuffers(0, 1, &Buf);
}
