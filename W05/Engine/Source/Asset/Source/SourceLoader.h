#pragma once

#include "Core/Containers/Array.h"
#include "Core/Platform/PlatformTypes.h"
#include <filesystem>


namespace Asset
{

    class FSourceLoader
    {
      public:
        static bool QueryFileInfo(const std::filesystem::path &Path, uint64 &OutFileSize,
                                  uint64 &OutWriteTimeTicks);
        static bool ReadAllBytes(const std::filesystem::path &Path, TArray<uint8> &OutBytes);
    };

} // namespace Asset
