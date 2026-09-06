#include "PlatformTime.h"

#if defined(_WIN32)

namespace
{
    inline LARGE_INTEGER GetQpcFrequency()
    {
        static LARGE_INTEGER Frequency = []()
        {
            LARGE_INTEGER Value{};
            ::QueryPerformanceFrequency(&Value);
            return Value;
        }();
        return Frequency;
    }
} // namespace

double FPlatformTime::Seconds()
{
    LARGE_INTEGER Counter{};
    ::QueryPerformanceCounter(&Counter);

    const LARGE_INTEGER Frequency = GetQpcFrequency();
    return static_cast<double>(Counter.QuadPart) / static_cast<double>(Frequency.QuadPart);
}

uint64 FPlatformTime::Cycles64()
{
    LARGE_INTEGER Counter{};
    ::QueryPerformanceCounter(&Counter);
    return static_cast<uint64>(Counter.QuadPart);
}

double FPlatformTime::GetSecondsPerCycle()
{
    const LARGE_INTEGER Frequency = GetQpcFrequency();
    const double        SafeFrequency =
        (Frequency.QuadPart > 0) ? static_cast<double>(Frequency.QuadPart) : 1.0;
    return 1.0 / SafeFrequency;
}

double FPlatformTime::ToMilliseconds(uint64 CycleDiff)
{
    return static_cast<double>(CycleDiff) * GetSecondsPerCycle() * 1000.0;
}

void FPlatformTime::Sleep(float Seconds)
{
    if (Seconds < 0.0f)
    {
        return;
    }

    const DWORD Milliseconds = static_cast<DWORD>(Seconds * 1000.0f);
    ::Sleep(Milliseconds);
}

#else

double FPlatformTime::Seconds()
{
    using namespace std::chrono;
    const auto Now = steady_clock::now().time_since_epoch();
    return duration<double>(Now).count();
}

uint64 FPlatformTime::Cycles64()
{
    using namespace std::chrono;
    const auto Now = steady_clock::now().time_since_epoch();
    return static_cast<uint64>(duration_cast<nanoseconds>(Now).count());
}

double FPlatformTime::GetSecondsPerCycle() { return 1.0e-9; }

double FPlatformTime::ToMilliseconds(uint64 CycleDiff)
{
    return static_cast<double>(CycleDiff) * GetSecondsPerCycle() * 1000.0;
}

void FPlatformTime::Sleep(float Seconds)
{
    if (Seconds <= 0.0f)
    {
        return;
    }

    std::this_thread::sleep_for(std::chrono::duration<float>(Seconds));
}

#endif
