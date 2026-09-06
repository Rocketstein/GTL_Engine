// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Scene/Proxies/Primitive/SubUVSceneProxy.h"
#include "Component/SubUVComponent.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Engine/Runtime/Engine.h"

// ============================================================
// FSubUVSceneProxy
// ============================================================
FSubUVSceneProxy::FSubUVSceneProxy(USubUVComponent* InComponent)
    : FBillboardSceneProxy(static_cast<UBillboardComponent*>(InComponent))
{
    bShowAABB = false;

    if (GEngine)
    {
        UVRegionCB.Create(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), sizeof(FSubUVRegionCBData));
    }
}

FSubUVSceneProxy::~FSubUVSceneProxy()
{
    UVRegionCB.Release();
}

void FSubUVSceneProxy::UpdateMesh()
{
    USubUVComponent* Comp = GetSubUVComponent();

    // TexturedQuad (FVertexPNCT with UVs) for rendering
    MeshBuffer = &FMeshBufferManager::Get().GetMeshBuffer(EPrimitiveMeshShape::TexturedQuad);
    Shader     = FShaderManager::Get().GetShader(EShaderType::SubUV);
    Pass       = ERenderPass::SubUV;

    ExtraCB.Bind<FSubUVRegionCBData>(&UVRegionCB, ECBSlot::PerShader0);

    // Set DiffuseSRV from SpriteVfx resource
    const FSpriteVfxResource* SpriteVfx = Comp->GetSpriteVfx();
    if (SpriteVfx && SpriteVfx->IsLoaded())
    {
        DiffuseSRV = SpriteVfx->SRV;
    }
}

void FSubUVSceneProxy::UpdatePerViewport(const FSceneView& SceneView)
{
    USubUVComponent* Comp = GetSubUVComponent();
    bVisible              = Comp->ShouldRenderInCurrentWorld();
    if (!bVisible)
        return;

    const FSpriteVfxResource* SpriteVfx = Comp->GetSpriteVfx();
    if (!SpriteVfx || !SpriteVfx->IsLoaded())
    {
        bVisible = false;
        return;
    }

    // Update DiffuseSRV (may change during play)
    DiffuseSRV = SpriteVfx->SRV;

    FMatrix BillboardMatrix = Comp->ComputeBillboardMatrix(SceneView.CameraForward);

    PerObjectConstants = FPerObjectCBData::FromWorldMatrix(BillboardMatrix);
    MarkPerObjectCBDirty();

    // Update UV region from frame index
    const uint32 Cols = SpriteVfx->Columns;
    const uint32 Rows = SpriteVfx->Rows;
    if (Cols > 0 && Rows > 0)
    {
        const float  FrameW   = 1.0f / static_cast<float>(Cols);
        const float  FrameH   = 1.0f / static_cast<float>(Rows);
        const uint32 FrameIdx = Comp->GetFrameIndex();
        const uint32 Col      = FrameIdx % Cols;
        const uint32 Row      = FrameIdx / Cols;

        FSubUVRegionCBData& Region = ExtraCB.As<FSubUVRegionCBData>();
        Region.U                   = Col * FrameW;
        Region.V                   = Row * FrameH;
        Region.Width               = FrameW;
        Region.Height              = FrameH;
    }
}

USubUVComponent* FSubUVSceneProxy::GetSubUVComponent() const
{
    return static_cast<USubUVComponent*>(Owner);
}
