#include "Scripting/LuaActorProxy.h"

#include "Component/ActorComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Shape/ShapeComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/Log.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Game/GameActors/Obstacle/ObstacleActorBase.h"
#include "Object/Object.h"
#include "Object/UClass.h"
#include "Scripting/LuaComponentProxy.h"

#include <algorithm>
#include <cmath>
#include <limits>

struct FLuaActorProxy::FLuaActorTaskState
{
	// MoveTo/MoveToActor는 Lua 호출 즉시 블로킹하지 않고,
	// C++ Tick에서 목표까지 조금씩 이동하기 위해 상태만 기록한다.
	bool bMoveActive = false;
	bool bMoveToActor = false;
	FVector MoveTargetLocation = FVector(0.0f, 0.0f, 0.0f);
	AActor* MoveTargetActor = nullptr;
	float MoveSpeed = 300.0f;
	float MoveAcceptRadius = 1.0f;
};

namespace
{
	AActor* ResolveAliveActor(AActor* Actor)
	{
		return (Actor && IsAliveObject(Actor)) ? Actor : nullptr;
	}

	UActorComponent* ResolveAliveComponent(UActorComponent* Component)
	{
		if (!Component || !IsAliveObject(Component))
		{
			return nullptr;
		}

		// Component만 살아 있어도 Owner Actor가 죽었으면 Lua가 건드리면 안 된다.
		AActor* OwnerActor = ResolveAliveActor(Component->GetOwner());
		if (!OwnerActor)
		{
			return nullptr;
		}

		const TArray<UActorComponent*>& OwnerComponents = OwnerActor->GetComponents();
		const auto It = std::find(OwnerComponents.begin(), OwnerComponents.end(), Component);
		return (It != OwnerComponents.end()) ? Component : nullptr;
	}

	FLuaComponentProxy MakeComponentProxy(UActorComponent* Component)
	{
		FLuaComponentProxy Proxy;
		// Proxy가 Lua 쪽에서 오래 살아남을 수 있으므로 실제 안전성은 각 함수의 재검증이 책임진다.
		Proxy.Component = ResolveAliveComponent(Component);
		return Proxy;
	}

	FString StripLuaClassPrefix(const FString& Name)
	{
		// C++ class 이름은 UStaticMeshComponent처럼 U prefix를 갖지만,
		// Lua 작성자는 StaticMeshComponent처럼 엔진 접두어를 빼고 쓰는 경우가 많다.
		if (Name.size() > 1 && Name[0] == 'U')
		{
			return Name.substr(1);
		}

		return Name;
	}

	bool IsLuaNameMatch(const FString& RequestedName, const FString& CandidateName)
	{
		if (RequestedName.empty() || CandidateName.empty())
		{
			return false;
		}

		return RequestedName == CandidateName
			|| StripLuaClassPrefix(RequestedName) == StripLuaClassPrefix(CandidateName);
	}

	bool IsComponentNameMatch(const UActorComponent* Component, const FString& ComponentName)
	{
		if (!Component || ComponentName.empty())
		{
			return false;
		}

		// GetComponent는 실제 객체 이름과 class 이름을 모두 받는다.
		if (IsLuaNameMatch(ComponentName, Component->GetFName().ToString()))
		{
			return true;
		}

		UClass* ComponentClass = Component->GetClass();
		return ComponentClass && IsLuaNameMatch(ComponentName, ComponentClass->GetName());
	}

	UClass* FindComponentClassByLuaName(const FString& TypeName)
	{
		if (TypeName.empty())
		{
			return nullptr;
		}

		for (UClass* Class : UClass::GetAllClasses())
		{
			if (!Class || !Class->IsA(UActorComponent::StaticClass()))
			{
				continue;
			}

			if (IsLuaNameMatch(TypeName, Class->GetName()))
			{
				return Class;
			}
		}

		return nullptr;
	}

