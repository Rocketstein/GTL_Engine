#include "Cinematic/CameraCinematicActor.h"

#include "Cinematic/CinematicWaypointComponent.h"
#include "Component/Camera/CineCameraComponent.h"
#include "Component/Primitive/BillboardComponent.h"
#include "Materials/MaterialManager.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/World.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Math/Quat.h"
#include "Math/MathUtils.h"
#include "Object/GarbageCollection.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr const char* RigIconMaterialPath = "Content/Material/Editor/EmptyActor.uasset";
	constexpr float Rad2Deg = 180.0f / 3.14159265358979f;

	// 큐빅 Hermite (0→1) — 시작/끝 접선으로 구간별 가감속을 제어.
	// 접선 1 = 등속(직선), 접선 0 = 그 끝에서 속도 0(가/감속).
	// StartTangent = 1 - 출발핀.AccelOut, EndTangent = 1 - 도착핀.DecelIn.
	// 둘 다 1 이면 직선(통과), 둘 다 0 이면 smoothstep(양끝 정지).
	// 접선이 [0,1] 이면 단조증가라 역주행/오버슈트가 없다.
	float HermiteEase(float A, float StartTangent, float EndTangent)
	{
		A = FMath::Clamp(A, 0.0f, 1.0f);
		const float A2 = A * A;
		const float A3 = A2 * A;
		return (A3 - 2.0f * A2 + A) * StartTangent
			+ (-2.0f * A3 + 3.0f * A2)
			+ (A3 - A2) * EndTangent;
	}

	// 균일 Catmull-Rom — P1..P2 구간을 P0,P3 접선으로 보간 (t in [0,1]).
	FVector CatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float T)
	{
		const float T2 = T * T;
		const float T3 = T2 * T;
		return (P1 * 2.0f
			+ (P2 - P0) * T
			+ (P0 * 2.0f - P1 * 5.0f + P2 * 4.0f - P3) * T2
			+ (P1 * 3.0f - P0 - P2 * 3.0f + P3) * T3) * 0.5f;
	}

	// From→To 방향을 바라보는 회전(+X 가 forward). UCameraComponent::LookAt 과 동일 규약.
	FQuat MakeLookQuat(const FVector& From, const FVector& To)
	{
		FVector Dir = To - From;
		if (Dir.IsNearlyZero())
		{
			return FQuat::Identity;
		}
		Dir = Dir.Normalized();
		FRotator R;
		R.Pitch = -asinf(FMath::Clamp(Dir.Z, -1.0f, 1.0f)) * Rad2Deg;
		if (fabsf(Dir.Z) < 0.999f)
		{
			R.Yaw = atan2f(Dir.Y, Dir.X) * Rad2Deg;
		}
		R.Roll = 0.0f;
		return R.ToQuaternion();
	}

	FColor ToFColor(const FLinearColor& C)
	{
		auto Ch = [](float V) -> uint32 { return static_cast<uint32>(FMath::Clamp(V, 0.0f, 1.0f) * 255.0f); };
		return FColor(Ch(C.R), Ch(C.G), Ch(C.B), Ch(C.A));
	}
}

ACameraCinematicActor::ACameraCinematicActor()
{
	bNeedsTick = true;
	bTickInEditor = true;
}

void ACameraCinematicActor::InitDefaultComponents()
{
	// 루트는 "카메라를 향해 회전하지 않는" 일반 씬 컴포넌트여야 한다.
	// UBillboardComponent::TickComponent 는 매 프레임 CachedWorldMatrix 를 활성 카메라
	// 방향으로 덮어쓰므로, 이를 루트로 쓰면 자식인 cine 카메라/웨이포인트의 월드 트랜스폼이
	// 매 프레임 왜곡되고(흔들림) 에디터↔PIE(기준 카메라가 다름) 간에도 위치/회전이 달라진다.
	USceneComponent* SceneRoot = AddComponent<USceneComponent>();
	SetRootComponent(SceneRoot);

	// 빌보드는 에디터 표시용 아이콘 — 자식 "리프"로만 둔다(자식이 없으니 왜곡 전파 없음).
	Billboard = AddComponent<UBillboardComponent>();
	if (Billboard)
	{
		Billboard->AttachToComponent(SceneRoot);
		Billboard->SetEditorOnlyComponent(true);
		Billboard->SetHiddenInComponentTree(true);
		Billboard->SetAbsoluteScale(true);
		if (UMaterial* Material = FMaterialManager::Get().GetOrCreateMaterial(RigIconMaterialPath))
		{
			Billboard->SetMaterial(Material);
		}
	}

	CineCamera = AddComponent<UCineCameraComponent>();
	if (CineCamera)
	{
		CineCamera->AttachToComponent(SceneRoot);
	}
}

