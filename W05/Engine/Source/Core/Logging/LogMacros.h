#pragma once

#include "Core/CoreGlobals.h"
#include "LogOutputDevice.h"
#include <cstdarg>
#include <cstdio>

#ifndef UE_DEFAULT_LOG_LEVEL
#define UE_DEFAULT_LOG_LEVEL ELogLevel::Verbose
#endif

inline void InitializeDefaultLogLevel()
{
    static bool bInitialized = false;
    if (!bInitialized)
    {
        SetGlobalLogLevel(UE_DEFAULT_LOG_LEVEL);
        bInitialized = true;
    }
}

inline void LogMessage(ELogLevel Level, const char *Category, const char *Format, ...)
{
    InitializeDefaultLogLevel();

    if (!GLog || !ShouldLog(Level))
    {
        return;
    }

    char Prefix[256];
    snprintf(Prefix, sizeof(Prefix), "[%s] %s: ", GetLogLevelLabel(Level), Category);

    char    Message[1024];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Message, sizeof(Message), Format, Args);
    va_end(Args);

    char FinalBuffer[1280];
    snprintf(FinalBuffer, sizeof(FinalBuffer), "%s%s", Prefix, Message);

    GLog->Log(Level, FinalBuffer);
}


#if defined(_DEBUG)
#define UE_LOG(Category, Level, Format, ...)                                                       \
    do                                                                                             \
    {                                                                                              \
        LogMessage(Level, #Category, Format, ##__VA_ARGS__);                                       \
    } while (0)
#else
#define UE_LOG(Category, Level, Format, ...)                                                       \
    do                                                                                             \
    {                                                                                              \
        (void)0;                                                                                   \
    } while (0)
#endif