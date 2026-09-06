#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Core/ResourceTypes.h"
#include "Resource/IAssetLoader.h"

#include <map>

class FSpriteVfxManager : public TSingleton<FSpriteVfxManager>, public IAssetLoader<FSpriteVfxResource>
{
    friend class TSingleton<FSpriteVfxManager>;

    TMap<FString, FSpriteVfxResource> SpriteVfxResources;
    ID3D11Device* Device = nullptr;

public:
    virtual ~FSpriteVfxManager();

    // IAssetLoader Implementation
    virtual void SetDevice(ID3D11Device* InDevice) override;
    virtual FSpriteVfxResource* Load(const FString& Path) override;
    virtual FSpriteVfxResource* Find(const FString& Key) override;
    virtual void Unload(const FString& Key) override;
    virtual void UnloadAll() override;

    /**
     * @brief Scans for .spritevfx JSON files in Asset/Content/
     */
    void ScanAssets();

    FSpriteVfxResource* FindSpriteVfx(const FName& SpriteVfxName);
    const FSpriteVfxResource* FindSpriteVfx(const FName& SpriteVfxName) const;

    TArray<FString> GetSpriteVfxNames() const;

    void ReleaseGPUResources();

    /**
     * @brief Loads GPU resources for all registered SpriteVfx.
     */
    bool LoadGPUResources(ID3D11Device* InDevice);

private:
    FSpriteVfxManager() = default;
};
