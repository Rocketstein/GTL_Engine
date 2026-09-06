#pragma once

#include "AI/BehaviorTree/Runtime/BTNode.h"

// =============================================================================
//  Behavior Tree 서비스 — 부착된 Composite 가 활성인 동안 주기적으로 센서/블랙보드 갱신
// =============================================================================
//  보고서의 "센서→블랙보드 펌프"를 BT 안에서 수행. 외부 자극(지각/Lua/노티파이)이
//  블랙보드에 쓰고, BT Condition/Blackboard 데코레이터가 읽는 루프를 매 주기 갱신한다.
// =============================================================================

// Interval 초마다 Brain_Sense + Brain_AcquireTarget 호출(센서/타겟 갱신).
class FBTService_BrainSense : public FBTService_Base
{
public:
    float Interval = 0.25f;

    const char* GetDebugName() const override { return "BrainSense"; }
    void        Tick(const FBTContext& Context) override;

private:
    float Accum = 0.0f;
};
