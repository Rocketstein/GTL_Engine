#include "LineGeometry.h"
#include "Math/MathUtils.h"
#include "Render/Types/FrameContext.h"

#include <algorithm>
#include <cmath>

namespace
{
	struct FViewPoint
	{
		FVector Position;
		float W = 1.0f;
	};

	float Comp(const FVector& V, int I) { return (&V.X)[I]; }

	FVector MakeGridPoint(int A0, int A1, int N, float V0, float V1, float VN)
	{
		FVector P;
		(&P.X)[A0] = V0;
		(&P.X)[A1] = V1;
		(&P.X)[N] = VN;
		return P;
	}

	FVector4 AxisColor(int Axis)
	{
		switch (Axis)
		{
		case 0: return FColor::Red().ToVector4();
		case 1: return FColor::Green().ToVector4();
		default: return FColor::Blue().ToVector4();
		}
	}

	float SnapToGrid(float Value, float Spacing)
	{
		return std::round(Value / Spacing) * Spacing;
	}

	bool IsAxisLine(float Coordinate, float Spacing)
	{
		return std::fabs(Coordinate) <= (Spacing * 0.25f);
	}

	int32 ComputeDynamicHalfCount(float Spacing, int32 BaseHalfCount, float CameraNormalDist)
	{
		const float BaseExtent = Spacing * static_cast<float>(std::max(BaseHalfCount, 1));
		const float HeightDrivenExtent = (std::fabs(CameraNormalDist) * 2.0f) + (Spacing * 4.0f);
		const float RequiredExtent = std::max(BaseExtent, HeightDrivenExtent);
		return std::max(BaseHalfCount, static_cast<int32>(std::ceil(RequiredExtent / Spacing)));
	}

	int DominantAxis(const FVector& V)
	{
		float AX = std::fabs(V.X), AY = std::fabs(V.Y), AZ = std::fabs(V.Z);
		if (AX >= AY && AX >= AZ) return 0;
		if (AY >= AX && AY >= AZ) return 1;
		return 2;
	}

	FViewPoint TransformToView(const FVector& Point, const FMatrix& View)
	{
		FViewPoint Result;
		Result.Position.X = Point.X * View.M[0][0] + Point.Y * View.M[1][0] + Point.Z * View.M[2][0] + View.M[3][0];
		Result.Position.Y = Point.X * View.M[0][1] + Point.Y * View.M[1][1] + Point.Z * View.M[2][1] + View.M[3][1];
		Result.Position.Z = Point.X * View.M[0][2] + Point.Y * View.M[1][2] + Point.Z * View.M[2][2] + View.M[3][2];
		Result.W = Point.X * View.M[0][3] + Point.Y * View.M[1][3] + Point.Z * View.M[2][3] + View.M[3][3];
		return Result;
	}

	FVector2 ProjectToNdc(const FVector& ViewPos, const FFrameContext& Frame)
	{
		const float ProjX = Frame.Proj.M[0][0];
		const float ProjY = Frame.Proj.M[1][1];
		if (Frame.bIsOrtho)
		{
			return FVector2(ViewPos.X * ProjX, ViewPos.Y * ProjY);
		}

		const float SafeZ = std::max(ViewPos.Z, 0.0001f);
		return FVector2((ViewPos.X * ProjX) / SafeZ, (ViewPos.Y * ProjY) / SafeZ);
	}

	FVector ComputeViewOffset(const FVector2& PerpNdc, const FVector& ViewPos, const FFrameContext& Frame, float HalfThicknessPixels)
	{
		const float SafeWidth = std::max(Frame.ViewportWidth, 1.0f);
		const float SafeHeight = std::max(Frame.ViewportHeight, 1.0f);
		const float DeltaNdcX = PerpNdc.X * (2.0f * HalfThicknessPixels / SafeWidth);
		const float DeltaNdcY = PerpNdc.Y * (2.0f * HalfThicknessPixels / SafeHeight);
		const float ProjX = std::max(std::fabs(Frame.Proj.M[0][0]), 0.0001f);
		const float ProjY = std::max(std::fabs(Frame.Proj.M[1][1]), 0.0001f);

		if (Frame.bIsOrtho)
		{
			return FVector(DeltaNdcX / ProjX, DeltaNdcY / ProjY, 0.0f);
		}

		const float SafeZ = std::max(ViewPos.Z, 0.0001f);
		return FVector((DeltaNdcX * SafeZ) / ProjX, (DeltaNdcY * SafeZ) / ProjY, 0.0f);
	}
}

void FLineGeometry::Create(ID3D11Device* InDevice)
{
	Release();
	Device = InDevice;
	if (!Device) return;
	Device->AddRef();

	VB.Create(Device, 512, sizeof(FLineVertex));
	IB.Create(Device, 1536);
}

void FLineGeometry::Release()
{
	VB.Release();
	IB.Release();
	IndexedVertices.clear();
	Indices.clear();
	if (Device) { Device->Release(); Device = nullptr; }
	Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
}

