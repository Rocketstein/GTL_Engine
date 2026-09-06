#pragma once
#include "AInfo.h"

class AWorldSettings final : public AInfo
{
public:
    DECLARE_CLASS(AWorldSettings, AInfo)
    AWorldSettings();
    
    void SetAllowTimeDilation(bool NewAllowTimeDilation);
    float GetEffectiveTimeDilation();
    void SetTimeDilation(float InTimeDilation);
    
    bool bAllowTimeDilation;
    
private:
    float TimeDilation;     // => TimeScale
};
