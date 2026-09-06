#pragma once

#include "Core/CollisionEventTypes.h"
#include "GameFramework/AActor.h"

class UBillboardComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UScriptComponent;

// Item 동작 flag -> Lua 쪽에도 동시에 존재. 같이 수정해줘야함.
enum class EItemFeatureFlags : uint32
{
	None = 0,
	PickupOnOverlap = 1 << 0,   // Trigger overlap이 발생하면 자동으로 pickup/interact를 시도합니다.
	ConsumeOnPickup = 1 << 1,   // pickup 성공 후 item actor를 제거하는 기본 동작입니다.
	ScoreReward = 1 << 2,       // GameManager.AddScore()로 점수 보상을 지급합니다.
	SingleUse = 1 << 4,         // 중복 overlap으로 같은 아이템이 여러 번 발동되는 것을 막습니다.
	DebugLog = 1 << 5,          // Lua item script에서 디버그 로그를 켭니다.
};

// 아이템별로 반복되는 값
struct FItemInteractionConfig
{
	int32 ScoreValue = 0;
	FString RequiredInteractorTag = "Player";
	float RespawnDelay = 0.0f;
	float Cooldown = 0.0f;
	bool bStartsEnabled = true;
};

// Item Actor 기반 클래스
// Trigger와 Actor 생명주기는 C++에서, 보상 처리는 Lua에서 담당

/****************************************************************
절대로 Item의 소멸을 Chunk EndPlay를 제외한 곳에서 진행하지 말 것
****************************************************************/
class AItemActorBase : public AActor
{
public:
	DECLARE_CLASS(AItemActorBase, AActor)

	AItemActorBase();

	// Life Cycle
	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void EndPlay() override;
	void Serialize(FArchive& Ar) override;

	// Item Getter
	UPrimitiveComponent* GetItemTrigger() const;
	UBoxComponent* GetItemTriggerBox() const { return ItemTrigger; }
	UScriptComponent* GetItemScript() const { return ItemScript; }
	UBillboardComponent* GetItemImage() const { return ItemImage; }

	void SetItemScript(const FString& ScriptPath);

	// Texture는 생성자에서 로드하지 않고 경로만 저장한다.
	// 실제 적용은 BeginPlay에서 한다.
	void SetItemTexturePath(const FString& TexturePath);
	const FString& GetItemTexturePath() const { return ItemTexturePath; }

	// Item Flag 관련
	bool HasFeature(EItemFeatureFlags Feature) const;
	void SetFeatureEnabled(EItemFeatureFlags Feature, bool bEnabled);
	void AddFeature(EItemFeatureFlags Feature);
	void RemoveFeature(EItemFeatureFlags Feature);
	uint32 GetFeatureFlags() const { return ItemFeatureFlags; }
	void SetFeatureFlags(uint32 InFlags) { ItemFeatureFlags = InFlags; }

	// Config
	FItemInteractionConfig& GetInteractionConfig() { return InteractionConfig; }
	const FItemInteractionConfig& GetInteractionConfig() const { return InteractionConfig; }

	// BoxComponent와 Trigger 활성화 여부
	void SetTriggerEnabled(bool bEnabled);
	bool IsTriggerEnabled() const;

protected:
	void OnItemBeginOverlap(const FComponentOverlapEvent& Event);
	bool IsValidInteractor(AActor* OtherActor) const;
	void ConsumeItem();

	// Item Actor Component
	UBoxComponent* ItemTrigger = nullptr;
	UScriptComponent* ItemScript = nullptr;
	UBillboardComponent* ItemImage = nullptr;

	// BeginPlay에서 적용할 Texture 경로
	FString ItemTexturePath = "None";


	// Item Config값
	FItemInteractionConfig InteractionConfig;

	bool bPicked = false;

	// default flag값
	uint32 ItemFeatureFlags =
		static_cast<uint32>(EItemFeatureFlags::PickupOnOverlap)
		| static_cast<uint32>(EItemFeatureFlags::ConsumeOnPickup)
		| static_cast<uint32>(EItemFeatureFlags::SingleUse);
};
