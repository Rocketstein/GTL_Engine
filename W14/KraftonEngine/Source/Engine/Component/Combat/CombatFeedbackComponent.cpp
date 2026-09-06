#include "Component/Combat/CombatFeedbackComponent.h"

#include "Audio/AudioManager.h"
#include "Component/Combat/CombatHitEventComponent.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Core/Logging/Log.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "GameFramework/GameMode/GameplayStatics.h"
#include "Math/Rotator.h"

#include <algorithm>
#include <cmath>

namespace
{
	static TSet<FString> GLoadedCombatFeedbackSounds;

	// 임팩트 normal 을 따라 이펙트를 정렬한다(+X = 분사 방향). 스프라이트형 카메라 정렬
	// 파티클에는 영향이 작지만, 메시/방향성 스파크에는 표면 바깥/공격자 쪽으로 향하게 한다.
	FRotator MakeRotationFromNormal(const FVector& Normal)
	{
		if (Normal.IsNearlyZero())
		{
			return FRotator();
		}
		const FVector N = Normal.Normalized();
		constexpr float RadToDeg = 57.29577951308232f;
		const float Yaw = std::atan2(N.Y, N.X) * RadToDeg;
		const float HorizLen = std::sqrt(N.X * N.X + N.Y * N.Y);
		const float Pitch = std::atan2(N.Z, HorizLen) * RadToDeg;
		return FRotator(Pitch, Yaw, 0.0f);
	}
}

UCombatFeedbackComponent* UCombatFeedbackComponent::EnsureForActor(AActor* OwnerActor)
{
	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	if (UCombatFeedbackComponent* Existing = OwnerActor->GetComponentByClass<UCombatFeedbackComponent>())
	{
		return Existing;
	}

	return OwnerActor->AddComponent<UCombatFeedbackComponent>();
}

void UCombatFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoBindCombatImpact)
	{
		BindOwnerImpactEvents();
	}
}

void UCombatFeedbackComponent::EndPlay()
{
	ClearImpactBindings();
	DestroyPooledEffects();
	Super::EndPlay();
}

void UCombatFeedbackComponent::BindOwnerImpactEvents()
{
	ClearImpactBindings();

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	UCombatHitEventComponent* HitEventComponent = UCombatHitEventComponent::FindOrCreate(OwnerActor);
	if (!HitEventComponent)
	{
		return;
	}

	BoundHitEventComponent = HitEventComponent;
	CombatImpactHandle = HitEventComponent->OnCombatImpact.AddWeakUObject(this, &UCombatFeedbackComponent::HandleCombatImpact);
	UE_LOG("[CombatFeedback] Bound OnCombatImpact for %s (sounds=%d effects=%d)",
		OwnerActor->GetName().c_str(),
		bPlayImpactSounds ? 1 : 0,
		bPlayImpactEffects ? 1 : 0);
}

void UCombatFeedbackComponent::ClearImpactBindings()
{
	UCombatHitEventComponent* HitEventComponent = BoundHitEventComponent.Get();
	if (HitEventComponent && CombatImpactHandle.IsValid())
	{
		HitEventComponent->OnCombatImpact.Remove(CombatImpactHandle);
	}

	BoundHitEventComponent.Reset();
	CombatImpactHandle.Reset();
}

void UCombatFeedbackComponent::HandleCombatImpact(UCombatHitEventComponent* /*EventComponent*/, const FCombatImpactEvent& ImpactEvent)
{
	// 사운드와 이펙트는 같은 임팩트 이벤트(=실제 데미지/접촉 순간)에서 함께 구동한다.
	// 둘은 서로 독립적으로 토글되며, 항상 데미지와 동기화된다.
	const uint32 TypeIndex = static_cast<uint32>(ImpactEvent.Type);
	const char* TypeName = TypeIndex < GCombatImpactTypeCount ? GCombatImpactTypeNames[TypeIndex] : "Unknown";
	UE_LOG("[CombatFeedback] Impact %s attacker=%s target=%s loc=(%.1f, %.1f, %.1f) sounds=%d effects=%d",
		TypeName,
		IsValid(ImpactEvent.Attacker) ? ImpactEvent.Attacker->GetName().c_str() : "None",
		IsValid(ImpactEvent.Target) ? ImpactEvent.Target->GetName().c_str() : "None",
		ImpactEvent.HitLocation.X,
		ImpactEvent.HitLocation.Y,
		ImpactEvent.HitLocation.Z,
		bPlayImpactSounds ? 1 : 0,
		bPlayImpactEffects ? 1 : 0);

	if (bPlayImpactSounds)
	{
		switch (ImpactEvent.Type)
		{
		case ECombatImpactType::EnemyHit:
			PlayImpactSound(EnemyHitSoundPath, EnemyHitVolume);
			break;
		// WorldHit(정적 메시/벽 타격) 사운드는 의도적으로 비활성화한다(헛스윙이 벽에 닿을 때 소리 제거).
		case ECombatImpactType::WorldHit:
			break;
		case ECombatImpactType::Parried:
			PlayImpactSound(ParrySoundPath, ParryVolume);
			break;
		case ECombatImpactType::Blocked:
			PlayImpactSound(BlockedSoundPath, BlockedVolume);
			break;
		default:
			break;
		}
	}

	if (bPlayImpactEffects)
	{
		const FVector& Location = ImpactEvent.HitLocation;
		const FVector& Normal = ImpactEvent.HitNormal;
		switch (ImpactEvent.Type)
		{
		case ECombatImpactType::EnemyHit:
			PlayImpactEffect(EnemyHitEffectPath, Location, Normal);
			break;
		case ECombatImpactType::WorldHit:
			PlayImpactEffect(WorldHitEffectPath, Location, Normal);
			break;
		case ECombatImpactType::Parried:
			PlayImpactEffect(ParryEffectPath, Location, Normal);
			break;
		case ECombatImpactType::Blocked:
			PlayImpactEffect(BlockedEffectPath, Location, Normal);
			break;
		default:
			break;
		}
	}
}

