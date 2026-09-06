#include "BatchedLine.h"

void FBatchedLine::Create(ID3D11Device* InDevice, uint32 MaxCount)
{
	DynamicBuffer.CreateDynamic(InDevice, MaxCount);
	Vertices.reserve(MaxCount);
	Indices.reserve(MaxCount);

}

void FBatchedLine::Release()
{
	DynamicBuffer.Release();
}


void FBatchedLine::AddGrid(int32 GridSize)
{
	int32 GridMax = GridSize * 100;
	int32 GridMin = -GridMax;
	
	FVector4 GridColor = { 0.38f, 0.38f, 0.38f, 1.f };
	FVector4 GridColorThick = { 0.64f, 0.64f, 0.64f, 1.f };

	float FloatGridMin = static_cast<float>(GridMin);
	float FloatGridMax = static_cast<float>(GridMax);

	FVector4 AxisColor[3] = {
		{1.f, 0.f, 0.f, 1.f},	
		{0.f, 1.f, 0.f, 1.f},	
		{0.f, 0.f, 1.f, 1.f}	
	};

	AddLine({ FloatGridMin, 0.f, 0.f }, { FloatGridMax, 0.f, 0.f }, AxisColor[0]); // X
	AddLine({ 0.f, FloatGridMin, 0.f }, { 0.f, FloatGridMax, 0.f }, AxisColor[1]); // Y
	AddLine({ 0.f, 0.f, FloatGridMin }, { 0.f, 0.f, FloatGridMax }, AxisColor[2]); // Z

	for (int32 GridIndex = GridSize; GridIndex <= GridMax; GridIndex += GridSize)
	{
		float CurrentGrid = static_cast<float>(GridIndex);
		bool bIsFifth = (GridIndex % (GridSize * 5) == 0);  
		FVector4 Color = bIsFifth ? GridColorThick : GridColor;

		AddLine({ FloatGridMin,  CurrentGrid, 0.f }, { FloatGridMax,  CurrentGrid, 0.f }, Color);
		AddLine({ FloatGridMin, -CurrentGrid, 0.f }, { FloatGridMax, -CurrentGrid, 0.f }, Color);
		AddLine({  CurrentGrid, FloatGridMin, 0.f }, {  CurrentGrid, FloatGridMax, 0.f }, Color);
		AddLine({ -CurrentGrid, FloatGridMin, 0.f }, { -CurrentGrid, FloatGridMax, 0.f }, Color);
	}
}

void FBatchedLine::AddLine(const FVector& StartPoint, const FVector& EndPoint, const FVector4& Color)
{
	FVertex StartVertex = { StartPoint, Color };
	FVertex EndVertex   = { EndPoint,	Color };

	Vertices.push_back(StartVertex);
	Indices.push_back(Vertices.size() - 1);
	Vertices.push_back(EndVertex);
	Indices.push_back(Vertices.size() - 1);
}

void FBatchedLine::AddLineRaw(const FVector& StartPoint, const FVector& EndPoint, const FVector4& Color)
{
	uint32 StartIdx = static_cast<uint32>(Vertices.size());
	Vertices.push_back({ StartPoint, Color });
	Vertices.push_back({ EndPoint,   Color });
	Indices.push_back(StartIdx);
	Indices.push_back(StartIdx + 1);
}

void FBatchedLine::AddAABB(const FVector& WorldMin, const FVector& WorldMax, const FVector4& Color)
{
	auto GetCornerPoint = [&](int32 Bit) -> FVector
		{
			FVector Corner = {};
			Corner.X = (Bit & 1) ? (WorldMax.X) : (WorldMin.X);
			Corner.Y = (Bit & 2) ? (WorldMax.Y) : (WorldMin.Y);
			Corner.Z = (Bit & 4) ? (WorldMax.Z) : (WorldMin.Z);
			return Corner;
		};

	//	0 -> 1 방향으로 선을 그립니다.
	for (int32 PointIdx = 1; PointIdx <= 8; ++PointIdx)
	{
		for (int32 mask = 1; mask <= 4; mask <<= 1)
		{
			if (PointIdx & mask) continue;
			AddLine(GetCornerPoint(PointIdx), GetCornerPoint(PointIdx | mask), Color);
		}
	}
}


void FBatchedLine::Update(ID3D11DeviceContext* InDeviceContext)
{
	if (Vertices.empty()) return;

	DynamicBuffer.GetFVertexBuffer().Update(InDeviceContext, Vertices);
	DynamicBuffer.GetFIndexBuffer().Update(InDeviceContext, Indices);
}

void FBatchedLine::Clear()
{

	Vertices.clear();
	Indices.clear();
}
