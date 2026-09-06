#include "AnimNotifyState_AttackHitWindow.h"

#include "Animation/AnimInstance.h"
#include "Animation/Notify/AnimNotifyState_ParryWindow.h"
#include "Component/Combat/CombatHitEventComponent.h"
#include "Component/Combat/CombatStateComponent.h"
#include "Component/Combat/HealthComponent.h"
#include "Component/Input/ActionComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Core/Types/CollisionTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Debug/DrawDebugHelpers.h"
#include "Core/Logging/Log.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/World.h"
#include "Math/Quat.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "Profiling/Stats/Stats.h"

#include <cfloat>
#include <cmath>

namespace
{
	int32 FindBoneIndex(USkeletalMeshComponent* MeshComp, const FString& BoneName)
	{
		if (!IsValid(MeshComp) || BoneName.empty()) return -1;

		USkeletalMesh* Mesh = MeshComp->GetSkeletalMesh();
		FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		if (!Asset) return -1;

		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Asset->Bones.size()); ++BoneIndex)
		{
			if (Asset->Bones[BoneIndex].Name == BoneName)
			{
				return BoneIndex;
			}
		}

		return -1;
	}

	FVector MakeActorLocalOffset(AActor* Actor, const FVector& LocalOffset)
	{
		if (!IsValid(Actor)) return LocalOffset;

		return Actor->GetActorForward() * LocalOffset.X
			+ Actor->GetActorRight() * LocalOffset.Y
			+ FVector::UpVector * LocalOffset.Z;
	}

	FVector GetHitCenter(USkeletalMeshComponent* MeshComp, AActor* Owner, const FString& BoneName, const FVector& LocalOffset)
	{
		const FVector WorldOffset = MakeActorLocalOffset(Owner, LocalOffset);
		const int32 BoneIndex = FindBoneIndex(MeshComp, BoneName);
		if (BoneIndex >= 0)
		{
			return MeshComp->GetBoneLocationByIndex(BoneIndex) + WorldOffset;
		}

		return IsValid(Owner) ? Owner->GetActorLocation() + WorldOffset : WorldOffset;
	}

	float DistanceSquaredPointAABB(const FVector& Point, const FBoundingBox& Box)
	{
		const float X = Point.X < Box.Min.X ? Box.Min.X - Point.X : (Point.X > Box.Max.X ? Point.X - Box.Max.X : 0.0f);
		const float Y = Point.Y < Box.Min.Y ? Box.Min.Y - Point.Y : (Point.Y > Box.Max.Y ? Point.Y - Box.Max.Y : 0.0f);
		const float Z = Point.Z < Box.Min.Z ? Box.Min.Z - Point.Z : (Point.Z > Box.Max.Z ? Point.Z - Box.Max.Z : 0.0f);
		return X * X + Y * Y + Z * Z;
	}

	FVector ClosestPointOnAABB(const FVector& Point, const FBoundingBox& Box)
	{
		return FVector(
			std::max(Box.Min.X, std::min(Point.X, Box.Max.X)),
			std::max(Box.Min.Y, std::min(Point.Y, Box.Max.Y)),
			std::max(Box.Min.Z, std::min(Point.Z, Box.Max.Z)));
	}

	void DrawDebugBounds(UWorld* World, const FBoundingBox& Bounds, const FColor& Color, float Duration)
	{
		if (!World || !Bounds.IsValid())
		{
			return;
		}

		DrawDebugBox(World, Bounds.GetCenter(), Bounds.GetExtent(), Color, Duration);
	}

	UActionComponent* GetOrCreateActionComponent(AActor* Actor, bool bAutoAdd)
	{
		if (!IsValid(Actor)) return nullptr;

		if (UActionComponent* Existing = Actor->GetComponentByClass<UActionComponent>())
		{
			return Existing;
		}

		return bAutoAdd ? Actor->AddComponent<UActionComponent>() : nullptr;
	}

	void ApplyHitStop(AActor* Actor, float Duration, bool bAutoAddActionComponent)
	{
		UActionComponent* Action = GetOrCreateActionComponent(Actor, bAutoAddActionComponent);
		if (IsValid(Action))
		{
			Action->LocalHitStop(Duration);
		}
	}

	FVector ResolveKnockbackDirection(AActor* Attacker, AActor* Target, EAttackKnockbackMode Mode)
	{
		switch (Mode)
		{
		case EAttackKnockbackMode::Up:
			return FVector::UpVector;
		case EAttackKnockbackMode::AwayFromAttacker:
		{
			if (!IsValid(Attacker) || !IsValid(Target)) return FVector::ForwardVector;
			FVector Delta = Target->GetActorLocation() - Attacker->GetActorLocation();
			Delta.Z = 0.0f; // 수평 성분만 — 높낮이 차이로 위/아래로 날아가는 일 방지.
			if (Delta.IsNearlyZero()) return Attacker->GetActorForward();
			return Delta.Normalized();
		}
		case EAttackKnockbackMode::Forward:
		default:
			return IsValid(Attacker) ? Attacker->GetActorForward() : FVector::ForwardVector;
		}
	}

	void ApplyKnockback(AActor* Attacker, AActor* Target, EAttackKnockbackMode Mode,
		float Distance, float Duration, bool bAutoAddActionComponent)
	{
		if (Distance <= 0.0f || !IsValid(Target)) return;

		UActionComponent* Action = GetOrCreateActionComponent(Target, bAutoAddActionComponent);
		if (!Action) return;

		const FVector Dir = ResolveKnockbackDirection(Attacker, Target, Mode);
		Action->Knockback(Dir, Distance, Duration);
	}

	UAnimInstance* GetActorAnimInstance(AActor* Actor)
	{
		if (!IsValid(Actor)) return nullptr;

		USkeletalMeshComponent* Mesh = Actor->GetComponentByClass<USkeletalMeshComponent>();
		return IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	}

	TArray<UPrimitiveComponent*> GetAttackHitCandidatePrimitives(AActor* Candidate)
	{
		TArray<UPrimitiveComponent*> Primitives;
		if (!IsValid(Candidate))
		{
			return Primitives;
		}

		// Character combat should be resolved against the gameplay body, not every
		// render/VFX/weapon primitive attached to the actor. Large particle or mesh
		// bounds can otherwise make a far-away character look intersecting.
		if (ACharacter* Character = Cast<ACharacter>(Candidate))
		{
			if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
			{
				Primitives.push_back(Capsule);
				return Primitives;
			}
		}

		return Candidate->GetPrimitiveComponents();
	}

	void FaceDefenderTowardAttacker(AActor* Defender, AActor* Attacker)
	{
		if (!IsValid(Defender) || !IsValid(Attacker))
		{
			return;
		}

		FVector Direction = Attacker->GetActorLocation() - Defender->GetActorLocation();
		Direction.Z = 0.0f;
		if (Direction.IsNearlyZero())
		{
			return;
		}

		constexpr float RadiansToDegrees = 57.29577951308232f;
		const float Yaw = std::atan2(Direction.Y, Direction.X) * RadiansToDegrees;
		const FRotator CurrentRotation = Defender->GetActorRotation();
		Defender->SetActorRotation(FRotator(CurrentRotation.Pitch, Yaw, CurrentRotation.Roll));
	}

	bool IsParryTarget(AActor* Target)
	{
		if (UAnimNotifyState_ParryWindow::IsParryWindowActive(GetActorAnimInstance(Target)))
		{
			return true;
		}

		if (IsValid(Target))
		{
			if (UCombatStateComponent* TargetCombat = Target->GetComponentByClass<UCombatStateComponent>())
			{
				return TargetCombat->IsDeflecting();
			}
		}

		return false;
	}

	bool ShouldRagdollParriedAttacker(AActor* Attacker, bool bRequested)
	{
		if (!bRequested || !IsValid(Attacker))
		{
			return false;
		}

		// 보스/강적은 패링 성공 시 EnemyCharacter 의 역재생 리코일이 우선이다.
		// 기존 ParryWindow 라우트에서 곧바로 래그돌에 들어가면 몽타주가 끊겨
		// 슬로모 중 역재생이 보이지 않는다. ParryGraceDuration 은 해당 보스 라우트의 명시적 옵트인 값이다.
		if (UCombatStateComponent* AttackerCombat = Attacker->GetComponentByClass<UCombatStateComponent>())
		{
			if (AttackerCombat->ParryGraceDuration > 0.0f)
			{
				return false;
			}
		}

		return true;
	}

	void ApplyParryReflectPoiseToAttacker(AActor* Attacker)
	{
		if (!IsValid(Attacker))
		{
			return;
		}

		if (UCombatStateComponent* AttackerCombat = Attacker->GetComponentByClass<UCombatStateComponent>())
		{
			const FPerilousResolution Resolution = GetPerilousResolution(AttackerCombat->GetActivePerilousType());
			AttackerCombat->ApplyParryReflectPoise(AttackerCombat->DeflectReflectPoise * Resolution.AnswerReflectPoiseScale);
		}
	}

	bool TryResolveParry(
		AActor* Attacker,
		AActor* Target,
		const FVector& HitLocation,
		bool bRagdollAttacker)
	{
		SCOPE_STAT_CAT("AttackHitWindow.TryResolveParry", "Combat");
		UAnimInstance* TargetAnimInstance = GetActorAnimInstance(Target);
		bool bResolved = false;
		if (UAnimNotifyState_ParryWindow::IsParryWindowActive(TargetAnimInstance))
		{
			bResolved = UAnimNotifyState_ParryWindow::ReportSuccessfulParry(TargetAnimInstance, Attacker, HitLocation);
			if (bResolved && IsValid(Attacker))
			{
				if (UCombatStateComponent* AttackerCombat = Attacker->GetComponentByClass<UCombatStateComponent>())
				{
					ApplyParryReflectPoiseToAttacker(Attacker);
					AttackerCombat->MarkParried();
					const FPerilousResolution Resolution = GetPerilousResolution(AttackerCombat->GetActivePerilousType());
					AttackerCombat->ApplyParryReflectDamage(AttackerCombat->DeflectReflectDamage, Target, Target, HitLocation, Resolution.AnswerReflectPoiseScale);
				}
			}
		}
		else if (IsValid(Target))
		{
			if (UCombatStateComponent* TargetCombat = Target->GetComponentByClass<UCombatStateComponent>())
			{
				if (TargetCombat->IsDeflecting())
				{
					const EDeflectGrade Grade = TargetCombat->ConsumeDeflect(Attacker);
					if (Grade == EDeflectGrade::Perfect || Grade == EDeflectGrade::Good)
					{
						UAnimNotifyState_ParryWindow::ReportCounterOpportunity(TargetAnimInstance, Attacker, HitLocation);
						bResolved = true;
					}
				}
			}
		}

		if (!bResolved)
		{
			return false;
		}

		FaceDefenderTowardAttacker(Target, Attacker);
		if (ShouldRagdollParriedAttacker(Attacker, bRagdollAttacker))
		{
			if (ACharacter* AttackerCharacter = Cast<ACharacter>(Attacker))
			{
				AttackerCharacter->EnterRagdoll();
			}
		}

		return true;
	}

	FCombatDamageSpec MakeDamageSpec(AActor* Owner, AActor* Target, const FVector& HitLocation, float Damage, float PoiseDamage)
	{
		FCombatDamageSpec DamageSpec;
		DamageSpec.Damage = Damage;
		DamageSpec.PoiseDamage = PoiseDamage;
		DamageSpec.DamageCauser = Owner;
		DamageSpec.InstigatorActor = Owner;
		DamageSpec.HitLocation = HitLocation;

		if (IsValid(Owner) && IsValid(Target))
		{
			FVector Direction = Target->GetActorLocation() - Owner->GetActorLocation();
			Direction.Z = 0.0f;
			DamageSpec.HitDirection = Direction.IsNearlyZero() ? Owner->GetActorForward() : Direction.Normalized();
		}
		else
		{
			DamageSpec.HitDirection = FVector::ForwardVector;
		}

		return DamageSpec;
	}

	void BroadcastAttackHitEvent(AActor* Owner, AActor* Target, UPrimitiveComponent* HitComponent, const FCombatDamageSpec& DamageSpec, FName HitEventName)
	{
		SCOPE_STAT_CAT("AttackHitWindow.BroadcastHit", "Combat");
		if (!IsValid(Owner))
		{
			return;
		}

		if (UCombatHitEventComponent* HitEventComponent = UCombatHitEventComponent::FindOrCreate(Owner))
		{
			HitEventComponent->BroadcastAttackHit(Target, HitComponent, DamageSpec, HitEventName);
		}
	}

	void BroadcastAttackParriedEvent(AActor* Owner, AActor* Defender, UPrimitiveComponent* HitComponent, const FCombatDamageSpec& DamageSpec, FName HitEventName)
	{
		SCOPE_STAT_CAT("AttackHitWindow.BroadcastParried", "Combat");
		if (!IsValid(Owner))
		{
			return;
		}

		if (UCombatHitEventComponent* HitEventComponent = UCombatHitEventComponent::FindOrCreate(Owner))
		{
			HitEventComponent->BroadcastAttackParried(Defender, HitComponent, DamageSpec, HitEventName);
		}
	}

	FCombatDamageReport ApplyDamageToTarget(AActor* Target, const FCombatDamageSpec& DamageSpec)
	{
		SCOPE_STAT_CAT("AttackHitWindow.ApplyDamageToTarget", "Combat");
		FCombatDamageReport Report;
		Report.RequestedDamage = DamageSpec.Damage;

		if (!IsValid(Target))
		{
			Report.Result = ECombatDamageResult::Rejected;
			return Report;
		}

		UHealthComponent* HealthComponent = Target->GetComponentByClass<UHealthComponent>();
		if (!HealthComponent)
		{
			Report.Result = ECombatDamageResult::Rejected;
			return Report;
		}

		return HealthComponent->ApplyDamageSpec(DamageSpec);
	}

	void PurgeInvalidAttackHitEntries(
		TMap<TWeakObjectPtr<USkeletalMeshComponent>, TSet<TWeakObjectPtr<AActor>>>& HitActorsByMesh,
		TMap<TWeakObjectPtr<USkeletalMeshComponent>, TSet<TWeakObjectPtr<AActor>>>& MissLoggedActorsByMesh,
		TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FAttackHitWindowPendingHit>>& PendingHitsByMesh,
		TMap<TWeakObjectPtr<USkeletalMeshComponent>, FVector>& LastHitCenterByMesh,
		TSet<TWeakObjectPtr<USkeletalMeshComponent>>& NoTargetLoggedMeshes,
		TSet<TWeakObjectPtr<USkeletalMeshComponent>>& WorldHitMeshes)
	{
		for (auto It = HitActorsByMesh.begin(); It != HitActorsByMesh.end(); )
		{
			if (!IsValid(It->first))
			{
				It = HitActorsByMesh.erase(It);
				continue;
			}
			for (auto ActorIt = It->second.begin(); ActorIt != It->second.end(); )
			{
				if (!IsValid(*ActorIt)) ActorIt = It->second.erase(ActorIt);
				else ++ActorIt;
			}
			++It;
		}
		for (auto It = MissLoggedActorsByMesh.begin(); It != MissLoggedActorsByMesh.end(); )
		{
			if (!IsValid(It->first))
			{
				It = MissLoggedActorsByMesh.erase(It);
				continue;
			}
			for (auto ActorIt = It->second.begin(); ActorIt != It->second.end(); )
			{
				if (!IsValid(*ActorIt)) ActorIt = It->second.erase(ActorIt);
				else ++ActorIt;
			}
			++It;
		}
		for (auto It = PendingHitsByMesh.begin(); It != PendingHitsByMesh.end(); )
		{
			if (!IsValid(It->first))
			{
				It = PendingHitsByMesh.erase(It);
				continue;
			}
			TArray<FAttackHitWindowPendingHit>& PendingHits = It->second;
			for (auto HitIt = PendingHits.begin(); HitIt != PendingHits.end(); )
			{
				if (!IsValid(HitIt->Target) || !IsValid(HitIt->HitComponent)) HitIt = PendingHits.erase(HitIt);
				else ++HitIt;
			}
			++It;
		}
		for (auto It = LastHitCenterByMesh.begin(); It != LastHitCenterByMesh.end(); )
		{
			if (!IsValid(It->first)) It = LastHitCenterByMesh.erase(It);
			else ++It;
		}
		for (auto It = NoTargetLoggedMeshes.begin(); It != NoTargetLoggedMeshes.end(); )
		{
			if (!IsValid(*It)) It = NoTargetLoggedMeshes.erase(It);
			else ++It;
		}
		for (auto It = WorldHitMeshes.begin(); It != WorldHitMeshes.end(); )
		{
			if (!IsValid(*It)) It = WorldHitMeshes.erase(It);
			else ++It;
		}
	}

}

