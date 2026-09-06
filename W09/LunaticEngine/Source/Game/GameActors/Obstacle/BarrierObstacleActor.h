#pragma once
#include "ObstacleActorBase.h"

class ABarrierObstacleActor : public AObstacleActorBase {
public:
	DECLARE_CLASS(ABarrierObstacleActor, AObstacleActorBase)
	void InitDefaultComponents(const FString& UStaticMeshFileName) override;
	void OnPlayerCollision() override {}

private:

};