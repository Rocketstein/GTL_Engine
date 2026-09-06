#pragma once

#include "Core/Platform/PlatformTypes.h"

enum class ELogLevel : uint8
{
    Verbose = 0,
    Debug,
    Info,
    Warning,
    Error
};

const char *GetLogLevelLabel(ELogLevel Level);
ELogLevel   GetGlobalLogLevel();
void        SetGlobalLogLevel(ELogLevel Level);
bool        ShouldLog(ELogLevel Level);

class ILogOutputDevice
{
  public:
    virtual ~ILogOutputDevice() = default;
    virtual void Log(ELogLevel Level, const char *Message) = 0;
};