UCineCameraComponent* ACameraCinematicActor::GetCineCamera() const
{
	if (CineCamera.Get())
	{
		return CineCamera.Get();
	}
	return const_cast<ACameraCinematicActor*>(this)->GetComponentByClass<UCineCameraComponent>();
}

void ACameraCinematicActor::ReacquireComponents()
{
	// 루트는 일반 USceneComponent. 빌보드/cine 카메라는 자식이므로 클래스로 재획득.
	if (!CineCamera.Get())
	{
		CineCamera = GetComponentByClass<UCineCameraComponent>();
	}
	if (!Billboard.Get())
	{
		Billboard = GetComponentByClass<UBillboardComponent>();
	}
	// editor-only / hidden 플래그는 직렬화되지 않으므로 로드/복제 후 재적용.
	if (Billboard.Get())
	{
		Billboard->SetEditorOnlyComponent(true);
		Billboard->SetHiddenInComponentTree(true);
	}
	for (UCinematicWaypointComponent* WP : GetOrderedWaypoints())
	{
		WP->SetEditorOnly(true);
	}
}

void ACameraCinematicActor::BeginPlay()
{
	Super::BeginPlay();
	ReacquireComponents();
	// 주의: 소유한 UCineCameraComponent 는 자기 BeginPlay 에서 PlayerCameraManager 에
	// 자동 등록된다. AutoPossessDefaultCamera 는 ActiveCamera 가 비어있을 때만 첫 등록
	// 카메라를 선택하므로, 플레이어 폰이 자기 카메라를 active 로 잡는 일반 흐름에서는
	// 시네마틱 카메라가 게임 뷰를 가로채지 않는다. 실제 전환은 Play() 에서만 일어난다.
	if (bPlayOnBeginPlay)
	{
		Play();
	}
}

void ACameraCinematicActor::EndPlay()
{
	Stop();
	Super::EndPlay();
}

void ACameraCinematicActor::OnPostLoad(FArchive& Ar)
{
	Super::OnPostLoad(Ar);
	ReacquireComponents();
}

void ACameraCinematicActor::PostDuplicate()
{
	Super::PostDuplicate();
	ReacquireComponents();
}

void ACameraCinematicActor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(CoordinateTarget, "ACameraCinematicActor.CoordinateTarget");
	Collector.AddReferencedObject(PreviousActiveCamera, "ACameraCinematicActor.PreviousActiveCamera");
	Collector.AddReferencedObject(PreviousViewTarget, "ACameraCinematicActor.PreviousViewTarget");
}

// ─────────────────────────────────────────────────────────────
// 웨이포인트 질의 / 편집
// ─────────────────────────────────────────────────────────────
TArray<UCinematicWaypointComponent*> ACameraCinematicActor::GetOrderedWaypoints() const
{
	TArray<UCinematicWaypointComponent*> Result;
	for (UActorComponent* Comp : GetComponents())
	{
		if (UCinematicWaypointComponent* WP = Cast<UCinematicWaypointComponent>(Comp))
		{
			if (IsValid(WP))
			{
				Result.push_back(WP);
			}
		}
	}
	std::sort(Result.begin(), Result.end(),
		[](const UCinematicWaypointComponent* A, const UCinematicWaypointComponent* B)
		{
			return A->GetSequenceIndex() < B->GetSequenceIndex();
		});
	return Result;
}

int32 ACameraCinematicActor::GetWaypointCount() const
{
	return static_cast<int32>(GetOrderedWaypoints().size());
}

UCinematicWaypointComponent* ACameraCinematicActor::AddWaypointAtWorld(const FVector& WorldLocation, const FRotator& WorldRotation)
{
	UCinematicWaypointComponent* WP = AddComponent<UCinematicWaypointComponent>();
	if (!WP)
	{
		return nullptr;
	}
	if (USceneComponent* Root = GetRootComponent())
	{
		WP->AttachToComponent(Root);
	}
	WP->InitWaypointVisual();
	// 방금 AddComponent 한 WP 도 카운트에 포함되므로 count-1 이 0-based 마지막 인덱스.
	WP->SetSequenceIndex(std::max(0, GetWaypointCount() - 1));
	WP->SetWorldLocation(WorldLocation);
	WP->SetWorldRotation(WorldRotation);
	return WP;
}

