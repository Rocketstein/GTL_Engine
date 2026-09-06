#pragma once
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/D3D11/Resources/D3D11Buffer.h"
#include "Renderer/D3D11/Resources/D3D11InputLayout.h"
#include "Renderer/D3D11/Resources/D3D11Shader.h"
#include "Renderer/D3D11/Types/D3D11Types.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Core/Containers/Array.h"
#include "Core/Geometry/Primitives/Ray.h"

class FD3D11Device;
class FScene;
class FSceneView;
class UGizmo;

// Vertex layout for all gizmo geometry
struct FGizmoVertex
{
    FVector3 Position;
    FVector4 Color;
    float    AxisIndex; // 0.0 = X, 1.0 = Y, 2.0 = Z
};

// Constant buffer bound to slot b1 in ShaderGizmo.hlsl
struct alignas(16) FD3D11GizmoParams
{
    float SelectedAxis; // -1.0 = none highlighted
    float Pad[3];
};

class FD3D11GizmoRenderer
{
  public:
    void Initialize(FD3D11Device *InDevice);
    void Shutdown();

    void Render(FScene *Scene, const FSceneView *InSceneView);

    // Populates the gizmo's pick mesh from the CPU-side geometry built during initialization.
    void UploadPickMesh(UGizmo *Gizmo);

  private:
    void CreatePipeline();
    void ReleasePipeline();
    void DrawGizmo(UGizmo *Gizmo, const FSceneView &SceneView);
    void UpdatePerObjectConstants(const FD3D11PerObjectConstants &Constants);

  private:
    FD3D11Device     *Device = nullptr;
    FD3D11Shader      GizmoShader;
    FD3D11InputLayout GizmoInputLayout;
    FD3D11Buffer      PerObjectCB;
    FD3D11Buffer      GizmoParamsCB;

    // One VB/IB per gizmo mode: [0]=Translation, [1]=Rotation, [2]=Scale
    FD3D11Buffer GizmoVB[3];
    FD3D11Buffer GizmoIB[3];
    uint32       GizmoIndexCount[3] = {};

    // CPU-side copies of each mode's mesh, kept for raycasting.
    TArray<FGizmoVertex> CpuVerts[3];
    TArray<uint32>       CpuIndices[3];

    ID3D11DepthStencilState *NoDepthDSS = nullptr;

    // True only when every D3D11 resource has been created successfully.
    // DrawGizmo and RaycastAxis are no-ops when false.
    bool bPipelineReady = false;

    UINT GizmoVertexStride = sizeof(FGizmoVertex);
};
