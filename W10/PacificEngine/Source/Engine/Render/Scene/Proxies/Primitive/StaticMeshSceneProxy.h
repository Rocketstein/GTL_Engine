#pragma once

#include "Render/Scene/Proxies/Primitive/MeshSceneProxy.h"

class UStaticMeshComponent;

// FStaticMeshSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FStaticMeshSceneProxy : public FMeshSceneProxy
{
public:
    static constexpr uint32 MAX_LOD = 4;

    FStaticMeshSceneProxy(UStaticMeshComponent* InComponent);

    void UpdateShadow() override;
    void UpdateLOD(uint32 LODLevel) override;

protected:
    UMeshComponent* GetMeshComponent() const override;

    // 모든 LOD의 SectionRenderData 재구축
    void RebuildSectionRenderData() override;
};
