#include "AWorldSettings.h"

IMPLEMENT_CLASS(AWorldSettings, AInfo)

AWorldSettings::AWorldSettings()
    : bAllowTimeDilation(true), TimeDilation(1.0f)
{
}

void AWorldSettings::SetAllowTimeDilation(bool NewAllowTimeDilation)
{
    bAllowTimeDilation = NewAllowTimeDilation;
}

// NOTE: CinematicTimeDilation 등의 개념 추가 시 여기에서 곱하도록 코드 수정.
float AWorldSettings::GetEffectiveTimeDilation()
{
    if (bAllowTimeDilation)
    {
        return TimeDilation >= 0.0f ? TimeDilation : 0.0f;
    }
    return 1.0f;
}

void AWorldSettings::SetTimeDilation(float InTimeDilation)
{
    TimeDilation = InTimeDilation;
}