UCinematicWaypointComponent* ACameraCinematicActor::AddWaypointAtCameraView()
{
	FVector Location = GetActorLocation();
	FRotator Rotation = GetActorRotation();
	float FOV = 0.0f;
	if (UWorld* World = GetWorld())
	{
		FMinimalViewInfo POV;
		if (World->GetActivePOV(POV))
		{
			Location = POV.Location;
			Rotation = POV.Rotation;
			FOV = POV.FOV;
		}
	}
	UCinematicWaypointComponent* WP = AddWaypointAtWorld(Location, Rotation);
	if (WP && FOV > 0.0f)
	{
		WP->SetFOVOverride(FOV);
	}
	return WP;
}

UCinematicWaypointComponent* ACameraCinematicActor::AddWaypointAtEnd()
{
	TArray<UCinematicWaypointComponent*> Ordered = GetOrderedWaypoints();
	FVector Location = GetActorLocation();
	FRotator Rotation = GetActorRotation();
	if (!Ordered.empty())
	{
		UCinematicWaypointComponent* Last = Ordered.back();
		// 마지막 핀에서 그 forward 방향으로 한 칸 떨어뜨려 배치.
		Location = Last->GetWorldLocation() + Last->GetWorldRotation().GetForwardVector() * 3.0f;
		Rotation = Last->GetWorldRotation();
	}
	return AddWaypointAtWorld(Location, Rotation);
}

void ACameraCinematicActor::RemoveWaypoint(int32 Index)
{
	TArray<UCinematicWaypointComponent*> Ordered = GetOrderedWaypoints();
	if (Index < 0 || Index >= static_cast<int32>(Ordered.size()))
	{
		return;
	}
	UCinematicWaypointComponent* WP = Ordered[Index];
	RemoveComponent(WP);
	// 남은 핀 SequenceIndex 재정렬.
	int32 Seq = 0;
	for (UCinematicWaypointComponent* Remaining : GetOrderedWaypoints())
	{
		Remaining->SetSequenceIndex(Seq++);
	}
}

void ACameraCinematicActor::ClearWaypoints()
{
	for (UCinematicWaypointComponent* WP : GetOrderedWaypoints())
	{
		RemoveComponent(WP);
	}
}

void ACameraCinematicActor::MoveWaypoint(int32 From, int32 To)
{
	TArray<UCinematicWaypointComponent*> Ordered = GetOrderedWaypoints();
	const int32 Count = static_cast<int32>(Ordered.size());
	if (From < 0 || From >= Count || To < 0 || To >= Count || From == To)
	{
		return;
	}
	UCinematicWaypointComponent* Moved = Ordered[From];
	Ordered.erase(Ordered.begin() + From);
	Ordered.insert(Ordered.begin() + To, Moved);
	for (int32 i = 0; i < static_cast<int32>(Ordered.size()); ++i)
	{
		Ordered[i]->SetSequenceIndex(i);
	}
}

// ─────────────────────────────────────────────────────────────
// 평가
// ─────────────────────────────────────────────────────────────
float ACameraCinematicActor::EvaluateFOVAt(const UCinematicWaypointComponent* Waypoint) const
{
	if (Waypoint && Waypoint->GetFOVOverride() > 0.0f)
	{
		return Waypoint->GetFOVOverride();
	}
	return DefaultFOV;
}

FQuat ACameraCinematicActor::EvaluateRotationAt(const TArray<UCinematicWaypointComponent*>& Ordered, int32 Index) const
{
	const int32 N = static_cast<int32>(Ordered.size());
	if (Index < 0 || Index >= N)
	{
		return FQuat::Identity;
	}
	UCinematicWaypointComponent* WP = Ordered[Index];
	switch (WP->GetRotMode())
	{
	case ECinematicRotMode::LookAtTarget:
	{
		AActor* Target = WP->GetLookAtActor();
		if (!IsValid(Target))
		{
			Target = CoordinateTarget.Get();
		}
		if (IsValid(Target))
		{
			return MakeLookQuat(WP->GetWorldLocation(), Target->GetActorLocation());
		}
		return WP->GetWorldRotation().ToQuaternion();
	}
	case ECinematicRotMode::LookAtNextPoint:
	{
		if (Index + 1 < N)
		{
			return MakeLookQuat(WP->GetWorldLocation(), Ordered[Index + 1]->GetWorldLocation());
		}
		if (Index - 1 >= 0)
		{
			return MakeLookQuat(Ordered[Index - 1]->GetWorldLocation(), WP->GetWorldLocation());
		}
		return WP->GetWorldRotation().ToQuaternion();
	}
	case ECinematicRotMode::UseWaypointRotation:
	default:
		return WP->GetWorldRotation().ToQuaternion();
	}
}

