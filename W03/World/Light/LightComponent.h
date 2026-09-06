#pragma once
#include "World/SceneComponent.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Common/RenderTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"

class ULightComponent : public USceneComponent {
protected:
	// Ray
	float Intensity = 0.0f;
	FVector4 Color = FVector4(1.f, 1.f, 0.f, 1.f);
	bool bIsEnabled = true;
	bool bIsEditorModeEnabled = true;
	bool bIsLightDirty = false;

public:
	DECLARE_CLASS(ULightComponent, USceneComponent)
	ULightComponent() = default;
	~ULightComponent() = default;
	
	virtual void Render() = 0;
	virtual void RenderLines(FBatchedLine* BatchLine) = 0;
	virtual void UpdateLight() = 0;

	float GetIntensity() const { return Intensity; }
	FVector4 GetColor() const { return Color; }
	bool GetIsEnabled() const { return bIsEnabled; }
	bool GetEditorModeEnabled() const { return bIsEditorModeEnabled; }

	void SetIntensity(float NewIntensity) { Intensity = NewIntensity; }
	void SetColor(const FVector4& NewColor) { bIsLightDirty = true; Color = NewColor; }
	void SetEnabled(bool IsEnabled) { bIsEnabled = IsEnabled; }
	void SetEditorModeEnabled(bool B) { bIsEditorModeEnabled = B; }

	void Serialize(json::JSON& j) const override;
	void Deserialize(const json::JSON& j) override;
};