void UCombatFeedbackComponent::PlayImpactSound(const FString& SoundPath, float Volume)
{
	if (SoundPath.empty())
	{
		return;
	}

	const FString Key = FString("CombatImpact:") + SoundPath;
	if (GLoadedCombatFeedbackSounds.find(SoundPath) == GLoadedCombatFeedbackSounds.end())
	{
		if (!FAudioManager::Get().LoadAudio(Key, SoundPath, false))
		{
			UE_LOG("[CombatFeedback] LoadAudio failed: %s", SoundPath.c_str());
			return;
		}
		GLoadedCombatFeedbackSounds.insert(SoundPath);
	}

	FAudioManager::Get().PlayAudio(Key, Volume);
	UE_LOG("[CombatFeedback] PlaySound %s vol=%.2f", SoundPath.c_str(), Volume);
}

void UCombatFeedbackComponent::PlayImpactEffect(const FString& EffectPath, const FVector& Location, const FVector& Normal)
{
	if (EffectPath.empty() || EffectPath == "None")
	{
		UE_LOG("[CombatFeedback] Effect skipped (no template assigned)");
		return;
	}

	AActor* OwnerActor = GetOwner();
	UWorld* World = IsValid(OwnerActor) ? OwnerActor->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG("[CombatFeedback] Effect skipped (no world): %s", EffectPath.c_str());
		return;
	}

	const FRotator Rotation = MakeRotationFromNormal(Normal);

	// 1) 풀에 완료된(비활성) PSC 가 있으면 위치/회전만 바꿔 재활성화한다(액터 누수 방지).
	if (UParticleSystemComponent* Reused = AcquirePooledEffect(EffectPath))
	{
		if (AActor* PoolActor = Reused->GetOwner())
		{
			PoolActor->SetActorLocation(Location);
			PoolActor->SetActorRotation(Rotation);
		}
		Reused->Activate(true);
		UE_LOG("[CombatFeedback] Effect reuse %s at (%.1f, %.1f, %.1f)",
			EffectPath.c_str(), Location.X, Location.Y, Location.Z);
		return;
	}

	// 2) 재사용할 게 없고 풀이 상한 미만이면 새로 스폰해 풀에 등록한다.
	UParticleSystemComponent* PSC = FGameplayStatics::SpawnEmitterAtLocation(World, EffectPath, Location, Rotation, true);
	if (PSC)
	{
		EffectPoolByPath[EffectPath].push_back(PSC);
		UE_LOG("[CombatFeedback] Effect spawn %s at (%.1f, %.1f, %.1f) pool=%d",
			EffectPath.c_str(), Location.X, Location.Y, Location.Z,
			static_cast<int32>(EffectPoolByPath[EffectPath].size()));
	}
	else
	{
		UE_LOG("[CombatFeedback] SpawnEmitterAtLocation failed: %s", EffectPath.c_str());
	}
}

UParticleSystemComponent* UCombatFeedbackComponent::AcquirePooledEffect(const FString& EffectPath)
{
	auto It = EffectPoolByPath.find(EffectPath);
	if (It == EffectPoolByPath.end())
	{
		return nullptr;
	}

	TArray<TWeakObjectPtr<UParticleSystemComponent>>& Pool = It->second;

	// 무효(파괴된) 엔트리 정리.
	for (auto P = Pool.begin(); P != Pool.end(); )
	{
		if (!IsValid(*P)) P = Pool.erase(P);
		else ++P;
	}

	// 1) 완료되어 비활성 상태인 인스턴스를 재사용한다.
	for (TWeakObjectPtr<UParticleSystemComponent>& Weak : Pool)
	{
		UParticleSystemComponent* PSC = Weak.Get();
		if (IsValid(PSC) && !PSC->IsActive())
		{
			return PSC;
		}
	}

	// 2) 풀이 상한 미만이면 nullptr → 호출자가 새 인스턴스를 스폰한다.
	const int32 Cap = (std::max)(1, MaxPooledEffectsPerType);
	if (static_cast<int32>(Pool.size()) < Cap)
	{
		return nullptr;
	}

	// 3) 풀이 가득 찼으면 라운드로빈으로 가장 오래된 것을 강제 재시작(재사용)한다.
	int32& Slot = NextEffectSlotByPath[EffectPath];
	if (Slot < 0 || Slot >= static_cast<int32>(Pool.size()))
	{
		Slot = 0;
	}
	UParticleSystemComponent* Chosen = Pool[Slot].Get();
	Slot = (Slot + 1) % static_cast<int32>(Pool.size());
	return IsValid(Chosen) ? Chosen : nullptr;
}

void UCombatFeedbackComponent::DestroyPooledEffects()
{
	for (auto& Pair : EffectPoolByPath)
	{
		for (TWeakObjectPtr<UParticleSystemComponent>& Weak : Pair.second)
		{
			UParticleSystemComponent* PSC = Weak.Get();
			if (!IsValid(PSC))
			{
				continue;
			}
			AActor* PoolActor = PSC->GetOwner();
			UWorld* World = IsValid(PoolActor) ? PoolActor->GetWorld() : nullptr;
			if (World && IsValid(PoolActor))
			{
				World->DestroyActor(PoolActor);
			}
		}
	}
	EffectPoolByPath.clear();
	NextEffectSlotByPath.clear();
}
