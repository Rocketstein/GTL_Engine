#pragma once

#include "Asset/Cache/AssetKey.h"
#include "Asset/Cache/BuildSettings.h"
#include "Asset/Intermediate/IntermediateMtlData.h"
#include "Asset/Intermediate/IntermediateObjData.h"
#include "Asset/Intermediate/IntermediateTextureData.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Misc/Paths.h"
#include <filesystem>
#include <sstream>


namespace Asset
{
    namespace KeyUtils
    {
        inline FString FinalizeHash(size_t Seed)
        {
            std::ostringstream Oss;
            Oss << std::hex << Seed;
            return Oss.str();
        }

        inline FString PathToKeyString(const std::filesystem::path &Path)
        {
            return FPaths::Utf8FromPath(Path);
        }

        inline void HashVector(size_t &Seed, const FVector3 &V)
        {
            KeyHash::Combine(Seed, V.X);
            KeyHash::Combine(Seed, V.Y);
            KeyHash::Combine(Seed, V.Z);
        }

        inline void HashVector2(size_t &Seed, const FVector2 &V)
        {
            KeyHash::Combine(Seed, V.X);
            KeyHash::Combine(Seed, V.Y);
        }

        inline FTextureIntermediateKey MakeIntermediateKey(const FIntermediateTextureData &Data)
        {
            size_t Seed = 0;
            KeyHash::Combine(Seed, Data.Width);
            KeyHash::Combine(Seed, Data.Height);
            KeyHash::Combine(Seed, Data.Channels);
            KeyHash::Combine(Seed, Data.Pixels.size());
            for (uint8 Byte : Data.Pixels)
            {
                KeyHash::Combine(Seed, Byte);
            }
            return {FinalizeHash(Seed)};
        }

        inline void HashMaterialData(size_t &Seed, const FIntermediateMtlData &Data)
        {
            KeyHash::CombineString(Seed, Data.Name);
            HashVector(Seed, Data.DiffuseColor);
            HashVector(Seed, Data.AmbientColor);
            HashVector(Seed, Data.SpecularColor);
            KeyHash::Combine(Seed, Data.Shininess);
            KeyHash::Combine(Seed, Data.Opacity);
            for (const auto &Ref : Data.TextureRefs)
            {
                KeyHash::CombineString(Seed, Ref.SlotName);
                KeyHash::CombineString(Seed, PathToKeyString(Ref.TexturePath));
            }
        }

        inline FMaterialIntermediateKey MakeIntermediateKey(const FIntermediateMtlData &Data)
        {
            size_t Seed = 0;
            HashMaterialData(Seed, Data);
            return {FinalizeHash(Seed)};
        }

        inline FMaterialIntermediateKey MakeIntermediateKey(const FIntermediateMtlLibraryData &Data)
        {
            size_t Seed = 0;
            KeyHash::CombineString(Seed, PathToKeyString(Data.SourcePath));
            for (const auto &Material : Data.Materials)
            {
                HashMaterialData(Seed, Material);
            }
            return {FinalizeHash(Seed)};
        }

        inline FStaticMeshIntermediateKey MakeIntermediateKey(const FIntermediateObjData &Data)
        {
            size_t Seed = 0;
            for (const auto &V : Data.Positions)
                HashVector(Seed, V);
            for (const auto &UV : Data.UVs)
                HashVector2(Seed, UV);
            for (const auto &Face : Data.Faces)
            {
                for (const auto &Vertex : Face.Vertices)
                {
                    KeyHash::Combine(Seed, Vertex.PositionIndex);
                    KeyHash::Combine(Seed, Vertex.UVIndex);
                }
            }
            return {FinalizeHash(Seed)};
        }

        inline FTextureCookedKey MakeCookedKey(const FTextureIntermediateKey &IntermediateKey,
                                               const FTextureBuildSettings   &Settings,
                                               uint32                         CookVersion = 1)
        {
            return {IntermediateKey, CookVersion, Settings.ToKeyString()};
        }

        inline FMaterialCookedKey MakeCookedKey(const FMaterialIntermediateKey &IntermediateKey,
                                                uint32                          CookVersion = 1,
                                                const FString &BuildKey = "MaterialLibraryCook")
        {
            return {IntermediateKey, CookVersion, BuildKey};
        }

        inline FStaticMeshCookedKey MakeCookedKey(const FStaticMeshIntermediateKey &IntermediateKey,
                                                  const FStaticMeshBuildSettings   &Settings,
                                                  uint32                            CookVersion = 1)
        {
            return {IntermediateKey, CookVersion, Settings.ToKeyString()};
        }
    } // namespace KeyUtils
} // namespace Asset