	float GetShapeHalfHeight(const UPrimitiveComponent* PrimitiveComponent)
	{
		if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(PrimitiveComponent))
		{
			return BoxComponent->GetBoxExtent().Z;
		}
		if (const USphereComponent* SphereComponent = Cast<USphereComponent>(PrimitiveComponent))
		{
			return SphereComponent->GetSphereRadius();
		}
		if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(PrimitiveComponent))
		{
			return CapsuleComponent->GetCapsuleHalfHeight();
		}

		const FBoundingBox Bounds = PrimitiveComponent ? PrimitiveComponent->GetWorldBoundingBox() : FBoundingBox();
		return Bounds.IsValid() ? Bounds.GetExtent().Z : 0.0f;
	}

	UPrimitiveComponent* GetPrimaryCollisionPrimitive(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
		{
			return RootPrimitive;
		}

		for (UPrimitiveComponent* PrimitiveComponent : Actor->GetPrimitiveComponents())
		{
			if (PrimitiveComponent && IsAliveObject(PrimitiveComponent) && PrimitiveComponent->IsCollisionEnabled())
			{
				return PrimitiveComponent;
			}
		}

		return nullptr;
	}

	bool IsGroundCandidatePrimitive(const UPrimitiveComponent* PrimitiveComponent)
	{
		if (!PrimitiveComponent || !IsAliveObject(PrimitiveComponent) || !PrimitiveComponent->IsCollisionEnabled())
		{
			return false;
		}

		return PrimitiveComponent->IsA<UShapeComponent>() || PrimitiveComponent->IsA<UStaticMeshComponent>();
	}

	bool HasHorizontalOverlap(const FBoundingBox& A, const FBoundingBox& B, float SkinWidth)
	{
		return (A.Min.X - SkinWidth <= B.Max.X && A.Max.X + SkinWidth >= B.Min.X)
			&& (A.Min.Y - SkinWidth <= B.Max.Y && A.Max.Y + SkinWidth >= B.Min.Y);
	}
}

FLuaActorProxy::FLuaActorProxy()
	: TaskState(std::make_shared<FLuaActorTaskState>())
{
}

bool FLuaActorProxy::IsValid() const
{
	// Lua가 들고 있는 Proxy는 Actor를 소유하지 않으므로 매 호출마다 생존 여부를 다시 본다.
	return GetActor() != nullptr;
}

AActor* FLuaActorProxy::GetActor() const
{
	// Proxy는 실제 AActor*를 Lua에 직접 노출하지 않는 얇은 허브다.
	return ResolveAliveActor(Actor);
}

FString FLuaActorProxy::GetName() const
{
	AActor* TargetActor = GetActor();
	return TargetActor ? TargetActor->GetFName().ToString() : FString();
}

uint32 FLuaActorProxy::GetUUID() const
{
	AActor* TargetActor = GetActor();
	return TargetActor ? TargetActor->GetUUID() : 0u;
}

FString FLuaActorProxy::GetTag() const
{
	AActor* TargetActor = GetActor();
	return TargetActor ? TargetActor->GetTag() : FString();
}

void FLuaActorProxy::SetTag(const FString& InTag)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		return;
	}

	TargetActor->SetTag(InTag);
}

bool FLuaActorProxy::HasTag(const FString& InTag) const
{
	AActor* TargetActor = GetActor();
	return TargetActor ? TargetActor->HasTag(InTag) : false;
}

FVector FLuaActorProxy::GetWorldLocation() const
{
	AActor* TargetActor = GetActor();
	return TargetActor ? TargetActor->GetActorLocation() : FVector(0.0f, 0.0f, 0.0f);
}

void FLuaActorProxy::SetWorldLocation(const FVector& InLocation)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		return;
	}

	TargetActor->SetActorLocation(InLocation);
}

void FLuaActorProxy::SetWorldLocationXYZ(float X, float Y, float Z)
{
	SetWorldLocation(FVector(X, Y, Z));
}

FRotator FLuaActorProxy::GetWorldRotation() const
{
	AActor* TargetActor = GetActor();
	return TargetActor ? TargetActor->GetActorRotation() : FRotator::ZeroRotator;
}

void FLuaActorProxy::SetWorldRotation(const FRotator& InRotation)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		return;
	}

	TargetActor->SetActorRotation(InRotation);
}

void FLuaActorProxy::SetWorldRotationXYZ(float Pitch, float Yaw, float Roll)
{
	SetWorldRotation(FRotator(Pitch, Yaw, Roll));
}

FVector FLuaActorProxy::GetWorldScale() const
{
	AActor* TargetActor = GetActor();
	return TargetActor ? TargetActor->GetActorScale() : FVector(1.0f, 1.0f, 1.0f);
}

