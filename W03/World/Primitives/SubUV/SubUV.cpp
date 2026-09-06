#include "SubUV.h"
#include "Engine/EngineServices/EngineServices.h"
#include "World/Mesh/MeshManager.h"
#include "SimpleJSON/json.hpp"

DEFINE_CLASS(USubUVComponent, UPrimitiveComponent)
REGISTER_FACTORY(USubUVComponent, UPrimitiveComponent)


void USubUVComponent::UpdateFrame(float deltaTime)
{
	ElaspedTime += deltaTime;

	if (CellRows == 0 || CellColumns == 0 || PlayRate == 0) return; 

	CellSizeX = (1.f - OffsetLeftX - OffsetRightX) / CellColumns;
	CellSizeY = (1.f - OffsetUpY - OffsetBottomY) / CellRows;
	FrameCount = CellRows * CellColumns;
	Timer = 1.f / PlayRate;


	if (ElaspedTime >= Timer) {
		ElaspedTime = 0;
		if (!bLoop && CurrentFrameIndex + 1 == FrameCount) return;
		CurrentFrameIndex = (++CurrentFrameIndex)%FrameCount;
	}
	currentU = OffsetLeftX +(CurrentFrameIndex % CellColumns) * CellSizeX;
	currentV = OffsetUpY +(CurrentFrameIndex / CellColumns) * CellSizeY;
	FVector tmp = LocalExtents;

}

USubUVComponent::USubUVComponent(std::wstring filename)
{
	this->filename = filename;
	ElaspedTime = 0;
	OffsetLeftX = 24.f / 857.f; OffsetUpY = 12.f / 852.f;
	OffsetRightX = 17.f/857.f, OffsetBottomY = 22.f / 852.f;

	CellRows = CellColumns = 6;
	FrameCount = CellRows * CellColumns;
	CurrentFrameIndex = 0;

	Texture2DId = FEngineServices::GetResourceManager()->CreateTexture(this->filename);
	Texture2Dinfo = FEngineServices::GetResourceManager()->GetTextureInfo(Texture2DId);

	Timer = 1.f / PlayRate;

	CellSizeX = (1.f - OffsetLeftX - OffsetRightX) / CellColumns;
	CellSizeY = (1.f - OffsetUpY - OffsetBottomY) / CellRows;

	currentU = OffsetLeftX; currentV = OffsetUpY;
	UVMeshData = &FMeshManager::GetUVRect();
}

bool USubUVComponent::GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) {

	if (!UVMeshData || !bIsVisible ) {

		return false;
	}

	if (Texture2DId == -1) {
		return false;
	}
	FVector Scale = GetScaleVector();
	FMatrix Translation = FMatrix::MakeTranslationMatrix(GetWorldLocation());

	FVector Forward = FVector(viewMatrix.M[0][2], viewMatrix.M[1][2], viewMatrix.M[2][2]); Forward.Normalize();
	FVector Right = FVector(viewMatrix.M[0][0], viewMatrix.M[1][0], viewMatrix.M[2][0]); Right.Normalize();

	FMatrix BillboardRotation(
		Forward.X, Forward.Y, Forward.Z, 0,
		-Right.X, -Right.Y, -Right.Z, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);
	
	FVector localScale = GetRelativeScale();
	localScale.Y *= (CellSizeX * Texture2Dinfo.TextureWidth) / (CellSizeY * Texture2Dinfo.TextureHeight);
	FMatrix model = FMatrix::MakeScaleMatrix(localScale) * BillboardRotation
		* FMatrix::MakeTranslationMatrix(GetWorldLocation());


	OutCommand.Type = ERenderCommandType::SubUV;
	if (bIsBillBoard) OutCommand.TransformConstants.Model = model;
	else OutCommand.TransformConstants.Model = GetWorldMatrix();
	OutCommand.TransformConstants.View = viewMatrix;
	OutCommand.TransformConstants.Projection = projMatrix;
	OutCommand.TextureID = Texture2DId;
	OutCommand.SubUVConstants.cellSizeWidth = CellSizeX;
	OutCommand.SubUVConstants.cellSizeheight = CellSizeY;
	OutCommand.SubUVConstants.startX = currentU;
	OutCommand.SubUVConstants.startY = currentV;
	
	return true;// UPrimitiveComponent::GetRenderCommand(viewMatrix, projMatrix, OutCommand);
}