bool UAnimNotifyState_AttackHitWindow::ResolveParryHit(
	AActor* Owner,
	AActor* Target,
	UPrimitiveComponent* HitComponent,
	const FCombatDamageSpec& DamageSpec,
	const FVector& Center)
{
	SCOPE_STAT_CAT("AttackHitWindow.ResolveParryHit", "Combat");
	if (!bAllowParry || !TryResolveParry(Owner, Target, DamageSpec.HitLocation, bRagdollOwnerOnParry))
	{
		return false;
	}

	if (bBroadcastCombatEvents)
	{
		BroadcastAttackParriedEvent(Owner, Target, HitComponent, DamageSpec, HitEventName);
	}
	FCombatImpactEvent ImpactEvent;
	ImpactEvent.Type = ECombatImpactType::Parried;
	ImpactEvent.Attacker = Owner;
	ImpactEvent.Target = Target;
	ImpactEvent.HitComponent = HitComponent;
	ImpactEvent.HitLocation = DamageSpec.HitLocation;
	ImpactEvent.HitNormal = DamageSpec.HitDirection * -1.0f;
	ImpactEvent.DamageSpec = DamageSpec;
	ImpactEvent.HitEventName = HitEventName;
	UCombatHitEventComponent::BroadcastImpactForActor(Owner, ImpactEvent);
	if (bDrawDebugHitWindow)
	{
		UWorld* World = IsValid(Owner) ? Owner->GetWorld() : nullptr;
		DrawDebugSphere(World, Center, Radius, DebugDrawSegments, FColor(80, 180, 255), DebugDrawDuration);
	}
	if (bLogHits)
	{
		UE_LOG("[AttackHitWindow] %s parried by %s via %s (center=%.1f, %.1f, %.1f radius=%.1f)",
			IsValid(Owner) ? Owner->GetName().c_str() : "None",
			IsValid(Target) ? Target->GetName().c_str() : "None",
			IsValid(HitComponent) ? HitComponent->GetName().c_str() : "None",
			Center.X,
			Center.Y,
			Center.Z,
			Radius);
	}
	return true;
}

