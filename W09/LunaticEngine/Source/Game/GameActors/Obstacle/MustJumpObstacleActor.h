#pragma once
#include "ObstacleActorBase.h"

// Not used for now
class AMustJumpObstacleActor : public AObstacleActorBase {
public:
	DECLARE_CLASS(AMustJumpObstacleActor, AObstacleActorBase)
	void InitDefaultComponents(const FString& UStaticMeshFileName) override;
	void OnPlayerCollision() override {}

private:


};