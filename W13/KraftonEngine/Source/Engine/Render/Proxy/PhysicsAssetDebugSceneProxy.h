#pragma once

#include "ShapeSceneProxy.h"   // FWireLine, FPrimitiveSceneProxy

class UPhysicsAssetDebugComponent;

// 솔리드 바디 삼각형 정점(월드 공간으로 미리 변환). 색/알파는 DrawCommandBuilder 가
// GetBodyColor()/GetSelectedColor() 로 부여하므로 여기엔 위치/법선만 담는다.
struct FPhysicsDebugVertex
{
	FVector Position;
	FVector Normal;
};

// =====================================================================================
// FPhysicsAssetDebugSceneProxy — PhysicsAsset 콜리전 프리미티브를 와이어프레임 라인으로 캐싱.
// 본 디버그 프록시와 동일하게 DrawCommandBuilder 가 EditorLines 패스로 병합한다.
// =====================================================================================
class FPhysicsAssetDebugSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FPhysicsAssetDebugSceneProxy(UPhysicsAssetDebugComponent* InComponent);
	~FPhysicsAssetDebugSceneProxy() override;

	void UpdateTransform() override;

	const TArray<FWireLine>& GetCachedLines() const { return CachedLines; }
	const TArray<FWireLine>& GetCachedSelectedLines() const { return CachedSelectedLines; }
	const TArray<FWireLine>& GetCachedConstraintLines() const { return CachedConstraintLines; }

	// 솔리드 바디 삼각형(3개 = 1 삼각형). AlphaBlend 패스로 채워진 반투명 바디.
	const TArray<FPhysicsDebugVertex>& GetCachedSolidTris() const { return CachedSolidTris; }
	const TArray<FPhysicsDebugVertex>& GetCachedSelectedSolidTris() const { return CachedSelectedSolidTris; }

	const FVector4& GetBodyColor() const { return BodyColor; }
	const FVector4& GetSelectedColor() const { return SelectedColor; }
	const FVector4& GetConstraintColor() const { return ConstraintColor; }

private:
	void RebuildDebugRender();

	TArray<FWireLine> CachedLines;            // 일반 바디
	TArray<FWireLine> CachedSelectedLines;    // 선택 본의 바디(하이라이트)
	TArray<FWireLine> CachedConstraintLines;  // 조인트 프레임 축 + Swing 콘 + Twist 호

	TArray<FPhysicsDebugVertex> CachedSolidTris;          // 일반 바디 솔리드(삼각형 수프)
	TArray<FPhysicsDebugVertex> CachedSelectedSolidTris;  // 선택 본 바디 솔리드

	FVector4 BodyColor;
	FVector4 SelectedColor;
	FVector4 ConstraintColor;
};