void UAnimNotifyState_AttackHitWindow::ResolveDamageHit(
	AActor* Owner,
	AActor* Target,
	UPrimitiveComponent* HitComponent,
	const FCombatDamageSpec& DamageSpec,
	const FVector& Center)
{
	SCOPE_STAT_CAT("AttackHitWindow.ResolveDamageHit", "Combat");
	if (bBroadcastCombatEvents)
	{
		BroadcastAttackHitEvent(Owner, Target, HitComponent, DamageSpec, HitEventName);
	}

	const FCombatDamageReport DamageReport = ApplyDamageToTarget(Target, DamageSpec);
	if (DamageReport.Result == ECombatDamageResult::Damaged ||
		DamageReport.Result == ECombatDamageResult::Killed ||
		DamageReport.Result == ECombatDamageResult::Deflected)
	{
		FCombatImpactEvent ImpactEvent;
		ImpactEvent.Type = (DamageReport.Result == ECombatDamageResult::Deflected)
			? ECombatImpactType::Parried
			: ECombatImpactType::EnemyHit;
		ImpactEvent.Attacker = Owner;
		ImpactEvent.Target = Target;
		ImpactEvent.HitComponent = HitComponent;
		ImpactEvent.HitLocation = DamageSpec.HitLocation;
		ImpactEvent.HitNormal = DamageSpec.HitDirection * -1.0f;
		ImpactEvent.DamageSpec = DamageSpec;
		ImpactEvent.DamageReport = DamageReport;
		ImpactEvent.HitEventName = HitEventName;
		UCombatHitEventComponent::BroadcastImpactForActor(Owner, ImpactEvent);
	}
	ApplyHitStop(Owner, HitStopDuration, bAutoAddActionComponent);
	ApplyHitStop(Target, HitStopDuration, bAutoAddActionComponent);
	if (bApplyKnockback)
	{
		ApplyKnockback(Owner, Target, KnockbackMode, KnockbackDistance, KnockbackDuration, bAutoAddActionComponent);
	}
	if (bDrawDebugHitWindow)
	{
		UWorld* World = IsValid(Owner) ? Owner->GetWorld() : nullptr;
		DrawDebugSphere(World, Center, Radius, DebugDrawSegments, FColor(255, 40, 40), DebugDrawDuration);
	}

	if (bLogHits)
	{
		const uint32 ResultIndex = static_cast<uint32>(DamageReport.Result);
		const char* ResultName = ResultIndex < GCombatDamageResultCount ? GCombatDamageResultNames[ResultIndex] : "Unknown";
		UE_LOG("[AttackHitWindow] %s hit %s via %s damage=%.1f result=%s (center=%.1f, %.1f, %.1f radius=%.1f)",
			IsValid(Owner) ? Owner->GetName().c_str() : "None",
			IsValid(Target) ? Target->GetName().c_str() : "None",
			IsValid(HitComponent) ? HitComponent->GetName().c_str() : "None",
			DamageReport.AppliedDamage,
			ResultName,
			Center.X,
			Center.Y,
			Center.Z,
			Radius);
	}
}

