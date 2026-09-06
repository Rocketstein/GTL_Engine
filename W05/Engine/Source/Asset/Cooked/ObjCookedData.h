#pragma once

#include "Asset/Core/StaticMeshTypes.h"
#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Core/Platform/PlatformTypes.h"
#include <filesystem>

namespace Asset
{
    struct FObjCookedMaterialRef
    {
        FString Name;
        uint32  LibraryIndex = 0;

        bool IsValid() const { return !Name.empty(); }

        void Reset()
        {
            Name.clear();
            LibraryIndex = 0;
        }
    };

    struct FObjCookedData
    {
        std::filesystem::path          SourcePath;
        TArray<uint8>                  VertexData;
        uint32                         VertexStride = 0;
        uint32                         VertexCount = 0;
        TArray<uint32>                 Indices;
        TArray<FStaticMeshSectionData> Sections;
        TArray<std::filesystem::path>  MaterialLibraries;
        TArray<FObjCookedMaterialRef>  Materials;
        bool                           bHasNormals = false;
        bool                           bHasColors = false;
        bool                           bHasUVs = false;

        bool IsValid() const
        {
            if (VertexData.empty() || VertexStride == 0 || VertexCount == 0 || Indices.empty())
            {
                return false;
            }

            for (const FStaticMeshSectionData &Section : Sections)
            {
                if (Section.IndexCount == 0)
                {
                    return false;
                }

                if (Section.StartIndex + Section.IndexCount > Indices.size())
                {
                    return false;
                }

                if (!Materials.empty() && Section.MaterialIndex >= Materials.size())
                {
                    return false;
                }
            }

            for (const FObjCookedMaterialRef &Material : Materials)
            {
                if (!Material.IsValid())
                {
                    return false;
                }

                if (Material.LibraryIndex >= MaterialLibraries.size())
                {
                    return false;
                }
            }

            return true;
        }

        void Reset()
        {
            SourcePath.clear();
            VertexData.clear();
            VertexStride = 0;
            VertexCount = 0;
            Indices.clear();
            Sections.clear();
            MaterialLibraries.clear();
            Materials.clear();
            bHasNormals = false;
            bHasColors = false;
            bHasUVs = false;
        }
    };
} // namespace Asset
