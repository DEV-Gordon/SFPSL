#include "pch.h"
#include "dx12_hook.h"
#include "../overlay/overlay.h"
#include "../limiter/fps_limiter.h"
#include <MinHook.h>

// Punteros a las funciones originales
typedef HRESULT(STDMETHODCALLTYPE* PFN_Present)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_ResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef void(STDMETHODCALLTYPE* PFN_ExecuteCommandLists)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

static PFN_Present              oPresent = nullptr;
static PFN_ResizeBuffers        oResizeBuffers = nullptr;
static PFN_ExecuteCommandLists  oExecuteCommandLists = nullptr;

// Capturamos el CommandQueue aquí — ImGui DX12 lo necesita
static ID3D12CommandQueue* g_commandQueue = nullptr;

// ExecuteCommandLists: lo hookeamos solo para capturar el CommandQueue
// El juego lo llama antes de Present, así que cuando llegue Present
// ya tenemos el puntero que necesitamos
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
    // Inicializar el overlay la primera vez que tenemos todo listo
    if (!Overlay::IsInitialized() && g_commandQueue)
        Overlay::Init(pSwapChain, g_commandQueue);

    // Renderizar overlay (calcula ImGui::Render internamente)
    if (Overlay::IsInitialized())
        Overlay::Render();

    // Limitar FPS antes de presentar
    FPSLimiter::Tick();

    return oPresent(pSwapChain, SyncInterval, Flags);
}

HRESULT STDMETHODCALLTYPE HookedResizeBuffers(
    IDXGISwapChain* pSwapChain,
    UINT bufferCount, UINT width, UINT height,
    DXGI_FORMAT format, UINT flags)
{
    // Cuando cambia el tamaño de ventana, ImGui necesita reinicializarse
    Overlay::OnResize();
    return oResizeBuffers(pSwapChain, bufferCount, width, height, format, flags);
}

void DX12Hook::Install() {
    // — Crear SwapChain temporal para leer la vtable —
    ID3D12Device* device = nullptr;
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (!device) return;

    D3D12_COMMAND_QUEUE_DESC qDesc{};
    ID3D12CommandQueue* queue = nullptr;
    device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&queue));

    IDXGIFactory4* factory = nullptr;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));

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

    // Leer las vtables
    // SwapChain vtable: [8] = Present, [13] = ResizeBuffers
    // CommandQueue vtable: [10] = ExecuteCommandLists
    void** scVtable = *reinterpret_cast<void***>(swapChain);
    void** cqVtable = *reinterpret_cast<void***>(queue);

    // Instalar los tres hooks
    MH_Initialize();
    MH_CreateHook(scVtable[8], reinterpret_cast<void*>(&HookedPresent),
        reinterpret_cast<void**>(&oPresent));
    MH_CreateHook(scVtable[13], reinterpret_cast<void*>(&HookedResizeBuffers),
        reinterpret_cast<void**>(&oResizeBuffers));
    MH_CreateHook(cqVtable[10], reinterpret_cast<void*>(&HookedExecuteCommandLists),
        reinterpret_cast<void**>(&oExecuteCommandLists));
    MH_EnableHook(MH_ALL_HOOKS);

    // Limpiar objetos temporales
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