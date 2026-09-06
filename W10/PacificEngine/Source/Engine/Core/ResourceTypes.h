// 엔진 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Object/FName.h"

// COM 인터페이스 전방 선언 (d3d11.h 없이 포인터 사용 가능)
struct ID3D11ShaderResourceView;

// Font/SpriteVfx 공통 텍스처 아틀라스 리소스.
// 각 매니저가 소유하며, 컴포넌트는 포인터로 참조만 합니다.
// Columns × Rows 그리드 정보를 함께 보유해 UV 계산에 활용합니다.
struct FTextureAtlasResource
{
    virtual ~FTextureAtlasResource() = default;

    FName Name;
    FString Path;                            // Asset 상대 경로 (Resource.ini 또는 .font/.spritevfx 에서 로드)
    ID3D11ShaderResourceView* SRV = nullptr; // GPU에 로드된 텍스처 SRV
    uint64 TrackedMemoryBytes = 0;

    uint32 Columns = 1; // 아틀라스 가로 프레임(셀) 수
    uint32 Rows = 1;    // 아틀라스 세로 프레임(셀) 수

    bool IsLoaded() const { return SRV != nullptr; }
};

// 의미론적 타입 분리 — 용도를 명시하며 추후 확장성을 고려합니다.
struct FFontResource : public FTextureAtlasResource {};
struct FSpriteVfxResource : public FTextureAtlasResource {};
struct FTextureResource : public FTextureAtlasResource {}; // 단일 정적 텍스처 (Columns=Rows=1)

// 하위 호환성을 위한 별칭 (점진적 교체용)
using FParticleResource = FSpriteVfxResource;
