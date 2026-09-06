#pragma once

#include "Core/Platform/PlatformTypes.h"
#include <cstddef>
#include <functional>


namespace Asset
{

    enum class EAssetType : uint8
    {
        Unknown = 0,
        Texture,
        Material,
        StaticMesh,
    };

    namespace HashUtils
    {
        template <typename T> inline void Combine(size_t &Seed, const T &Value)
        {
            const size_t H = std::hash<T>{}(Value);
            Seed ^= H + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
        }
    } // namespace HashUtils

} // namespace Asset
