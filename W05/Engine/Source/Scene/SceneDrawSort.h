#pragma once

#include "Engine/Asset/Material.h"
#include "Engine/Asset/StaticMesh.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Scene/Scene.h"
#include <algorithm>
#include <cstdint>

namespace SceneSort
{
    namespace Detail
    {
        inline uint64 HashStringFNV1a64(const FString &Text)
        {
            constexpr uint64 OffsetBasis = 1469598103934665603ull;
            constexpr uint64 Prime = 1099511628211ull;

            uint64 Hash = OffsetBasis;
            for (unsigned char Byte : Text)
            {
                Hash ^= static_cast<uint64>(Byte);
                Hash *= Prime;
            }
            return Hash;
        }

        inline uint64 MakeStableAssetKey(const UAsset *Asset, uint64 FallbackPointerKey, uint64 Mask)
        {
            if (Asset != nullptr && !Asset->GetAssetPath().empty())
            {
                return HashStringFNV1a64(Asset->GetAssetPath()) & Mask;
            }

            return FallbackPointerKey & Mask;
        }
    } // namespace Detail

    inline uint64 MakeOpaqueSortKey(UStaticMeshComponent *Component, uint32 FrontToBackRank)
    {
        // Opaque 전용 정렬 우선순위:
        //  1) Material 변경 최소화
        //  2) Mesh(VB/IB) 변경 최소화
        //  3) front-to-back (가시 큐의 기존 순서 반영)
        //  4) tie-break
        constexpr uint64 MaterialMask = (1ull << 20) - 1ull;
        constexpr uint64 MeshMask = (1ull << 20) - 1ull;
        constexpr uint64 RankMask = (1ull << 20) - 1ull;
        constexpr uint64 TieMask = (1ull << 4) - 1ull;

        uint64 MaterialKey = 0;
        uint64 MeshKey = 0;
        uint64 TieKey = 0;

        if (Component != nullptr)
        {
            if (UMaterial *Material = Component->GetMaterial(0))
            {
                const uint64 Fallback = reinterpret_cast<uintptr_t>(Material) >> 4;
                MaterialKey = Detail::MakeStableAssetKey(Material, Fallback, MaterialMask);
            }

            if (UStaticMesh *StaticMesh = Component->GetStaticMesh())
            {
                const uint64 Fallback = reinterpret_cast<uintptr_t>(StaticMesh) >> 4;
                MeshKey = Detail::MakeStableAssetKey(StaticMesh, Fallback, MeshMask);
            }

            TieKey = (reinterpret_cast<uintptr_t>(Component) >> 4) & TieMask;
        }

        const uint64 RankKey = static_cast<uint64>(FrontToBackRank) & RankMask;
        return (MaterialKey << 44) | (MeshKey << 24) | (RankKey << 4) | TieKey;
    }

    inline void SortOpaqueDrawItems(TArray<FSceneDrawItem> &DrawItems)
    {
        // 동일 키에 대해 입력 순서를 보존해 front-to-back 로컬 이점을 유지.
        std::stable_sort(DrawItems.begin(), DrawItems.end(),
                         [](const FSceneDrawItem &A, const FSceneDrawItem &B)
                         {
                             return A.SortKey < B.SortKey;
                         });
    }
} // namespace SceneSort
