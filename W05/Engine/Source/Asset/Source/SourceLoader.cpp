#include "Asset/Source/SourceLoader.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Misc/Paths.h"
#include <chrono>
#include <filesystem>
#include <fstream>


namespace Asset
{

    namespace
    {
        static uint64 ToUnixLikeTicks(const std::filesystem::file_time_type &InTime)
        {
            const auto Ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(InTime.time_since_epoch())
                    .count();
            return static_cast<uint64>(Ns);
        }
    } // namespace

    bool FSourceLoader::QueryFileInfo(const std::filesystem::path &Path, uint64 &OutFileSize,
                                      uint64 &OutWriteTimeTicks)
    {
        OutFileSize = 0;
        OutWriteTimeTicks = 0;

        const FString DebugPath = FPaths::Utf8FromPath(Path);

        UE_LOG(AssetSource, ELogLevel::Verbose, "QueryFileInfo native path: %s", DebugPath.c_str());

        UE_LOG(AssetSource, ELogLevel::Verbose, "QueryFileInfo is_absolute=%d is_relative=%d",
               Path.is_absolute() ? 1 : 0, Path.is_relative() ? 1 : 0);

        std::error_code Ec;
        const bool      bExists = std::filesystem::exists(Path, Ec);

        UE_LOG(AssetSource, ELogLevel::Verbose, "QueryFileInfo exists=%d ec=%d", bExists ? 1 : 0,
               static_cast<int>(Ec.value()));

        if (!bExists || Ec)
        {
            UE_LOG(AssetSource, ELogLevel::Error, "QueryFileInfo exists() failed: %s",
                   DebugPath.c_str());
            return false;
        }

        OutFileSize = static_cast<uint64>(std::filesystem::file_size(Path, Ec));
        if (Ec)
        {
            return false;
        }

        const auto WriteTime = std::filesystem::last_write_time(Path, Ec);
        if (Ec)
        {
            return false;
        }

        OutWriteTimeTicks = ToUnixLikeTicks(WriteTime);
        return true;
    }

    bool FSourceLoader::ReadAllBytes(const std::filesystem::path &Path, TArray<uint8> &OutBytes)
    {
        OutBytes.clear();

        std::ifstream File(Path, std::ios::binary);
        if (!File)
        {
            return false;
        }

        File.seekg(0, std::ios::end);
        const std::streamoff Size = File.tellg();
        if (Size < 0)
        {
            return false;
        }

        File.seekg(0, std::ios::beg);

        OutBytes.resize(static_cast<size_t>(Size));
        if (Size > 0)
        {
            File.read(reinterpret_cast<char *>(OutBytes.data()), Size);
            if (!File)
            {
                OutBytes.clear();
                return false;
            }
        }

        return true;
    }

} // namespace Asset