void UAnimNotifyState_AttackHitWindow::ResolveWorldHit(
	USkeletalMeshComponent* MeshComp,
	AActor* Owner,
	UWorld* World,
	const FVector& PreviousCenter,
	const FVector& Center)
{
	SCOPE_STAT_CAT("AttackHitWindow.ResolveWorldHit", "Combat");
	if (!bHitWorldStatic || !IsValid(MeshComp) || !IsValid(Owner) || !World || Radius <= 0.0f)
	{
		return;
	}

	if (WorldHitMeshes.find(MeshComp) != WorldHitMeshes.end())
	{
		return;
	}

	FHitResult Hit;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	if (!World->PhysicsSweepByObjectTypes(
		PreviousCenter,
		Center,
		FQuat::Identity,
		Shape,
		Hit,
		ObjectTypeBit(ECollisionChannel::WorldStatic),
		Owner))
	{
		return;
	}

	WorldHitMeshes.insert(MeshComp);

	FCombatImpactEvent ImpactEvent;
	ImpactEvent.Type = ECombatImpactType::WorldHit;
	ImpactEvent.Attacker = Owner;
	ImpactEvent.Target = Hit.HitActor;
	ImpactEvent.HitComponent = Hit.HitComponent;
	ImpactEvent.HitLocation = Hit.WorldHitLocation;
	ImpactEvent.HitNormal = Hit.WorldNormal;
	ImpactEvent.HitEventName = HitEventName;
	UCombatHitEventComponent::BroadcastImpactForActor(Owner, ImpactEvent);

	if (bDrawDebugHitWindow)
	{
		DrawDebugSphere(World, Center, Radius, DebugDrawSegments, FColor(130, 130, 255), DebugDrawDuration);
	}
	if (bLogHits)
	{
		UE_LOG("[AttackHitWindow] %s hit world via %s (location=%.1f, %.1f, %.1f center=%.1f, %.1f, %.1f radius=%.1f)",
			Owner->GetName().c_str(),
			IsValid(Hit.HitComponent) ? Hit.HitComponent->GetName().c_str() : "None",
			Hit.WorldHitLocation.X,
			Hit.WorldHitLocation.Y,
			Hit.WorldHitLocation.Z,
			Center.X,
			Center.Y,
			Center.Z,
			Radius);
	}
}