void FLuaActorProxy::SetWorldScale(const FVector& InScale)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		return;
	}

	TargetActor->SetActorScale(InScale);
}

void FLuaActorProxy::SetWorldScaleXYZ(float X, float Y, float Z)
{
	SetWorldScale(FVector(X, Y, Z));
}

FVector FLuaActorProxy::GetVelocity() const
{
	return Velocity;
}

void FLuaActorProxy::SetVelocity(const FVector& InVelocity)
{
	Velocity = InVelocity;
}

void FLuaActorProxy::AddWorldOffset(const FVector& Delta)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		return;
	}

	TargetActor->AddActorWorldOffset(Delta);
}

void FLuaActorProxy::AddWorldOffsetXYZ(float X, float Y, float Z)
{
	AddWorldOffset(FVector(X, Y, Z));
}

FVector FLuaActorProxy::GetForwardVector() const
{
	AActor* TargetActor = GetActor();
	if (TargetActor && TargetActor->GetRootComponent())
	{
		return TargetActor->GetRootComponent()->GetForwardVector();
	}
	return FVector(1.0f, 0.0f, 0.0f);
}

FVector FLuaActorProxy::GetRightVector() const
{
	AActor* TargetActor = GetActor();
	if (TargetActor && TargetActor->GetRootComponent())
	{
		return TargetActor->GetRootComponent()->GetRightVector();
	}
	return FVector(0.0f, 1.0f, 0.0f);
}

FVector FLuaActorProxy::GetUpVector() const
{
	AActor* TargetActor = GetActor();
	if (TargetActor && TargetActor->GetRootComponent())
	{
		return TargetActor->GetRootComponent()->GetUpVector();
	}
	return FVector(0.0f, 0.0f, 1.0f);
}

FLuaGroundHit FLuaActorProxy::FindGround(float MaxDistance, float SkinWidth) const
{
	FLuaGroundHit Result;

	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		return Result;
	}

	UWorld* World = TargetActor->GetWorld();
	UPrimitiveComponent* PrimaryPrimitive = GetPrimaryCollisionPrimitive(TargetActor);
	if (!World || !PrimaryPrimitive)
	{
		return Result;
	}

	const float SafeMaxDistance = (std::max)(0.0f, std::isfinite(MaxDistance) ? MaxDistance : 0.0f);
	const float SafeSkinWidth = (std::max)(0.0f, std::isfinite(SkinWidth) ? SkinWidth : 0.0f);
	const FVector ActorLocation = TargetActor->GetActorLocation();
	const float FootZ = ActorLocation.Z - GetShapeHalfHeight(PrimaryPrimitive);

	FBoundingBox QueryBounds = PrimaryPrimitive->GetWorldBoundingBox();
	if (!QueryBounds.IsValid())
	{
		const float HalfHeight = GetShapeHalfHeight(PrimaryPrimitive);
		QueryBounds = FBoundingBox(
			ActorLocation - FVector(1.0f, 1.0f, HalfHeight),
			ActorLocation + FVector(1.0f, 1.0f, HalfHeight));
	}

	float BestGroundZ = -std::numeric_limits<float>::infinity();
	float BestDistance = std::numeric_limits<float>::max();
	AActor* BestActor = nullptr;
	UPrimitiveComponent* BestComponent = nullptr;

	for (AActor* CandidateActor : World->GetActors())
	{
		if (!CandidateActor || !IsAliveObject(CandidateActor) || CandidateActor == TargetActor)
		{
			continue;
		}

		for (UPrimitiveComponent* CandidateComponent : CandidateActor->GetPrimitiveComponents())
		{
			if (!IsGroundCandidatePrimitive(CandidateComponent))
			{
				continue;
			}

			const FBoundingBox CandidateBounds = CandidateComponent->GetWorldBoundingBox();
			if (!CandidateBounds.IsValid() || !HasHorizontalOverlap(QueryBounds, CandidateBounds, SafeSkinWidth))
			{
				continue;
			}

			const float CandidateTopZ = CandidateBounds.Max.Z;
			const float Distance = FootZ - CandidateTopZ;
			if (CandidateTopZ > FootZ + SafeSkinWidth || Distance > SafeMaxDistance)
			{
				continue;
			}

			if (CandidateTopZ > BestGroundZ)
			{
				BestGroundZ = CandidateTopZ;
				BestDistance = Distance;
				BestActor = CandidateActor;
				BestComponent = CandidateComponent;
			}
		}
	}

	if (!BestActor || !BestComponent)
	{
		return Result;
	}

	Result.bHit = true;
	Result.GroundZ = BestGroundZ;
	Result.Distance = BestDistance;
	Result.Location = FVector(ActorLocation.X, ActorLocation.Y, BestGroundZ);
	Result.Normal = FVector(0.0f, 0.0f, 1.0f);
	Result.Actor.Actor = ResolveAliveActor(BestActor);
	Result.Component = MakeComponentProxy(BestComponent);
	return Result;
}

