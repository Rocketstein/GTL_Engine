#pragma once
#include "Render/Resource/Buffer.h"

class FBatchedLine
{
public:
	FBatchedLine() = default;
	~FBatchedLine() = default;

private:
	FMeshBuffer				DynamicBuffer;

	TArray<FVertex>			Vertices;
	TArray<uint32>			Indices;

public:
	void Create(ID3D11Device* InDevice, uint32 MaxCount = (1 << 20));
	void Release();

	void AddGrid(int32 GridSize);
	void AddLine(const FVector& StartPoint, const FVector& EndPoint, const FVector4& Color);
	void AddLineRaw(const FVector& StartPoint, const FVector& EndPoint, const FVector4& Color);
	void AddAABB(const FVector& WorldMin, const FVector& WorldMax, const FVector4& Color = { 1.f, 1.f ,1.f ,1.f});

	void Update(ID3D11DeviceContext* InDeviceContext);

	bool HasLines() const { return !Vertices.empty(); }
	void Clear();

	FMeshBuffer* GetBatchedLineBuffer() { return &DynamicBuffer; };
};