#include "Asset/Serialization/CookedDataSerialization.h"

namespace Asset
{
    FArchive &operator<<(FArchive &Ar, FVector2 &Value)
    {
        Ar << Value.X;
        Ar << Value.Y;
        return Ar;
    }

    FArchive &operator<<(FArchive &Ar, FVector3 &Value)
    {
        Ar << Value.X;
        Ar << Value.Y;
        Ar << Value.Z;
        return Ar;
    }

    FArchive &operator<<(FArchive &Ar, FColor &Value)
    {
        Ar << Value.r;
        Ar << Value.g;
        Ar << Value.b;
        Ar << Value.a;
        return Ar;
    }

    FArchive &operator<<(FArchive &Ar, FStaticMeshSectionData &Value)
    {
        Ar << Value.StartIndex;
        Ar << Value.IndexCount;
        Ar << Value.MaterialIndex;
        return Ar;
    }

    FArchive &operator<<(FArchive &Ar, FObjCookedMaterialRef &Value)
    {
        Ar << Value.Name;
        Ar << Value.LibraryIndex;
        return Ar;
    }

    FArchive &operator<<(FArchive &Ar, FObjCookedData &Value)
    {
        Ar << Value.SourcePath;
        Ar << Value.VertexData;
        Ar << Value.VertexStride;
        Ar << Value.VertexCount;
        Ar << Value.Indices;
        Ar << Value.Sections;
        Ar << Value.MaterialLibraries;
        Ar << Value.Materials;
        Ar << Value.bHasNormals;
        Ar << Value.bHasColors;
        Ar << Value.bHasUVs;
        return Ar;
    }

    FArchive &operator<<(FArchive &Ar, FMtlTextureBinding &Value)
    {
        Ar << Value.Slot;
        Ar << Value.TexturePath;
        return Ar;
    }

    FArchive &operator<<(FArchive &Ar, FMtlCookedData &Value)
    {
        Ar << Value.SourcePath;
        Ar << Value.Name;
        Ar << Value.DiffuseColor;
        Ar << Value.AmbientColor;
        Ar << Value.SpecularColor;
        Ar << Value.Shininess;
        Ar << Value.Opacity;
        Ar << Value.TextureBindings;
        return Ar;
    }

    FArchive &operator<<(FArchive &Ar, FMtlCookedLibraryData &Value)
    {
        Ar << Value.SourcePath;
        Ar << Value.Materials;

        if (Ar.IsLoading())
        {
            Value.NameToIndex.clear();
            for (uint32 Index = 0; Index < static_cast<uint32>(Value.Materials.size()); ++Index)
            {
                Value.NameToIndex.emplace(Value.Materials[Index].Name, Index);
            }
        }

        return Ar;
    }

    FArchive &operator<<(FArchive &Ar, FTextureMipLevel &Value)
    {
        Ar << Value.Width;
        Ar << Value.Height;
        Ar << Value.Pixels;
        return Ar;
    }

    FArchive &operator<<(FArchive &Ar, FTextureCookedData &Value)
    {
        Ar << Value.SourcePath;
        Ar << Value.Width;
        Ar << Value.Height;
        Ar << Value.Channels;
        Ar << Value.bSRGB;
        Ar << Value.Pixels;
        Ar << Value.MipChain;
        return Ar;
    }
} // namespace Asset
