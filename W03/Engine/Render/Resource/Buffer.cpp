#include "Buffer.h"

#pragma region __FMESHBUFFER__

void FMeshBuffer::Create(ID3D11Device* InDevice, const FMeshData& InMeshData)
{
	Release();

	if (InMeshData.Vertices.empty()) return;

	VertexBuffer.Create(InDevice, InMeshData.Vertices, sizeof(FVertex));
	if (!InMeshData.Indices.empty())
	{
		IndexBuffer.Create(InDevice, InMeshData.Indices);
	}
}

void FMeshBuffer::CreateDynamic(ID3D11Device* InDevice, uint32 InMaxVertices, uint64 sizeofVertex)
{
	Release();
	VertexBuffer.CreateDynamic(InDevice, InMaxVertices, sizeofVertex);
	IndexBuffer.CreateDynamic(InDevice, InMaxVertices);
}

//void FMeshBuffer::CreateDynamic(ID3D11Device* InDevice, uint32 InMaxVertices)
//{
//	Release();
//	VertexBuffer.CreateDynamic(InDevice, InMaxVertices, sizeof(FVertex));
//	IndexBuffer.CreateDynamic(InDevice, InMaxVertices);
//}

void FMeshBuffer::Release()
{
	VertexBuffer.Release();
	IndexBuffer.Release();
}

#pragma endregion

#pragma region __FVERTEXBUFFER__

void FVertexBuffer::Create(ID3D11Device* InDevice, const TArray<FVertex>& InData, uint32 InStride)
{
	Release();

	if (InData.empty()) return;

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = static_cast<uint32>(InData.size()) * InStride;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA srd = { InData.data() };
	HRESULT hr = InDevice->CreateBuffer(&desc, &srd, &Buffer);
	if (FAILED(hr)) { Release(); return; }

	VertexCount = static_cast<uint32>(InData.size());
	Stride = InStride;
}

void FVertexBuffer::Create(ID3D11Device* InDevice, const TArray<FUVVertex>& InData, uint32 InStride)
{
	Release();

	if (InData.empty()) return;

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = static_cast<uint32>(InData.size()) * InStride;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA srd = { InData.data() };
	HRESULT hr = InDevice->CreateBuffer(&desc, &srd, &Buffer);
	if (FAILED(hr)) { Release(); return; }

	VertexCount = static_cast<uint32>(InData.size());
	Stride = InStride;
}

void FVertexBuffer::CreateDynamic(ID3D11Device* InDevice, uint32 InMaxVertices, uint32 InStride)
{
	Release();

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = InMaxVertices * InStride;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = InDevice->CreateBuffer(&desc, nullptr, &Buffer);
	if (FAILED(hr)) { Release(); return; }

	VertexCount = InMaxVertices;
	Stride = InStride;
}

void FVertexBuffer::Release()
{
	if (Buffer)
	{
		Buffer->Release();
		Buffer = nullptr;
	}
}

void FVertexBuffer::Update(ID3D11DeviceContext* InDeviceContext, const TArray<FVertex>& InData)
{
	if (!Buffer || InData.empty()) return;

	D3D11_MAPPED_SUBRESOURCE MSR = {};
	InDeviceContext->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MSR);
	memcpy(MSR.pData, InData.data(), sizeof(FVertex) * InData.size());
	InDeviceContext->Unmap(Buffer, 0);

	VertexCount = static_cast<uint32>(InData.size());
}

ID3D11Buffer* FVertexBuffer::GetBuffer() const
{
	return Buffer;
}

#pragma endregion

#pragma region __FINSTANCEBUFFER__

void FInstanceBuffer::CreateDynamic(ID3D11Device* InDevice, uint32 InMaxVertices, uint32 InStride)
{
	Release();
	MaxInstanceCount = InMaxVertices;

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = InMaxVertices * InStride;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = InDevice->CreateBuffer(&desc, nullptr, &Buffer);
	if (FAILED(hr)) { Release(); return; }

	InstanceCount = 0;
	Stride = InStride;
}

void FInstanceBuffer::Release()
{
	if (Buffer)
	{
		Buffer->Release();
		Buffer = nullptr;
	}
}

void FInstanceBuffer::Update(ID3D11DeviceContext* InDeviceContext, const TArray<FVertex>& InData)
{
	if (!Buffer || InData.empty()) return;

	D3D11_MAPPED_SUBRESOURCE MSR = {};
	InDeviceContext->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MSR);
	memcpy(MSR.pData, InData.data(), sizeof(FVertex) * InData.size());
	InDeviceContext->Unmap(Buffer, 0);

	InstanceCount = static_cast<uint32>(InData.size());
}


