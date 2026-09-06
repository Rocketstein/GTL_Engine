#pragma once

#include "Asset/Cooked/MtlCookedData.h"
#include "Asset/Cooked/ObjCookedData.h"
#include "Asset/Cooked/TextureCookedData.h"
#include "Asset/Serialization/Archive.h"
#include "Core/Math/Color.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"

namespace Asset
{
    FArchive &operator<<(FArchive &Ar, FVector2 &Value);
    FArchive &operator<<(FArchive &Ar, FVector3 &Value);
    FArchive &operator<<(FArchive &Ar, FColor &Value);

    FArchive &operator<<(FArchive &Ar, FTextureMipLevel &Value);
    FArchive &operator<<(FArchive &Ar, FTextureCookedData &Value);
    FArchive &operator<<(FArchive &Ar, FMtlTextureBinding &Value);
    FArchive &operator<<(FArchive &Ar, FMtlCookedData &Value);
    FArchive &operator<<(FArchive &Ar, FMtlCookedLibraryData &Value);
    FArchive &operator<<(FArchive &Ar, FStaticMeshSectionData &Value);
    FArchive &operator<<(FArchive &Ar, FObjCookedMaterialRef &Value);
    FArchive &operator<<(FArchive &Ar, FObjCookedData &Value);
} // namespace Asset
