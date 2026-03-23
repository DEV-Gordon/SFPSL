#include "pch.h"
#include "hooks/dx12_hook.h"
#include "config/config.h"
#include <fstream>
#include <ctime>

static HMODULE g_realDxgi = nullptr;

// Logger simple — escribe en un .txt junto al juego
static void Log(const char* msg) {
    std::ofstream f("SimpleFPSLimiter_log.txt", std::ios::app);
    if (f.is_open()) f << msg << "\n";
}

extern "C" {

    HRESULT __stdcall CreateDXGIFactory(REFIID riid, void** ppFactory) {
        Log("CreateDXGIFactory llamado");
        static auto fn = (decltype(&CreateDXGIFactory))
            GetProcAddress(g_realDxgi, "CreateDXGIFactory");
        if (!fn) { Log("ERROR: CreateDXGIFactory no encontrado"); return E_FAIL; }
        return fn(riid, ppFactory);
    }

    HRESULT __stdcall CreateDXGIFactory1(REFIID riid, void** ppFactory) {
        Log("CreateDXGIFactory1 llamado");
        static auto fn = (decltype(&CreateDXGIFactory1))
            GetProcAddress(g_realDxgi, "CreateDXGIFactory1");
        if (!fn) { Log("ERROR: CreateDXGIFactory1 no encontrado"); return E_FAIL; }
        return fn(riid, ppFactory);
    }

    HRESULT __stdcall CreateDXGIFactory2(UINT flags, REFIID riid, void** ppFactory) {
        Log("CreateDXGIFactory2 llamado");
        static auto fn = (decltype(&CreateDXGIFactory2))
            GetProcAddress(g_realDxgi, "CreateDXGIFactory2");
        if (!fn) { Log("ERROR: CreateDXGIFactory2 no encontrado"); return E_FAIL; }
        return fn(flags, riid, ppFactory);
    }

    HRESULT __stdcall DXGIGetDebugInterface1(UINT flags, REFIID riid, void** pDebug) {
        Log("DXGIGetDebugInterface1 llamado");
        static auto fn = (decltype(&DXGIGetDebugInterface1))
            GetProcAddress(g_realDxgi, "DXGIGetDebugInterface1");
        if (!fn) { Log("ERROR: DXGIGetDebugInterface1 no encontrado"); return E_FAIL; }
        return fn(flags, riid, pDebug);
    }

} // extern "C"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        // Limpiar log anterior
        { std::ofstream f("SimpleFPSLimiter_log.txt"); }
        Log("=== SimpleFPSLimiter iniciando ===");

        // Cargar dxgi.dll real de System32
        wchar_t systemPath[MAX_PATH];
        GetSystemDirectoryW(systemPath, MAX_PATH);
        wcscat_s(systemPath, L"\\dxgi.dll");
        Log("Cargando dxgi.dll real...");
        g_realDxgi = LoadLibraryW(systemPath);

        if (!g_realDxgi) {
            Log("ERROR FATAL: no se pudo cargar dxgi.dll de System32");
            return FALSE;
        }
        Log("dxgi.dll real cargada OK");

        Config::Instance().Load("SimpleFPSLimiter.ini");
        Log("Config cargada");

        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            Log("Thread de hooks iniciado, esperando...");
            Sleep(2000);
            Log("Instalando hooks DX12...");
            DX12Hook::Install();
            Log("Hooks instalados");
            return 0;
            }, nullptr, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        Log("DLL descargando...");
        DX12Hook::Uninstall();
        if (g_realDxgi) FreeLibrary(g_realDxgi);
        Log("=== SimpleFPSLimiter terminado ===");
    }
    return TRUE;
}