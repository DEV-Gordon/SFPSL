#include "pch.h"
#include "hooks/dx12_hook.h"
#include "config/config.h"
#include <fstream>

// Handle a la version.dll real de System32
HMODULE g_realVersion = nullptr;

// Logger simple
static void Log(const char* msg) {
    std::ofstream f("SimpleFPSLimiter_log.txt", std::ios::app);
    if (f.is_open()) f << msg << "\n";
}

// Exportaciones de version.dll
// El juego llama estas funciones — nosotros las recibimos y reenviamos a la real
extern "C" {

    BOOL __stdcall GetFileVersionInfoA(
        LPCSTR f, DWORD h, DWORD l, LPVOID d)
    {
        static auto fn = (decltype(&GetFileVersionInfoA))
            GetProcAddress(g_realVersion, "GetFileVersionInfoA");
        return fn ? fn(f, h, l, d) : FALSE;
    }

    BOOL __stdcall GetFileVersionInfoW(
        LPCWSTR f, DWORD h, DWORD l, LPVOID d)
    {
        static auto fn = (decltype(&GetFileVersionInfoW))
            GetProcAddress(g_realVersion, "GetFileVersionInfoW");
        return fn ? fn(f, h, l, d) : FALSE;
    }

    DWORD __stdcall GetFileVersionInfoSizeA(LPCSTR f, LPDWORD h) {
        static auto fn = (decltype(&GetFileVersionInfoSizeA))
            GetProcAddress(g_realVersion, "GetFileVersionInfoSizeA");
        return fn ? fn(f, h) : 0;
    }

    DWORD __stdcall GetFileVersionInfoSizeW(LPCWSTR f, LPDWORD h) {
        static auto fn = (decltype(&GetFileVersionInfoSizeW))
            GetProcAddress(g_realVersion, "GetFileVersionInfoSizeW");
        return fn ? fn(f, h) : 0;
    }

    BOOL __stdcall VerQueryValueA(
        LPCVOID b, LPCSTR s, LPVOID* p, PUINT l)
    {
        static auto fn = (decltype(&VerQueryValueA))
            GetProcAddress(g_realVersion, "VerQueryValueA");
        return fn ? fn(b, s, p, l) : FALSE;
    }

    BOOL __stdcall VerQueryValueW(
        LPCVOID b, LPCWSTR s, LPVOID* p, PUINT l)
    {
        static auto fn = (decltype(&VerQueryValueW))
            GetProcAddress(g_realVersion, "VerQueryValueW");
        return fn ? fn(b, s, p, l) : FALSE;
    }

    DWORD __stdcall GetFileVersionInfoSizeExA(
        DWORD f, LPCSTR n, LPDWORD h)
    {
        static auto fn = (decltype(&GetFileVersionInfoSizeExA))
            GetProcAddress(g_realVersion, "GetFileVersionInfoSizeExA");
        return fn ? fn(f, n, h) : 0;
    }

    DWORD __stdcall GetFileVersionInfoSizeExW(
        DWORD f, LPCWSTR n, LPDWORD h)
    {
        static auto fn = (decltype(&GetFileVersionInfoSizeExW))
            GetProcAddress(g_realVersion, "GetFileVersionInfoSizeExW");
        return fn ? fn(f, n, h) : 0;
    }

    BOOL __stdcall GetFileVersionInfoExA(
        DWORD f, LPCSTR n, DWORD h, DWORD l, LPVOID d)
    {
        static auto fn = (decltype(&GetFileVersionInfoExA))
            GetProcAddress(g_realVersion, "GetFileVersionInfoExA");
        return fn ? fn(f, n, h, l, d) : FALSE;
    }

    BOOL __stdcall GetFileVersionInfoExW(
        DWORD f, LPCWSTR n, DWORD h, DWORD l, LPVOID d)
    {
        static auto fn = (decltype(&GetFileVersionInfoExW))
            GetProcAddress(g_realVersion, "GetFileVersionInfoExW");
        return fn ? fn(f, n, h, l, d) : FALSE;
    }

} // extern "C"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        // Limpiar log anterior
        { std::ofstream f("SimpleFPSLimiter_log.txt"); }
        Log("=== SimpleFPSLimiter iniciando ===");

        // Cargar version.dll REAL de System32
        wchar_t systemPath[MAX_PATH];
        GetSystemDirectoryW(systemPath, MAX_PATH);
        wcscat_s(systemPath, L"\\version.dll");
        Log("Cargando version.dll real...");
        g_realVersion = LoadLibraryW(systemPath);

        if (!g_realVersion) {
            Log("ERROR FATAL: no se pudo cargar version.dll de System32");
            return FALSE;
        }
        Log("version.dll real cargada OK");

        Config::Instance().Load("SimpleFPSLimiter.ini");
        Log("Config cargada");

        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            Log("Thread iniciado, esperando 2s...");
            Sleep(2000);
            Log("Instalando hooks DX12...");
            DX12Hook::Install();
            Log("Hooks instalados OK");
            return 0;
            }, nullptr, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        Log("DLL descargando...");
        DX12Hook::Uninstall();
        if (g_realVersion) FreeLibrary(g_realVersion);
        Log("=== SimpleFPSLimiter terminado ===");
    }
    return TRUE;
}