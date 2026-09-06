#pragma once

#include "Asset/Cache/AssetKey.h"
#include "Asset/Source/SourceHash.h"
#include "Asset/Source/SourceLoader.h"
#include "Asset/Source/SourceRecord.h"
#include "Core/Containers/Map.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Misc/Paths.h"
#include <filesystem>


namespace Asset
{
    namespace SourceCacheDetail
    {
        inline std::filesystem::path NormalizePath(const std::filesystem::path &InPath)
        {
            if (InPath.empty())
            {
                return {};
            }

            return FPaths::Normalize(InPath);
        }

        inline FString StringFromPath(const std::filesystem::path &Path)
        {
            if (Path.empty())
            {
                return {};
            }
            return FPaths::Utf8FromPath(Path);
        }

        inline bool IsVirtualContentPath(const std::filesystem::path &Path)
        {
            const FString PathString = StringFromPath(Path);
            return PathString.rfind("/Content", 0) == 0;
        }

        template <typename TKey>
        inline TKey MakeKeyFromNormalizedPath(const std::filesystem::path &NormalizedPath)
        {
            return TKey::FromPath(NormalizedPath);
        }
    } // namespace SourceCacheDetail

    template <typename TSourceKey> class TSourceCache
    {
      public:
        const FSourceRecord *GetOrLoad(const std::filesystem::path &Path)
        {
            const std::filesystem::path NormalizedPath = SourceCacheDetail::NormalizePath(Path);
            if (NormalizedPath.empty())
            {
                UE_LOG(AssetSource, ELogLevel::Error,
                       "SourceCache GetOrLoad failed: empty input path.");
                return nullptr;
            }

            if (SourceCacheDetail::IsVirtualContentPath(NormalizedPath))
            {
                UE_LOG(AssetSource, ELogLevel::Error,
                       "SourceCache GetOrLoad received virtual path. Resolve to absolute path "
                       "first: %s",
                       SourceCacheDetail::StringFromPath(NormalizedPath).c_str());
                return nullptr;
            }

            const TSourceKey Key =
                SourceCacheDetail::MakeKeyFromNormalizedPath<TSourceKey>(NormalizedPath);
            return GetOrLoad(Key);
        }

        const FSourceRecord *GetOrLoad(const TSourceKey &Key)
        {
            if (Key.NormalizedPath.empty())
            {
                UE_LOG(AssetSource, ELogLevel::Error,
                       "SourceCache GetOrLoad failed: empty normalized path.");
                return nullptr;
            }

            if (SourceCacheDetail::IsVirtualContentPath(Key.NormalizedPath))
            {
                UE_LOG(AssetSource, ELogLevel::Error,
                       "SourceCache GetOrLoad received virtual key path. Resolve to absolute path "
                       "first: %s",
                       SourceCacheDetail::StringFromPath(Key.NormalizedPath).c_str());
                return nullptr;
            }

            UE_LOG(AssetSource, ELogLevel::Debug, "SourceCache GetOrLoad: path = %s",
                   SourceCacheDetail::StringFromPath(Key.NormalizedPath).c_str());

            uint64 CurrentFileSize = 0;
            uint64 CurrentWriteTimeTicks = 0;
            if (!FSourceLoader::QueryFileInfo(Key.NormalizedPath, CurrentFileSize,
                                              CurrentWriteTimeTicks))
            {
                UE_LOG(AssetSource, ELogLevel::Error, "SourceCache QueryFileInfo failed: %s",
                       SourceCacheDetail::StringFromPath(Key.NormalizedPath).c_str());

                UE_LOG(AssetSource, ELogLevel::Error,
                       "SourceCache hint: verify the file exists and that the path was resolved to "
                       "an absolute filesystem path before GetOrLoad.");

                Records.erase(Key);
                return nullptr;
            }

            auto ExistingIt = Records.find(Key);
            if (ExistingIt != Records.end())
            {
                if (!HasFileChanged(ExistingIt->second, CurrentFileSize, CurrentWriteTimeTicks))
                {
                    UE_LOG(AssetSource, ELogLevel::Verbose, "SourceCache cache hit: %s",
                           SourceCacheDetail::StringFromPath(Key.NormalizedPath).c_str());
                    return &ExistingIt->second;
                }

                UE_LOG(AssetSource, ELogLevel::Debug, "SourceCache source changed, reloading: %s",
                       SourceCacheDetail::StringFromPath(Key.NormalizedPath).c_str());
            }

            FSourceRecord NewRecord;
            if (!ReloadRecord(Key, NewRecord))
            {
                UE_LOG(AssetSource, ELogLevel::Error, "SourceCache ReloadRecord failed: %s",
                       SourceCacheDetail::StringFromPath(Key.NormalizedPath).c_str());

                Records.erase(Key);
                return nullptr;
            }

            auto [It, bInserted] = Records.insert_or_assign(Key, std::move(NewRecord));
            UE_LOG(AssetSource, ELogLevel::Debug,
                   bInserted ? "SourceCache cached new record: %s"
                             : "SourceCache refreshed record: %s",
                   SourceCacheDetail::StringFromPath(Key.NormalizedPath).c_str());
            return &It->second;
        }

        bool EnsureContentHashLoaded(const std::filesystem::path &Path)
        {
            const std::filesystem::path NormalizedPath = SourceCacheDetail::NormalizePath(Path);
            if (NormalizedPath.empty())
            {
                UE_LOG(AssetSource, ELogLevel::Error,
                       "SourceCache EnsureContentHashLoaded failed: empty input path.");
                return false;
            }

            if (SourceCacheDetail::IsVirtualContentPath(NormalizedPath))
            {
                UE_LOG(AssetSource, ELogLevel::Error,
                       "SourceCache EnsureContentHashLoaded received virtual path. Resolve to "
                       "absolute path first: %s",
                       SourceCacheDetail::StringFromPath(NormalizedPath).c_str());
                return false;
            }

            const TSourceKey Key =
                SourceCacheDetail::MakeKeyFromNormalizedPath<TSourceKey>(NormalizedPath);
            return EnsureContentHashLoaded(Key);
        }

        bool EnsureContentHashLoaded(const TSourceKey &Key)
        {
            if (Key.NormalizedPath.empty())
            {
                UE_LOG(AssetSource, ELogLevel::Error,
                       "SourceCache EnsureContentHashLoaded failed: empty normalized path.");
                return false;
            }

            auto It = Records.find(Key);
            if (It == Records.end())
            {
                UE_LOG(AssetSource, ELogLevel::Warning,
                       "SourceCache EnsureContentHashLoaded cache miss: %s",
                       SourceCacheDetail::StringFromPath(Key.NormalizedPath).c_str());
                return false;
            }

            if (It->second.bHasContentHash)
            {
                return true;
            }

            TArray<uint8> FileBytes;
            if (!FSourceLoader::ReadAllBytes(Key.NormalizedPath, FileBytes))
            {
                UE_LOG(AssetSource, ELogLevel::Error,
                       "SourceCache ReadAllBytes failed while loading content hash: %s",
                       SourceCacheDetail::StringFromPath(Key.NormalizedPath).c_str());
                return false;
            }

            FString ContentHash;
            if (!FSourceHash::Compute(FileBytes, ContentHash))
            {
                UE_LOG(AssetSource, ELogLevel::Error, "SourceCache content hash compute failed: %s",
                       SourceCacheDetail::StringFromPath(Key.NormalizedPath).c_str());
                return false;
            }

            It->second.ContentHash = std::move(ContentHash);
            It->second.bHasContentHash = true;
            UE_LOG(AssetSource, ELogLevel::Verbose, "SourceCache content hash loaded: %s",
                   SourceCacheDetail::StringFromPath(Key.NormalizedPath).c_str());
            return true;
        }

        const FSourceRecord *Find(const std::filesystem::path &Path) const
        {
            const std::filesystem::path NormalizedPath = SourceCacheDetail::NormalizePath(Path);
            if (NormalizedPath.empty())
            {
                return nullptr;
            }

            if (SourceCacheDetail::IsVirtualContentPath(NormalizedPath))
            {
                UE_LOG(AssetSource, ELogLevel::Warning,
                       "SourceCache Find received virtual path. Resolve to absolute path first: %s",
                       SourceCacheDetail::StringFromPath(NormalizedPath).c_str());
                return nullptr;
            }

            const TSourceKey Key =
                SourceCacheDetail::MakeKeyFromNormalizedPath<TSourceKey>(NormalizedPath);
            return Find(Key);
        }

        const FSourceRecord *Find(const TSourceKey &Key) const
        {
            auto It = Records.find(Key);
            return It != Records.end() ? &It->second : nullptr;
        }

        void Invalidate(const std::filesystem::path &Path)
        {
            const std::filesystem::path NormalizedPath = SourceCacheDetail::NormalizePath(Path);
            if (NormalizedPath.empty())
            {
                return;
            }

            if (SourceCacheDetail::IsVirtualContentPath(NormalizedPath))
            {
                UE_LOG(AssetSource, ELogLevel::Warning,
                       "SourceCache Invalidate received virtual path. Resolve to absolute path "
                       "first: %s",
                       SourceCacheDetail::StringFromPath(NormalizedPath).c_str());
                return;
            }

            Records.erase(SourceCacheDetail::MakeKeyFromNormalizedPath<TSourceKey>(NormalizedPath));
        }

        void Invalidate(const TSourceKey &Key) { Records.erase(Key); }
        void Clear() { Records.clear(); }

      private:
        bool HasFileChanged(const FSourceRecord &Record, uint64 CurrentFileSize,
                            uint64 CurrentWriteTimeTicks) const
        {
            return Record.FileSize != CurrentFileSize ||
                   Record.LastWriteTimeTicks != CurrentWriteTimeTicks;
        }

        bool ReloadRecord(const TSourceKey &Key, FSourceRecord &OutRecord) const
        {
            uint64 FileSize = 0;
            uint64 WriteTimeTicks = 0;
            if (!FSourceLoader::QueryFileInfo(Key.NormalizedPath, FileSize, WriteTimeTicks))
            {
                return false;
            }

            OutRecord = {};
            OutRecord.NormalizedPath = Key.NormalizedPath;
            OutRecord.FileSize = FileSize;
            OutRecord.LastWriteTimeTicks = WriteTimeTicks;
            return true;
        }

      private:
        TMap<TSourceKey, FSourceRecord> Records;
    };

} // namespace Asset
