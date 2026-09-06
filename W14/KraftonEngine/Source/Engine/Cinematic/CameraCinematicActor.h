#pragma once

#include "GameFramework/AActor.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Core/Types/EngineTypes.h"

struct FQuat;
class UCinematicWaypointComponent;
class UCineCameraComponent;
class UCameraComponent;
class UBillboardComponent;

#include "Source/Engine/Cinematic/CameraCinematicActor.generated.h"

// ============================================================
// ACameraCinematicActor — 카메라 시네마틱 "릭/트랙".
//
// 순서가 있는 웨이포인트(UCinematicWaypointComponent, 릭의 자식 컴포넌트)들을
// 따라 스플라인 경로를 만들고:
//   1) world / 특정 object(CoordinateTarget) 좌표계 기준 카메라 이동·회전
//   2) 에디터에서 경로/핀/방향 시각화 (bTickInEditor)
//   3) 런타임(PIE)에서 소유한 UCineCameraComponent 로 실제 카메라 구동
//
// 웨이포인트는 별도 Save 프로퍼티가 아니라 컴포넌트 트리로 직렬화되며,
// 순서는 각 웨이포인트의 SequenceIndex 로 관리한다 (이중 직렬화 회피).
// ============================================================
UCLASS()
class ACameraCinematicActor : public AActor
{
public:
	GENERATED_BODY()
	ACameraCinematicActor();

	// 루트 빌보드 + 소유 CineCamera 생성. SpawnActor 직후 호출.
	void InitDefaultComponents();

	void BeginPlay() override;
	void EndPlay() override;
	void OnPostLoad(FArchive& Ar) override;
	void PostDuplicate() override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;

	// ─── 재생 ─────────────────────────────────────────────
	UFUNCTION(Callable, Exec, Category="Cinematic")
	void Play();
	UFUNCTION(Callable, Exec, Category="Cinematic")
	void Stop();
	UFUNCTION(Callable, Exec, Category="Cinematic")
	void Pause();
	UFUNCTION(Callable, Category="Cinematic")
	void SetTime(float InTimeSeconds);
	UFUNCTION(Pure, Category="Cinematic")
	bool IsPlaying() const { return bPlaying; }
	UFUNCTION(Pure, Category="Cinematic")
	float GetCurrentTime() const { return CurrentTime; }
	UFUNCTION(Pure, Category="Cinematic")
	float GetTotalTimeline() const { return ComputeTotalTimeline(); }

	// ─── 웨이포인트 편집 ──────────────────────────────────
	// 월드 트랜스폼으로 핀 추가 (뷰포트 "Add Waypoint Here" 가 호출).
	UFUNCTION(Callable, Category="Cinematic")
	UCinematicWaypointComponent* AddWaypointAtWorld(const FVector& WorldLocation, const FRotator& WorldRotation);
	// 현재 활성 POV(에디터 뷰포트/PIE 카메라) 위치·회전·FOV 로 핀 추가.
	UFUNCTION(Callable, Category="Cinematic")
	UCinematicWaypointComponent* AddWaypointAtCameraView();
	// 마지막 핀 뒤(또는 릭 앞)에 핀 추가.
	UFUNCTION(Callable, Category="Cinematic")
	UCinematicWaypointComponent* AddWaypointAtEnd();
	UFUNCTION(Callable, Category="Cinematic")
	void RemoveWaypoint(int32 Index);
	UFUNCTION(Callable, Exec, Category="Cinematic")
	void ClearWaypoints();
	// From 위치의 핀을 To 위치로 이동(순서 재배치).
	UFUNCTION(Callable, Category="Cinematic")
	void MoveWaypoint(int32 From, int32 To);

	UFUNCTION(Pure, Category="Cinematic")
	int32 GetWaypointCount() const;
	// SequenceIndex 로 정렬된 순서 목록 (온디맨드 계산).
	TArray<UCinematicWaypointComponent*> GetOrderedWaypoints() const;

	AActor* GetCoordinateTarget() const { return CoordinateTarget.Get(); }
	UCineCameraComponent* GetCineCamera() const;

