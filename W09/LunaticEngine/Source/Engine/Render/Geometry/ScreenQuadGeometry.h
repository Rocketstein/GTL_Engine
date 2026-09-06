#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resource/Buffer.h"
#include "Render/Types/VertexTypes.h"

struct ID3D11ShaderResourceView;

class FScreenQuadGeometry
{
public:
	struct FBatch
	{
		uint32 FirstIndex = 0;
		uint32 IndexCount = 0;
		ID3D11ShaderResourceView* SRV = nullptr;
		uint16 ZOrder = 0;
		bool bSolidColorOnly = false;
	};

	void Create(ID3D11Device* InDevice);
	void Release();
	void Clear();

	void AddScreenQuad(
		float ScreenX,
		float ScreenY,
		float Width,
		float Height,
		float ViewportWidth,
		float ViewportHeight,
		const FVector4& TopColor,
		const FVector4& BottomColor,
		const FVector2& UVMin,
		const FVector2& UVMax,
		ID3D11ShaderResourceView* SRV,
		uint16 ZOrder,
		bool bSolidColorOnly);

	bool UploadBuffers(ID3D11DeviceContext* Context);

	ID3D11Buffer* GetVBBuffer() const { return VertexBuffer.GetBuffer(); }
	uint32 GetVBStride() const { return VertexBuffer.GetStride(); }
	ID3D11Buffer* GetIBBuffer() const { return IndexBuffer.GetBuffer(); }
	const TArray<FBatch>& GetBatches() const { return Batches; }
	bool HasAnyQuads() const { return !Vertices.empty() && !Batches.empty(); }

private:
	TArray<FVertexPNCT> Vertices;
	TArray<uint32> Indices;
	TArray<FBatch> Batches;
	FDynamicVertexBuffer VertexBuffer;
	FDynamicIndexBuffer IndexBuffer;
	ID3D11Device* Device = nullptr;
};
