#include "PlaneComponent.h"
#include "World/Mesh/MeshManager.h"

DEFINE_CLASS(UPlaneComponent, UPrimitiveComponent)
REGISTER_FACTORY(UPlaneComponent)

UPlaneComponent::UPlaneComponent()
{
	LocalExtents = { 0.5f, 0.5f, 0.01f };

	MeshData = &FMeshManager::GetPlane();
}
void UPlaneComponent::UpdateWorldAABB()
{
	FVector forward = GetForwardVector();
	FVector right = GetRightVector();
	FVector up = GetUpVector();
	FVector Scale = GetScaleVector();

	float halfX = LocalExtents.X * Scale.X;
	float halfY = LocalExtents.Y * Scale.Y;
	float halfZ = LocalExtents.Z * Scale.Z;

	FVector extent = {
		abs(forward.X) * halfX + abs(right.X) * halfY + abs(up.X) * halfZ,
		abs(forward.Y) * halfX + abs(right.Y) * halfY + abs(up.Y) * halfZ,
		abs(forward.Z) * halfX + abs(right.Z) * halfY + abs(up.Z) * halfZ
	};

	extent.Z += LocalExtents.Z * 0.5f;

	FVector WorldCenter = GetWorldLocation();
	BoundingBox.WorldAABBMinLocation = WorldCenter - extent;
	BoundingBox.WorldAABBMaxLocation = WorldCenter + extent;
}
bool UPlaneComponent::GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) {
	if (!MeshData || !bIsVisible) {
		return false;
	}

	// Fetch vertexbuffer info here
	/*OutCommand.VertexBuffer = ...;
	OutCommand.VertexCount = ...;
	OutCommand.Stride = sizeof(vbuffer);*/

	return UPrimitiveComponent::GetRenderCommand(viewMatrix, projMatrix, OutCommand);
}