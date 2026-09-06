#pragma once

/*
	Vertex Buffer와 Constant Buffer를 관리하는 Class 입니다.
	추후에 Index Buffer, Structured Buffer 등 다양한 Buffer들을 관리하는 Class로 확장할 수 있습니다.
	또한 코드가 길어지면, 각 Buffer들의 source를 분리할 수도 있습니다. (이후)
*/

#include "Render/Common/RenderTypes.h"

#include "Core/CoreTypes.h"
#include "Render/Resource/VertexTypes.h"

class FVertexBuffer
{ 
private:
	ID3D11Buffer* Buffer = nullptr;
	uint32 VertexCount = 0;
	uint32 Stride = 0;

public:
	void Create(ID3D11Device* InDevice, const TArray<FVertex>& InData, uint32 InStride);
	void Create(ID3D11Device* InDevice, const TArray<FUVVertex>& InData, uint32 InStride);
	void CreateDynamic(ID3D11Device* InDevice, uint32 InMaxVertices, uint32 InStride);
	void Release();

	void Update(ID3D11DeviceContext* InDeviceContext, const TArray<FVertex>& InData);

	uint32 GetVertexCount() const { return VertexCount; }
	uint32 GetStride() const { return Stride; }
	ID3D11Buffer* GetBuffer() const;
};

class FInstanceBuffer
{
private:
	ID3D11Buffer* Buffer = nullptr;
	uint32 MaxInstanceCount = 0;
	uint32 InstanceCount = 0;
	uint32 Stride = 0;

public:
	void CreateDynamic(ID3D11Device* InDevice, uint32 InMaxVertices, uint32 InStride);
	void Release();

	void Update(ID3D11DeviceContext* InDeviceContext, const TArray<FVertex>& InData);
	void FontUpdate(ID3D11DeviceContext* InDeviceContext, const TArray<FFontInstance>& InData);

	uint32 GetInstanceCount() const { return InstanceCount; }
	uint32 GetStride() const { return Stride; }
	ID3D11Buffer* GetBuffer() const;
};

class FConstantBuffer
{
private:
	ID3D11Buffer* Buffer = nullptr;

public:

private:

public:
	void Create(ID3D11Device* InDevice, uint32 InByteWidth);
	void Release();

	void Update(ID3D11DeviceContext* InDeviceContext, const void * InData, uint32 InByteWidth);
	
	ID3D11Buffer* GetBuffer();
};


class FIndexBuffer
{
private:
	ID3D11Buffer* Buffer = nullptr;
	uint32 MaxIndexCount = 0;
	uint32 IndexCount = 0;

public:

private:

public:
	void Create(ID3D11Device* InDevice, const TArray<uint32>& InData);
	void CreateDynamic(ID3D11Device* InDevice, uint32 InMaxIndices);
	void Release();

	void Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData);

	uint32 GetIndexCount() const { return IndexCount; }
	uint32 GetMaxIndexCount() const { return MaxIndexCount; };

	ID3D11Buffer* GetBuffer() const;
};

//	하나의 PrimitiveComponent에서 사용할 Mesh Buffer입니다. Vertex Buffer와 Index Buffer를 포함합니다.
//	종류에 따라 달라질 수 있음.
class FMeshBuffer
{
private:
	FVertexBuffer VertexBuffer;
	FIndexBuffer  IndexBuffer;
public:

private:

public:
	void Create(ID3D11Device* InDevice, const FMeshData& InMeshData);
	void CreateDynamic(ID3D11Device* InDevice, uint32 InMaxVertices, uint64 sizeofVertex = sizeof(FVertex));
	void Release();

	FVertexBuffer& GetFVertexBuffer() { return VertexBuffer; }
	FIndexBuffer& GetFIndexBuffer() { return IndexBuffer; }
	const FVertexBuffer& GetFVertexBuffer() const { return VertexBuffer; }
	const FIndexBuffer& GetFIndexBuffer() const { return IndexBuffer; }
	bool IsValid() const { return VertexBuffer.GetBuffer() != nullptr && VertexBuffer.GetVertexCount() > 0; }



};


class FUVMeshBuffer
{
private:
	FVertexBuffer VertexBuffer;
	FIndexBuffer  IndexBuffer;
public:

private:

public:
	void Create(ID3D11Device* InDevice, const FUVMeshData& InMeshData);
	void CreateDynamic(ID3D11Device* InDevice, uint32 InMaxVertices, uint64 sizeofVertex = sizeof(FUVVertex));
	void Release();

	FVertexBuffer& GetFVertexBuffer() { return VertexBuffer; }
	FIndexBuffer& GetFIndexBuffer() { return IndexBuffer; }
	const FVertexBuffer& GetFVertexBuffer() const { return VertexBuffer; }
	const FIndexBuffer& GetFIndexBuffer() const { return IndexBuffer; }
	bool IsValid() const { return VertexBuffer.GetBuffer() != nullptr && VertexBuffer.GetVertexCount() > 0; }

};