void UAnimNotifyState_AttackHitWindow::ResolvePendingHits(USkeletalMeshComponent* MeshComp, float DeltaTime, bool bForceResolve)
{
	SCOPE_STAT_CAT("AttackHitWindow.ResolvePendingHits", "Combat");
	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		PendingHitsByMesh.erase(MeshComp);
		return;
	}

	auto It = PendingHitsByMesh.find(MeshComp);
	if (It == PendingHitsByMesh.end())
	{
		return;
	}

	TArray<FAttackHitWindowPendingHit>& PendingHits = It->second;
	for (auto HitIt = PendingHits.begin(); HitIt != PendingHits.end(); )
	{
		AActor* Target = HitIt->Target.Get();
		UPrimitiveComponent* HitComponent = HitIt->HitComponent.Get();
		if (!IsValid(Target) || !IsValid(HitComponent))
		{
			HitIt = PendingHits.erase(HitIt);
			continue;
		}

		HitIt->ElapsedSeconds += DeltaTime;
		if (ResolveParryHit(Owner, Target, HitComponent, HitIt->DamageSpec, HitIt->Center))
		{
			HitIt = PendingHits.erase(HitIt);
			continue;
		}

		if (bForceResolve || HitIt->ElapsedSeconds >= ParryResolveDelay)
		{
			ResolveDamageHit(Owner, Target, HitComponent, HitIt->DamageSpec, HitIt->Center);
			HitIt = PendingHits.erase(HitIt);
			continue;
		}

		++HitIt;
	}
}