void FLuaActorProxy::MoveTo(const FVector& Target)
{
	if (!TaskState)
	{
		TaskState = std::make_shared<FLuaActorTaskState>();
	}

	// MoveTo는 방향 벡터가 아니라 월드 좌표 목표를 저장한다.
	TaskState->bMoveActive = true;
	TaskState->bMoveToActor = false;
	TaskState->MoveTargetLocation = Target;
	TaskState->MoveTargetActor = nullptr;
}

void FLuaActorProxy::MoveTo2D(float X, float Y)
{
	const FVector CurrentLocation = GetWorldLocation();
	MoveTo(FVector(X, Y, CurrentLocation.Z));
}

void FLuaActorProxy::MoveTo3D(float X, float Y, float Z)
{
	MoveTo(FVector(X, Y, Z));
}

void FLuaActorProxy::MoveBy(const FVector& Delta)
{
	MoveTo(GetWorldLocation() + Delta);
}

void FLuaActorProxy::MoveBy2D(float X, float Y)
{
	MoveBy(FVector(X, Y, 0.0f));
}

void FLuaActorProxy::MoveBy3D(float X, float Y, float Z)
{
	MoveBy(FVector(X, Y, Z));
}

void FLuaActorProxy::MoveToActor(const FLuaActorProxy& TargetActor)
{
	if (!TaskState)
	{
		TaskState = std::make_shared<FLuaActorTaskState>();
	}

	// target Actor는 나중에 파괴될 수 있으므로 Tick마다 다시 검증한다.
	TaskState->bMoveActive = true;
	TaskState->bMoveToActor = true;
	TaskState->MoveTargetActor = TargetActor.GetActor();
	TaskState->MoveTargetLocation = TargetActor.GetWorldLocation();
}

void FLuaActorProxy::StopMove()
{
	if (!TaskState)
	{
		return;
	}

	TaskState->bMoveActive = false;
	TaskState->bMoveToActor = false;
	TaskState->MoveTargetActor = nullptr;
}

bool FLuaActorProxy::IsMoveDone() const
{
	return !TaskState || !TaskState->bMoveActive;
}

void FLuaActorProxy::SetMoveSpeed(float InSpeed)
{
	if (!TaskState)
	{
		TaskState = std::make_shared<FLuaActorTaskState>();
	}

	// 음수/비정상 속도는 이동 방향을 뒤집거나 NaN을 퍼뜨릴 수 있으므로 0 이상으로 제한한다.
	TaskState->MoveSpeed = (std::max)(0.0f, std::isfinite(InSpeed) ? InSpeed : 0.0f);
}

float FLuaActorProxy::GetMoveSpeed() const
{
	return TaskState ? TaskState->MoveSpeed : 0.0f;
}

FLuaComponentProxy FLuaActorProxy::GetComponent(const FString& ComponentName)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor || ComponentName.empty())
	{
		return FLuaComponentProxy();
	}

	for (UActorComponent* Component : TargetActor->GetComponents())
	{
		if (!ResolveAliveComponent(Component))
		{
			continue;
		}

		if (IsComponentNameMatch(Component, ComponentName))
		{
			return MakeComponentProxy(Component);
		}
	}

	return FLuaComponentProxy();
}

