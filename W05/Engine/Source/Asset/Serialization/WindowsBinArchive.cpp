#include "Asset/Serialization/WindowsBinArchive.h"

namespace Asset
{
    FWindowsBinWriter::FWindowsBinWriter(const std::filesystem::path &Path)
        : FArchive(EMode::Save), Stream(Path, std::ios::binary | std::ios::trunc)
    {
    }

    FWindowsBinWriter::~FWindowsBinWriter() = default;

    bool FWindowsBinWriter::IsOk() const { return Stream.good(); }

    uint64 FWindowsBinWriter::Tell() const
    {
        return static_cast<uint64>(const_cast<std::ofstream &>(Stream).tellp());
    }

    bool FWindowsBinWriter::SerializeBytes(void *Data, uint64 SizeInBytes)
    {
        if (SizeInBytes == 0)
        {
            return true;
        }

        Stream.write(reinterpret_cast<const char *>(Data),
                     static_cast<std::streamsize>(SizeInBytes));
        return Stream.good();
    }

    FWindowsBinReader::FWindowsBinReader(const std::filesystem::path &Path)
        : FArchive(EMode::Load), Stream(Path, std::ios::binary)
    {
    }

    FWindowsBinReader::~FWindowsBinReader() = default;

    bool FWindowsBinReader::IsOk() const { return Stream.good(); }

    uint64 FWindowsBinReader::Tell() const
    {
        return static_cast<uint64>(const_cast<std::ifstream &>(Stream).tellg());
    }

    bool FWindowsBinReader::SerializeBytes(void *Data, uint64 SizeInBytes)
    {
        if (SizeInBytes == 0)
        {
            return true;
        }

        Stream.read(reinterpret_cast<char *>(Data), static_cast<std::streamsize>(SizeInBytes));
        return Stream.good();
    }

} // namespace Asset