void FInstanceBuffer::FontUpdate(ID3D11DeviceContext* InDeviceContext, const TArray<FFontInstance>& InData)
{
	if (!Buffer || InData.empty()) return;

	uint32 WriteCount = static_cast<uint32>(InData.size());
	if (InData.size() > MaxInstanceCount) {
		WriteCount = MaxInstanceCount;
	}

	D3D11_MAPPED_SUBRESOURCE MSR = {};
	InDeviceContext->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MSR);
	memcpy(MSR.pData, InData.data(), sizeof(FFontInstance) * WriteCount);
	InDeviceContext->Unmap(Buffer, 0);

	InstanceCount = WriteCount;
}

ID3D11Buffer* FInstanceBuffer::GetBuffer() const
{
	return Buffer;
}

#pragma endregion

#pragma region __FCONSTANTBUFFER__

//	 Constant buffer는 Dynamic으로 생성하여 업데이트가 가능하도록 구현
void FConstantBuffer::Create(ID3D11Device* InDevice, uint32 InByteWidth)
{
	D3D11_BUFFER_DESC constantBufferDesc = {};

	constantBufferDesc.ByteWidth = InByteWidth + 0xf & 0xfffffff0;
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	InDevice->CreateBuffer(&constantBufferDesc, nullptr, &Buffer);
}

void FConstantBuffer::Release()
{
	if (Buffer)
	{
		Buffer->Release();
		Buffer = nullptr;
	}
}

//	Constant buffer는 Dynamic으로 생성했으므로 업데이트가 가능. 업데이트가 필요하다면 Map/Unmap을 이용하여 업데이트
//	InData는 Constant buffer에 업데이트할 데이터의 포인터입니다. InByteWidth는 업데이트할 데이터의 총 byte 크기입니다.
//	즉, InData는 FPerObjectConstants 구조체의 포인터입니다.
void FConstantBuffer::Update(ID3D11DeviceContext* InDeviceContext, const void* InData, uint32 InByteWidth)
{
	if (!Buffer || !InData) return;

	D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
	InDeviceContext->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
	memcpy(constantbufferMSR.pData, InData, InByteWidth);
	InDeviceContext->Unmap(Buffer, 0);

}

ID3D11Buffer* FConstantBuffer::GetBuffer()
{
	return Buffer;
}

#pragma endregion

#pragma region __FINDEXBUFFER__

void FIndexBuffer::Create(ID3D11Device* InDevice, const TArray<uint32>& InData)
{
	Release();

	if (InData.empty()) return;

	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.ByteWidth = static_cast<uint32>(InData.size()) * sizeof(uint32);
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA srd = { InData.data() };
	HRESULT hr = InDevice->CreateBuffer(&desc, &srd, &Buffer);
	if (FAILED(hr)) { Release(); return; }

	IndexCount = static_cast<uint32>(InData.size());
}

void FIndexBuffer::CreateDynamic(ID3D11Device* InDevice, uint32 InMaxIndices)
{
	 Release();


    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth      = sizeof(uint32) * InMaxIndices;
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = InDevice->CreateBuffer(&desc, nullptr, &Buffer);
    if (FAILED(hr)) { Release(); return; }

    MaxIndexCount = InMaxIndices;
    IndexCount    = 6;
}


void FIndexBuffer::Release()
{
	if (Buffer)
	{
		Buffer->Release();
		Buffer = nullptr;
	}
}

void FIndexBuffer::Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData)
{
	if (!Buffer || InData.empty()) return;
	if (InData.size() > MaxIndexCount) return;

	D3D11_MAPPED_SUBRESOURCE MSR = {};
	InDeviceContext->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MSR);
	memcpy(MSR.pData, InData.data(), sizeof(uint32) * InData.size());
	InDeviceContext->Unmap(Buffer, 0);

	IndexCount = InData.size();
}

ID3D11Buffer* FIndexBuffer::GetBuffer() const
{
	return Buffer;
}

#pragma endregion

void FUVMeshBuffer::Create(ID3D11Device* InDevice, const FUVMeshData& InMeshData)
{
	Release();

	if (InMeshData.Vertices.empty()) return;

	VertexBuffer.Create(InDevice, InMeshData.Vertices, sizeof(FUVVertex));
	if (!InMeshData.Indices.empty())
	{
		IndexBuffer.Create(InDevice, InMeshData.Indices);
	}
}

void FUVMeshBuffer::CreateDynamic(ID3D11Device* InDevice, uint32 InMaxVertices, uint64 sizeofVertex)
{
	Release();
	VertexBuffer.CreateDynamic(InDevice, InMaxVertices, sizeofVertex);
	IndexBuffer.CreateDynamic(InDevice, InMaxVertices);
}

void FUVMeshBuffer::Release()
{
	VertexBuffer.Release();
	IndexBuffer.Release();
}