	void Tick(float DeltaTime) override;

private:
	// 시간 t(0..TotalTimeline) 에서의 카메라 트랜스폼/FOV 산출. 핀 2개 미만이면 false.
	bool EvaluateAtTime(float TimeSeconds, FVector& OutLocation, FQuat& OutRotation, float& OutFOV) const;
	// 웨이포인트 i 의 카메라 회전(쿼터니언) — RotMode 반영.
	FQuat EvaluateRotationAt(const TArray<UCinematicWaypointComponent*>& Ordered, int32 Index) const;
	float EvaluateFOVAt(const UCinematicWaypointComponent* Waypoint) const;
	// 구간 i(핀 i→i+1) 통과 시간(초). 핀 override 없으면 Duration 을 균등 분배.
	float SegmentSecondsFor(const TArray<UCinematicWaypointComponent*>& Ordered, int32 Index) const;
	float ComputeTotalTimeline() const;

	void DrawPathDebug() const;
	void ApplyCoordinateTargetFollow();
	void ReacquireComponents();         // 로드/복제 후 캐시 포인터 재획득
	void ApplyEvaluatedToCineCamera(float TimeSeconds);

	// ─── 저장 프로퍼티 ────────────────────────────────────
	// 좌표 기준 대상. null = world space, 지정 시 그 액터의 좌표계(object space).
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Coordinate Target", Type=ObjectRef, AllowedClass=AActor)
	TObjectPtr<AActor> CoordinateTarget = nullptr;

	// 핀별 Segment Seconds 가 0(자동)인 구간에 쓰는 기준 총 이동 시간(초).
	// 자동 구간들은 Duration/(구간 수)로 균등 분배되고, 핀에서 Segment Seconds 를
	// 지정하면 그 구간만 해당 시간으로 덮어쓴다. Dwell 은 별도로 더해진다.
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Duration (auto segments)", Type=Float, Min=0.1f, Max=600.0f, Speed=0.1f)
	float Duration = 5.0f;

	// 웨이포인트 FOVOverride 가 0 이하일 때 쓰는 기본 FOV(radians).
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Default FOV (rad)", Type=Float, Min=0.1f, Max=3.14f, Speed=0.01f)
	float DefaultFOV = 1.0471975512f; // 60deg

	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Loop", Type=Bool)
	bool bLoop = false;

	// true = Catmull-Rom 부드러운 곡선, false = 직선 구간.
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Smooth Path", Type=Bool)
	bool bSmoothPath = true;

	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Play On Begin Play", Type=Bool)
	bool bPlayOnBeginPlay = false;

	// 0 = 즉시 컷(권장). >0 이면 카메라 매니저의 선형 POV 보간을 쓰는데, 시작/도착
	// 카메라 회전 차가 크면 보간이 거칠 수 있어 기본은 컷.
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Blend In Time", Type=Float, Min=0.0f, Max=10.0f, Speed=0.05f)
	float BlendInTime = 0.0f;
	UPROPERTY(Edit, Save, Category="Cinematic", DisplayName="Blend Out Time", Type=Float, Min=0.0f, Max=10.0f, Speed=0.05f)
	float BlendOutTime = 0.0f;

	// 경로 가시화 색상.
	UPROPERTY(Edit, Save, Category="Cinematic|Debug", DisplayName="Path Color", Type=Color4)
	FLinearColor PathColor = FLinearColor(0.2f, 0.8f, 1.0f, 1.0f);

	// ─── 런타임/Transient ─────────────────────────────────
	TObjectPtr<UCineCameraComponent> CineCamera = nullptr; // 소유 자식 (루트의 자식)
	TObjectPtr<UBillboardComponent> Billboard = nullptr;   // 에디터 아이콘 (루트의 자식 리프)
	TObjectPtr<UCameraComponent> PreviousActiveCamera = nullptr; // Stop 시 복귀용
	TObjectPtr<AActor> PreviousViewTarget = nullptr;             // Stop 시 복귀용 ViewTarget

	bool bPlaying = false;
	bool bPaused = false;
	bool bSwitchedCamera = false; // PIE 에서 view target 을 우리 것으로 바꿨는지
	float CurrentTime = 0.0f;
};
