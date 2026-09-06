#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Core/ResourceTypes.h"
#include "Resource/IAssetLoader.h"

#include <map>

class FTextureManager : public TSingleton<FTextureManager>, public IAssetLoader<FTextureResource>
{
    friend class TSingleton<FTextureManager>;

    TMap<FString, FTextureResource> TextureResources;
    ID3D11Device* Device = nullptr;

public:
    virtual ~FTextureManager();

    // IAssetLoader Implementation
    virtual void SetDevice(ID3D11Device* InDevice) override;
    virtual FTextureResource* Load(const FString& Path) override;
    virtual FTextureResource* Find(const FString& Key) override;
    virtual void Unload(const FString& Key) override;
    virtual void UnloadAll() override;

    /**
     * @brief Scans for texture files (.png, .jpg, .jpeg, .dds, .bmp, .tga) in Asset/Content/
     */
    void ScanAssets();

    FTextureResource* FindTexture(const FName& TextureName);
    const FTextureResource* FindTexture(const FName& TextureName) const;

    TArray<FString> GetTextureNames() const;
    TArray<FString> GetEditorTextureNames() const;

    void ReleaseGPUResources();

private:
    FTextureManager() = default;
};
