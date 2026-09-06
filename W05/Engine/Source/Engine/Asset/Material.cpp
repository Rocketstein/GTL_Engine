#include "Engine/Asset/Material.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Misc/Paths.h"
#include "Engine/Asset/Texture.h"
#include <filesystem>


REGISTER_CLASS(, UMaterial)

namespace
{
    static FString BuildAssetNameFromPath(const FString                         &InAssetPath,
                                          const std::shared_ptr<FMtlCookedData> &InCookedData)
    {
        if (InCookedData != nullptr && !InCookedData->Name.empty())
        {
            return InCookedData->Name;
        }

        if (InAssetPath.empty())
        {
            return {};
        }

        const std::filesystem::path FilePath = FPaths::PathFromUtf8(InAssetPath);
        return FPaths::Utf8FromPath(FilePath.stem());
    }
} // namespace

bool UMaterial::LoadFromCooked(const FString                   &InAssetPath,
                               std::shared_ptr<FMtlCookedData> InCookedData,
                               FD3D11Device                    &InDevice)
{
    if (InCookedData == nullptr || !InCookedData->IsValid())
    {
        UE_LOG(MaterialAsset, ELogLevel::Error, "Material asset load failed: %s",
               InAssetPath.c_str());
        return false;
    }

    if (CookedData == InCookedData && GetAssetPath() == InAssetPath && RenderResource != nullptr)
    {
        SetLoaded(true);
        return true;
    }

    SetAssetPath(InAssetPath);
    SetAssetName(BuildAssetNameFromPath(InAssetPath, InCookedData));
    SetCookedData(std::move(InCookedData));

    if (!RebuildRenderResource(InDevice))
    {
        UE_LOG(MaterialAsset, ELogLevel::Error, "Material asset load failed: %s",
               InAssetPath.c_str());
        return false;
    }

    SetLoaded(true);
    UE_LOG(MaterialAsset, ELogLevel::Debug, "Material asset load succeeded: %s",
           InAssetPath.c_str());
    return true;
}

bool UMaterial::RebuildRenderResource(FD3D11Device &InDevice)
{
    if (CookedData == nullptr || !CookedData->IsValid())
    {
        return false;
    }

    std::shared_ptr<FMaterialRenderResource> NewRenderResource = FMaterialRenderResource::Create(
        *CookedData, BaseColorTexture, NormalTexture, ORMTexture, InDevice);
    if (NewRenderResource == nullptr)
    {
        return false;
    }

    SetRenderResource(std::move(NewRenderResource));
    return true;
}

void UMaterial::SetTextureForSlot(EMaterialTextureSlot Slot, UTexture *InTexture)
{
    switch (Slot)
    {
    case EMaterialTextureSlot::Diffuse:
        BaseColorTexture = InTexture;
        break;
    case EMaterialTextureSlot::Normal:
        NormalTexture = InTexture;
        break;
    case EMaterialTextureSlot::Specular:
        ORMTexture = InTexture;
        break;
    default:
        break;
    }
}

UTexture *UMaterial::GetTextureForSlot(EMaterialTextureSlot Slot) const
{
    switch (Slot)
    {
    case EMaterialTextureSlot::Diffuse:
        return BaseColorTexture;
    case EMaterialTextureSlot::Normal:
        return NormalTexture;
    case EMaterialTextureSlot::Specular:
        return ORMTexture;
    default:
        return nullptr;
    }
}

bool UMaterial::IsValidLowLevel() const
{
    if (CookedData == nullptr)
    {
        return false;
    }

    if (!CookedData->IsValid())
    {
        return false;
    }

    if (RenderResource == nullptr)
    {
        return false;
    }

    return true;
}
