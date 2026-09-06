#include "Engine/Asset/AssetObjectManager.h"
#include <cctype>
#include "Asset/Builder/MaterialBuilder.h"
#include "Asset/Manager/AssetCacheManager.h"
#include "Asset/Runtime/StaticMeshRenderResource.h"
#include "Asset/Serialization/CookedDataBinaryIO.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Misc/Paths.h"
#include "Core/Platform/PlatformTime.h"
#include "Engine/Asset/Material.h"
#include "Engine/Asset/StaticMesh.h"
#include "Engine/Asset/Texture.h"
#include "Engine/EngineStatics.h"

namespace
{
    double ToMilliseconds(double Seconds) { return Seconds * 1000.0; }

    bool IsTextureBakedPath(const std::filesystem::path &Path)
    {
        FString Extension = FPaths::Utf8FromPath(Path.extension());
        for (char &Ch : Extension)
        {
            Ch = static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
        }
        return Extension == ".texture";
    }


    template <typename TObjectType>
    TObjectType *LogAssetLoadResult(const char *AssetTypeLabel, const FString &AssetPath,
                                    double StartSeconds, TObjectType *Result,
                                    bool bWasCacheHit = false)
    {
        const double ElapsedMs = ToMilliseconds(FPlatformTime::Seconds() - StartSeconds);
        UEngineStatics::RecordResourceLoad(AssetTypeLabel, AssetPath, ElapsedMs, bWasCacheHit,
                                           Result != nullptr);

        if (Result != nullptr)
        {
            if (bWasCacheHit)
            {
                UE_LOG(AssetObject, ELogLevel::Info, "%s load served from cache: %s (%.3f ms)",
                       AssetTypeLabel, AssetPath.c_str(), ElapsedMs);
            }
            else
            {
                UE_LOG(AssetObject, ELogLevel::Info, "%s load succeeded: %s (%.3f ms)",
                       AssetTypeLabel, AssetPath.c_str(), ElapsedMs);
            }
        }
        else
        {
            UE_LOG(AssetObject, ELogLevel::Error, "%s load failed: %s (%.3f ms)", AssetTypeLabel,
                   AssetPath.c_str(), ElapsedMs);
        }
        return Result;
    }
} // namespace

UObject *FAssetObjectManager::LoadAssetObject(const FString &AssetPath)
{
    UE_LOG(AssetObject, ELogLevel::Debug, "LoadAssetObject requested: %s", AssetPath.c_str());
    return LoadStaticMeshObject(AssetPath);
}

UStaticMesh *FAssetObjectManager::LoadStaticMeshObject(const FString &InPath)
{
    const double StartSeconds = FPlatformTime::Seconds();

    if (!IsReady())
    {
        UE_LOG(AssetObject, ELogLevel::Error,
               "AssetObjectManager is not ready. path=%s cacheManager=%p device=%p", InPath.c_str(),
               AssetCacheManager, Device);
        return LogAssetLoadResult<UStaticMesh>("Static mesh asset", InPath, StartSeconds, nullptr);
    }

    if (InPath.empty())
    {
        UE_LOG(AssetObject, ELogLevel::Error, "Static mesh path is empty.");
        return LogAssetLoadResult<UStaticMesh>("Static mesh asset", InPath, StartSeconds, nullptr);
    }

    if (UStaticMesh *Existing = FindStaticMeshObject(InPath))
    {
        UE_LOG(AssetObject, ELogLevel::Verbose, "Static mesh cache hit (object index): %s",
               InPath.c_str());
        return LogAssetLoadResult<UStaticMesh>("Static mesh asset", InPath, StartSeconds, Existing,
                                               true);
    }

    auto CookedData = AssetCacheManager->BuildStaticMesh(InPath);
    if (!CookedData)
    {
        UE_LOG(AssetObject, ELogLevel::Error, "BuildStaticMesh failed: %s", InPath.c_str());
        return LogAssetLoadResult<UStaticMesh>("Static mesh asset", InPath, StartSeconds, nullptr);
    }

    UStaticMesh *Mesh = new UStaticMesh();
    Mesh->SetAssetPath(InPath);
    Mesh->BuildFromCookedData(*CookedData);
    Mesh->SetCookedData(CookedData);

    auto RenderResource = Asset::FStaticMeshRenderResource::Create(*CookedData, *Device);
    if (RenderResource == nullptr || !RenderResource->IsValid())
    {
        UE_LOG(AssetObject, ELogLevel::Error, "Static mesh render resource creation failed: %s",
               InPath.c_str());
        delete Mesh;
        return LogAssetLoadResult<UStaticMesh>("Static mesh asset", InPath, StartSeconds, nullptr);
    }
    Mesh->SetRenderResource(std::move(RenderResource));
    BindStaticMeshMaterialSlots(Mesh);

    RegisterStaticMeshObject(InPath, Mesh);
    UE_LOG(AssetObject, ELogLevel::Debug, "Static mesh cached in object index: %s", InPath.c_str());
    return LogAssetLoadResult<UStaticMesh>("Static mesh asset", InPath, StartSeconds, Mesh);
}

