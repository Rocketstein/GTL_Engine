#include "AI/BehaviorTree/BehaviorTreeManager.h"

#include "AI/BehaviorTree/BehaviorTreeAsset.h"
#include "Asset/AssetPackage.h"
#include "Object/GarbageCollection.h"
#include "Object/Object.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <filesystem>

UBehaviorTreeAsset* FBehaviorTreeManager::Load(const FString& Path)
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

    auto It = LoadedTrees.find(NormalizedPath);
    if (It != LoadedTrees.end())
    {
        if (IsValid(It->second))
        {
            return It->second;
        }
        LoadedTrees.erase(It);
    }

    if (!FAssetPackage::IsAssetPackagePath(NormalizedPath)) return nullptr;

    FWindowsBinReader Ar(NormalizedPath);
    if (!Ar.IsValid()) return nullptr;

    FAssetPackageHeader  Header;
    FAssetImportMetadata Metadata;
    if (!FAssetPackage::ReadPackagePrelude(Ar, EAssetPackageType::BehaviorTree, Header, Metadata))
    {
        return nullptr;
    }

    UBehaviorTreeAsset* NewAsset = UObjectManager::Get().CreateObject<UBehaviorTreeAsset>();
    NewAsset->Serialize(Ar);
    if (!Ar.IsValid())
    {
        UObjectManager::Get().DestroyObject(NewAsset);
        return nullptr;
    }

    NewAsset->SetSourcePath(NormalizedPath);
    if (NewAsset->IsCompileDirty())
    {
        NewAsset->Compile();
    }
    LoadedTrees.emplace(NormalizedPath, NewAsset);
    return NewAsset;
}

UBehaviorTreeAsset* FBehaviorTreeManager::Find(const FString& Path) const
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
    auto          It             = LoadedTrees.find(NormalizedPath);
    if (It == LoadedTrees.end())
    {
        return nullptr;
    }
    if (IsValid(It->second))
    {
        return It->second;
    }
    return nullptr;
}

bool FBehaviorTreeManager::Save(UBehaviorTreeAsset* Asset)
{
    if (!Asset) return false;
    const FString& Path = Asset->GetSourcePath();
    if (Path.empty()) return false;

    if (Asset->IsCompileDirty())
    {
        Asset->Compile();
    }

    FWindowsBinWriter Ar(FPaths::MakeProjectRelative(Path));
    if (!Ar.IsValid()) return false;

    FAssetPackageHeader  Header;
    FAssetImportMetadata Metadata;
    if (!FAssetPackage::WritePackagePrelude(Ar, EAssetPackageType::BehaviorTree, Metadata, &Header))
    {
        return false;
    }
    Asset->Serialize(Ar);
    return Ar.IsValid();
}

void FBehaviorTreeManager::RefreshAvailableBehaviorTrees()
{
    const std::filesystem::path ContentRoot = std::filesystem::path(FPaths::RootDir()) / L"Content";
    std::error_code             ExistsError;
    if (!std::filesystem::exists(ContentRoot, ExistsError) || ExistsError) return;

    const std::filesystem::path ProjectRoot(FPaths::RootDir());
    AvailableBehaviorTreeFiles.clear();

    std::error_code IterError;
    for (std::filesystem::recursive_directory_iterator It(
             ContentRoot, std::filesystem::directory_options::skip_permission_denied, IterError), End;
         !IterError && It != End;
         It.increment(IterError))
    {
        const std::filesystem::directory_entry& Entry = *It;
        std::error_code                          EntryError;
        if (!Entry.is_regular_file(EntryError) || EntryError) continue;

        std::wstring Ext = Entry.path().extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
        if (Ext != L".uasset") continue;

        const FString        RelPath = FPaths::ToUtf8(Entry.path().lexically_relative(ProjectRoot).generic_wstring());
        FAssetImportMetadata Metadata;
        if (!FAssetPackage::ReadMetadata(RelPath, EAssetPackageType::BehaviorTree, Metadata))
        {
            continue;
        }

        FAssetListItem Item;
        Item.DisplayName = FPaths::ToUtf8(Entry.path().stem().wstring());
        Item.FullPath    = RelPath;
        AvailableBehaviorTreeFiles.push_back(std::move(Item));
    }
}

void FBehaviorTreeManager::AddReferencedObjects(FReferenceCollector& Collector)
{
    for (auto& Pair : LoadedTrees)
    {
        Collector.AddReferencedObject(Pair.second);
    }
}

void FBehaviorTreeManager::ClearCache()
{
    LoadedTrees.clear();
    AvailableBehaviorTreeFiles.clear();
}
