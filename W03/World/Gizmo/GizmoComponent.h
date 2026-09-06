#pragma once

#include "World/PrimitiveComponent.h"

class UGizmoComponent : public UPrimitiveComponent
{
private:
	int32 SelectedAxis = -1;
	bool  bIsHolding   = false;  // synced from GizmoManager, used to lock hover axis during drag

public:
	DECLARE_CLASS(UGizmoComponent, UPrimitiveComponent)
	UGizmoComponent();

	// Rendering
	bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) override;
	void Deactivate() override;
	EPrimitiveType GetPrimitiveType() const override;

	// Bounds / Picking
	void UpdateWorldAABB() override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	inline bool IsHolding() const { return bIsHolding; }

	// Hover state (written by raycast, read by renderer)
	void UpdateHoveredAxis(int Index);
	void ResetSelectedAxis()          { SelectedAxis = -1; }
	void SetHoldingState(bool bHolding) { bIsHolding = bHolding; }
	inline int32  GetSelectedAxis() const { return SelectedAxis; }
	inline bool   IsHovered()       const { return SelectedAxis != -1; }

	// Mesh swap driven by GizmoManager
	void UpdateMeshForMode(int Mode);
};
