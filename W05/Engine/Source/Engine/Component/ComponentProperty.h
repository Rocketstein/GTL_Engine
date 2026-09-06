#pragma once

#include "Asset/Core/AssetNaming.h"
#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Core/Math/Color.h"
#include "Core/Math/Vector3.h"
#include "Core/Platform/PlatformTypes.h"
#include <functional>

// Editor property용
// WEEK05에서 사용하지 않음

enum class EComponentAssetPathKind : uint8
{
    Any,
    StaticMeshFile,
    MaterialFile,
    TextureImage,
    SceneFile
};

inline bool MatchesExpectedAssetPathKind(const FString &Path, EComponentAssetPathKind Kind)
{
    switch (Kind)
    {
    case EComponentAssetPathKind::Any:
        return Asset::ClassifyAssetPath(Path) != Asset::EAssetFileKind::Unknown;
    case EComponentAssetPathKind::StaticMeshFile:
        return Asset::ClassifyAssetPath(Path) == Asset::EAssetFileKind::StaticMesh;
    case EComponentAssetPathKind::MaterialFile:
        return Asset::ClassifyAssetPath(Path) == Asset::EAssetFileKind::MaterialLibrary;
    case EComponentAssetPathKind::TextureImage:
        return Asset::ClassifyAssetPath(Path) == Asset::EAssetFileKind::Texture;
    case EComponentAssetPathKind::SceneFile:
        return Asset::ClassifyAssetPath(Path) == Asset::EAssetFileKind::Scene;
    default:
        return false;
    }
}

enum class EComponentPropertyType : uint8
{
    Bool,
    Int,
    Float,
    String,
    Vector3,
    Color,
    AssetPath
};

struct FComponentPropertyOptions
{
    bool                    bExposeInDetails = true;
    bool                    bSerializeInScene = true;
    float                   DragSpeed = 0.1f;
    EComponentAssetPathKind ExpectedAssetPathKind = EComponentAssetPathKind::Any;
};

struct FComponentPropertyDescriptor
{
    FString                 Key;
    FWString                DisplayLabel;
    EComponentPropertyType  Type = EComponentPropertyType::String;
    bool                    bExposeInDetails = true;
    bool                    bSerializeInScene = true;
    float                   DragSpeed = 0.1f;
    EComponentAssetPathKind ExpectedAssetPathKind = EComponentAssetPathKind::Any;

    std::function<bool()>     BoolGetter;
    std::function<void(bool)> BoolSetter;

    std::function<int32()>     IntGetter;
    std::function<void(int32)> IntSetter;

    std::function<float()>     FloatGetter;
    std::function<void(float)> FloatSetter;

    std::function<FString()>             StringGetter;
    std::function<void(const FString &)> StringSetter;

    std::function<FVector3()>             VectorGetter;
    std::function<void(const FVector3 &)> VectorSetter;

    std::function<FColor()>             ColorGetter;
    std::function<void(const FColor &)> ColorSetter;
};

class FComponentPropertyBuilder
{
  public:
    const TArray<FComponentPropertyDescriptor> &GetProperties() const { return Properties; }
    void                                        Clear() { Properties.clear(); }

    void AddBool(const FString &Key, const FWString &DisplayLabel, std::function<bool()> Getter,
                 std::function<void(bool)> Setter, const FComponentPropertyOptions &Options = {})
    {
        FComponentPropertyDescriptor Descriptor;
        Descriptor.Key = Key;
        Descriptor.DisplayLabel = DisplayLabel;
        Descriptor.Type = EComponentPropertyType::Bool;
        Descriptor.bExposeInDetails = Options.bExposeInDetails;
        Descriptor.bSerializeInScene = Options.bSerializeInScene;
        Descriptor.DragSpeed = Options.DragSpeed;
        Descriptor.BoolGetter = std::move(Getter);
        Descriptor.BoolSetter = std::move(Setter);
        Properties.push_back(std::move(Descriptor));
    }

    void AddInt(const FString &Key, const FWString &DisplayLabel, std::function<int32()> Getter,
                std::function<void(int32)> Setter, const FComponentPropertyOptions &Options = {})
    {
        FComponentPropertyDescriptor Descriptor;
        Descriptor.Key = Key;
        Descriptor.DisplayLabel = DisplayLabel;
        Descriptor.Type = EComponentPropertyType::Int;
        Descriptor.bExposeInDetails = Options.bExposeInDetails;
        Descriptor.bSerializeInScene = Options.bSerializeInScene;
        Descriptor.DragSpeed = Options.DragSpeed;
        Descriptor.IntGetter = std::move(Getter);
        Descriptor.IntSetter = std::move(Setter);
        Properties.push_back(std::move(Descriptor));
    }

