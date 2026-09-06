#include "World/Primitives/TextComponent.h"
#include "World/Mesh/MeshManager.h"
#include "SimpleJSON/json.hpp"

DEFINE_CLASS(UTextComponent, UPrimitiveComponent)
REGISTER_FACTORY(UTextComponent)

UTextComponent::UTextComponent()
{
	LocalExtents = { 0.0f, 0.5f, 0.5f };

	MeshData = &FMeshManager::GetQuad();
}

void UTextComponent::UpdateWorldAABB()
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

	extent.X += LocalExtents.X * 0.5f;

	FVector WorldCenter = GetWorldLocation();
	BoundingBox.WorldAABBMinLocation = WorldCenter - extent;
	BoundingBox.WorldAABBMaxLocation = WorldCenter + extent;
}

bool UTextComponent::GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand)
{
	if (!MeshData || !bIsVisible) {
		return false;
	}
	OutCommand.Type = ERenderCommandType::TextPrimitive;
	
	return UPrimitiveComponent::GetRenderCommand(viewMatrix, projMatrix, OutCommand);
}

bool UTextComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	if (!MeshData || MeshData->Vertices.empty())
	{
		return false;
	}

	FMatrix invWorld = CachedWorldMatrix.GetInverse();
	FVector localOrigin = invWorld.TransformPositionWithW(Ray.Origin);
	FVector localDirection = invWorld.TransformVector(Ray.Direction);
	localDirection.Normalize();

	bool bHit = false;
	float closestT = FLT_MAX;

	for (size_t i = 0; i < MeshData->Vertices.size() - 2; i += 3)
	{
		FVector v0 = MeshData->Vertices[i].Position;
		FVector v1 = MeshData->Vertices[i+1].Position;
		FVector v2 = MeshData->Vertices[i+2].Position;

		float t = 0.0f;

		if (IntersectTriangle(localOrigin, localDirection, v0, v1, v2, t))
		{
			if (t > 0.0f && t < closestT)
			{
				closestT = t;
				bHit = true;
				OutHitResult.FaceIndex = i;
			}
		}
	}

	OutHitResult.bHit = bHit;

	if (bHit)
	{
		FVector localHitPoint = localOrigin + (localDirection * closestT);
		FVector worldHitPoint = CachedWorldMatrix.TransformPositionWithW(localHitPoint);
		OutHitResult.Distance = FVector::Distance(Ray.Origin, worldHitPoint);
		OutHitResult.HitComponent = this;
		return true;
	}

	return false;
}

std::string WStringToUTF8(const std::wstring& wstr)
{
	if (wstr.empty()) return {};

	int sizeNeeded = WideCharToMultiByte(
		CP_UTF8, 0,
		wstr.data(), (int)wstr.size(),
		nullptr, 0,
		nullptr, nullptr);

	std::string result(sizeNeeded, 0);

	WideCharToMultiByte(
		CP_UTF8, 0,
		wstr.data(), (int)wstr.size(),
		result.data(), sizeNeeded,
		nullptr, nullptr);

	return result;
}

std::wstring UTF8ToWString(const std::string& str)
{
	if (str.empty())
		return {};

	int sizeNeeded = MultiByteToWideChar(
		CP_UTF8,
		0,
		str.data(),
		(int)str.size(),
		nullptr,
		0);

	std::wstring result(sizeNeeded, 0);

	MultiByteToWideChar(
		CP_UTF8,
		0,
		str.data(),
		(int)str.size(),
		result.data(),
		sizeNeeded);

	return result;
}

void UTextComponent::Serialize(json::JSON& j) const {
	UPrimitiveComponent::Serialize(j);
	j["Text"] = WStringToUTF8(Text);
	j["R"] = Color.X;
	j["G"] = Color.Y;
	j["B"] = Color.Z;
	j["A"] = Color.A;
}

void UTextComponent::Deserialize(const json::JSON& j) {
	UPrimitiveComponent::Deserialize(j);
	auto& jj = const_cast<json::JSON&>(j);
	Text = UTF8ToWString(jj["Text"].ToString());
	float R = jj["R"].ToFloat();
	float G = jj["G"].ToFloat();
	float B = jj["B"].ToFloat();
	float A = jj["A"].ToFloat();
	Color = FVector4(R, G, B, A);
}