#include "SphereComponent.h"
#include "World/Mesh/MeshManager.h"

DEFINE_CLASS(USphereComponent, UPrimitiveComponent)
REGISTER_FACTORY(USphereComponent)

USphereComponent::USphereComponent()
{
	MeshData = &FMeshManager::GetSphere();
}
void USphereComponent::UpdateWorldAABB()
{
	// 타원체의 AABB
	FVector LExt = LocalExtents;
	FVector Scale = GetScaleVector();

	FVector Forward = GetForwardVector(); Forward.Normalize();
	FVector Right = GetRightVector();  Right.Normalize();
	FVector Up = GetUpVector(); Up.Normalize();

	float XComponent = LExt.X * Scale.X;
	float YComponent = LExt.Y * Scale.Y;
	float ZComponent = LExt.Z * Scale.Z;

	FVector NewExtent;
	NewExtent.X = sqrtf(
		Forward.X * Forward.X * XComponent * XComponent +
		Right.X * Right.X * YComponent * YComponent +
		Up.X * Up.X * ZComponent * ZComponent);

	NewExtent.Y = sqrtf(
		Forward.Y * Forward.Y * XComponent * XComponent +
		Right.Y * Right.Y * YComponent * YComponent +
		Up.Y * Up.Y * ZComponent * ZComponent);

	NewExtent.Z = sqrtf(
		Forward.Z * Forward.Z * XComponent * XComponent +
		Right.Z * Right.Z * YComponent * YComponent +
		Up.Z * Up.Z * ZComponent * ZComponent);

	FVector WorldCenter = GetWorldLocation();
	BoundingBox.WorldAABBMinLocation = WorldCenter - NewExtent;
	BoundingBox.WorldAABBMaxLocation = WorldCenter + NewExtent;
}
bool USphereComponent::GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) {
	if (!MeshData || !bIsVisible) {
		return false;
	}

	// Fetch vertexbuffer info here
	/*OutCommand.VertexBuffer = ...;
	OutCommand.VertexCount = ...;
	OutCommand.Stride = sizeof(vbuffer);*/

	return UPrimitiveComponent::GetRenderCommand(viewMatrix, projMatrix, OutCommand);
}