#include "AI/SquadCoordinator.h"

#include "GameFramework/AActor.h"

FSquadCoordinator& FSquadCoordinator::Get()
{
    static FSquadCoordinator Instance;
    return Instance;
}

void FSquadCoordinator::Prune(float Now)
{
    for (auto It = Tokens.begin(); It != Tokens.end(); )
    {
        if (!It->Holder.IsValid() || !It->Target.IsValid() || It->ExpirySeconds < Now)
        {
            It = Tokens.erase(It);
        }
        else
        {
            ++It;
        }
    }
}

bool FSquadCoordinator::TryAcquireToken(AActor* Holder, AActor* Target, float Now, float Duration, int32 MaxAttackers)
{
    if (!Holder || !Target)
    {
        return false;
    }
    Prune(Now);

    // 이미 보유 중이면 갱신.
    for (FAttackToken& Token : Tokens)
    {
        if (Token.Holder.Get() == Holder && Token.Target.Get() == Target)
        {
            Token.ExpirySeconds = Now + Duration;
            return true;
        }
    }

    // 자신을 제외한 동일 타깃 공격자 수.
    int32 Others = 0;
    for (const FAttackToken& Token : Tokens)
    {
        if (Token.Target.Get() == Target && Token.Holder.Get() != Holder)
        {
            ++Others;
        }
    }
    if (MaxAttackers > 0 && Others >= MaxAttackers)
    {
        return false;
    }

    FAttackToken NewToken;
    NewToken.Holder        = Holder;
    NewToken.Target        = Target;
    NewToken.ExpirySeconds = Now + Duration;
    Tokens.push_back(NewToken);
    return true;
}

void FSquadCoordinator::ReleaseToken(AActor* Holder)
{
    for (auto It = Tokens.begin(); It != Tokens.end(); )
    {
        if (It->Holder.Get() == Holder)
        {
            It = Tokens.erase(It);
        }
        else
        {
            ++It;
        }
    }
}

int32 FSquadCoordinator::CountActiveAttackers(AActor* Target, float Now)
{
    Prune(Now);
    int32 Count = 0;
    for (const FAttackToken& Token : Tokens)
    {
        if (Token.Target.Get() == Target)
        {
            ++Count;
        }
    }
    return Count;
}

int32 FSquadCoordinator::GetSlotIndex(AActor* Holder, AActor* Target, float Now)
{
    Prune(Now);
    int32 Slot = 0;
    for (const FAttackToken& Token : Tokens)
    {
        if (Token.Target.Get() != Target)
        {
            continue;
        }
        if (Token.Holder.Get() == Holder)
        {
            return Slot;
        }
        ++Slot;
    }
    return -1;
}

bool FSquadCoordinator::HoldsToken(AActor* Holder, AActor* Target, float Now) const
{
    for (const FAttackToken& Token : Tokens)
    {
        if (Token.Holder.Get() == Holder && Token.Target.Get() == Target && Token.ExpirySeconds >= Now)
        {
            return true;
        }
    }
    return false;
}

void FSquadCoordinator::PruneEngagers(float Now)
{
    for (auto It = Engagers.begin(); It != Engagers.end(); )
    {
        if (!It->Holder.IsValid() || !It->Target.IsValid() || It->ExpirySeconds < Now)
        {
            It = Engagers.erase(It);
        }
        else
        {
            ++It;
        }
    }
}

int32 FSquadCoordinator::SlotIndexOf(AActor* Holder, AActor* Target) const
{
    int32 Index = 0;
    for (const FEngagerSlot& Slot : Engagers)
    {
        if (Slot.Target.Get() != Target)
        {
            continue;
        }
        if (Slot.Holder.Get() == Holder)
        {
            return Index;
        }
        ++Index;
    }
    return -1;
}

int32 FSquadCoordinator::RegisterEngager(AActor* Holder, AActor* Target, float Now, float Duration)
{
    if (!Holder || !Target)
    {
        return -1;
    }
    PruneEngagers(Now);
    for (FEngagerSlot& Slot : Engagers)
    {
        if (Slot.Holder.Get() == Holder && Slot.Target.Get() == Target)
        {
            Slot.ExpirySeconds = Now + Duration; // 갱신
            return SlotIndexOf(Holder, Target);
        }
    }
    FEngagerSlot NewSlot;
    NewSlot.Holder        = Holder;
    NewSlot.Target        = Target;
    NewSlot.ExpirySeconds = Now + Duration;
    Engagers.push_back(NewSlot);
    return SlotIndexOf(Holder, Target);
}

int32 FSquadCoordinator::GetEngagerCount(AActor* Target, float Now)
{
    PruneEngagers(Now);
    int32 Count = 0;
    for (const FEngagerSlot& Slot : Engagers)
    {
        if (Slot.Target.Get() == Target)
        {
            ++Count;
        }
    }
    return Count;
}

void FSquadCoordinator::ReleaseEngager(AActor* Holder)
{
    for (auto It = Engagers.begin(); It != Engagers.end(); )
    {
        AActor* H = It->Holder.Get();
        if (H == Holder || H == nullptr)
        {
            It = Engagers.erase(It);
        }
        else
        {
            ++It;
        }
    }
}
