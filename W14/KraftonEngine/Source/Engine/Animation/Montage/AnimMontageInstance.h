#pragma once

#include "Object/Object.h"
#include "Object/FName.h"
#include "Math/Transform.h"

class UAnimMontage;
class FReferenceCollector;
class UAnimInstance;
struct FPoseContext;

// Montage 재생 상태 머신.
//   AnimInstance 슬롯이 active 1개와 blend-out 중인 outgoing 여러 개를 보유한다.
//
// 라이프사이클:
//   Inactive → Play() → BlendingIn → (alpha 도달) → Playing → (section end / Stop)
//                                                            ↓
//                                                       BlendingOut → Inactive
//
// 매 프레임:
//   Tick(dt, Owner)        — SectionTime 진행, section 끝나면 chain, blend alpha 갱신
//   GetBlendWeight()       — 현재 blend 가중치 (0..1) → AnimInstance 가 base pose 와 blend
//   EvaluateMontagePose()  — 현재 시점 montage 의 본 pose 생성 (base 와 blend 는 caller 담당)

#include "Source/Engine/Animation/Montage/AnimMontageInstance.generated.h"

UCLASS()
class UAnimMontageInstance : public UObject
{
public:
    GENERATED_BODY()

    UAnimMontageInstance()           = default;
    ~UAnimMontageInstance() override = default;

    // ── 제어 API ──
    void Play(UAnimMontage* InMontage, FName StartSection, float InPlayRate, float InBlendInTime);
    void Stop(float InBlendOutTime);
    void JumpToSection(FName Name);
    void SetNextSection(FName From, FName To);

    // ── 상태 조회 ──
    bool          IsActive()       const { return State != EState::Inactive; }
    bool          IsBlendingOut()  const { return State == EState::BlendingOut; }
    UAnimMontage* GetCurrentMontage() const;
    FName         GetCurrentSectionName() const;
    float         GetSectionTime() const { return SectionTime; }
    float         GetCurrentSectionLength() const;
    void          SetSectionTime(float InSectionTime);
    float         GetBlendWeight() const;
    void          SetNotifiesEnabled(bool bEnabled) { bNotifiesEnabled = bEnabled; }
    // 재생 속도 직접 제어. 음수면 역재생(Tick 이 SectionTime 을 뒤로 진행, 섹션 시작에서 clamp).
    // 패링 리코일(공격 몽타주를 잠깐 거꾸로 감기) 등 외부 구동용 — Play() 의 양수 클램프를 우회한다.
    void          SetPlayRate(float InRate) { PlayRate = InRate; }
    float         GetPlayRate() const { return PlayRate; }
    void          SetHoldFinalFrame(bool bInHold) { bHoldFinalFrame = bInHold; }
    bool          ShouldHoldFinalFrame() const { return bHoldFinalFrame; }

    // ── 매 프레임 ──
    void Tick(float DeltaSeconds, UAnimInstance* Owner);
    void EvaluateMontagePose(FPoseContext& OutMontagePose);   // section + time → SourceSequence GetBonePose

    // 매 Tick 이 채우는 raw root motion delta (BlendWeight 곱 안 함). Slot 노드가 GetBlendWeight
    // 로 InputPose.LastRM 과 lerp 합성. 외부 누적은 RootNode 한 곳 (UAnimInstance::UpdateAnimation 끝).
    const FTransform& GetLastRootMotionDelta() const { return LastRootMotionDelta; }

    void AddReferencedObjects(FReferenceCollector& Collector) override;
    void BeginDestroy() override;

private:
    void EnterBlendingIn(float InBlendInTime);
    void EnterBlendingOut(float InBlendOutTime);
    void FinishStop();
    bool AdvanceSection(UAnimInstance* Owner);   // section 끝 도달 시 다음 section 으로 이동 — 더 진행 가능하면 true

    enum class EState
    {
        Inactive,
        BlendingIn,
        Playing,
        BlendingOut
    };

    UAnimMontage* CurrentMontage      = nullptr;
    int32         CurrentSectionIndex = -1;
    float         SectionTime         = 0.0f;   // 현재 section 시작 시점 이후 경과 (sec)
    FName         PendingNextSection  = FName::None;   // SetNextSection 의 1회성 override

    EState State           = EState::Inactive;
    float  BlendAlpha      = 0.0f;
    float  BlendInTime     = 0.25f;
    float  BlendOutTime    = 0.25f;
    float  PlayRate        = 1.0f;
    bool   bNotifiesEnabled = true;
    bool   bHoldFinalFrame = false;

    // 매 Tick 의 raw RM delta (W 곱 안 함). Slot 노드가 GetBlendWeight 로 lerp 책임.
    FTransform LastRootMotionDelta;
};
