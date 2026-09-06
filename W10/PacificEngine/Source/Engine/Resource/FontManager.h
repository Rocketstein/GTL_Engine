#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Core/ResourceTypes.h"
#include "Resource/IAssetLoader.h"

#include <map>

class FFontManager : public TSingleton<FFontManager>, public IAssetLoader<FFontResource>
{
    friend class TSingleton<FFontManager>;

    TMap<FString, FFontResource> FontResources;
    ID3D11Device* Device = nullptr;

public:
    virtual ~FFontManager();

    // IAssetLoader Implementation
    virtual void SetDevice(ID3D11Device* InDevice) override;
    virtual FFontResource* Load(const FString& Path) override;
    virtual FFontResource* Find(const FString& Key) override;
    virtual void Unload(const FString& Key) override;
    virtual void UnloadAll() override;

    /**
     * @brief Scans for .font JSON files in Asset/Content/
     */
    void ScanAssets();

    FFontResource* FindFont(const FName& FontName);
    const FFontResource* FindFont(const FName& FontName) const;

    TArray<FString> GetFontNames() const;

    void ReleaseGPUResources();

    /**
     * @brief Loads GPU resources for all registered fonts.
     */
    bool LoadGPUResources(ID3D11Device* InDevice);

private:
    FFontManager() = default;
};
