#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Core/Platform/PlatformTypes.h"

namespace Asset
{

    class FSourceHash
    {
      public:
        static bool Compute(const TArray<uint8> &Bytes, FString &OutHash);
    };

} // namespace Asset
