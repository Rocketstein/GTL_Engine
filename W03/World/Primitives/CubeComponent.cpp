#include "World/Primitives/CubeComponent.h"
#include "World/Mesh/MeshManager.h"

DEFINE_CLASS(UCubeComponent, UPrimitiveComponent)
REGISTER_FACTORY(UCubeComponent)

UCubeComponent::UCubeComponent()
{
	MeshData = &FMeshManager::GetCube();
}

void UCubeComponent::UpdateWorldAABB()
{
	FVector forward = GetForwardVector();
	FVector right = GetRightVector();
	FVector up = GetUpVector();
	FVector Scale = GetScaleVector();

	float halfX = LocalExtents.X * Scale.X;
	float halfY = LocalExtents.Y * Scale.Y;
	float halfZ = LocalExtents.Z * Scale.Z;

	FVector extent = {
		std::abs(forward.X) * halfX + std::abs(right.X) * halfY + std::abs(up.X) * halfZ,
		std::abs(forward.Y) * halfX + std::abs(right.Y) * halfY + std::abs(up.Y) * halfZ,
		std::abs(forward.Z) * halfX + std::abs(right.Z) * halfY + std::abs(up.Z) * halfZ
	};

	FVector WorldCenter = GetWorldLocation();
	BoundingBox.WorldAABBMinLocation = WorldCenter - extent;
	BoundingBox.WorldAABBMaxLocation = WorldCenter + extent;
}

bool UCubeComponent::GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) {
	if (!MeshData || !bIsVisible) {
		return false;
	}

	// Fetch vertexbuffer info here
	/*OutCommand.VertexBuffer = ...;
	OutCommand.VertexCount = ...;
	OutCommand.Stride = sizeof(vbuffer);*/

	return UPrimitiveComponent::GetRenderCommand(viewMatrix, projMatrix, OutCommand);
}