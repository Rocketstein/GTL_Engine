#include "Render/Geometry/ScreenQuadGeometry.h"

void FScreenQuadGeometry::Create(ID3D11Device* InDevice)
{
	Device = InDevice;
	if (!Device)
	{
		return;
	}

	Device->AddRef();
	VertexBuffer.Create(Device, 128, sizeof(FVertexPNCT));
	IndexBuffer.Create(Device, 192);
}

void FScreenQuadGeometry::Release()
{
	Vertices.clear();
	Indices.clear();
	Batches.clear();
	VertexBuffer.Release();
	IndexBuffer.Release();

	if (Device)
	{
		Device->Release();
		Device = nullptr;
	}
}

void FScreenQuadGeometry::Clear()
{
	Vertices.clear();
	Indices.clear();
	Batches.clear();
}

void FScreenQuadGeometry::AddScreenQuad(
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
	bool bSolidColorOnly)
{
	if (Width <= 0.0f || Height <= 0.0f || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
	{
		return;
	}

	auto PixelToClipX = [ViewportWidth](float X)
	{
		return (X / ViewportWidth) * 2.0f - 1.0f;
	};

	auto PixelToClipY = [ViewportHeight](float Y)
	{
		return 1.0f - (Y / ViewportHeight) * 2.0f;
	};

	const float Left = PixelToClipX(ScreenX);
	const float Right = PixelToClipX(ScreenX + Width);
	const float Top = PixelToClipY(ScreenY);
	const float Bottom = PixelToClipY(ScreenY + Height);

	const uint32 BaseVertex = static_cast<uint32>(Vertices.size());
	const uint32 FirstIndex = static_cast<uint32>(Indices.size());

	Vertices.push_back({ FVector(Left, Top, 0.0f), FVector(0.0f, 0.0f, 1.0f), TopColor, FVector2(UVMin.X, UVMin.Y) });
	Vertices.push_back({ FVector(Right, Top, 0.0f), FVector(0.0f, 0.0f, 1.0f), TopColor, FVector2(UVMax.X, UVMin.Y) });
	Vertices.push_back({ FVector(Left, Bottom, 0.0f), FVector(0.0f, 0.0f, 1.0f), BottomColor, FVector2(UVMin.X, UVMax.Y) });
	Vertices.push_back({ FVector(Right, Bottom, 0.0f), FVector(0.0f, 0.0f, 1.0f), BottomColor, FVector2(UVMax.X, UVMax.Y) });

	Indices.push_back(BaseVertex + 0);
	Indices.push_back(BaseVertex + 1);
	Indices.push_back(BaseVertex + 2);
	Indices.push_back(BaseVertex + 1);
	Indices.push_back(BaseVertex + 3);
	Indices.push_back(BaseVertex + 2);

	Batches.push_back({ FirstIndex, 6u, SRV, ZOrder, bSolidColorOnly });
}

bool FScreenQuadGeometry::UploadBuffers(ID3D11DeviceContext* Context)
{
	if (Vertices.empty() || Indices.empty() || !Context || !Device)
	{
		return false;
	}

	VertexBuffer.EnsureCapacity(Device, static_cast<uint32>(Vertices.size()));
	IndexBuffer.EnsureCapacity(Device, static_cast<uint32>(Indices.size()));
	if (!VertexBuffer.Update(Context, Vertices.data(), static_cast<uint32>(Vertices.size())))
	{
		return false;
	}
	if (!IndexBuffer.Update(Context, Indices.data(), static_cast<uint32>(Indices.size())))
	{
		return false;
	}

	return true;
}