float ACameraCinematicActor::SegmentSecondsFor(const TArray<UCinematicWaypointComponent*>& Ordered, int32 Index) const
{
	const int32 N = static_cast<int32>(Ordered.size());
	if (N < 2 || Index < 0 || Index >= N - 1)
	{
		return 0.0f;
	}
	// 핀별 override 가 있으면 그걸로 구간 속도 결정, 없으면 Duration 을 구간 수로 균등 분배.
	const float Override = Ordered[Index]->GetSegmentSeconds();
	if (Override > 0.0f)
	{
		return Override;
	}
	const float Auto = Duration / static_cast<float>(N - 1);
	return Auto > 0.0001f ? Auto : 0.0001f;
}

float ACameraCinematicActor::ComputeTotalTimeline() const
{
	TArray<UCinematicWaypointComponent*> Ordered = GetOrderedWaypoints();
	const int32 N = static_cast<int32>(Ordered.size());
	if (N < 2)
	{
		return 0.0f;
	}
	float Total = 0.0f;
	for (UCinematicWaypointComponent* WP : Ordered)
	{
		Total += WP->GetDwellSeconds();
	}
	for (int32 i = 0; i + 1 < N; ++i)
	{
		Total += SegmentSecondsFor(Ordered, i);
	}
	return Total;
}

bool ACameraCinematicActor::EvaluateAtTime(float TimeSeconds, FVector& OutLocation, FQuat& OutRotation, float& OutFOV) const
{
	TArray<UCinematicWaypointComponent*> W = GetOrderedWaypoints();
	const int32 N = static_cast<int32>(W.size());
	if (N == 0)
	{
		return false;
	}
	if (N == 1)
	{
		OutLocation = W[0]->GetWorldLocation();
		OutRotation = EvaluateRotationAt(W, 0);
		OutFOV = EvaluateFOVAt(W[0]);
		return true;
	}

	const float Total = ComputeTotalTimeline();
	float T = FMath::Clamp(TimeSeconds, 0.0f, Total);

	float Acc = 0.0f;
	for (int32 i = 0; i < N; ++i)
	{
		const float Dwell = W[i]->GetDwellSeconds();
		// 웨이포인트 i 에서 머무는 구간 (마지막 핀이면 무조건 여기서 종료).
		if (T <= Acc + Dwell || i == N - 1)
		{
			OutLocation = W[i]->GetWorldLocation();
			OutRotation = EvaluateRotationAt(W, i);
			OutFOV = EvaluateFOVAt(W[i]);
			return true;
		}
		Acc += Dwell;

		// 구간 i → i+1 — 핀별 SegmentSeconds(없으면 Duration 균등)로 통과 시간 결정.
		const float SegTime = SegmentSecondsFor(W, i);
		if (T <= Acc + SegTime)
		{
			const float A = SegTime > 0.0f ? (T - Acc) / SegTime : 1.0f;
			// 출발핀(i)의 AccelOut, 도착핀(i+1)의 DecelIn 으로 이 구간의 가감속을 결정.
			// 둘 다 0 → 등속 통과(핀에서 안 멈춤). 값을 키우면 그 핀에서 가/감속.
			const float E = HermiteEase(A, 1.0f - W[i]->GetAccelOut(), 1.0f - W[i + 1]->GetDecelIn());
			if (bSmoothPath)
			{
				const FVector P0 = W[std::max(0, i - 1)]->GetWorldLocation();
				const FVector P1 = W[i]->GetWorldLocation();
				const FVector P2 = W[i + 1]->GetWorldLocation();
				const FVector P3 = W[std::min(N - 1, i + 2)]->GetWorldLocation();
				OutLocation = CatmullRom(P0, P1, P2, P3, E);
			}
			else
			{
				OutLocation = FVector::Lerp(W[i]->GetWorldLocation(), W[i + 1]->GetWorldLocation(), E);
			}
			OutRotation = FQuat::Slerp(EvaluateRotationAt(W, i), EvaluateRotationAt(W, i + 1), E);
			OutFOV = FMath::Lerp(EvaluateFOVAt(W[i]), EvaluateFOVAt(W[i + 1]), E);
			return true;
		}
		Acc += SegTime;
	}

	OutLocation = W[N - 1]->GetWorldLocation();
	OutRotation = EvaluateRotationAt(W, N - 1);
	OutFOV = EvaluateFOVAt(W[N - 1]);
	return true;
}

