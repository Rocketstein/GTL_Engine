#pragma once
#include "ObstacleActorBase.h"

class ASimpleObstacleActor : public AObstacleActorBase {
public:
	DECLARE_CLASS(ASimpleObstacleActor, AObstacleActorBase)
	void InitDefaultComponents(const FString& UStaticMeshFileName) override;

private:
	void OnPlayerCollision() override {}
};
