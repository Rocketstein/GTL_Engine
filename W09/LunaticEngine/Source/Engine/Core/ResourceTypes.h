#pragma once

#include "Audio/AudioTypes.h"
#include "Core/CoreTypes.h"
#include "Object/FName.h"

// COM 인터페이스 전방 선언 (d3d11.h 없이 포인터 사용 가능)
struct ID3D11ShaderResourceView;

struct FFontGlyph
{
	float U0 = 0.0f;
	float V0 = 0.0f;
	float U1 = 0.0f;
	float V1 = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
	float XOffset = 0.0f;
	float YOffset = 0.0f;
	float XAdvance = 0.0f;
};

// Font/Particle 공통 텍스처 아틀라스 리소스.
// ResourceManager가 소유하며, 컴포넌트는 포인터로 참조만 합니다.
// Columns × Rows 그리드 정보를 함께 보유해 UV 계산에 활용합니다.
struct FTextureAtlasResource
{
	FName Name;
	FString Path;							// Asset 상대 경로 (Resource.ini에서 로드)
	ID3D11ShaderResourceView* SRV = nullptr; // GPU에 로드된 텍스처 SRV
	uint64 TrackedMemoryBytes = 0;
	bool bEditorResource = false;

	uint32 Columns = 1;						// 아틀라스 가로 프레임(셀) 수
	uint32 Rows    = 1;						// 아틀라스 세로 프레임(셀) 수
	uint32 AtlasWidth = 0;
	uint32 AtlasHeight = 0;
	float LineHeight = 0.0f;
	float Base = 0.0f;
	bool bHasGlyphMetrics = false;
	TMap<uint32, FFontGlyph> Glyphs;
	TMap<uint64, float> Kernings;

	bool IsLoaded() const { return SRV != nullptr; }
	const FFontGlyph* FindGlyph(uint32 Codepoint) const
	{
		auto It = Glyphs.find(Codepoint);
		return (It != Glyphs.end()) ? &It->second : nullptr;
	}
	float GetKerning(uint32 First, uint32 Second) const
	{
		const uint64 Key = (static_cast<uint64>(First) << 32) | static_cast<uint64>(Second);
		auto It = Kernings.find(Key);
		return (It != Kernings.end()) ? It->second : 0.0f;
	}
};

// 의미론적 별칭 — 타입은 동일하지만 용도를 명시합니다.
using FFontResource     = FTextureAtlasResource;
using FParticleResource = FTextureAtlasResource;
using FTextureResource  = FTextureAtlasResource;	// 단일 정적 텍스처 (Columns=Rows=1)

struct FPathResource
{
	FName Name;
	FString Path; // Asset 상대 경로
};

using FMeshResource = FPathResource;

struct FSoundResource : FPathResource
{
	ESoundCategory Category = ESoundCategory::SFX;
};

using FMaterialResource = FPathResource;
using FGenericPathResource = FPathResource;
