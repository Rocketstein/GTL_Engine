#pragma once

#include "Component/SceneComponent.h"

class FScene;

class UCanvasRootComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UCanvasRootComponent, USceneComponent)

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void ContributeSelectedVisuals(FScene& Scene) const override;
	bool SupportsUIScreenPicking() const override { return false; }
	bool HitTestUIScreenPoint(float X, float Y) const override;
	int32 GetUIScreenPickingZOrder() const override { return INT_MIN / 2; }
	bool SupportsWorldGizmo() const override { return false; }

	const FVector& GetCanvasSize() const { return CanvasSize; }
	void SetCanvasSize(const FVector& InCanvasSize);
	FVector2 GetCanvasOrigin() const;

private:
	FVector CanvasSize = FVector(1920.0f, 1080.0f, 0.0f);
};
