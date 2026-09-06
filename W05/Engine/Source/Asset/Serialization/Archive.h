#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/String.h"
#include "Core/Platform/PlatformTypes.h"
#include <filesystem>
#include <type_traits>

namespace Asset
{
    class FArchive
    {
      public:
        enum class EMode : uint8
        {
            Load,
            Save,
        };

        explicit FArchive(EMode InMode) : Mode(InMode) {}
        virtual ~FArchive() = default;

        bool IsLoading() const { return Mode == EMode::Load; }
        bool IsSaving() const { return Mode == EMode::Save; }

        virtual bool   IsOk() const = 0;
        virtual uint64 Tell() const = 0;
        virtual bool   SerializeBytes(void *Data, uint64 SizeInBytes) = 0;

      private:
        EMode Mode;
    };

    inline FArchive &operator<<(FArchive &Ar, bool &Value)
    {
        uint8 Serialized = Value ? 1u : 0u;
        Ar.SerializeBytes(&Serialized, sizeof(Serialized));
        if (Ar.IsLoading())
        {
            Value = Serialized != 0u;
        }
        return Ar;
    }

    template <typename T>
        requires(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
    inline FArchive &operator<<(FArchive &Ar, T &Value)
    {
        Ar.SerializeBytes(&Value, sizeof(T));
        return Ar;
    }

    template <typename T>
        requires(std::is_enum_v<T>)
    inline FArchive &operator<<(FArchive &Ar, T &Value)
    {
        using Underlying = std::underlying_type_t<T>;
        Underlying RawValue = static_cast<Underlying>(Value);
        Ar.SerializeBytes(&RawValue, sizeof(RawValue));
        if (Ar.IsLoading())
        {
            Value = static_cast<T>(RawValue);
        }
        return Ar;
    }

    inline FArchive &operator<<(FArchive &Ar, FString &Value)
    {
        uint64 Size = static_cast<uint64>(Value.size());
        Ar << Size;

        if (Ar.IsLoading())
        {
            Value.resize(static_cast<size_t>(Size));
        }

        if (Size > 0)
        {
            Ar.SerializeBytes(Value.data(), Size);
        }
        return Ar;
    }

    inline FArchive &operator<<(FArchive &Ar, std::filesystem::path &Value)
    {
        FWString WidePath = Value.generic_wstring();
        uint64   Size = static_cast<uint64>(WidePath.size());
        Ar << Size;

        if (Ar.IsLoading())
        {
            WidePath.resize(static_cast<size_t>(Size));
        }

        if (Size > 0)
        {
            Ar.SerializeBytes(WidePath.data(), Size * sizeof(FWString::value_type));
        }

        if (Ar.IsLoading())
        {
            Value = std::filesystem::path(WidePath);
        }

        return Ar;
    }

    template <typename T> inline FArchive &SerializeArray(FArchive &Ar, TArray<T> &Values)
    {
        uint64 Count = static_cast<uint64>(Values.size());
        Ar << Count;

        if (Ar.IsLoading())
        {
            Values.resize(static_cast<size_t>(Count));
        }

        if constexpr (std::is_same_v<T, uint8>)
        {
            if (Count > 0)
            {
                Ar.SerializeBytes(Values.data(), Count);
            }
        }
        else
        {
            for (T &Value : Values)
            {
                Ar << Value;
            }
        }

        return Ar;
    }

    template <typename T> inline FArchive &operator<<(FArchive &Ar, TArray<T> &Values)
    {
        return SerializeArray(Ar, Values);
    }

    template <typename K, typename V> inline FArchive &operator<<(FArchive &Ar, TMap<K, V> &Values)
    {
        uint64 Count = static_cast<uint64>(Values.size());
        Ar << Count;

        if (Ar.IsLoading())
        {
            Values.clear();
            for (uint64 Index = 0; Index < Count; ++Index)
            {
                K Key{};
                V Value{};
                Ar << Key;
                Ar << Value;
                Values.emplace(std::move(Key), std::move(Value));
            }
            return Ar;
        }

        for (auto &Entry : Values)
        {
            K Key = Entry.first;
            Ar << Key;
            Ar << Entry.second;
        }
        return Ar;
    }

} // namespace Asset
