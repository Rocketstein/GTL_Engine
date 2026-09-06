#include "ObjectIterator.h"
#include "Object.h"

FObjectIterator::FObjectIterator() : CurrentIndex(0) { AdvanceToNextValid(); }

FObjectIterator::operator bool() const
{
    return CurrentIndex < static_cast<int32>(GUObjectArray.size());
}

FObjectIterator &FObjectIterator::operator++()
{
    ++CurrentIndex;
    AdvanceToNextValid();
    return *this;
}

UObject *FObjectIterator::operator*() const { return GUObjectArray[CurrentIndex]; }

UObject *FObjectIterator::operator->() const { return GUObjectArray[CurrentIndex]; }

void FObjectIterator::AdvanceToNextValid()
{
    const int32 Count = static_cast<int32>(GUObjectArray.size());
    while (CurrentIndex < Count)
    {
        if (GUObjectArray[CurrentIndex] != nullptr)
        {
            return;
        }
        ++CurrentIndex;
    }
}