void UAnimNotifyState_AttackHitWindow::QueueDelayedHit(
	USkeletalMeshComponent* MeshComp,
	AActor* Target,
	UPrimitiveComponent* HitComponent,
	const FCombatDamageSpec& DamageSpec,
	const FVector& Center)
{
	SCOPE_STAT_CAT("AttackHitWindow.QueueDelayedHit", "Combat");
	if (!IsValid(MeshComp) || !IsValid(Target) || !IsValid(HitComponent))
	{
		return;
	}

	FAttackHitWindowPendingHit PendingHit;
	PendingHit.Target = Target;
	PendingHit.HitComponent = HitComponent;
	PendingHit.DamageSpec = DamageSpec;
	PendingHit.Center = Center;
	PendingHit.ElapsedSeconds = 0.0f;
	PendingHitsByMesh[MeshComp].push_back(PendingHit);
}

void UAnimNotifyState_AttackHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/, float /*TotalDuration*/)
{
	SCOPE_STAT_CAT("AttackHitWindow.NotifyBegin", "Combat");
	PurgeInvalidAttackHitEntries(HitActorsByMesh, MissLoggedActorsByMesh, PendingHitsByMesh, LastHitCenterByMesh, NoTargetLoggedMeshes, WorldHitMeshes);
	if (!IsValid(MeshComp))
	{
		return;
	}

	HitActorsByMesh[MeshComp].clear();
	MissLoggedActorsByMesh[MeshComp].clear();
	PendingHitsByMesh[MeshComp].clear();
	LastHitCenterByMesh.erase(MeshComp);
	NoTargetLoggedMeshes.erase(MeshComp);
	WorldHitMeshes.erase(MeshComp);
}

