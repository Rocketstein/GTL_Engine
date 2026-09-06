#pragma once
#include "ObstacleActorBase.h"

class AWireballActor : public AObstacleActorBase {
public:
	DECLARE_CLASS(AWireballActor, AObstacleActorBase)
	void InitDefaultComponents(const FString& UStaticMeshFileName) override;
	void OnPlayerCollision() override {}

private:

};