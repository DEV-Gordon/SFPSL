#include "pch.h"
#include "fps_limiter.h"
#include "../config/config.h"

// Tiempo absoluto en nanosegundos
// Igual que OptiScaler: más robusto que QPC
uint64_t FPSLimiter::GetTimestamp() {
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    uint64_t t = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    return t * 100; // convierte de unidades de 100ns a nanosegundos
}

// Waitable timer de alta resolución
// No necesita timeBeginPeriod — no afecta al sistema
int FPSLimiter::TimerSleep(int64_t hundred_ns) {
    static HANDLE timer = CreateWaitableTimerExW(
        NULL, NULL,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS
    );
    if (!timer) return 1;

    LARGE_INTEGER due;
    due.QuadPart = -hundred_ns; // negativo = relativo al momento actual

    if (!SetWaitableTimerEx(timer, &due, 0, NULL, NULL, NULL, 0)) return 2;
    if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0)    return 3;
    return 0;
}

// Espera activa para los últimos nanosegundos
// Precisa pero consume CPU — solo para intervalos cortos
int FPSLimiter::BusyWaitSleep(int64_t ns) {
    uint64_t start = GetTimestamp();
    uint64_t waitUntil = start + ns;
    while (GetTimestamp() < waitUntil) { /* spin */ }
    return 0;
}

// Estrategia híbrida: timer para la mayor parte + busy-wait para el resto
// Igual que OptiScaler: mide la desviación real y la corrige
int FPSLimiter::CombinedSleep(int64_t ns) {
    constexpr int64_t BUSYWAIT_THRESHOLD = 2'000'000; // 2ms en ns

    uint64_t before = GetTimestamp();
    int status = 0;

    if (ns <= BUSYWAIT_THRESHOLD) {
        status = BusyWaitSleep(ns);
    }
    else {
        // Duerme la mayor parte con el timer de alta resolución
        // Reserva 2ms para el busy-wait final
        status = TimerSleep((ns - BUSYWAIT_THRESHOLD) / 100);
    }

    // Mide cuánto se desvió el sleep real
    // Si el timer durmió de más o de menos, lo corrige
    int64_t deviation = ns - static_cast<int64_t>(GetTimestamp() - before);
    if (deviation > 0 && !status)
        status = BusyWaitSleep(deviation);

    return status;
}

void FPSLimiter::Tick() {
    auto& cfg = Config::Instance();
    if (!cfg.limiterEnabled || cfg.targetFPS <= 0.0f) return;

    // Intervalo objetivo en nanosegundos
    // clamp: mínimo 1 FPS, máximo protección contra targetFPS=0
    uint64_t targetIntervalNs = static_cast<uint64_t>(
        std::clamp(1'000'000'000.0 / cfg.targetFPS,
            1'000'000.0,        // mínimo: ~1 FPS
            100'000'000'000.0)  // máximo: protección
        );

    static uint64_t lastFrameTime = 0;
    uint64_t now = GetTimestamp();

    if (lastFrameTime != 0) {
        uint64_t elapsed = now - lastFrameTime;
        if (elapsed < targetIntervalNs) {
            CombinedSleep(targetIntervalNs - elapsed);
        }
    }

    // Tomar el timestamp DESPUÉS del sleep
    // así el siguiente frame mide desde que realmente terminamos
    lastFrameTime = GetTimestamp();
}