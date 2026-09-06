#pragma once
#include "Engine/Component/Primitive/BillboardComponent.h"

#include "Source/Game/Components/LockOnMarkerComponent.generated.h"

class FPrimitiveSceneProxy;

// ============================================================
// ULockOnMarkerComponent — 락온 마커 전용 빌보드
// ============================================================
// 일반 빌보드와 동일하되, 전용 프록시가 NeverCull 플래그를 세워
// occlusion/frustum 컬링을 면제한다. 마커는 적의 몸 안/뒤에 위치하므로
// 일반 빌보드라면 GPU occlusion 쿼리에 의해 그려지기 전에 컬링된다.
// (NoDepth 는 래스터 단계 깊이 테스트만 끄지, 수집 단계 컬링은 막지 못한다.)
UCLASS()
class ULockOnMarkerComponent : public UBillboardComponent
{
	GENERATED_BODY()
public:
	FPrimitiveSceneProxy* CreateSceneProxy() override;
};
