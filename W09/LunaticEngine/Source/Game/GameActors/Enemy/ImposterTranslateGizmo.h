#pragma once
#include "ImposterGizmoActorBase.h"

class AImposterTranslateGizmo : public AImposterGizmoActorBase {
public:
	DECLARE_CLASS(AImposterTranslateGizmo, AImposterGizmoActorBase)
	void Transform(float DeltaTime) override;
	void Capture(AActor* InTarget)  override;

private:
	FVector GetTranslateOffset();

private:
	FVector StartLocation  = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
};
