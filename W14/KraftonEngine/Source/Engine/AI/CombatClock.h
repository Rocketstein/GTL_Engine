#pragma once

#include "Core/Types/CoreTypes.h"

// =============================================================================
//  FCombatClock — 전투 전용 고정틱 시계 (엔진 코어 메커니즘)
// =============================================================================
//  보고서 1순위: "전투 시계와 프레임 데이터 표준화". 월드의 가변 DeltaTime 과
//  분리된 고정 스텝(기본 60Hz) 누적기. AI 두뇌는 이 시계가 스텝을 내어줄 때만
//  "생각"해, 프레임레이트가 흔들려도 의사결정·판정 타이밍이 결정적으로 유지된다.
//
//  사용:
//    매 프레임  : Clock.Accumulate(DeltaTime)
//    두뇌 think : while (Clock.ConsumeStep()) { ... }  또는 if (Clock.ConsumeStep())
//
//  순수 데이터 구조체 — 리플렉션/직렬화 없음. 컴포넌트가 멤버로 보유한다.
// =============================================================================
struct FCombatClock
{
    // 고정 스텝 길이(초). 기본 1/60.
    float StepSeconds = 1.0f / 60.0f;

    // 누적된 미소비 시간.
    float Accumulator = 0.0f;

    // 소비 대기 중인 스텝 수.
    int32 PendingSteps = 0;

    // 누적 스텝 카운터(디버그/트레이스 틱 인덱스로 사용).
    int64 TotalSteps = 0;

    // 한 프레임 스파이크가 무한정 스텝을 쌓아 spiral-of-death 가 나지 않도록 상한.
    int32 MaxStepsPerAccumulate = 8;

    void Accumulate(float DeltaSeconds)
    {
        if (StepSeconds <= 0.0f)
        {
            return;
        }
        Accumulator += DeltaSeconds;
        int32 Produced = 0;
        while (Accumulator >= StepSeconds)
        {
            Accumulator -= StepSeconds;
            ++PendingSteps;
            if (++Produced >= MaxStepsPerAccumulate)
            {
                // 큰 스파이크: 잔여 누적을 버려 따라잡기 폭주를 막는다.
                Accumulator = 0.0f;
                break;
            }
        }
    }

    // 대기 스텝이 있으면 하나 소비하고 true. think 게이트로 사용.
    bool ConsumeStep()
    {
        if (PendingSteps > 0)
        {
            --PendingSteps;
            ++TotalSteps;
            return true;
        }
        return false;
    }

    void Reset()
    {
        Accumulator  = 0.0f;
        PendingSteps = 0;
    }
};
