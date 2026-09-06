#include "TriangleGeometry.h"

void FTriangleGeometry::Create(ID3D11Device* InDevice)
{
	Release();
	Device = InDevice;
	if (!Device) return;
	Device->AddRef();

	VB.Create(Device, 1024, sizeof(FTriangleVertex));
	IB.Create(Device, 3072);
}

void FTriangleGeometry::Release()
{
	VB.Release();
	IB.Release();
	IndexedVertices.clear();
	Indices.clear();
	if (Device) { Device->Release(); Device = nullptr; }
}

uint32 FTriangleGeometry::AddVertex(const FVector& Pos, const FVector& Normal, const FVector4& Color)
{
	const uint32 Index = static_cast<uint32>(IndexedVertices.size());
	IndexedVertices.emplace_back(Pos, Normal, Color);
	return Index;
}

void FTriangleGeometry::AddTriangle(uint32 I0, uint32 I1, uint32 I2)
{
	Indices.push_back(I0);
	Indices.push_back(I1);
	Indices.push_back(I2);
}

void FTriangleGeometry::AddTriangle(const FVector& P0, const FVector& P1, const FVector& P2,
	const FVector& Normal, const FVector4& Color)
{
	const uint32 Base = static_cast<uint32>(IndexedVertices.size());
	IndexedVertices.emplace_back(P0, Normal, Color);
	IndexedVertices.emplace_back(P1, Normal, Color);
	IndexedVertices.emplace_back(P2, Normal, Color);
	Indices.push_back(Base);
	Indices.push_back(Base + 1);
	Indices.push_back(Base + 2);
}

void FTriangleGeometry::AddTriangle(const FVector& P0, const FVector& P1, const FVector& P2, const FVector4& Color)
{
	const FVector Normal = (P1 - P0).Cross(P2 - P0).Normalized();
	AddTriangle(P0, P1, P2, Normal, Color);
}

void FTriangleGeometry::Clear()
{
	IndexedVertices.clear();
	Indices.clear();
}

bool FTriangleGeometry::UploadBuffers(ID3D11DeviceContext* Context)
{
	const uint32 VertexCount = static_cast<uint32>(IndexedVertices.size());
	const uint32 IndexCount = static_cast<uint32>(Indices.size());
	if (VertexCount == 0 || IndexCount == 0) return false;

	VB.EnsureCapacity(Device, VertexCount);
	IB.EnsureCapacity(Device, IndexCount);
	if (!VB.Update(Context, IndexedVertices.data(), VertexCount)) return false;
	if (!IB.Update(Context, Indices.data(), IndexCount)) return false;
	return true;
}
