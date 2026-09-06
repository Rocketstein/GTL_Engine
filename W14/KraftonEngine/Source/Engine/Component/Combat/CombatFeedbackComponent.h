#pragma once

#include "Component/ActorComponent.h"
#include "Component/Combat/CombatTypes.h"
#include "Core/Delegate.h"
#include "Object/Ptr/WeakObjectPtr.h"

class UCombatHitEventComponent;
class UParticleSystemComponent;

#include "Source/Engine/Component/Combat/CombatFeedbackComponent.generated.h"

UCLASS()
class UCombatFeedbackComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UCombatFeedbackComponent() = default;
	~UCombatFeedbackComponent() override = default;

	static UCombatFeedbackComponent* EnsureForActor(AActor* OwnerActor);

	void BeginPlay() override;
	void EndPlay() override;

	void BindOwnerImpactEvents();
	void ClearImpactBindings();
	void HandleCombatImpact(UCombatHitEventComponent* EventComponent, const FCombatImpactEvent& ImpactEvent);

	UPROPERTY(Edit, Save, Category="Combat|Feedback", DisplayName="Auto Bind Combat Impact")
	bool bAutoBindCombatImpact = true;

	UPROPERTY(Edit, Save, Category="Combat|Feedback", DisplayName="Play Impact Sounds")
	bool bPlayImpactSounds = true;

	UPROPERTY(Edit, Save, Category="Combat|Feedback|Sound", DisplayName="Enemy Hit Sound")
	FString EnemyHitSoundPath = "Hit_Enemy.wav";

	UPROPERTY(Edit, Save, Category="Combat|Feedback|Sound", DisplayName="World Hit Sound")
	FString WorldHitSoundPath = "Gaurd.wav";

	UPROPERTY(Edit, Save, Category="Combat|Feedback|Sound", DisplayName="Parry Sound")
	FString ParrySoundPath = "Parry1.wav";

	UPROPERTY(Edit, Save, Category="Combat|Feedback|Sound", DisplayName="Blocked Sound")
	FString BlockedSoundPath = "Gaurd.wav";

	UPROPERTY(Edit, Save, Category="Combat|Feedback|Sound", DisplayName="Enemy Hit Volume", Min=0.0f, Max=1.0f, Speed=0.01f)
	float EnemyHitVolume = 0.9f;

	UPROPERTY(Edit, Save, Category="Combat|Feedback|Sound", DisplayName="World Hit Volume", Min=0.0f, Max=1.0f, Speed=0.01f)
	float WorldHitVolume = 0.75f;

	UPROPERTY(Edit, Save, Category="Combat|Feedback|Sound", DisplayName="Parry Volume", Min=0.0f, Max=1.0f, Speed=0.01f)
	float ParryVolume = 1.0f;

	UPROPERTY(Edit, Save, Category="Combat|Feedback|Sound", DisplayName="Blocked Volume", Min=0.0f, Max=1.0f, Speed=0.01f)
	float BlockedVolume = 0.85f;

	// ── 임팩트 이펙트 (VFX) ─────────────────────────────────────────────────────
	// 사운드와 동일하게 OnCombatImpact 에서 구동된다. 즉 "고정 애니 노티파이 프레임"이 아니라
	// 실제 데미지/접촉이 적용되는 순간(노티파이 경로=ResolveDamageHit, 프레임데이터 경로=
	// HitTimeFraction 시점)에 HitLocation 에 스폰 → 데미지와 항상 동기화된다(사운드 부조화 해결).
	UPROPERTY(Edit, Save, Category="Combat|Feedback", DisplayName="Play Impact Effects")
	bool bPlayImpactEffects = true;

	// 파티클 시스템 .uasset 프로젝트 상대경로. 비우면("" 또는 "None") 해당 타입은 이펙트 없음.
	UPROPERTY(Edit, Save, Category="Combat|Feedback|Effect", DisplayName="Enemy Hit Effect")
	FString EnemyHitEffectPath = "";

	UPROPERTY(Edit, Save, Category="Combat|Feedback|Effect", DisplayName="World Hit Effect")
	FString WorldHitEffectPath = "";

	UPROPERTY(Edit, Save, Category="Combat|Feedback|Effect", DisplayName="Parry Effect")
	FString ParryEffectPath = "Content/Particle System/Parry.uasset";

	UPROPERTY(Edit, Save, Category="Combat|Feedback|Effect", DisplayName="Blocked Effect")
	FString BlockedEffectPath = "Content/Particle System/Parry.uasset";

	// 타입별 풀 상한. 같은 템플릿용 비활성 PSC 를 재활성화해 액터 누수를 막는다.
	UPROPERTY(Edit, Save, Category="Combat|Feedback|Effect", DisplayName="Max Pooled Effects Per Type", Min=1.0f, Max=32.0f, Speed=1.0f)
	int32 MaxPooledEffectsPerType = 6;

private:
	void PlayImpactSound(const FString& SoundPath, float Volume);
	void PlayImpactEffect(const FString& EffectPath, const FVector& Location, const FVector& Normal);
	UParticleSystemComponent* AcquirePooledEffect(const FString& EffectPath);
	void DestroyPooledEffects();

	TWeakObjectPtr<UCombatHitEventComponent> BoundHitEventComponent = nullptr;
	FDelegateHandle CombatImpactHandle;

	// 임팩트 VFX 재사용 풀 — 템플릿 경로별로 스폰된 PSC 를 보관, 완료(비활성)된 것을 재활성화.
	TMap<FString, TArray<TWeakObjectPtr<UParticleSystemComponent>>> EffectPoolByPath;
	TMap<FString, int32> NextEffectSlotByPath;
};
