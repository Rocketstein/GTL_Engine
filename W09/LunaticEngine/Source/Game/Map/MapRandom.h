#pragma once

#include "Core/CoreTypes.h"

#include <algorithm>
#include <chrono>
#include <random>

namespace MapRandom
{
	inline std::mt19937& Engine()
	{
		static std::mt19937 Generator = []()
		{
			std::random_device Device;
			const auto Now = static_cast<uint32>(
				std::chrono::high_resolution_clock::now().time_since_epoch().count());

			std::seed_seq Seed{
				Device(),
				Device(),
				Device(),
				Device(),
				Now
			};

			return std::mt19937(Seed);
		}();

		return Generator;
	}

	inline bool Chance(float Probability)
	{
		const float ClampedProbability = std::clamp(Probability, 0.0f, 1.0f);
		std::bernoulli_distribution Distribution(ClampedProbability);
		return Distribution(Engine());
	}

	inline int32 Index(int32 Count)
	{
		if (Count <= 0)
		{
			return 0;
		}

		std::uniform_int_distribution<int32> Distribution(0, Count - 1);
		return Distribution(Engine());
	}
}
