#include "GizmoComponent.h"
#include "World/Mesh/MeshManager.h"

DEFINE_CLASS(UGizmoComponent, UPrimitiveComponent)
REGISTER_FACTORY(UGizmoComponent)

UGizmoComponent::UGizmoComponent()
{
	MeshData = &FMeshManager::GetTranslationGizmo();
	LocalExtents = FVector(1.5f, 1.5f, 1.5f);
}

void UGizmoComponent::UpdateMeshForMode(int Mode)
{
	switch (Mode)
	{
	case 0: MeshData = &FMeshManager::Get().GetTranslationGizmo(); break;
	case 1: MeshData = &FMeshManager::Get().GetRotationGizmo();    break;
	case 2: MeshData = &FMeshManager::Get().GetScaleGizmo();       break;
	}
}

bool UGizmoComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	UPrimitiveComponent::RaycastMesh(Ray, OutHitResult);
	UpdateHoveredAxis(OutHitResult.FaceIndex);
	return OutHitResult.bHit;
}

void UGizmoComponent::UpdateWorldAABB()
{
	FVector LExt = LocalExtents;
	FMatrix worldMatrix = GetWorldMatrix();

	float NewEx = std::abs(worldMatrix.M[0][0]) * LExt.X + std::abs(worldMatrix.M[1][0]) * LExt.Y + std::abs(worldMatrix.M[2][0]) * LExt.Z;
	float NewEy = std::abs(worldMatrix.M[0][1]) * LExt.X + std::abs(worldMatrix.M[1][1]) * LExt.Y + std::abs(worldMatrix.M[2][1]) * LExt.Z;
	float NewEz = std::abs(worldMatrix.M[0][2]) * LExt.X + std::abs(worldMatrix.M[1][2]) * LExt.Y + std::abs(worldMatrix.M[2][2]) * LExt.Z;

	FVector WorldCenter = GetWorldLocation();
	BoundingBox.WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
	BoundingBox.WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);
}

void UGizmoComponent::UpdateHoveredAxis(int Index)
{
	if (Index < 0)
	{
		if (!bIsHolding) SelectedAxis = -1;
	}
	else
	{
		if (!bIsHolding)
		{
			uint32 VertexIndex = MeshData->Indices[Index];
			SelectedAxis = MeshData->Vertices[VertexIndex].SubID;
		}
	}
}

bool UGizmoComponent::GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand)
{
	if (!MeshData || !bIsVisible)
		return false;
	return UPrimitiveComponent::GetRenderCommand(viewMatrix, projMatrix, OutCommand);
}

void UGizmoComponent::Deactivate()
{
	SetVisibility(false);
	SelectedAxis = -1;
}

EPrimitiveType UGizmoComponent::GetPrimitiveType() const
{
	// Mode is managed externally; mesh is swapped via UpdateMeshForMode.
	// Derive type from current mesh pointer.
	if (MeshData == &FMeshManager::Get().GetRotationGizmo())  return EPrimitiveType::EPT_RotGizmo;
	if (MeshData == &FMeshManager::Get().GetScaleGizmo())     return EPrimitiveType::EPT_ScaleGizmo;
	return EPrimitiveType::EPT_TransGizmo;
}
