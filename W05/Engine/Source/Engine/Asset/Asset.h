#pragma once

#include "Core/Containers/String.h"
#include "CoreUObject/Object.h"

class UAsset : public UObject
{
    DECLARE_RTTI(UAsset, UObject)

  public:
    virtual ~UAsset() = default;

    const FString &GetAssetPath() const { return AssetPath; }

    void SetAssetPath(const FString &InAssetPath) { AssetPath = InAssetPath; }

    const FString &GetAssetName() const { return AssetName; }

    void SetAssetName(const FString &InAssetName) { AssetName = InAssetName; }

    bool IsLoaded() const { return bLoaded; }

    void SetLoaded(bool bInLoaded) { bLoaded = bInLoaded; }

    virtual void ResetAsset() { bLoaded = false; }

    virtual bool IsValidLowLevel() const { return !AssetPath.empty(); }

    FString GetStatResourceKey() const override { return AssetPath; }

  private:
    FString AssetPath;
    FString AssetName;
    bool    bLoaded = false;
};
