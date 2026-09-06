#pragma once
#include "Engine/Component/ActorComponent.h"
#include "Object/Ptr/WeakObjectPtr.h"

#include "Source/Game/Components/EnemySpawnEffect.generated.h"

class UParticleSystemComponent;

// 적 액터에 붙이는 옵트인 "스폰 디렉터" 컴포넌트.
// BeginPlay 에서 소유 액터를 숨기고 대기(Idle)하다가, PlayEffect()(인보크)를 받으면
// SpawnDelay 만큼 기다린 뒤 소유 액터를 드러내고 파티클 버스트를 재생한다(= 스폰 순간).
// 가시성 토글 + 파티클만 담당한다(AI 는 건드리지 않음 — 시네마틱 디렉터가 관리).
// 자동재생 안 함(bAutoActivate=false) — 시네마틱/디렉터가 PlayEffect 로 호출한다.
UCLASS()
class UEnemySpawnEffectComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UEnemySpawnEffectComponent();

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void EndPlay() override;
	void Activate() override;

	// 스폰 인보크 — SpawnDelay 카운트다운 시작. 디렉터/코루틴이 호출한다.
	UFUNCTION(Callable, Category="EnemySpawnEffect")
	void PlayEffect();

	// 인보크됨(대기/방출 중) 여부.
	UFUNCTION(Pure, Category="EnemySpawnEffect")
	bool IsPlaying() const;

private:
	// 파티클 시스템 컴포넌트를 보장하고 에셋을 세팅한다.
	void EnsureParticleSystemAsset();
	// SpawnDelay 경과 시점: 소유 액터를 드러내고 파티클을 켠다.
	void DoSpawn();

	// 스폰 진행 상태. Elapsed 는 단계별 의미가 다르다(대기→방출).
	enum class ESpawnState
	{
		Idle,      // 인보크 전 — 숨김
		Delaying,  // 인보크됨 — SpawnDelay 카운트다운(Elapsed = 경과 대기시간)
		Emitting,  // 스폰됨 — 파티클 방출 중(Elapsed = 경과 방출시간)
		Done       // 종료
	};

private:
	// 효과 재생 시간(초). 지나면 파티클을 끄고 비활성화한다.
	UPROPERTY(Edit, Save, Category="EnemySpawnEffect", DisplayName="Duration", Min=0.0f, Max=30.0f, Speed=0.1f)
	float Duration = 0.f;

	UPROPERTY(Edit, Save, Category="EnemySpawnEffect", DisplayName="Spawn Delay after invoked", Min=0.f, Max=30.f, Speed=0.1f)
	float SpawnDelay = 0.f;

	UPROPERTY(Edit, Save, Category="EnemySpawnEffect", DisplayName="Particle System", AssetType="UParticleSystem")
	FString ParticleSystemAssetPath = "Content/Game/Texture/GameScene/Play/EnemySpawn.uasset";

	float Elapsed = 0.f;
	ESpawnState State = ESpawnState::Idle;

	// 이 컴포넌트가 소유·구동하는 파티클 컴포넌트(소유 액터에 붙인다).
	TWeakObjectPtr<UParticleSystemComponent> EffectPSC;
};
