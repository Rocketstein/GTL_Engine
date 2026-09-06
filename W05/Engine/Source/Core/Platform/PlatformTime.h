#pragma once

#include "PlatformTypes.h"

#if defined(_WIN32)
#include <Windows.h>
#else
#include <chrono>
#include <thread>
#endif

struct FPlatformTime
{
    static double Seconds();
    static uint64 Cycles64();
    static double GetSecondsPerCycle();
    static double ToMilliseconds(uint64 CycleDiff);
    static void   Sleep(float Seconds);
};
