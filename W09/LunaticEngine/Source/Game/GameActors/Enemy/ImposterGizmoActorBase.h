#pragma once
#include "GimmickActorBase.h"
#include "Scripting/LuaActorProxy.h"
#include "Component/GizmoComponent.h"

class UPrimitiveComponent;

// "Captures" Actor (Obstacles). 
// Obstacles could be captured and move, rotate, and scale arbitraily (in a way that does not coerce game over)
class AImposterGizmoActorBase : public AGimmickActorBase {
public:
	DECLARE_CLASS(AImposterGizmoActorBase, AGimmickActorBase)
	
	virtual void Tick(float DeltaTime) override;
	virtual void Capture(AActor* InTarget);
	virtual void Transform(float DeltaTime) = 0;

	uint8 SetOffsetAxis();
	void Release();

	AActor* GetCapturedActor() const;
	FLuaActorProxy GetCapturedActorProxy() const;

protected:
	virtual ~AImposterGizmoActorBase() = default;
	bool HasAliveTarget() const;
	void ApplyCapturedActorTint();
	void ReleaseCapturedActorTint();

protected:
	UGizmoComponent* PreviewGizmo = nullptr; 
	AActor* Target			= nullptr;
	float	ActivationDelay = 1.5f;
	float   ElapsedDelay	= 0.f;
	float	Elapsed			= 0.f;
	bool	bTransforming   = false;
	uint8	OffsetAxis		= 0;
	bool	bCapturedActorWasSelected = false;
	TArray<UPrimitiveComponent*> CapturedTintedComponents;
	TArray<bool> CapturedTintedComponentWasSelected;
};
