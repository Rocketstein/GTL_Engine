#pragma once

#include "Asset/Core/MaterialTypes.h"
#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/String.h"
#include "Core/Math/Vector3.h"
#include "Core/Platform/PlatformTypes.h"
#include <filesystem>


namespace Asset
{
    struct FMtlTextureBinding
    {
        EMaterialTextureSlot  Slot = EMaterialTextureSlot::Diffuse;
        std::filesystem::path TexturePath;
    };

    struct FMtlCookedData
    {
        std::filesystem::path      SourcePath;
        FString                    Name;
        FVector3                   DiffuseColor = FVector3(1.0f, 1.0f, 1.0f);
        FVector3                   AmbientColor = FVector3(0.0f, 0.0f, 0.0f);
        FVector3                   SpecularColor = FVector3(0.0f, 0.0f, 0.0f);
        float                      Shininess = 0.0f;
        float                      Opacity = 1.0f;
        TArray<FMtlTextureBinding> TextureBindings;

        bool IsValid() const { return !Name.empty(); }

        void Reset()
        {
            SourcePath.clear();
            Name.clear();
            DiffuseColor = FVector3(1.0f, 1.0f, 1.0f);
            AmbientColor = FVector3(0.0f, 0.0f, 0.0f);
            SpecularColor = FVector3(0.0f, 0.0f, 0.0f);
            Shininess = 0.0f;
            Opacity = 1.0f;
            TextureBindings.clear();
        }
    };

    struct FMtlCookedLibraryData
    {
        std::filesystem::path  SourcePath;
        TArray<FMtlCookedData> Materials;
        TMap<FString, uint32>  NameToIndex;

        bool IsValid() const { return !Materials.empty(); }

        void Reset()
        {
            SourcePath.clear();
            Materials.clear();
            NameToIndex.clear();
        }

        const FMtlCookedData *FindMaterial(const FString &InName) const
        {
            auto It = NameToIndex.find(InName);
            if (It == NameToIndex.end() || It->second >= Materials.size())
            {
                return nullptr;
            }
            return &Materials[It->second];
        }
    };
} // namespace Asset
