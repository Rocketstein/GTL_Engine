#pragma once
#include "ObstacleActorBase.h"

class AMustSlideObstacleActor : public AObstacleActorBase {
public:
	DECLARE_CLASS(AMustSlideObstacleActor, AObstacleActorBase)
	void InitDefaultComponents(const FString& UStaticMeshFileName) override;
	void OnPlayerCollision() override {}

private:

};