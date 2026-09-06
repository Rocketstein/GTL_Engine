#pragma once

#include "Core/Containers/String.h"
#include "Core/Platform/PlatformTypes.h"
#include <filesystem>


namespace Asset
{

    struct FSourceRecord
    {
        std::filesystem::path NormalizedPath;
        uint64                FileSize = 0;
        uint64                LastWriteTimeTicks = 0;
        FString               ContentHash;
        bool                  bHasContentHash = false;
    };

} // namespace Asset
