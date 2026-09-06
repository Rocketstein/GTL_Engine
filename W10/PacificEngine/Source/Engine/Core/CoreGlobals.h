#pragma once

#include "Core/Logging/LogOutputDevice.h"

class ILogOutputDevice;
class FLogBuffer;
extern ILogOutputDevice* GLog;
extern ELogLevel         GMinimumLogLevel;

FLogBuffer& GetGlobalLogBuffer();