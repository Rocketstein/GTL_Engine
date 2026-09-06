#pragma once

#include "Collision/OverlapInfo.h"
#include "Core/Delegate.h"

class AActor;
class UPrimitiveComponent;

/*
 * Collision Event 개념
 *
 * Overlap:
 * - 두 Component가 공간적으로 겹쳤다는 이벤트이다.
 * - 상대를 막거나 밀어내는 의미는 아니다.
 * - Trigger, 감지 영역, 아이템 획득 범위 등에 사용한다.
 *
 * Hit:
 * - 두 Component가 block 성격으로 충돌했다는 이벤트이다.
 * - 벽에 막힘, 바닥과 충돌, 투사체 명중 등에 사용한다.
 */

/*
 * Component Overlap 이벤트 데이터
 */
struct FComponentOverlapEvent
{
    // Overlap 이벤트를 발생시킨 Component
    UPrimitiveComponent *OverlappedComponent = nullptr;

    // 겹친 상대 Actor
    AActor *OtherActor = nullptr;

    // 겹친 상대 Component
    UPrimitiveComponent *OtherComponent = nullptr;

    // Sweep 검사로 발생한 Overlap인지 여부
    bool bFromSweep = false;

    // Sweep 검사 결과 정보
    FHitResult SweepResult;
};

/*
 * Component Hit 이벤트 데이터
 */
struct FComponentHitEvent
{
    // Hit 이벤트를 발생시킨 Component
    UPrimitiveComponent *HitComponent = nullptr;

    // 충돌한 상대 Actor
    AActor *OtherActor = nullptr;

    // 충돌한 상대 Component
    UPrimitiveComponent *OtherComponent = nullptr;

    // 충돌로 인해 전달된 충격량
    FVector NormalImpulse = FVector(0, 0, 0);

    // 충돌 위치, 법선, 거리 등의 정보
    FHitResult Hit;
};

/*
 * Component BeginOverlap 이벤트 Delegate 타입
 */
DECLARE_DELEGATE_TYPE(FComponentBeginOverlapSignature, const FComponentOverlapEvent &);

/*
 * Component EndOverlap 이벤트 Delegate 타입
 */
DECLARE_DELEGATE_TYPE(FComponentEndOverlapSignature, const FComponentOverlapEvent &);

/*
 * Component Hit 이벤트 Delegate 타입
 */
DECLARE_DELEGATE_TYPE(FComponentHitSignature, const FComponentHitEvent &);