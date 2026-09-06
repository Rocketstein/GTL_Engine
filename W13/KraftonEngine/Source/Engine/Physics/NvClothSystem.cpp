#include "Physics/NvClothSystem.h"

#include "Component/Primitive/ClothComponent.h"
#include "Core/Logging/Log.h"

#include <algorithm>

#if WITH_NVCLOTH
#include "NvCloth/Callbacks.h"
#include "NvCloth/Cloth.h"
#include "NvCloth/Factory.h"
#include "NvCloth/Solver.h"

#include <foundation/PxErrorCallback.h>
#include <cmath>
#include <malloc.h>

namespace
{
	class FNvClothAllocator final : public physx::PxAllocatorCallback
	{
	public:
		void* allocate(size_t Size, const char* /*TypeName*/, const char* /*Filename*/, int /*Line*/) override
		{
			return _aligned_malloc(Size, 16);
		}

		void deallocate(void* Ptr) override
		{
			_aligned_free(Ptr);
		}
	};

	class FNvClothErrorCallback final : public physx::PxErrorCallback
	{
	public:
		void reportError(physx::PxErrorCode::Enum Code, const char* Message, const char* File, int Line) override
		{
			UE_LOG("[NvCloth][Error %d] %s (%s:%d)", static_cast<int>(Code), Message, File, Line);
		}
	};

	class FNvClothAssertHandler final : public nv::cloth::PxAssertHandler
	{
	public:
		void operator()(const char* Expression, const char* File, int Line, bool& Ignore) override
		{
			UE_LOG("[NvCloth][Assert] %s (%s:%d)", Expression, File, Line);
			Ignore = true;
		}
	};

	FNvClothAllocator GNvClothAllocator;
	FNvClothErrorCallback GNvClothErrorCallback;
	FNvClothAssertHandler GNvClothAssertHandler;

	constexpr float GClothFixedTimeStep = 1.0f / 60.0f;
	constexpr int GClothMaxSubSteps = 4;
	constexpr float GClothMaxFrameDelta = GClothFixedTimeStep * static_cast<float>(GClothMaxSubSteps);
}
#endif

bool FNvClothSystem::Initialize()
{
	if (bInitialized)
	{
		return true;
	}

	FixedStepAccumulator = 0.0f;

#if WITH_NVCLOTH
	UE_LOG("[NvCloth] Initializing callbacks");
	nv::cloth::InitializeNvCloth(&GNvClothAllocator, &GNvClothErrorCallback, &GNvClothAssertHandler, nullptr);

	UE_LOG("[NvCloth] Creating CPU factory");
	Factory = NvClothCreateFactoryCPU();
	if (!Factory)
	{
		UE_LOG("[NvCloth] Failed to create CPU factory");
		return false;
	}

	UE_LOG("[NvCloth] Creating CPU solver");
	Solver = Factory->createSolver();
	if (!Solver)
	{
		NvClothDestroyFactory(Factory);
		Factory = nullptr;
		UE_LOG("[NvCloth] Failed to create CPU solver");
		return false;
	}

	bInitialized = true;
	UE_LOG("[NvCloth] CPU factory initialized (Factory=%p, Solver=%p, CUDA=%d, DX=%d)",
		Factory,
		Solver,
		NvClothCompiledWithCudaSupport() ? 1 : 0,
		NvClothCompiledWithDxSupport() ? 1 : 0);
	return true;
#else
	UE_LOG("[NvCloth] Disabled (WITH_NVCLOTH=0)");
	return false;
#endif
}

void FNvClothSystem::Shutdown()
{
	RegisteredComponents.clear();
	FixedStepAccumulator = 0.0f;

#if WITH_NVCLOTH
	if (Solver)
	{
		delete Solver;
		Solver = nullptr;
		UE_LOG("[NvCloth] CPU solver destroyed");
	}

	if (Factory)
	{
		NvClothDestroyFactory(Factory);
		Factory = nullptr;
		UE_LOG("[NvCloth] CPU factory destroyed");
	}
#endif

	bInitialized = false;
}

void FNvClothSystem::Tick(float DeltaTime)
{
	bool bHasPendingSimulationInput = false;
	for (UClothComponent* Component : RegisteredComponents)
	{
		if (Component && Component->HasPendingSimulationInput())
		{
			bHasPendingSimulationInput = true;
			break;
		}
	}

	if (!bHasPendingSimulationInput)
	{
		FixedStepAccumulator = 0.0f;
		return;
	}

#if WITH_NVCLOTH
	SimulateFixedSteps(DeltaTime);
#endif

	for (UClothComponent* Component : RegisteredComponents)
	{
		if (Component)
		{
			Component->ApplySimulationResult();
		}
	}
}

void FNvClothSystem::RegisterComponent(UClothComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (std::find(RegisteredComponents.begin(), RegisteredComponents.end(), Component) == RegisteredComponents.end())
	{
		RegisteredComponents.push_back(Component);
	}
}

void FNvClothSystem::UnregisterComponent(UClothComponent* Component)
{
	if (!Component)
	{
		return;
	}

	RegisteredComponents.erase(
		std::remove(RegisteredComponents.begin(), RegisteredComponents.end(), Component),
		RegisteredComponents.end());
}

#if WITH_NVCLOTH
bool FNvClothSystem::AddCloth(nv::cloth::Cloth* Cloth)
{
	if (!Solver || !Cloth)
	{
		return false;
	}

	Solver->addCloth(Cloth);
	return true;
}

void FNvClothSystem::RemoveCloth(nv::cloth::Cloth* Cloth)
{
	if (Solver && Cloth)
	{
		Solver->removeCloth(Cloth);
	}
}

bool FNvClothSystem::Simulate(float DeltaTime)
{
	if (!Solver || DeltaTime <= 0.0f)
	{
		return false;
	}

	if (!Solver->beginSimulation(DeltaTime))
	{
		return false;
	}

	const int ChunkCount = Solver->getSimulationChunkCount();
	for (int Chunk = 0; Chunk < ChunkCount; ++Chunk)
	{
		Solver->simulateChunk(Chunk);
	}
	Solver->endSimulation();

	return !Solver->hasError();
}

bool FNvClothSystem::SimulateFixedSteps(float DeltaTime)
{
	if (!Solver || DeltaTime <= 0.0f || !std::isfinite(DeltaTime))
	{
		return false;
	}

	const float FrameDelta = std::clamp(DeltaTime, 0.0f, GClothMaxFrameDelta);
	FixedStepAccumulator = std::min(FixedStepAccumulator + FrameDelta, GClothMaxFrameDelta);

	bool bSimulated = false;
	int StepCount = 0;
	while (FixedStepAccumulator + 1.0e-6f >= GClothFixedTimeStep && StepCount < GClothMaxSubSteps)
	{
		if (!Simulate(GClothFixedTimeStep))
		{
			FixedStepAccumulator = 0.0f;
			return bSimulated;
		}

		FixedStepAccumulator -= GClothFixedTimeStep;
		bSimulated = true;
		++StepCount;
	}

	return bSimulated;
}
#endif
