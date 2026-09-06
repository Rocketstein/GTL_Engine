#pragma once

#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"

#ifndef WITH_NVCLOTH
#define WITH_NVCLOTH 0
#endif

class UClothComponent;

#if WITH_NVCLOTH
namespace nv
{
namespace cloth
{
class Cloth;
class Factory;
class Solver;
}
}
#endif

class FNvClothSystem : public TSingleton<FNvClothSystem>
{
	friend class TSingleton<FNvClothSystem>;

public:
	bool Initialize();
	void Shutdown();
	void Tick(float DeltaTime);

	bool IsInitialized() const { return bInitialized; }
	void RegisterComponent(UClothComponent* Component);
	void UnregisterComponent(UClothComponent* Component);

#if WITH_NVCLOTH
	nv::cloth::Factory* GetFactory() const { return Factory; }
	nv::cloth::Solver* GetSolver() const { return Solver; }
	bool AddCloth(nv::cloth::Cloth* Cloth);
	void RemoveCloth(nv::cloth::Cloth* Cloth);
	bool Simulate(float DeltaTime);
	bool SimulateFixedSteps(float DeltaTime);
#endif

private:
	FNvClothSystem() = default;
	~FNvClothSystem() = default;

	bool bInitialized = false;
	TArray<UClothComponent*> RegisteredComponents;
	float FixedStepAccumulator = 0.0f;

#if WITH_NVCLOTH
	nv::cloth::Factory* Factory = nullptr;
	nv::cloth::Solver* Solver = nullptr;
#endif
};
