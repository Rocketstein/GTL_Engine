#pragma once

#include "Asset/Serialization/Archive.h"
#include <filesystem>
#include <fstream>

namespace Asset
{
    class FWindowsBinWriter : public FArchive
    {
      public:
        explicit FWindowsBinWriter(const std::filesystem::path &Path);
        ~FWindowsBinWriter() override;

        bool   IsOk() const override;
        uint64 Tell() const override;
        bool   SerializeBytes(void *Data, uint64 SizeInBytes) override;

      private:
        std::ofstream Stream;
    };

    class FWindowsBinReader : public FArchive
    {
      public:
        explicit FWindowsBinReader(const std::filesystem::path &Path);
        ~FWindowsBinReader() override;

        bool   IsOk() const override;
        uint64 Tell() const override;
        bool   SerializeBytes(void *Data, uint64 SizeInBytes) override;

      private:
        std::ifstream Stream;
    };

} // namespace Asset
