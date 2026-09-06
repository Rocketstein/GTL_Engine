#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/Reflection/ObjectMacros.h"

// ============================================================
// 카메라 시네마틱 공용 enum
//
// 순수 UENUM 헤더 — GENERATED_BODY 가 없으므로 .generated.h 를 포함하지 않는다
// (코드젠은 EnumRegistry.generated.cpp 로 전역 등록).
// ============================================================

// 각 웨이포인트에서 카메라가 바라보는 방향 결정 방식.
UENUM()
enum class ECinematicRotMode : uint8
{
	UseWaypointRotation,  // 웨이포인트(핀) 자체 회전을 카메라 회전으로 사용
	LookAtTarget,         // LookAtActor(없으면 릭의 CoordinateTarget) 을 바라봄
	LookAtNextPoint,      // 다음 웨이포인트를 바라봄 (마지막은 이전 방향 유지)
};
