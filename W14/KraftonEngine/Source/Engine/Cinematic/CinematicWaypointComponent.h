#pragma once

#include "Component/Primitive/StaticMeshComponent.h"
#include "Cinematic/CinematicTypes.h"
#include "Object/Ptr/ObjectPtr.h"

class AActor;

#include "Source/Engine/Cinematic/CinematicWaypointComponent.generated.h"

// ============================================================
// UCinematicWaypointComponent — 시네마틱 경로의 핀(웨이포인트).
//
// ACameraCinematicActor 의 자식 씬 컴포넌트로 붙는다. 컴포넌트의 월드
// 트랜스폼(위치/회전)이 곧 그 지점에서의 카메라 트랜스폼이다.
//
// 작은 스피어 메시를 백킹으로 둬서 (1) 뷰포트 BVH 피킹으로 클릭 선택,
// (2) 기즈모로 직접 이동/회전 이 가능하다. PIE/Game 에서는 editor-only 라
// 렌더되지 않는다.
// ============================================================
UCLASS()
class UCinematicWaypointComponent : public UStaticMeshComponent
{
public:
	GENERATED_BODY()
	UCinematicWaypointComponent() = default;

	// 작은 스피어 메시 로드 + editor-only + 작은 스케일. 릭이 AddComponent 후 호출.
	void InitWaypointVisual();

	void AddReferencedObjects(FReferenceCollector& Collector) override;

	// 이 웨이포인트의 카메라 FOV (radians). <= 0 이면 릭 기본 FOV 상속.
	UFUNCTION(Pure, Category="Cinematic")
	float GetFOVOverride() const { return FOVOverride; }
	UFUNCTION(Callable, Category="Cinematic")
	void SetFOVOverride(float InFOV) { FOVOverride = InFOV; }

	ECinematicRotMode GetRotMode() const { return RotMode; }
	float GetDwellSeconds() const { return DwellSeconds < 0.0f ? 0.0f : DwellSeconds; }
	AActor* GetLookAtActor() const { return LookAtActor.Get(); }

	// 이 핀에 "도착"할 때 감속 정도 (0 = 등속 통과, 1 = 정지까지 감속).
	float GetDecelIn() const { return DecelIn < 0.0f ? 0.0f : (DecelIn > 1.0f ? 1.0f : DecelIn); }
	// 이 핀에서 "출발"할 때 가속 정도 (0 = 등속 출발, 1 = 정지에서 서서히 가속).
	float GetAccelOut() const { return AccelOut < 0.0f ? 0.0f : (AccelOut > 1.0f ? 1.0f : AccelOut); }

	// 이 핀 → 다음 핀 구간을 지나는 데 걸리는 시간(초). <=0 이면 릭의 기본값(자동) 사용.
	// 이 값으로 구간별 "속도"를 직접 조절한다(시간↑ = 느림, 시간↓ = 빠름).
	float GetSegmentSeconds() const { return SegmentSeconds; }

	// 경로 상의 순서. 릭이 자식 웨이포인트들을 이 값으로 정렬한다.
	int32 GetSequenceIndex() const { return SequenceIndex; }
	void SetSequenceIndex(int32 InIndex) { SequenceIndex = InIndex; }

private:
	// FOV override (radians). <=0 = 릭 기본값 상속.
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="FOV Override (rad)", Type=Float, Min=0.0f, Max=3.14f, Speed=0.01f)
	float FOVOverride = 0.0f;

	// 이 핀 → 다음 핀 구간 통과 시간(초). 0 = 릭의 Default Segment Seconds(자동) 사용.
	// 구간 "속도" 조절의 핵심: 값↑ = 그 구간 느리게, 값↓ = 빠르게. (마지막 핀은 무시)
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Segment Seconds (to next)", Type=Float, Min=0.0f, Max=120.0f, Speed=0.05f)
	float SegmentSeconds = 0.0f;

	// 이 지점에 도착해 머무르는(완전 정지) 시간(초). 가감속/속도와 별개의 "홀드".
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Dwell Seconds", Type=Float, Min=0.0f, Max=60.0f, Speed=0.05f)
	float DwellSeconds = 0.0f;

	// 도착 감속(0=등속 통과, 1=정지까지 감속). 0 이면 이 핀에서 멈추지 않고 흘러간다.
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Decel In (arrive)", Type=Float, Min=0.0f, Max=1.0f, Speed=0.01f)
	float DecelIn = 0.0f;

	// 출발 가속(0=등속 출발, 1=정지에서 서서히 가속).
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Accel Out (leave)", Type=Float, Min=0.0f, Max=1.0f, Speed=0.01f)
	float AccelOut = 0.0f;

	// 카메라가 바라보는 방향 결정 방식.
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Rotation Mode", Enum=ECinematicRotMode)
	ECinematicRotMode RotMode = ECinematicRotMode::UseWaypointRotation;

	// RotMode=LookAtTarget 일 때 바라볼 대상. 없으면 릭의 CoordinateTarget 사용.
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Look At Actor", Type=ObjectRef, AllowedClass=AActor)
	TObjectPtr<AActor> LookAtActor = nullptr;

	// 경로 순서 인덱스. 릭이 자식 웨이포인트를 정렬·렌더·평가하는 기준.
	UPROPERTY(Save, Category="Cinematic", DisplayName="Sequence Index")
	int32 SequenceIndex = 0;
};
