#pragma once
#include "ImposterGizmoActorBase.h"

class AImposterRotationGizmo : public AImposterGizmoActorBase {
public:
	DECLARE_CLASS(AImposterRotationGizmo, AImposterGizmoActorBase)
	void Transform(float DeltaTime) override;
	void Capture(AActor* InTarget)  override;
	
private:
	FRotator GetRotationOffset();

private:
	FRotator StartRotation = FRotator::ZeroRotator;
	FRotator TargetRotation = FRotator::ZeroRotator;
};
