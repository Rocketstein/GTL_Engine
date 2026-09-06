#pragma once

#include "Object.h"

// extern  TArray<UObject*> GUObjectArray;

/**
 * FObjectIterator — 모든 UObject를 순회합니다.
 */
class FObjectIterator
{
  public:
    FObjectIterator();

    explicit operator bool() const;

    FObjectIterator &operator++();

    UObject *operator*() const;
    UObject *operator->() const;

  private:
    void AdvanceToNextValid();

    int32 CurrentIndex;
};

/*
 * TObjectIterator<T>는 특정 타입의 UObject들을 순회하는 반복자입니다.
 */
template <typename TObject> class TObjectIterator
{
  public:
    TObjectIterator() : CurrentIndex(0) { AdvanceToNextValidObject(); }

    explicit operator bool() const
    {
        return CurrentIndex < static_cast<int32>(GUObjectArray.size());
    }

    TObjectIterator &operator++()
    {
        ++CurrentIndex;
        AdvanceToNextValidObject();
        return *this;
    }

    TObject *operator*() const { return static_cast<TObject *>(GUObjectArray[CurrentIndex]); }

    TObject *operator->() const { return static_cast<TObject *>(GUObjectArray[CurrentIndex]); }

  private:
    void AdvanceToNextValidObject()
    {
        const int32 Count = static_cast<int32>(GUObjectArray.size());
        while (CurrentIndex < Count)
        {
            UObject *Obj = GUObjectArray[CurrentIndex];
            if (Obj != nullptr && Obj->IsA(TObject::GetClass()))
            {
                return;
            }
            ++CurrentIndex;
        }
    }

    int32 CurrentIndex;
};