bool FLineGeometry::EnsureTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology)
{
	if (!Indices.empty() && Topology != InTopology)
	{
		return false;
	}

	Topology = InTopology;
	return true;
}

void FLineGeometry::AddLine(const FVector& Start, const FVector& End, const FVector4& InColor)
{
	AddLine(Start, End, InColor, InColor);
}

void FLineGeometry::AddLine(const FVector& Start, const FVector& End, const FVector4& StartColor, const FVector4& EndColor)
{
	if (!EnsureTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST))
	{
		return;
	}

	const uint32 BaseVertex = static_cast<uint32>(IndexedVertices.size());
	IndexedVertices.emplace_back(Start, StartColor);
	IndexedVertices.emplace_back(End, EndColor);
	Indices.push_back(BaseVertex);
	Indices.push_back(BaseVertex + 1);
}

void FLineGeometry::AddAABB(const FBoundingBox& Box, const FColor& InColor)
{
	if (!EnsureTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST))
	{
		return;
	}

	const FVector4 BoxColor = InColor.ToVector4();
	const uint32 BaseVertex = static_cast<uint32>(IndexedVertices.size());

	IndexedVertices.emplace_back(FVector(Box.Min.X, Box.Min.Y, Box.Min.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Max.X, Box.Min.Y, Box.Min.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Max.X, Box.Max.Y, Box.Min.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Min.X, Box.Max.Y, Box.Min.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Min.X, Box.Min.Y, Box.Max.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Max.X, Box.Min.Y, Box.Max.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Max.X, Box.Max.Y, Box.Max.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Min.X, Box.Max.Y, Box.Max.Z), BoxColor);

	static constexpr uint32 AABBEdgeIndices[] =
	{
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7
	};

	for (uint32 EdgeIndex : AABBEdgeIndices)
	{
		Indices.push_back(BaseVertex + EdgeIndex);
	}
}

void FLineGeometry::AddBillboardLine(const FVector& Start, const FVector& End, const FVector4& Color, const FFrameContext& Frame, float ThicknessPixels)
{
	AddBillboardLine(Start, End, Color, Color, Frame, ThicknessPixels);
}

void FLineGeometry::AddBillboardLine(const FVector& Start, const FVector& End, const FVector4& StartColor, const FVector4& EndColor,
	const FFrameContext& Frame, float ThicknessPixels)
{
	if (ThicknessPixels <= 1.0f)
	{
		AddLine(Start, End, StartColor, EndColor);
		return;
	}

	if (!EnsureTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST))
	{
		return;
	}

	const FViewPoint StartView = TransformToView(Start, Frame.View);
	const FViewPoint EndView = TransformToView(End, Frame.View);
	if (!Frame.bIsOrtho && (StartView.Position.Z <= 0.0f || EndView.Position.Z <= 0.0f))
	{
		return;
	}

	FVector2 ScreenDir = ProjectToNdc(EndView.Position, Frame) - ProjectToNdc(StartView.Position, Frame);
	if (ScreenDir.Length() <= 0.0001f)
	{
		return;
	}
	ScreenDir.Normalize();
	const FVector2 PerpNdc(-ScreenDir.Y, ScreenDir.X);
	const float HalfThickness = ThicknessPixels * 0.5f;

	const FVector StartOffsetView = ComputeViewOffset(PerpNdc, StartView.Position, Frame, HalfThickness);
	const FVector EndOffsetView = ComputeViewOffset(PerpNdc, EndView.Position, Frame, HalfThickness);
	const FMatrix InvView = Frame.View.GetInverseFast();

	const FVector P0 = (StartView.Position - StartOffsetView) * InvView;
	const FVector P1 = (StartView.Position + StartOffsetView) * InvView;
	const FVector P2 = (EndView.Position + EndOffsetView) * InvView;
	const FVector P3 = (EndView.Position - EndOffsetView) * InvView;

	const uint32 BaseVertex = static_cast<uint32>(IndexedVertices.size());
	IndexedVertices.emplace_back(P0, StartColor);
	IndexedVertices.emplace_back(P1, StartColor);
	IndexedVertices.emplace_back(P2, EndColor);
	IndexedVertices.emplace_back(P3, EndColor);

	Indices.push_back(BaseVertex + 0);
	Indices.push_back(BaseVertex + 1);
	Indices.push_back(BaseVertex + 2);
	Indices.push_back(BaseVertex + 0);
	Indices.push_back(BaseVertex + 2);
	Indices.push_back(BaseVertex + 3);
}

