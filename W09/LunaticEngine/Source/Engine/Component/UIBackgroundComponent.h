#pragma once

#include "Component/UIImageComponent.h"

class FScene;

class UUIBackgroundComponent : public UUIImageComponent
{
public:
	DECLARE_CLASS(UUIBackgroundComponent, UUIImageComponent)

	UUIBackgroundComponent();

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void ContributeVisuals(FScene& Scene) const override;
	void ContributeSelectedVisuals(FScene& Scene) const override;
	bool SupportsUIScreenPicking() const override { return false; }
	bool HitTestUIScreenPoint(float X, float Y) const override;
	int32 GetUIScreenPickingZOrder() const override { return GetZOrder(); }
	bool ParticipatesInPickingSpatialStructure() const override { return false; }

private:
	bool ResolveBackgroundRect(FVector2& OutPosition, FVector2& OutSize) const;
	FVector2 GetViewportSize2D() const;
};