// ─────────────────────────────────────────────────────────────
// 재생 제어
// ─────────────────────────────────────────────────────────────
void ACameraCinematicActor::Play()
{
	if (GetWaypointCount() < 2)
	{
		return;
	}
	bPlaying = true;
	bPaused = false;
	CurrentTime = 0.0f;

	// 평가 결과를 먼저 cine 카메라에 반영 — view target 전환 시 첫 프레임부터 올바른 POV.
	ApplyEvaluatedToCineCamera(CurrentTime);

	if (HasActorBegunPlay() && !bSwitchedCamera)
	{
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (APlayerCameraManager* CM = PC->GetPlayerCameraManager())
				{
					if (GetCineCamera())
					{
						PreviousViewTarget = CM->GetViewTarget();
						PreviousActiveCamera = CM->GetActiveCamera();
						FViewTargetTransitionParams Params;
						Params.BlendTime = BlendInTime;
						CM->SetViewTarget(this, Params);
						bSwitchedCamera = true;
					}
				}
			}
		}
	}
}

void ACameraCinematicActor::Stop()
{
	bPlaying = false;
	bPaused = false;
	if (bSwitchedCamera)
	{
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (APlayerCameraManager* CM = PC->GetPlayerCameraManager())
				{
					// 이전 view target 복원 (없으면 이전 active 카메라의 소유 액터).
					AActor* RestoreTarget = IsValid(PreviousViewTarget.Get())
						? PreviousViewTarget.Get()
						: (IsValid(PreviousActiveCamera.Get()) ? PreviousActiveCamera->GetOwner() : nullptr);
					if (IsValid(RestoreTarget))
					{
						FViewTargetTransitionParams Params;
						Params.BlendTime = BlendOutTime;
						CM->SetViewTarget(RestoreTarget, Params);
					}
				}
			}
		}
		bSwitchedCamera = false;
		PreviousActiveCamera = nullptr;
		PreviousViewTarget = nullptr;
	}
}

void ACameraCinematicActor::Pause()
{
	if (bPlaying)
	{
		bPaused = !bPaused;
	}
}

void ACameraCinematicActor::SetTime(float InTimeSeconds)
{
	const float Total = ComputeTotalTimeline();
	CurrentTime = FMath::Clamp(InTimeSeconds, 0.0f, Total);
	ApplyEvaluatedToCineCamera(CurrentTime);
}

void ACameraCinematicActor::ApplyEvaluatedToCineCamera(float TimeSeconds)
{
	FVector Location;
	FQuat Rotation;
	float FOV = DefaultFOV;
	if (!EvaluateAtTime(TimeSeconds, Location, Rotation, FOV))
	{
		return;
	}
	if (UCineCameraComponent* Cam = GetCineCamera())
	{
		Cam->SetWorldLocation(Location);
		Cam->SetWorldRotation(Rotation);
		Cam->SetFOV(FOV);
	}
}

// ─────────────────────────────────────────────────────────────
// 좌표 타깃 추종 + 시각화
// ─────────────────────────────────────────────────────────────
void ACameraCinematicActor::ApplyCoordinateTargetFollow()
{
	AActor* Target = CoordinateTarget.Get();
	if (!IsValid(Target))
	{
		return;
	}
	if (USceneComponent* Root = GetRootComponent())
	{
		Root->SetWorldLocation(Target->GetActorLocation());
		Root->SetWorldRotation(Target->GetActorRotation());
	}
}

