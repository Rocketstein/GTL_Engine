#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Core/ResourceTypes.h"
#include "Math/Vector.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Resource/Buffer.h"

// Texture Atlas UV 정보
struct FCharacterInfo
{
	float U;
	float V;
	float Width;
	float Height;
};

// FFontGeometry — 동적 VB/IB와 문자 지오메트리 생성을 직접 소유.
class FFontGeometry
{
public:
	struct FTextBatch
	{
		uint32 FirstIndex = 0;
		uint32 IndexCount = 0;
		const FFontResource* Font = nullptr;
	};

	void Create(ID3D11Device* InDevice);
	void Release();

	// 월드 좌표 빌보드 텍스트
	void AddWorldText(const FString& Text,
		const FVector& WorldPos,
		const FVector& TextRight,
		const FVector& TextUp,
		const FVector& WorldScale,
		const FVector4& Color,
		const FFontResource* Font,
		float Scale = 1.0f);

	// 스크린 공간 오버레이 텍스트
	void AddScreenText(const FString& Text,
		float ScreenX, float ScreenY,
		float ViewportWidth, float ViewportHeight,
		const FVector4& Color,
		const FFontResource* Font,
		float Scale = 1.0f,
		float LineSpacing = 1.14f,
		float LetterSpacing = 0.0f);

	void Clear();
	void ClearScreen();

	void EnsureCharInfoMap(const FFontResource* Resource);

	bool UploadWorldBuffers(ID3D11DeviceContext* Context);
	bool UploadScreenBuffers(ID3D11DeviceContext* Context);

	ID3D11Buffer* GetWorldVBBuffer() const { return WorldVB.GetBuffer(); }
	uint32 GetWorldVBStride() const { return WorldVB.GetStride(); }
	ID3D11Buffer* GetWorldIBBuffer() const { return WorldIB.GetBuffer(); }
	uint32 GetWorldIndexCount() const { return static_cast<uint32>(WorldIndices.size()); }

	ID3D11Buffer* GetScreenVBBuffer() const { return ScreenVB.GetBuffer(); }
	uint32 GetScreenVBStride() const { return ScreenVB.GetStride(); }
	ID3D11Buffer* GetScreenIBBuffer() const { return ScreenIB.GetBuffer(); }
	uint32 GetScreenIndexCount() const { return static_cast<uint32>(ScreenIndices.size()); }

	uint32 GetWorldQuadCount() const { return static_cast<uint32>(WorldVertices.size() / 4); }
	uint32 GetScreenQuadCount() const { return static_cast<uint32>(ScreenVertices.size() / 4); }
	const TArray<FTextBatch>& GetWorldBatches() const { return WorldBatches; }
	const TArray<FTextBatch>& GetScreenBatches() const { return ScreenBatches; }

private:
	struct FResolvedGlyph
	{
		FFontGlyph Glyph;
		bool bDrawSolidMagenta = false;
	};

	void BuildCharInfoMap(uint32 Columns, uint32 Rows);
	const FFontResource* ResolveFontForText(const FFontResource* PreferredFont, const FString& Text) const;
	bool HasGlyphForText(const FFontResource* Font, const FString& Text) const;
	bool GetGlyph(const FFontResource* Resource, uint32 Codepoint, FFontGlyph& OutGlyph) const;
	bool ResolveGlyph(const FFontResource* Resource, uint32 Codepoint, FResolvedGlyph& OutGlyph) const;

	// CPU 누적 배열
	TArray<FTextureVertex> WorldVertices;
	TArray<uint32>         WorldIndices;
	TArray<FTextureVertex> ScreenVertices;
	TArray<uint32>         ScreenIndices;
	TArray<FTextBatch>     WorldBatches;
	TArray<FTextBatch>     ScreenBatches;

	// GPU Dynamic Buffers
	FDynamicVertexBuffer WorldVB;
	FDynamicIndexBuffer  WorldIB;
	FDynamicVertexBuffer ScreenVB;
	FDynamicIndexBuffer  ScreenIB;

	// Device
	ID3D11Device* Device = nullptr;

	// CharInfoMap
	TMap<uint32, FCharacterInfo> CharInfoMap;
	uint32 CachedColumns = 0;
	uint32 CachedRows    = 0;
};