FLuaComponentProxy FLuaActorProxy::GetComponentByType(const FString& TypeName)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor || TypeName.empty())
	{
		return FLuaComponentProxy();
	}

	UClass* RequestedClass = FindComponentClassByLuaName(TypeName);
	for (UActorComponent* Component : TargetActor->GetComponents())
	{
		if (!ResolveAliveComponent(Component))
		{
			continue;
		}

		UClass* ComponentClass = Component->GetClass();
		if (!ComponentClass)
		{
			continue;
		}

		// 등록된 class를 찾은 경우 상속 관계까지 허용한다.
		if (RequestedClass)
		{
			if (ComponentClass->IsA(RequestedClass))
			{
				return MakeComponentProxy(Component);
			}
			continue;
		}

		if (IsLuaNameMatch(TypeName, ComponentClass->GetName()))
		{
			return MakeComponentProxy(Component);
		}
	}

	return FLuaComponentProxy();
}

FLuaComponentProxy FLuaActorProxy::GetScriptComponent()
{
	// Script 전용 Proxy가 생기기 전까지는 공통 ComponentProxy로 반환한다.
	// TODO: ScriptComponent 전용 Lua API가 필요해지면 FLuaScriptComponentProxy로 분리한다.
	return GetComponentByType("ScriptComponent");
}

FLuaComponentProxy FLuaActorProxy::GetStaticMeshComponent()
{
	return GetComponentByType("StaticMeshComponent");
}

void FLuaActorProxy::TickLuaTasks(float DeltaTime)
{
	TickMovement(DeltaTime);
}

void FLuaActorProxy::TickMovement(float DeltaTime)
{
	if (!TaskState || !TaskState->bMoveActive)
	{
		return;
	}

	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		StopMove();
		return;
	}

	FVector TargetLocation = TaskState->MoveTargetLocation;
	if (TaskState->bMoveToActor)
	{
		AActor* MoveTargetActor = ResolveAliveActor(TaskState->MoveTargetActor);
		if (!MoveTargetActor)
		{
			StopMove();
			return;
		}

		TargetLocation = MoveTargetActor->GetActorLocation();
		TaskState->MoveTargetLocation = TargetLocation;
	}

	const FVector CurrentLocation = TargetActor->GetActorLocation();
	const FVector ToTarget = TargetLocation - CurrentLocation;
	const float Distance = ToTarget.Length();
	if (Distance <= TaskState->MoveAcceptRadius)
	{
		TargetActor->SetActorLocation(TargetLocation);
		StopMove();
		return;
	}

	const float Step = TaskState->MoveSpeed * (std::max)(0.0f, DeltaTime);
	if (Step <= 0.0f)
	{
		return;
	}

	if (Step >= Distance)
	{
		TargetActor->SetActorLocation(TargetLocation);
		StopMove();
		return;
	}

	FVector Direction = ToTarget;
	Direction.Normalize();
	TargetActor->SetActorLocation(CurrentLocation + Direction * Step);
}

void FLuaActorProxy::PrintLocation() const
{
	const FVector Location = GetWorldLocation();
	UE_LOG("[Lua] Actor %u Location = (%.3f, %.3f, %.3f)", GetUUID(), Location.X, Location.Y, Location.Z);
}

void FLuaActorProxy::Destroy()
{
	// Lua에서 Destroy를 호출해도 실제 파괴는 World를 통해 진행한다.
	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		return;
	}

	UWorld* World = TargetActor->GetWorld();
	if (!World)
	{
		return;
	}

	World->DestroyActor(TargetActor);
	Actor = nullptr;
	StopMove();
}

int FLuaActorProxy::GetDamage() const
{
	// 장애물별 피해량을 다르게 줄 수 있게 C++ Damage 값을 Lua에서 읽는다.
	// ActorProxy는 모든 actor에 붙는 타입이라, 장애물이 아니면 Lua fallback과 같은 기본 피해량 1을 돌려준다.
	AActor* TargetActor = GetActor();
	if (AObstacleActorBase* Obstacle = Cast<AObstacleActorBase>(TargetActor))
	{
		return Obstacle->GetDamage();
	}

	return 1;
}

bool FLuaActorProxy::SetDamage(int InDamage)
{
	// 나중에 생성자/툴에서 장애물별 Damage를 바꾸고 싶을 때 쓰는 안전한 setter입니다.
	AActor* TargetActor = GetActor();
	if (AObstacleActorBase* Obstacle = Cast<AObstacleActorBase>(TargetActor))
	{
		Obstacle->SetDamage((std::max)(0, InDamage));
		return true;
	}

	return false;
}
