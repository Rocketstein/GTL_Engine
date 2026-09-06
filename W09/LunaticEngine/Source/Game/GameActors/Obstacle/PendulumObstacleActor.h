#pragma once
#include "ObstacleActorBase.h"

class APendulumObstacleActor : public AObstacleActorBase {
public:
	DECLARE_CLASS(APendulumObstacleActor, AObstacleActorBase)
	void InitDefaultComponents(const FString& UStaticMeshFileName) override;
	void OnPlayerCollision() override {}

private:


};