void UAnimNotifyState_AttackHitWindow::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/, float FrameDeltaTime)
{
	SCOPE_STAT_CAT("AttackHitWindow.NotifyTick", "Combat");
	PurgeInvalidAttackHitEntries(HitActorsByMesh, MissLoggedActorsByMesh, PendingHitsByMesh, LastHitCenterByMesh, NoTargetLoggedMeshes, WorldHitMeshes);
	if (!IsValid(MeshComp) || Radius <= 0.0f)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	UWorld* World = MeshComp->GetWorld();
	if (!IsValid(Owner) || !World)
	{
		return;
	}

	ResolvePendingHits(MeshComp, FrameDeltaTime, false);

	TSet<TWeakObjectPtr<AActor>>& HitActors = HitActorsByMesh[MeshComp];
	TSet<TWeakObjectPtr<AActor>>& MissLoggedActors = MissLoggedActorsByMesh[MeshComp];
	const FVector Center = GetHitCenter(MeshComp, Owner, BoneName, LocalOffset);
	const auto LastCenterIt = LastHitCenterByMesh.find(MeshComp);
	const FVector PreviousCenter = LastCenterIt != LastHitCenterByMesh.end() ? LastCenterIt->second : Center;
	ResolveWorldHit(MeshComp, Owner, World, PreviousCenter, Center);
	LastHitCenterByMesh[MeshComp] = Center;
	if (bDrawDebugHitWindow)
	{
		DrawDebugSphere(World, Center, Radius, DebugDrawSegments, FColor(255, 220, 0), DebugDrawDuration);
	}

	bool bSawTargetCandidate = false;
	{
		SCOPE_STAT_CAT("AttackHitWindow.ScanCandidates", "Combat");
		for (AActor* Candidate : World->GetActors())
		{
			if (!IsValid(Candidate) || Candidate == Owner)
			{
				continue;
			}

			const bool bMatchesTargetActorTag = !TargetActorTag.empty() && Candidate->HasTag(FName(TargetActorTag));
			const bool bParryTarget = bAllowParry && IsParryTarget(Candidate);
			if (bRequireTargetActorTag)
			{
				if (!bMatchesTargetActorTag && !bParryTarget)
				{
					continue;
				}
			}
			else if (!TargetActorTag.empty() && !bMatchesTargetActorTag && !bParryTarget)
			{
				continue;
			}

			bSawTargetCandidate = true;
			if (HitActors.find(Candidate) != HitActors.end())
			{
				continue;
			}

			UPrimitiveComponent* HitComponent = nullptr;
			UPrimitiveComponent* ClosestComponent = nullptr;
			const char* MissReason = "no primitive components";
			float ClosestDistanceSquared = FLT_MAX;
			for (UPrimitiveComponent* Primitive : GetAttackHitCandidatePrimitives(Candidate))
			{
				if (!IsValid(Primitive))
				{
					continue;
				}

				const FBoundingBox Bounds = Primitive->GetWorldBoundingBox();
				if (bRequireQueryCollision && !Primitive->IsQueryCollisionEnabled())
				{
					if (bDrawDebugTargetBounds)
					{
						DrawDebugBounds(World, Bounds, FColor(90, 90, 90), DebugDrawDuration);
					}
					MissReason = "query collision disabled";
					continue;
				}

				if (!bHitWorldStatic && !bMatchesTargetActorTag && Primitive->GetCollisionObjectType() == ECollisionChannel::WorldStatic)
				{
					if (bDrawDebugTargetBounds)
					{
						DrawDebugBounds(World, Bounds, FColor(80, 80, 160), DebugDrawDuration);
					}
					MissReason = "world static filtered";
					continue;
				}

				if (!Bounds.IsValid())
				{
					MissReason = "invalid bounds";
					continue;
				}

				const float DistanceSquared = DistanceSquaredPointAABB(Center, Bounds);
				const bool bIntersects = DistanceSquared <= Radius * Radius;
				if (bDrawDebugTargetBounds)
				{
					DrawDebugBounds(World, Bounds, bIntersects ? FColor(255, 40, 40) : FColor(0, 180, 255), DebugDrawDuration);
				}

				if (DistanceSquared < ClosestDistanceSquared)
				{
					ClosestDistanceSquared = DistanceSquared;
					ClosestComponent = Primitive;
					MissReason = "outside radius";
				}

				if (bIntersects)
				{
					HitComponent = Primitive;
					break;
				}
			}

			if (!HitComponent)
			{
				if (bLogMisses && MissLoggedActors.find(Candidate) == MissLoggedActors.end())
				{
					MissLoggedActors.insert(Candidate);
					UE_LOG("[AttackHitWindow] miss %s -> %s (%s%s%s center=%.1f, %.1f, %.1f radius=%.1f)",
						Owner->GetName().c_str(),
						Candidate->GetName().c_str(),
						MissReason,
						ClosestComponent ? " closest=" : "",
						ClosestComponent ? ClosestComponent->GetName().c_str() : "",
						Center.X,
						Center.Y,
						Center.Z,
						Radius);
				}
				continue;
			}

			HitActors.insert(Candidate);
			const FVector ImpactLocation = ClosestPointOnAABB(Center, HitComponent->GetWorldBoundingBox());
			const FCombatDamageSpec DamageSpec = MakeDamageSpec(Owner, Candidate, ImpactLocation, Damage, PoiseDamage);
			if (bAllowParry && bDelayDamageForParry && ParryResolveDelay > 0.0f)
			{
				QueueDelayedHit(MeshComp, Candidate, HitComponent, DamageSpec, Center);
				if (bDrawDebugHitWindow)
				{
					DrawDebugSphere(World, Center, Radius, DebugDrawSegments, FColor(255, 140, 0), DebugDrawDuration);
				}
				if (bLogHits)
				{
					UE_LOG("[AttackHitWindow] %s pending hit %s via %s delay=%.2f (center=%.1f, %.1f, %.1f radius=%.1f)",
						Owner->GetName().c_str(),
						Candidate->GetName().c_str(),
						HitComponent->GetName().c_str(),
						ParryResolveDelay,
						Center.X,
						Center.Y,
						Center.Z,
						Radius);
				}
				continue;
			}

			if (ResolveParryHit(Owner, Candidate, HitComponent, DamageSpec, Center))
			{
				return;
			}

			ResolveDamageHit(Owner, Candidate, HitComponent, DamageSpec, Center);
		}
	}

	if (bLogMisses && !bSawTargetCandidate && NoTargetLoggedMeshes.find(MeshComp) == NoTargetLoggedMeshes.end())
	{
		NoTargetLoggedMeshes.insert(MeshComp);
		UE_LOG("[AttackHitWindow] no target candidate for %s (RequireTargetTag=%d TargetActorTag=%s)",
			Owner->GetName().c_str(),
			bRequireTargetActorTag ? 1 : 0,
			TargetActorTag.c_str());
	}
}

void UAnimNotifyState_AttackHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/)
{
	SCOPE_STAT_CAT("AttackHitWindow.NotifyEnd", "Combat");
	PurgeInvalidAttackHitEntries(HitActorsByMesh, MissLoggedActorsByMesh, PendingHitsByMesh, LastHitCenterByMesh, NoTargetLoggedMeshes, WorldHitMeshes);
	ResolvePendingHits(MeshComp, 0.0f, true);
	HitActorsByMesh.erase(MeshComp);
	MissLoggedActorsByMesh.erase(MeshComp);
	PendingHitsByMesh.erase(MeshComp);
	LastHitCenterByMesh.erase(MeshComp);
	NoTargetLoggedMeshes.erase(MeshComp);
	WorldHitMeshes.erase(MeshComp);
}
