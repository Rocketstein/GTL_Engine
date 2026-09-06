#include "Asset/Source/SourceHash.h"
#include <functional>
#include <sstream>
#include <string_view>

namespace Asset
{

    bool FSourceHash::Compute(const TArray<uint8> &Bytes, FString &OutHash)
    {
        std::string_view View(reinterpret_cast<const char *>(Bytes.data()), Bytes.size());

        const size_t HashValue = std::hash<std::string_view>{}(View);

        std::ostringstream Oss;
        Oss << std::hex << HashValue;
        OutHash = Oss.str();
        return true;
    }

} // namespace Asset
