#pragma once
#include "pch.h"

class FPSLimiter {
public:
    static void Tick();

private:
    static uint64_t  GetTimestamp();
    static int       TimerSleep(int64_t hundred_ns);
    static int       BusyWaitSleep(int64_t ns);
    static int       CombinedSleep(int64_t ns);
};