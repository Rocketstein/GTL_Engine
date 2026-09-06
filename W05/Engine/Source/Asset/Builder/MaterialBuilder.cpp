#include "Asset/Builder/MaterialBuilder.h"
#include "Asset/Builder/TextureBuilder.h"
#include "Asset/Cache/AssetKeyUtils.h"
#include "Asset/Core/AssetNaming.h"
#include "Asset/Serialization/CookedDataBinaryIO.h"
#include "Core/Math/Vector3.h"
#include "Core/Misc/Paths.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Asset
{
    namespace
    {
        static bool TryParseFloat3(std::istringstream &Iss, FVector3 &OutValue)
        {
            float X = 0.0f;
            float Y = 0.0f;
            float Z = 0.0f;
            if (!(Iss >> X >> Y >> Z))
            {
                return false;
            }

            OutValue = FVector3(X, Y, Z);
            return true;
        }

        static FIntermediateMtlData *EnsureCurrentMaterial(FIntermediateMtlLibraryData &Library,
                                                           const FString &FallbackName)
        {
            if (Library.Materials.empty())
            {
                FIntermediateMtlData Material;
                Material.Name = FallbackName;
                const uint32 MaterialIndex = static_cast<uint32>(Library.Materials.size());
                Library.Materials.push_back(std::move(Material));
                Library.NameToIndex[Library.Materials.back().Name] = MaterialIndex;
            }

            return &Library.Materials.back();
        }
    } // namespace

    std::shared_ptr<FMtlCookedLibraryData>
    FMaterialBuilder::BuildLibrary(const std::filesystem::path &Path)
    {
        LastBuildReport.Reset();
        const FSourceRecord *Source = Cache.GetSource(FMaterialAssetTag{}, Path);
        if (Source == nullptr)
        {
            return nullptr;
        }

        const FString BakedLibraryPath =
            MakeBakedAssetPath(FPaths::Utf8FromPath(Source->NormalizedPath));
        if (!BakedLibraryPath.empty())
        {
            FMtlCookedLibraryData BakedData;
            if (Binary::LoadMaterialLibraryIfCurrent(BakedLibraryPath, *Source, BakedData) &&
                BakedData.IsValid())
            {
                LastBuildReport.bUsedCachedCooked = true;
                LastBuildReport.ResultSource = EAssetBuildResultSource::CookedCache;
                return std::make_shared<FMtlCookedLibraryData>(std::move(BakedData));
            }
        }

        // Week4 lightweight path:
        // material은 baked library 우선 -> 없으면 parse 후 바로 cooked 생성.
        // intermediate cache는 제거해서 흐름을 단순화한다.
        std::shared_ptr<FIntermediateMtlLibraryData> Intermediate = ParseMaterialLibrary(*Source);
        if (!Intermediate)
        {
            return nullptr;
        }

        const FMaterialIntermediateKey IntermediateKey =
            KeyUtils::MakeIntermediateKey(*Intermediate);
        const FMaterialCookedKey CookedKey = KeyUtils::MakeCookedKey(IntermediateKey);

        auto &CookedCache = Cache.GetCookedCache(FMaterialAssetTag{});
        std::shared_ptr<FMtlCookedLibraryData> Cooked = CookedCache.Find(CookedKey);
        if (!Cooked)
        {
            LastBuildReport.bBuiltNewCooked = true;
            Cooked = CookMaterialLibrary(*Source, *Intermediate);
            if (!Cooked)
            {
                return nullptr;
            }
            CookedCache.Insert(CookedKey, Cooked);
        }
        else
        {
            LastBuildReport.bUsedCachedCooked = true;
        }

        if (LastBuildReport.bUsedCachedCooked)
        {
            LastBuildReport.ResultSource = EAssetBuildResultSource::CookedCache;
        }
        else if (Cooked)
        {
            LastBuildReport.ResultSource = EAssetBuildResultSource::BuiltFromFreshIntermediate;
        }

        return Cooked;
    }

    std::shared_ptr<FMtlCookedData>
    FMaterialBuilder::BuildMaterial(const std::filesystem::path &Path, const FString &MaterialName)
    {
        std::shared_ptr<FMtlCookedLibraryData> Library = BuildLibrary(Path);
        if (Library == nullptr || Library->Materials.empty())
        {
            return nullptr;
        }

        if (MaterialName.empty())
        {
            return std::make_shared<FMtlCookedData>(Library->Materials.front());
        }

        const FMtlCookedData *FoundMaterial = Library->FindMaterial(MaterialName);
        if (FoundMaterial == nullptr)
        {
            return nullptr;
        }

        return std::make_shared<FMtlCookedData>(*FoundMaterial);
    }

    FString FMaterialBuilder::MakeMaterialAssetPath(const std::filesystem::path &LibraryPath,
                                                    const FString               &MaterialName)
    {
        if (MaterialName.empty())
        {
            return FPaths::Utf8FromPath(LibraryPath);
        }

        return FPaths::Utf8FromPath(LibraryPath) + "::" + MaterialName;
    }

    bool FMaterialBuilder::SplitMaterialAssetPath(const FString &InAssetPath,
                                                  FString &OutLibraryPath, FString &OutMaterialName)
    {
        OutLibraryPath = InAssetPath;
        OutMaterialName.clear();

        const size_t SeparatorPos = InAssetPath.rfind("::");
        if (SeparatorPos == FString::npos)
        {
            return false;
        }

        OutLibraryPath = InAssetPath.substr(0, SeparatorPos);
        OutMaterialName = InAssetPath.substr(SeparatorPos + 2);
        return true;
    }

    std::shared_ptr<FIntermediateMtlLibraryData>
    FMaterialBuilder::ParseMaterialLibrary(const FSourceRecord &Source)
    {
        FString Text;
        if (!ReadAllText(Source.NormalizedPath, Text))
        {
            return nullptr;
        }

        auto Result = std::make_shared<FIntermediateMtlLibraryData>();
        Result->SourcePath = Source.NormalizedPath;

        const FString DefaultName = FPaths::Utf8FromPath(Source.NormalizedPath.stem());

        std::istringstream Stream(Text);
        FString            Line;
        while (std::getline(Stream, Line))
        {
            Line = Trim(Line);
            if (Line.empty() || Line[0] == '#')
            {
                continue;
            }

            std::istringstream Iss(Line);
            FString            Tag;
            Iss >> Tag;

            if (Tag == "newmtl")
            {
                FString Name;
                Iss >> Name;
                if (Name.empty())
                {
                    continue;
                }

                auto Existing = Result->NameToIndex.find(Name);
                if (Existing != Result->NameToIndex.end())
                {
                    continue;
                }

                FIntermediateMtlData Material;
                Material.Name = Name;
                const uint32 MaterialIndex = static_cast<uint32>(Result->Materials.size());
                Result->Materials.push_back(std::move(Material));
                Result->NameToIndex[Name] = MaterialIndex;
                continue;
            }

            FIntermediateMtlData *CurrentMaterial = EnsureCurrentMaterial(*Result, DefaultName);
            if (CurrentMaterial == nullptr)
            {
                continue;
            }

            if (Tag == "Kd")
            {
                TryParseFloat3(Iss, CurrentMaterial->DiffuseColor);
            }
            else if (Tag == "Ka")
            {
                TryParseFloat3(Iss, CurrentMaterial->AmbientColor);
            }
            else if (Tag == "Ks")
            {
                TryParseFloat3(Iss, CurrentMaterial->SpecularColor);
            }
            else if (Tag == "Ns")
            {
                Iss >> CurrentMaterial->Shininess;
            }
            else if (Tag == "d")
            {
                Iss >> CurrentMaterial->Opacity;
            }
            else if (Tag == "Tr")
            {
                float Transparency = 0.0f;
                if (Iss >> Transparency)
                {
                    CurrentMaterial->Opacity = 1.0f - Transparency;
                }
            }
            else if (Tag == "map_Kd" || Tag == "map_Bump" || Tag == "bump" || Tag == "map_Ks" ||
                     Tag == "map_d" || Tag == "map_Ke")
            {
                FString TexturePath;
                std::getline(Iss, TexturePath);
                TexturePath = Trim(TexturePath);
                if (!TexturePath.empty())
                {
                    CurrentMaterial->TextureRefs.push_back({Tag, TexturePath});
                }
            }
        }

        return Result->IsValid() ? Result : nullptr;
    }

    std::shared_ptr<FMtlCookedLibraryData>
    FMaterialBuilder::CookMaterialLibrary(const FSourceRecord               &Source,
                                          const FIntermediateMtlLibraryData &Intermediate)
    {
        auto Result = std::make_shared<FMtlCookedLibraryData>();
        Result->SourcePath = Source.NormalizedPath;

        for (const FIntermediateMtlData &MaterialIntermediate : Intermediate.Materials)
        {
            FMtlCookedData Material;
            Material.SourcePath = Result->SourcePath;
            Material.Name = MaterialIntermediate.Name.empty()
                                ? FPaths::Utf8FromPath(Source.NormalizedPath.stem())
                                : MaterialIntermediate.Name;
            Material.DiffuseColor = MaterialIntermediate.DiffuseColor;
            Material.AmbientColor = MaterialIntermediate.AmbientColor;
            Material.SpecularColor = MaterialIntermediate.SpecularColor;
            Material.Shininess = MaterialIntermediate.Shininess;
            Material.Opacity = MaterialIntermediate.Opacity;

            for (const FIntermediateMtlTextureRef &TextureRef : MaterialIntermediate.TextureRefs)
            {
                const EMaterialTextureSlot TextureSlot = ResolveTextureSlot(TextureRef.SlotName);
                const FWString             TexturePath =
                    ResolveRelativePath(Source.NormalizedPath.parent_path(),
                                        FPaths::Utf8FromPath(TextureRef.TexturePath));
                if (TexturePath.empty())
                {
                    continue;
                }

                const std::filesystem::path SourceTexturePath(TexturePath);
                const FString               BakedTexturePath =
                    MakeBakedAssetPath(FPaths::Utf8FromPath(SourceTexturePath));
                if (BakedTexturePath.empty())
                {
                    continue;
                }

                const FTextureBuildSettings         TextureSettings{};
                FTextureBuildSettings               TextureSettingsBySlot = TextureSettings;
                if (TextureSlot == EMaterialTextureSlot::Normal ||
                    TextureSlot == EMaterialTextureSlot::Specular ||
                    TextureSlot == EMaterialTextureSlot::Opacity)
                {
                    TextureSettingsBySlot.bSRGB = false;
                }
                FTextureBuilder                     TextureBuilder(Cache);
                std::shared_ptr<FTextureCookedData> TextureCooked =
                    TextureBuilder.Build(TexturePath, TextureSettingsBySlot);
                if (TextureCooked == nullptr || !TextureCooked->IsValid())
                {
                    continue;
                }

                // TextureBuilder.Build()가 baked texture 저장까지 담당한다.
                // material cook 단계에서는 dependency path 연결만 유지한다.
                Material.TextureBindings.push_back(
                    {TextureSlot, FPaths::PathFromUtf8(BakedTexturePath)});
            }

            const uint32 MaterialIndex = static_cast<uint32>(Result->Materials.size());
            Result->NameToIndex[Material.Name] = MaterialIndex;
            Result->Materials.push_back(std::move(Material));
        }

        if (!Result->IsValid())
        {
            return nullptr;
        }

        const FString BakedLibraryPath =
            MakeBakedAssetPath(FPaths::Utf8FromPath(Result->SourcePath));
        if (!BakedLibraryPath.empty())
        {
            Binary::SaveMaterialLibrary(*Result, BakedLibraryPath, &Source);
        }

        return Result;
    }

    bool FMaterialBuilder::ReadAllText(const std::filesystem::path &Path, FString &OutText)
    {
        std::ifstream File(Path);
        if (!File)
        {
            return false;
        }

        std::ostringstream Oss;
        Oss << File.rdbuf();
        OutText = Oss.str();
        return true;
    }

    FString FMaterialBuilder::Trim(const FString &Value)
    {
        const size_t Begin = Value.find_first_not_of(" \t\r\n");
        if (Begin == FString::npos)
        {
            return {};
        }

        const size_t End = Value.find_last_not_of(" \t\r\n");
        return Value.substr(Begin, End - Begin + 1);
    }

    EMaterialTextureSlot FMaterialBuilder::ResolveTextureSlot(const FString &SlotName)
    {
        if (SlotName == "map_Kd")
        {
            return EMaterialTextureSlot::Diffuse;
        }
        if (SlotName == "map_Bump" || SlotName == "bump")
        {
            return EMaterialTextureSlot::Normal;
        }
        if (SlotName == "map_Ks")
        {
            return EMaterialTextureSlot::Specular;
        }
        if (SlotName == "map_d")
        {
            return EMaterialTextureSlot::Opacity;
        }
        if (SlotName == "map_Ke")
        {
            return EMaterialTextureSlot::Emissive;
        }
        return EMaterialTextureSlot::Diffuse;
    }

    FWString FMaterialBuilder::ResolveRelativePath(const std::filesystem::path &BasePath,
                                                   const FString               &RelativePath)
    {
        if (RelativePath.empty())
        {
            return {};
        }

        std::filesystem::path ResolvedPath(RelativePath);
        if (ResolvedPath.is_relative())
        {
            ResolvedPath = BasePath / ResolvedPath;
        }

        std::error_code       ErrorCode;
        std::filesystem::path CanonicalPath =
            std::filesystem::weakly_canonical(ResolvedPath, ErrorCode);
        if (ErrorCode)
        {
            CanonicalPath = ResolvedPath.lexically_normal();
        }

        return CanonicalPath.native();
    }

} // namespace Asset
