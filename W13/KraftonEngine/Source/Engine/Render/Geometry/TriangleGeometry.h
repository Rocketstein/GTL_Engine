#pragma once
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/Resource/Buffer.h"

// FTriangleVertex — 솔리드 디버그 지오메트리용 버텍스 (Position + Normal + Color).
// Normal 은 라이팅(N·L)으로 형태가 입체적으로 읽히게 하는 핵심 요소다.
struct FTriangleVertex
{
	FVector  Position;
	FVector  Normal;
	FVector4 Color;

	FTriangleVertex(const FVector& InPos, const FVector& InNormal, const FVector4& InColor)
		: Position(InPos), Normal(InNormal), Color(InColor) {}
};

// FTriangleGeometry — 동적 VB/IB를 직접 소유하는 삼각형 지오메트리 헬퍼.
// FLineGeometry 의 TRIANGLELIST 대응물. 정점은 월드 공간으로 미리 변환해 채워 넣는다
// (라인 경로와 동일 — 셰이더는 per-object 모델 행렬이 필요 없다).
class FTriangleGeometry
{
public:
	void Create(ID3D11Device* InDevice);
	void Release();

	// 정점 1개 추가 후 인덱스 반환. 정점을 공유하는 메시(구/캡슐)에 사용.
	uint32 AddVertex(const FVector& Pos, const FVector& Normal, const FVector4& Color);
	// 기존 정점 인덱스로 삼각형 1개 구성.
	void AddTriangle(uint32 I0, uint32 I1, uint32 I2);
	// 평면 삼각형 1개 추가(정점 비공유, flat shading). 명시한 Normal 을 세 정점에 공유.
	void AddTriangle(const FVector& P0, const FVector& P1, const FVector& P2,
		const FVector& Normal, const FVector4& Color);
	// 위와 동일하나 면 법선을 (P1-P0)×(P2-P0) 로 계산(CCW 전면 가정).
	void AddTriangle(const FVector& P0, const FVector& P1, const FVector& P2, const FVector4& Color);

	void Clear();

	bool UploadBuffers(ID3D11DeviceContext* Context);
	ID3D11Buffer* GetVBBuffer() const { return VB.GetBuffer(); }
	uint32 GetVBStride() const { return VB.GetStride(); }
	ID3D11Buffer* GetIBBuffer() const { return IB.GetBuffer(); }
	uint32 GetIndexCount() const { return static_cast<uint32>(Indices.size()); }
	uint32 GetTriangleCount() const { return static_cast<uint32>(Indices.size() / 3); }

private:
	TArray<FTriangleVertex> IndexedVertices;
	TArray<uint32> Indices;

	FDynamicVertexBuffer VB;
	FDynamicIndexBuffer  IB;
	ID3D11Device* Device = nullptr;
};