void FLineGeometry::AddBillboardAABB(const FBoundingBox& Box, const FColor& Color, const FFrameContext& Frame, float ThicknessPixels)
{
	const FVector4 BoxColor = Color.ToVector4();
	const FVector V[8] =
	{
		FVector(Box.Min.X, Box.Min.Y, Box.Min.Z),
		FVector(Box.Max.X, Box.Min.Y, Box.Min.Z),
		FVector(Box.Max.X, Box.Max.Y, Box.Min.Z),
		FVector(Box.Min.X, Box.Max.Y, Box.Min.Z),
		FVector(Box.Min.X, Box.Min.Y, Box.Max.Z),
		FVector(Box.Max.X, Box.Min.Y, Box.Max.Z),
		FVector(Box.Max.X, Box.Max.Y, Box.Max.Z),
		FVector(Box.Min.X, Box.Max.Y, Box.Max.Z)
	};

	static constexpr uint32 AABBEdges[][2] =
	{
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
	};

	for (const auto& Edge : AABBEdges)
	{
		AddBillboardLine(V[Edge[0]], V[Edge[1]], BoxColor, Frame, ThicknessPixels);
	}
}

void FLineGeometry::AddWorldHelpers(const FShowFlags& ShowFlags, float GridSpacing, int32 GridHalfLineCount,
	const FVector& CameraPosition, const FVector& CameraForward, bool bIsOrtho)
{
	const float Spacing = GridSpacing;
	const int32 BaseHalfCount = std::max(GridHalfLineCount, 1);

	if (Spacing <= 0.0f) return;

	int N = 2;
	if (bIsOrtho)
	{
		N = DominantAxis(CameraForward);
	}

	const int A0 = (N == 0) ? 1 : 0;
	const int A1 = (N == 2) ? 1 : 2;
	constexpr float PlaneOffset = 0.0f;

	const float Center0 = SnapToGrid(Comp(CameraPosition, A0), Spacing);
	const float Center1 = SnapToGrid(Comp(CameraPosition, A1), Spacing);
	const float CameraNormalDist = Comp(CameraPosition, N) - PlaneOffset;
	const int32 DynamicHalfCount = ComputeDynamicHalfCount(Spacing, BaseHalfCount, CameraNormalDist);
	const float BaseGridExtent = Spacing * static_cast<float>(DynamicHalfCount);

	const float Min0 = Center0 - BaseGridExtent;
	const float Max0 = Center0 + BaseGridExtent;
	const float Min1 = Center1 - BaseGridExtent;
	const float Max1 = Center1 + BaseGridExtent;

	const int32 Min0Idx = static_cast<int32>(std::floor((Min0 - Center0) / Spacing));
	const int32 Max0Idx = static_cast<int32>(std::ceil((Max0 - Center0) / Spacing));
	const int32 Min1Idx = static_cast<int32>(std::floor((Min1 - Center1) / Spacing));
	const int32 Max1Idx = static_cast<int32>(std::ceil((Max1 - Center1) / Spacing));

	const float AxisBias = std::max(Spacing * 0.001f, 0.001f);

	const bool bShowAxis0 = (Min1 <= 0.0f) && (Max1 >= 0.0f);
	const bool bShowAxis1 = (Min0 <= 0.0f) && (Max0 >= 0.0f);

	if (ShowFlags.bGrid)
	{
		const FVector4 GridColor = FColor::Gray().ToVector4();

		for (int32 I = Min1Idx; I <= Max1Idx; ++I)
		{
			const float W1 = Center1 + (static_cast<float>(I) * Spacing);
			if (bShowAxis0 && IsAxisLine(W1, Spacing)) continue;
			AddLine(
				MakeGridPoint(A0, A1, N, Min0, W1, PlaneOffset),
				MakeGridPoint(A0, A1, N, Max0, W1, PlaneOffset),
				GridColor);
		}

		for (int32 I = Min0Idx; I <= Max0Idx; ++I)
		{
			const float W0 = Center0 + (static_cast<float>(I) * Spacing);
			if (bShowAxis1 && IsAxisLine(W0, Spacing)) continue;
			AddLine(
				MakeGridPoint(A0, A1, N, W0, Min1, PlaneOffset),
				MakeGridPoint(A0, A1, N, W0, Max1, PlaneOffset),
				GridColor);
		}
	}

	if (ShowFlags.bWorldAxis)
	{
		const float AxisLen = std::max(BaseGridExtent, Spacing * 10.0f);

		AddLine(
			MakeGridPoint(A0, A1, N, -AxisLen, 0.0f, AxisBias),
			MakeGridPoint(A0, A1, N, AxisLen, 0.0f, AxisBias),
			AxisColor(A0));

		AddLine(
			MakeGridPoint(A0, A1, N, 0.0f, -AxisLen, AxisBias),
			MakeGridPoint(A0, A1, N, 0.0f, AxisLen, AxisBias),
			AxisColor(A1));

		AddLine(
			MakeGridPoint(A0, A1, N, 0.0f, 0.0f, -AxisLen),
			MakeGridPoint(A0, A1, N, 0.0f, 0.0f, AxisLen),
			AxisColor(N));
	}
}

void FLineGeometry::Clear()
{
	IndexedVertices.clear();
	Indices.clear();
	Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
}

bool FLineGeometry::UploadBuffers(ID3D11DeviceContext* Context)
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
