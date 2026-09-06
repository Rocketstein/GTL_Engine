#include "Component/AI/AIBlackboardComponent.h"

#include "GameFramework/AActor.h"

int32 UAIBlackboardComponent::FindFloat(const FName& Key) const
{
    for (int32 Index = 0; Index < static_cast<int32>(FloatEntries.size()); ++Index)
    {
        if (FloatEntries[Index].Key == Key)
        {
            return Index;
        }
    }
    return -1;
}

int32 UAIBlackboardComponent::FindBool(const FName& Key) const
{
    for (int32 Index = 0; Index < static_cast<int32>(BoolEntries.size()); ++Index)
    {
        if (BoolEntries[Index].Key == Key)
        {
            return Index;
        }
    }
    return -1;
}

int32 UAIBlackboardComponent::FindVector(const FName& Key) const
{
    for (int32 Index = 0; Index < static_cast<int32>(VectorEntries.size()); ++Index)
    {
        if (VectorEntries[Index].Key == Key)
        {
            return Index;
        }
    }
    return -1;
}

void UAIBlackboardComponent::SetFloat(const FName& Key, float Value)
{
    const int32 Index = FindFloat(Key);
    if (Index >= 0)
    {
        FloatEntries[Index].Value = Value;
    }
    else
    {
        FBlackboardFloatEntry Entry;
        Entry.Key   = Key;
        Entry.Value = Value;
        FloatEntries.push_back(Entry);
    }
    ++ChangeVersion;
}

float UAIBlackboardComponent::GetFloat(const FName& Key) const
{
    const int32 Index = FindFloat(Key);
    return Index >= 0 ? FloatEntries[Index].Value : 0.0f;
}

bool UAIBlackboardComponent::HasFloat(const FName& Key) const
{
    return FindFloat(Key) >= 0;
}

void UAIBlackboardComponent::SetBool(const FName& Key, bool Value)
{
    const int32 Index = FindBool(Key);
    if (Index >= 0)
    {
        BoolEntries[Index].Value = Value;
    }
    else
    {
        FBlackboardBoolEntry Entry;
        Entry.Key   = Key;
        Entry.Value = Value;
        BoolEntries.push_back(Entry);
    }
    ++ChangeVersion;
}

bool UAIBlackboardComponent::GetBool(const FName& Key) const
{
    const int32 Index = FindBool(Key);
    return Index >= 0 ? BoolEntries[Index].Value : false;
}

void UAIBlackboardComponent::SetVector(const FName& Key, const FVector& Value)
{
    const int32 Index = FindVector(Key);
    if (Index >= 0)
    {
        VectorEntries[Index].Value = Value;
    }
    else
    {
        FBlackboardVectorEntry Entry;
        Entry.Key   = Key;
        Entry.Value = Value;
        VectorEntries.push_back(Entry);
    }
    ++ChangeVersion;
}

FVector UAIBlackboardComponent::GetVector(const FName& Key) const
{
    const int32 Index = FindVector(Key);
    return Index >= 0 ? VectorEntries[Index].Value : FVector::ZeroVector;
}

void UAIBlackboardComponent::SetTarget(AActor* InTarget)
{
    Target = InTarget;
    ++ChangeVersion;
}

void UAIBlackboardComponent::PushRecentMove(const FName& MoveId)
{
    if (!MoveId.IsValid())
    {
        return;
    }
    RecentMoves.insert(RecentMoves.begin(), MoveId);
    const int32 Limit = RecentMoveMemory > 0 ? RecentMoveMemory : 0;
    while (static_cast<int32>(RecentMoves.size()) > Limit)
    {
        RecentMoves.pop_back();
    }
    ++ChangeVersion;
}

int32 UAIBlackboardComponent::GetRecentMoveRepeat(const FName& MoveId) const
{
    int32 Count = 0;
    for (const FName& Move : RecentMoves)
    {
        if (Move == MoveId)
        {
            ++Count;
        }
    }
    return Count;
}

void UAIBlackboardComponent::ResetRuntime()
{
    FloatEntries.clear();
    BoolEntries.clear();
    VectorEntries.clear();
    RecentMoves.clear();
    Target            = nullptr;
    LastSeenLocation  = FVector::ZeroVector;
    LastHeardLocation = FVector::ZeroVector;
    ++ChangeVersion;
}