    void AddFloat(const FString &Key, const FWString &DisplayLabel, std::function<float()> Getter,
                  std::function<void(float)> Setter, const FComponentPropertyOptions &Options = {})
    {
        FComponentPropertyDescriptor Descriptor;
        Descriptor.Key = Key;
        Descriptor.DisplayLabel = DisplayLabel;
        Descriptor.Type = EComponentPropertyType::Float;
        Descriptor.bExposeInDetails = Options.bExposeInDetails;
        Descriptor.bSerializeInScene = Options.bSerializeInScene;
        Descriptor.DragSpeed = Options.DragSpeed;
        Descriptor.FloatGetter = std::move(Getter);
        Descriptor.FloatSetter = std::move(Setter);
        Properties.push_back(std::move(Descriptor));
    }

    void AddString(const FString &Key, const FWString &DisplayLabel,
                   std::function<FString()> Getter, std::function<void(const FString &)> Setter,
                   const FComponentPropertyOptions &Options = {})
    {
        FComponentPropertyDescriptor Descriptor;
        Descriptor.Key = Key;
        Descriptor.DisplayLabel = DisplayLabel;
        Descriptor.Type = EComponentPropertyType::String;
        Descriptor.bExposeInDetails = Options.bExposeInDetails;
        Descriptor.bSerializeInScene = Options.bSerializeInScene;
        Descriptor.DragSpeed = Options.DragSpeed;
        Descriptor.StringGetter = std::move(Getter);
        Descriptor.StringSetter = std::move(Setter);
        Properties.push_back(std::move(Descriptor));
    }

    void AddVector3(const FString &Key, const FWString &DisplayLabel,
                    std::function<FVector3()> Getter, std::function<void(const FVector3 &)> Setter,
                    const FComponentPropertyOptions &Options = {})
    {
        FComponentPropertyDescriptor Descriptor;
        Descriptor.Key = Key;
        Descriptor.DisplayLabel = DisplayLabel;
        Descriptor.Type = EComponentPropertyType::Vector3;
        Descriptor.bExposeInDetails = Options.bExposeInDetails;
        Descriptor.bSerializeInScene = Options.bSerializeInScene;
        Descriptor.DragSpeed = Options.DragSpeed;
        Descriptor.VectorGetter = std::move(Getter);
        Descriptor.VectorSetter = std::move(Setter);
        Properties.push_back(std::move(Descriptor));
    }

    void AddColor(const FString &Key, const FWString &DisplayLabel, std::function<FColor()> Getter,
                  std::function<void(const FColor &)> Setter,
                  const FComponentPropertyOptions    &Options = {})
    {
        FComponentPropertyDescriptor Descriptor;
        Descriptor.Key = Key;
        Descriptor.DisplayLabel = DisplayLabel;
        Descriptor.Type = EComponentPropertyType::Color;
        Descriptor.bExposeInDetails = Options.bExposeInDetails;
        Descriptor.bSerializeInScene = Options.bSerializeInScene;
        Descriptor.DragSpeed = Options.DragSpeed;
        Descriptor.ColorGetter = std::move(Getter);
        Descriptor.ColorSetter = std::move(Setter);
        Properties.push_back(std::move(Descriptor));
    }

    void AddAssetPath(const FString &Key, const FWString &DisplayLabel,
                      std::function<FString()> Getter, std::function<void(const FString &)> Setter,
                      const FComponentPropertyOptions &Options = {})
    {
        FComponentPropertyDescriptor Descriptor;
        Descriptor.Key = Key;
        Descriptor.DisplayLabel = DisplayLabel;
        Descriptor.Type = EComponentPropertyType::AssetPath;
        Descriptor.bExposeInDetails = Options.bExposeInDetails;
        Descriptor.bSerializeInScene = Options.bSerializeInScene;
        Descriptor.DragSpeed = Options.DragSpeed;
        Descriptor.ExpectedAssetPathKind = Options.ExpectedAssetPathKind;
        Descriptor.StringGetter = std::move(Getter);
        Descriptor.StringSetter = std::move(Setter);
        Properties.push_back(std::move(Descriptor));
    }

  private:
    TArray<FComponentPropertyDescriptor> Properties;
};
