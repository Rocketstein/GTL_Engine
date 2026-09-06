#pragma once
#include "ImposterGizmoActorBase.h"

class AImposterScaleGizmo : public AImposterGizmoActorBase {
public:
	DECLARE_CLASS(AImposterScaleGizmo, AImposterGizmoActorBase)
	void Transform(float DeltaTime) override;
	void Capture(AActor* InTarget)  override;

private:
	FVector GetScaleOffset();

private:
	FVector StartScale = FVector::OneVector;
	FVector TargetScale = FVector::OneVector;
};