void ACameraCinematicActor::DrawPathDebug() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TArray<UCinematicWaypointComponent*> W = GetOrderedWaypoints();
	const int32 N = static_cast<int32>(W.size());
	if (N == 0)
	{
		return;
	}

	const FColor LineColor = ToFColor(PathColor);
	const FColor PinColor(255, 200, 40);
	const FColor WaypointFrustumColor(120, 200, 255);
	const FColor CurrentFrustumColor(255, 255, 80);

	// 프러스텀 종횡비는 cine 카메라 설정과 맞춘다.
	float Aspect = 16.0f / 9.0f;
	if (UCineCameraComponent* Cam = GetCineCamera())
	{
		Aspect = Cam->GetAspectRatio();
	}

	// 주어진 포즈/FOV 로 짧은 프러스텀(방향 표시용)을 그린다.
	auto DrawPoseFrustum = [&](const FVector& Loc, const FQuat& Quat, float Fov, float FarLen, const FColor& Col)
	{
		FMinimalViewInfo POV;
		POV.Location = Loc;
		POV.Rotation = FRotator::FromQuaternion(Quat);
		POV.FOV = Fov > 0.0f ? Fov : DefaultFOV;
		POV.AspectRatio = Aspect;
		POV.NearClip = 0.05f;
		POV.FarClip = FarLen;
		DrawDebugFrustum(World, POV.CalculateViewProjectionMatrix(), Col);
	};

	// 경로 곡선 (기하학적 샘플 — 타이밍과 무관).
	constexpr int32 StepsPerSegment = 16;
	for (int32 i = 0; i + 1 < N; ++i)
	{
		const FVector P0 = W[std::max(0, i - 1)]->GetWorldLocation();
		const FVector P1 = W[i]->GetWorldLocation();
		const FVector P2 = W[i + 1]->GetWorldLocation();
		const FVector P3 = W[std::min(N - 1, i + 2)]->GetWorldLocation();
		FVector Prev = P1;
		const int32 Steps = bSmoothPath ? StepsPerSegment : 1;
		for (int32 s = 1; s <= Steps; ++s)
		{
			const float U = static_cast<float>(s) / static_cast<float>(Steps);
			const FVector Cur = bSmoothPath ? CatmullRom(P0, P1, P2, P3, U) : FVector::Lerp(P1, P2, U);
			DrawDebugLine(World, Prev, Cur, LineColor);
			Prev = Cur;
		}
	}

	// 핀 마커 + 각 웨이포인트가 "바라보는 방향"을 그 지점 카메라 프러스텀으로 표시.
	for (int32 i = 0; i < N; ++i)
	{
		const FVector Pos = W[i]->GetWorldLocation();
		DrawDebugSphere(World, Pos, 0.2f, 10, PinColor);
		DrawPoseFrustum(Pos, EvaluateRotationAt(W, i), EvaluateFOVAt(W[i]), 1.5f, WaypointFrustumColor);
	}

	// 에디터 프리뷰: 현재 시간(CurrentTime)의 cine 카메라 프러스텀(밝게).
	if (!HasActorBegunPlay())
	{
		if (UCineCameraComponent* Cam = GetCineCamera())
		{
			FMinimalViewInfo POV;
			Cam->GetCameraView(0.0f, POV);
			DrawDebugFrustum(World, POV.CalculateViewProjectionMatrix(), CurrentFrustumColor);
		}
	}
}

void ACameraCinematicActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ApplyCoordinateTargetFollow();

	if (bPlaying && !bPaused)
	{
		CurrentTime += DeltaTime;
		const float Total = ComputeTotalTimeline();
		if (Total <= 0.0f)
		{
			Stop();
		}
		else if (CurrentTime >= Total)
		{
			if (bLoop)
			{
				CurrentTime = fmodf(CurrentTime, Total);
				ApplyEvaluatedToCineCamera(CurrentTime);
			}
			else
			{
				CurrentTime = Total;
				ApplyEvaluatedToCineCamera(CurrentTime);
				Stop();
			}
		}
		else
		{
			ApplyEvaluatedToCineCamera(CurrentTime);
		}
	}

	const bool bEditor = !HasActorBegunPlay();

	// 에디터에서 재생 중이 아니면 cine 카메라를 현재 스크럽 시간 위치/방향에 맞춰 둔다.
	// (그래야 프리뷰 프러스텀이 기본 포즈가 아니라 경로 상의 실제 POV 를 보여준다.)
	if (bEditor && !bPlaying)
	{
		ApplyEvaluatedToCineCamera(CurrentTime);
	}

	if (bEditor || bPlaying)
	{
		DrawPathDebug();
	}
}