UStaticMesh *FAssetObjectManager::FindStaticMeshObject(const FString &InPath)
{
    auto It = AssetObjectIndex.find(InPath);
    if (It != AssetObjectIndex.end())
    {
        return dynamic_cast<UStaticMesh *>(It->second);
    }
    return nullptr;
}

UTexture *FAssetObjectManager::FindTextureObject(const FString &InPath)
{
    auto It = AssetObjectIndex.find(InPath);
    if (It != AssetObjectIndex.end())
    {
        return dynamic_cast<UTexture *>(It->second);
    }
    return nullptr;
}

UMaterial *FAssetObjectManager::FindMaterialObject(const FString &InPath)
{
    auto It = AssetObjectIndex.find(InPath);
    if (It != AssetObjectIndex.end())
    {
        return dynamic_cast<UMaterial *>(It->second);
    }
    return nullptr;
}

void FAssetObjectManager::RegisterStaticMeshObject(const FString &InPath, UStaticMesh *Mesh)
{
    AssetObjectIndex[InPath] = Mesh;
}

UTexture *FAssetObjectManager::LoadTextureObject(const FString &InPath)
{
    const double StartSeconds = FPlatformTime::Seconds();
    if (!IsReady())
    {
        UE_LOG(AssetObject, ELogLevel::Error,
               "Texture load requested while manager is not ready: %s", InPath.c_str());
        return LogAssetLoadResult<UTexture>("Texture asset", InPath, StartSeconds, nullptr);
    }

    if (InPath.empty())
    {
        UE_LOG(AssetObject, ELogLevel::Warning, "Texture asset path is empty.");
        return LogAssetLoadResult<UTexture>("Texture asset", InPath, StartSeconds, nullptr);
    }

    if (UTexture *Existing = FindTextureObject(InPath))
    {
        UE_LOG(AssetObject, ELogLevel::Verbose, "Texture cache hit: %s", InPath.c_str());
        return LogAssetLoadResult<UTexture>("Texture asset", InPath, StartSeconds, Existing, true);
    }

    std::shared_ptr<FTextureCookedData> CookedData;
    const std::filesystem::path         TexturePath = FPaths::PathFromUtf8(InPath);
    if (TexturePath.extension() == ".texture")
    {
        auto LoadedCookedData = std::make_shared<FTextureCookedData>();
        if (!Asset::Binary::LoadTexture(TexturePath, *LoadedCookedData) ||
            !LoadedCookedData->IsValid())
        {
            UE_LOG(AssetObject, ELogLevel::Error, "Baked texture load failed: %s", InPath.c_str());
            return LogAssetLoadResult<UTexture>("Texture asset", InPath, StartSeconds, nullptr);
        }
        CookedData = std::move(LoadedCookedData);
    }
    else
    {
        CookedData = AssetCacheManager->BuildTexture(InPath);
        if (CookedData == nullptr)
        {
            UE_LOG(AssetObject, ELogLevel::Error, "Texture build failed: %s", InPath.c_str());
            return LogAssetLoadResult<UTexture>("Texture asset", InPath, StartSeconds, nullptr);
        }
    }

    UTexture *Texture = new UTexture();
    if (!Texture->LoadFromCooked(InPath, CookedData, *Device))
    {
        UE_LOG(AssetObject, ELogLevel::Error, "Texture runtime object creation failed: %s",
               InPath.c_str());
        delete Texture;
        return LogAssetLoadResult<UTexture>("Texture asset", InPath, StartSeconds, nullptr);
    }

    RegisterTextureObject(InPath, Texture);
    UE_LOG(AssetObject, ELogLevel::Debug, "Texture loaded and registered: %s", InPath.c_str());
    return LogAssetLoadResult<UTexture>("Texture asset", InPath, StartSeconds, Texture);
}

UMaterial *FAssetObjectManager::LoadMaterialObject(const FString &InPath)
{
    const double StartSeconds = FPlatformTime::Seconds();
    if (!IsReady())
    {
        UE_LOG(AssetObject, ELogLevel::Error,
               "Material load requested while manager is not ready: %s", InPath.c_str());
        return LogAssetLoadResult<UMaterial>("Material asset", InPath, StartSeconds, nullptr);
    }

    if (InPath.empty())
    {
        UE_LOG(AssetObject, ELogLevel::Warning, "Material asset path is empty.");
        return LogAssetLoadResult<UMaterial>("Material asset", InPath, StartSeconds, nullptr);
    }

    if (UMaterial *Existing = FindMaterialObject(InPath))
    {
        UE_LOG(AssetObject, ELogLevel::Verbose, "Material cache hit: %s", InPath.c_str());
        return LogAssetLoadResult<UMaterial>("Material asset", InPath, StartSeconds, Existing,
                                             true);
    }

    std::shared_ptr<FMtlCookedData> CookedData = AssetCacheManager->BuildMaterial(InPath);
    if (CookedData == nullptr)
    {
        UE_LOG(AssetObject, ELogLevel::Error, "Material build failed: %s", InPath.c_str());
        return LogAssetLoadResult<UMaterial>("Material asset", InPath, StartSeconds, nullptr);
    }

    UMaterial *Material = new UMaterial();
    if (!Material->LoadFromCooked(InPath, CookedData, *Device))
    {
        UE_LOG(AssetObject, ELogLevel::Error, "Material runtime object creation failed: %s",
               InPath.c_str());
        delete Material;
        return LogAssetLoadResult<UMaterial>("Material asset", InPath, StartSeconds, nullptr);
    }

    RegisterMaterialObject(InPath, Material);
    UE_LOG(AssetObject, ELogLevel::Debug, "Material loaded and registered: %s", InPath.c_str());
    return LogAssetLoadResult<UMaterial>("Material asset", InPath, StartSeconds, Material);
}

