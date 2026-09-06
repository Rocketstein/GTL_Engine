#pragma once
#include "MeshSceneProxy.h"
#include "Render/RHI/D3D11/Buffers/SkeletalMeshBuffer.h"
#include "Render/Scene/Debug/SkeletalDebug.h"

class USkeletalMeshComponent;

class FSkeletalMeshSceneProxy : public FMeshSceneProxy
{
public:
    FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent);
    void UpdateMaterial() override;
    void UpdateMesh() override;
    void UpdateShadow() override;
    void UpdateLOD(uint32 LODLevel) override { /* SkeletalMesh does not support LOD for now */ };
    void BuildSkeletalDebugInstance(FSkeletalDebugInstance& OutInstance) const;

protected:
    UMeshComponent* GetMeshComponent() const override;
    void            RebuildSectionRenderData() override;
};
