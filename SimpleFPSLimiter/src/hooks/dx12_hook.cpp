#include "pch.h"
#include "dx12_hook.h"
#include "../overlay/overlay.h"
#include "../limiter/fps_limiter.h"
#include <MinHook.h>

typedef HRESULT(STDMETHODCALLTYPE* PFN_Present)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_ResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef void(STDMETHODCALLTYPE* PFN_ExecuteCommandLists)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

static PFN_Present             oPresent = nullptr;
static PFN_ResizeBuffers       oResizeBuffers = nullptr;
static PFN_ExecuteCommandLists oExecuteCommandLists = nullptr;
static ID3D12CommandQueue* g_commandQueue = nullptr;

void STDMETHODCALLTYPE HookedExecuteCommandLists(
    ID3D12CommandQueue* queue,
    UINT numLists,
    ID3D12CommandList* const* lists)
{
    if (!g_commandQueue)
        g_commandQueue = queue;
    oExecuteCommandLists(queue, numLists, lists);
}

HRESULT STDMETHODCALLTYPE HookedPresent(
    IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    if (!Overlay::IsInitialized() && g_commandQueue)
        Overlay::Init(pSwapChain, g_commandQueue);

    if (Overlay::IsInitialized())
        Overlay::Render();

    FPSLimiter::Tick();

    return oPresent(pSwapChain, SyncInterval, Flags);
}

HRESULT STDMETHODCALLTYPE HookedResizeBuffers(
    IDXGISwapChain* pSwapChain,
    UINT bufferCount, UINT width, UINT height,
    DXGI_FORMAT format, UINT flags)
{
    Overlay::OnResize();
    return oResizeBuffers(pSwapChain, bufferCount,
        width, height, format, flags);
}

void DX12Hook::Install() {
    // FIX CRÍTICO: cargar d3d12.dll y dxgi.dll explícitamente de System32
    // para que CreateDXGIFactory1 no llame a nuestra propia función exportada
    wchar_t sysPath[MAX_PATH];
    GetSystemDirectoryW(sysPath, MAX_PATH);

    wchar_t dxgiPath[MAX_PATH], d3d12Path[MAX_PATH];
    wcscpy_s(dxgiPath, sysPath); wcscat_s(dxgiPath, L"\\dxgi.dll");
    wcscpy_s(d3d12Path, sysPath); wcscat_s(d3d12Path, L"\\d3d12.dll");

    HMODULE hDxgi = LoadLibraryW(dxgiPath);
    HMODULE hD3D12 = LoadLibraryW(d3d12Path);
    if (!hDxgi || !hD3D12) return;

    // Obtener punteros a las funciones REALES de System32
    auto realCreateFactory = (decltype(&CreateDXGIFactory1))
        GetProcAddress(hDxgi, "CreateDXGIFactory1");
    auto realCreateDevice = (PFN_D3D12_CREATE_DEVICE)
        GetProcAddress(hD3D12, "D3D12CreateDevice");
    if (!realCreateFactory || !realCreateDevice) return;

    // Crear objetos temporales usando las funciones REALES
    ID3D12Device* device = nullptr;
    realCreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device));
    if (!device) return;

    D3D12_COMMAND_QUEUE_DESC qDesc{};
    ID3D12CommandQueue* queue = nullptr;
    device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&queue));

    IDXGIFactory4* factory = nullptr;
    realCreateFactory(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC scDesc{};
    scDesc.BufferCount = 2;
    scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.OutputWindow = GetForegroundWindow();
    scDesc.SampleDesc.Count = 1;
    scDesc.Windowed = TRUE;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain* swapChain = nullptr;
    factory->CreateSwapChain(queue, &scDesc, &swapChain);
    if (!swapChain) {
        queue->Release(); device->Release(); factory->Release();
        return;
    }

    void** scVtable = *reinterpret_cast<void***>(swapChain);
    void** cqVtable = *reinterpret_cast<void***>(queue);

    MH_Initialize();
    MH_CreateHook(scVtable[8],
        reinterpret_cast<void*>(&HookedPresent),
        reinterpret_cast<void**>(&oPresent));
    MH_CreateHook(scVtable[13],
        reinterpret_cast<void*>(&HookedResizeBuffers),
        reinterpret_cast<void**>(&oResizeBuffers));
    MH_CreateHook(cqVtable[10],
        reinterpret_cast<void*>(&HookedExecuteCommandLists),
        reinterpret_cast<void**>(&oExecuteCommandLists));
    MH_EnableHook(MH_ALL_HOOKS);

    swapChain->Release();
    queue->Release();
    device->Release();
    factory->Release();
}

void DX12Hook::Uninstall() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    Overlay::Shutdown();
}