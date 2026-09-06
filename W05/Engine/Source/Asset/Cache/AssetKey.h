#pragma once

#include "Core/Containers/String.h"
#include "Core/Platform/PlatformTypes.h"
#include <filesystem>
#include <functional>
#include <string>


namespace Asset
{
    namespace KeyHash
    {
        template <typename T> inline void Combine(size_t &Seed, const T &Value)
        {
            const size_t H = std::hash<T>{}(Value);
            Seed ^= H + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
        }

        inline void CombinePath(size_t &Seed, const std::filesystem::path &Value)
        {
            Combine(Seed, Value);
        }
        inline void CombineString(size_t &Seed, const FString &Value) { Combine(Seed, Value); }
        inline void CombineWString(size_t &Seed, const FWString &Value) { Combine(Seed, Value); }
    } // namespace KeyHash

    struct FTextureSourceKey
    {
        std::filesystem::path    NormalizedPath;
        static FTextureSourceKey FromPath(const std::filesystem::path &InPath) { return {InPath}; }
        bool                     operator==(const FTextureSourceKey &Other) const
        {
            return NormalizedPath == Other.NormalizedPath;
        }
    };

    struct FMaterialSourceKey
    {
        std::filesystem::path     NormalizedPath;
        static FMaterialSourceKey FromPath(const std::filesystem::path &InPath) { return {InPath}; }
        bool                      operator==(const FMaterialSourceKey &Other) const
        {
            return NormalizedPath == Other.NormalizedPath;
        }
    };

    struct FStaticMeshSourceKey
    {
        std::filesystem::path       NormalizedPath;
        static FStaticMeshSourceKey FromPath(const std::filesystem::path &InPath)
        {
            return {InPath};
        }
        bool operator==(const FStaticMeshSourceKey &Other) const
        {
            return NormalizedPath == Other.NormalizedPath;
        }
    };

    struct FTextureIntermediateKey
    {
        FString ParsedHash;
        bool    operator==(const FTextureIntermediateKey &Other) const
        {
            return ParsedHash == Other.ParsedHash;
        }
    };

    struct FMaterialIntermediateKey
    {
        FString ParsedHash;
        bool    operator==(const FMaterialIntermediateKey &Other) const
        {
            return ParsedHash == Other.ParsedHash;
        }
    };

    struct FStaticMeshIntermediateKey
    {
        FString ParsedHash;
        bool    operator==(const FStaticMeshIntermediateKey &Other) const
        {
            return ParsedHash == Other.ParsedHash;
        }
    };

    struct FTextureCookedKey
    {
        FTextureIntermediateKey IntermediateKey;
        uint32                  CookVersion = 0;
        FString                 BuildKey;
        bool                    operator==(const FTextureCookedKey &Other) const
        {
            return IntermediateKey == Other.IntermediateKey && CookVersion == Other.CookVersion &&
                   BuildKey == Other.BuildKey;
        }
    };

    struct FMaterialCookedKey
    {
        FMaterialIntermediateKey IntermediateKey;
        uint32                   CookVersion = 0;
        FString                  BuildKey;
        bool                     operator==(const FMaterialCookedKey &Other) const
        {
            return IntermediateKey == Other.IntermediateKey && CookVersion == Other.CookVersion &&
                   BuildKey == Other.BuildKey;
        }
    };

    struct FStaticMeshCookedKey
    {
        FStaticMeshIntermediateKey IntermediateKey;
        uint32                     CookVersion = 0;
        FString                    BuildKey;
        bool                       operator==(const FStaticMeshCookedKey &Other) const
        {
            return IntermediateKey == Other.IntermediateKey && CookVersion == Other.CookVersion &&
                   BuildKey == Other.BuildKey;
        }
    };

} // namespace Asset

namespace std
{
    template <> struct hash<Asset::FTextureSourceKey>
    {
        size_t operator()(const Asset::FTextureSourceKey &Key) const
        {
            return std::hash<std::filesystem::path>{}(Key.NormalizedPath);
        }
    };
    template <> struct hash<Asset::FMaterialSourceKey>
    {
        size_t operator()(const Asset::FMaterialSourceKey &Key) const
        {
            return std::hash<std::filesystem::path>{}(Key.NormalizedPath);
        }
    };
    template <> struct hash<Asset::FStaticMeshSourceKey>
    {
        size_t operator()(const Asset::FStaticMeshSourceKey &Key) const
        {
            return std::hash<std::filesystem::path>{}(Key.NormalizedPath);
        }
    };

    template <> struct hash<Asset::FTextureIntermediateKey>
    {
        size_t operator()(const Asset::FTextureIntermediateKey &Key) const
        {
            return std::hash<FString>{}(Key.ParsedHash);
        }
    };
    template <> struct hash<Asset::FMaterialIntermediateKey>
    {
        size_t operator()(const Asset::FMaterialIntermediateKey &Key) const
        {
            return std::hash<FString>{}(Key.ParsedHash);
        }
    };
    template <> struct hash<Asset::FStaticMeshIntermediateKey>
    {
        size_t operator()(const Asset::FStaticMeshIntermediateKey &Key) const
        {
            return std::hash<FString>{}(Key.ParsedHash);
        }
    };

    template <> struct hash<Asset::FTextureCookedKey>
    {
        size_t operator()(const Asset::FTextureCookedKey &Key) const
        {
            size_t Seed = std::hash<Asset::FTextureIntermediateKey>{}(Key.IntermediateKey);
            Asset::KeyHash::Combine(Seed, Key.CookVersion);
            Asset::KeyHash::CombineString(Seed, Key.BuildKey);
            return Seed;
        }
    };
    template <> struct hash<Asset::FMaterialCookedKey>
    {
        size_t operator()(const Asset::FMaterialCookedKey &Key) const
        {
            size_t Seed = std::hash<Asset::FMaterialIntermediateKey>{}(Key.IntermediateKey);
            Asset::KeyHash::Combine(Seed, Key.CookVersion);
            Asset::KeyHash::CombineString(Seed, Key.BuildKey);
            return Seed;
        }
    };
    template <> struct hash<Asset::FStaticMeshCookedKey>
    {
        size_t operator()(const Asset::FStaticMeshCookedKey &Key) const
        {
            size_t Seed = std::hash<Asset::FStaticMeshIntermediateKey>{}(Key.IntermediateKey);
            Asset::KeyHash::Combine(Seed, Key.CookVersion);
            Asset::KeyHash::CombineString(Seed, Key.BuildKey);
            return Seed;
        }
    };
} // namespace std