void FAssetObjectManager::BindStaticMeshMaterialSlots(UStaticMesh *StaticMesh)
{
    if (StaticMesh == nullptr)
    {
        return;
    }

    const auto &CookedData = StaticMesh->GetCookedData();
    if (CookedData == nullptr)
    {
        UE_LOG(AssetObject, ELogLevel::Warning,
               "Static mesh has no cooked data for material binding: %s",
               StaticMesh->GetAssetPath().c_str());
        return;
    }

    auto &MaterialSlots = StaticMesh->GetMaterialSlots();
    if (MaterialSlots.size() != CookedData->Materials.size())
    {
        MaterialSlots.resize(CookedData->Materials.size(), nullptr);
    }

    UE_LOG(AssetObject, ELogLevel::Debug,
           "Binding material slots for static mesh: %s (slots=%zu, libraries=%zu)",
           StaticMesh->GetAssetPath().c_str(), MaterialSlots.size(),
           CookedData->MaterialLibraries.size());

    for (size_t SlotIndex = 0; SlotIndex < CookedData->Materials.size(); ++SlotIndex)
    {
        const Asset::FObjCookedMaterialRef &MaterialRef = CookedData->Materials[SlotIndex];
        if (MaterialRef.Name.empty() ||
            MaterialRef.LibraryIndex >= CookedData->MaterialLibraries.size())
        {
            UE_LOG(AssetObject, ELogLevel::Warning,
                   "Invalid material ref on mesh=%s slot=%zu name=%s libraryIndex=%u",
                   StaticMesh->GetAssetPath().c_str(), SlotIndex, MaterialRef.Name.c_str(),
                   MaterialRef.LibraryIndex);
            continue;
        }

        const std::filesystem::path &LibraryPath =
            CookedData->MaterialLibraries[MaterialRef.LibraryIndex];
        const FString MaterialAssetPath =
            Asset::FMaterialBuilder::MakeMaterialAssetPath(LibraryPath, MaterialRef.Name);
        UE_LOG(AssetObject, ELogLevel::Verbose, "Loading material for mesh=%s slot=%zu: %s",
               StaticMesh->GetAssetPath().c_str(), SlotIndex, MaterialAssetPath.c_str());

        UMaterial *Material = LoadMaterialObject(MaterialAssetPath);
        MaterialSlots[SlotIndex] = Material;

        if (Material == nullptr)
        {
            UE_LOG(AssetObject, ELogLevel::Warning, "Material load failed for mesh=%s slot=%zu: %s",
                   StaticMesh->GetAssetPath().c_str(), SlotIndex, MaterialAssetPath.c_str());
        }
        else
        {
            UE_LOG(AssetObject, ELogLevel::Debug, "Material slot bound for mesh=%s slot=%zu: %s",
                   StaticMesh->GetAssetPath().c_str(), SlotIndex, MaterialAssetPath.c_str());

            const auto &MaterialCookedData = Material->GetCookedData();
            if (MaterialCookedData != nullptr)
            {
                for (const Asset::FMtlTextureBinding &TextureBinding :
                     MaterialCookedData->TextureBindings)
                {
                    if (TextureBinding.TexturePath.empty())
                    {
                        continue;
                    }

                    const FString TexturePath = FPaths::Utf8FromPath(TextureBinding.TexturePath);
                    UE_LOG(AssetObject, ELogLevel::Verbose,
                           "Loading texture for material=%s usage=%d path=%s",
                           MaterialAssetPath.c_str(), static_cast<int>(TextureBinding.Slot),
                           TexturePath.c_str());

                    UTexture *TextureObject = LoadTextureObject(TexturePath);
                    Material->SetTextureForSlot(TextureBinding.Slot, TextureObject);
                }

                if (!Material->RebuildRenderResource(*Device))
                {
                    UE_LOG(AssetObject, ELogLevel::Warning,
                           "Material render resource rebuild failed after texture binding: %s",
                           MaterialAssetPath.c_str());
                }
            }
        }
    }
}

void FAssetObjectManager::RegisterTextureObject(const FString &InPath, UTexture *Texture)
{
    AssetObjectIndex[InPath] = Texture;
}

void FAssetObjectManager::RegisterMaterialObject(const FString &InPath, UMaterial *Material)
{
    AssetObjectIndex[InPath] = Material;
}
