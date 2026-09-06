#pragma once

#include "Core/Types/CoreTypes.h"
#include "Profiling/Stats/Stats.h"

#if STATS
// 물리 씬이 한 프레임의 simulate/fetchResults 직후 채우는 카운터.
// 타이밍(physics thread time, simulation step time)은 SCOPE_STAT_CAT("...", "Physics")
// 로 이미 FStatManager 스냅샷에 들어가므로 여기서는 PxSimulationStatistics 로만 얻을 수
// 있는 카운트(활성 컨스트레인트/시뮬레이션 바디)만 보관한다 — FShadowStats 패턴과 동일.
struct FPhysicsStats
{
	// PxSimulationStatistics::nbActiveConstraints
	static uint32 NumActiveConstraints;

	// 이번 step 에서 실제로 시뮬레이션된(자고 있지 않은) 다이내믹 바디 수
	// = PxSimulationStatistics::nbActiveDynamicBodies
	static uint32 NumSimulatingBodies;

	static void Reset()
	{
		NumActiveConstraints = 0;
		NumSimulatingBodies = 0;
	}
};

#define PHYSICS_STATS_RESET()                          FPhysicsStats::Reset()
#define PHYSICS_STATS_SET_CONSTRAINTS(Count)           FPhysicsStats::NumActiveConstraints = (Count)
#define PHYSICS_STATS_SET_SIMULATING_BODIES(Count)     FPhysicsStats::NumSimulatingBodies = (Count)
#else
#define PHYSICS_STATS_RESET()                          ((void)0)
#define PHYSICS_STATS_SET_CONSTRAINTS(Count)           ((void)0)
#define PHYSICS_STATS_SET_SIMULATING_BODIES(Count)     ((void)0)
#endif