void USubUVComponent::Update(float deltatime)
{
	UpdateFrame(deltatime);
}

void USubUVComponent::ReloadTextureResource(std::wstring filename)
{
	
	int newTexture2DId = FEngineServices::GetResourceManager()->CreateTexture(filename);
	if (newTexture2DId < 0) return;
	Texture2DId = newTexture2DId;
	this->filename = filename;
	Texture2Dinfo = FEngineServices::GetResourceManager()->GetTextureInfo(Texture2DId);

}

bool USubUVComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	if (!UVMeshData || UVMeshData->Indices.empty())
		return false;

	FMatrix invWorld = CachedWorldMatrix.GetInverse();
	FVector localOrigin = invWorld.TransformPositionWithW(Ray.Origin);
	FVector localDirection = invWorld.TransformVector(Ray.Direction);
	localDirection.Normalize();

	bool bHit = false;
	float closestT = FLT_MAX;

	for (size_t i = 0; i < UVMeshData->Indices.size(); i += 3)
	{
		FVector v0 = UVMeshData->Vertices[UVMeshData->Indices[i]].Position;
		FVector v1 = UVMeshData->Vertices[UVMeshData->Indices[i + 1]].Position;
		FVector v2 = UVMeshData->Vertices[UVMeshData->Indices[i + 2]].Position;

		float t = 0.0f;
		if (IntersectTriangle(localOrigin, localDirection, v0, v1, v2, t) && t > 0.0f && t < closestT)
		{
			closestT = t;
			bHit = true;
			OutHitResult.FaceIndex = i;
		}
	}

	OutHitResult.bHit = bHit;
	if (bHit)
	{
		FVector localHit = localOrigin + localDirection * closestT;
		FVector worldHit = CachedWorldMatrix.TransformPositionWithW(localHit);
		OutHitResult.Distance = FVector::Distance(Ray.Origin, worldHit);
		OutHitResult.HitComponent = this;
		return true;
	}
	return false;
}

void USubUVComponent::Serialize(json::JSON& j) const {
	UPrimitiveComponent::Serialize(j);
	j["filename"]    = std::string(filename.begin(), filename.end());
	j["PlayRate"]    = PlayRate;
	j["bLoop"]       = bLoop;
	j["CellRows"]	 = CellRows;
	j["CellColumns"] = CellColumns;
}

void USubUVComponent::Deserialize(const json::JSON& j) {
	UPrimitiveComponent::Deserialize(j);
	auto& jj = const_cast<json::JSON&>(j);

	std::string path = jj["filename"].ToString();
	if (!path.empty())
		filename = std::wstring(path.begin(), path.end());
	PlayRate	 = jj["PlayRate"].ToInt();
	bLoop		 = jj["bLoop"].ToBool();
	CellRows	 = jj["CellRows"].ToInt();
	CellColumns	 = jj["CellColumns"].ToInt();

	// Recalculate derived values
	FrameCount = CellRows * CellColumns;
	Timer      = 1.f / PlayRate;
	CellSizeX  = (1.f - OffsetLeftX - OffsetRightX) / CellRows;
	CellSizeY  = (1.f - OffsetUpY   - OffsetBottomY) / CellColumns;
	if (!filename.empty())
		ReloadTextureResource(filename);
}

void USubUVComponent::UpdateWorldAABB()
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

	FVector WorldCenter = GetWorldLocation();
	BoundingBox.WorldAABBMinLocation = WorldCenter - extent;
	BoundingBox.WorldAABBMaxLocation = WorldCenter + extent;
}
