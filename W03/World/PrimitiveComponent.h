#pragma once

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Common/RenderTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"

struct FMeshData;

class UPrimitiveComponent : public USceneComponent
{
protected:
	const FMeshData* MeshData = nullptr;
	FBoundingBox BoundingBox;

	FVector LocalExtents = { 0.5f, 0.5f, 0.5f };
	bool bIsVisible = true;

public:
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)
	inline const FMeshData* GetMeshData() const { return MeshData; };
	inline void SetVisibility(bool bVisible) { bIsVisible = bVisible; }

	virtual const FBoundingBox& GetBoundingBox() const { return BoundingBox; }

	//Collision
	virtual void UpdateWorldAABB() = 0;

	bool CheckAABB(const FRay& Ray);
	bool Raycast(const FRay& Ray, FHitResult& OutHitResult);
	bool IntersectTriangle(const FVector& RayOrigin, const FVector& RayDir, const FVector& V0, const FVector& V1, const FVector& V2, float& OutT);
	virtual bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult);
	inline bool IsVisible() const { return bIsVisible; }

	void UpdateWorldMatrix() override;

	virtual bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) {
		//OutCommand.Type = ERenderCommandType::Primitive;
		OutCommand.TransformConstants.Model = GetWorldMatrix();
		OutCommand.TransformConstants.View = viewMatrix;
		OutCommand.TransformConstants.Projection = projMatrix;

		return true;
	}

	//	각 Primitive Component는 자신이 어떤 Primitive Type인지 Renderer에게 알려줄 수 있어야 합니다. (Dynamic Binding)
	virtual EPrimitiveType GetPrimitiveType() const = 0;

	void Serialize(json::JSON& j) const override;
	void Deserialize(const json::JSON& j) override;
};

