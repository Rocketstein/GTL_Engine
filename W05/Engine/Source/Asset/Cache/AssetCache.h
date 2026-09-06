#pragma once

#include "Core/Containers/Map.h"
#include <memory>

// ============================== Generic Cache ===============================

template <typename TKey, typename TData> class TAssetCache
{
  public:
    std::shared_ptr<TData> Find(const TKey &Key) const
    {
        auto It = Records.find(Key);
        return It != Records.end() ? It->second : nullptr;
    }

    void Insert(const TKey &Key, const std::shared_ptr<TData> &Data) { Records[Key] = Data; }
    void Remove(const TKey &Key) { Records.erase(Key); }
    void Clear() { Records.clear(); }

  private:
    TMap<TKey, std::shared_ptr<TData>> Records;
};

template <typename TKey, typename TIntermediate>
using TIntermediateCache = TAssetCache<TKey, TIntermediate>;

template <typename TKey, typename TCooked> using TCookedCache = TAssetCache<TKey, TCooked>;
