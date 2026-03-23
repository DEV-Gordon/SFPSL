#include "pch.h"
#include "overlay.h"
#include "../config/config.h"
#include "../limiter/fps_limiter.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

// Definición de statics
bool                       Overlay::s_initialized = false;
bool                       Overlay::s_visible = true;
ID3D12Device* Overlay::s_device = nullptr;
ID3D12DescriptorHeap* Overlay::s_rtvHeap = nullptr;
ID3D12DescriptorHeap* Overlay::s_srvHeap = nullptr;
ID3D12GraphicsCommandList* Overlay::s_cmdList = nullptr;
ID3D12CommandAllocator* Overlay::s_cmdAllocator = nullptr;
ID3D12CommandQueue* Overlay::s_cmdQueue = nullptr;
UINT                       Overlay::s_bufferCount = 0;
float                      Overlay::s_fpsAccum = 0.0f;
int                        Overlay::s_fpsFrames = 0;
float                      Overlay::s_fpsDisplay = 0.0f;
uint64_t                   Overlay::s_fpsLastTime = 0;

void Overlay::Init(IDXGISwapChain* swapChain, ID3D12CommandQueue* cmdQueue) {
    if (s_initialized) return;
    s_cmdQueue = cmdQueue;

    // Obtener el device desde el swapchain
    IDXGISwapChain3* swapChain3 = nullptr;
    swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3));
    if (!swapChain3) return;

    swapChain3->GetDevice(IID_PPV_ARGS(&s_device));
    if (!s_device) { swapChain3->Release(); return; }

    DXGI_SWAP_CHAIN_DESC desc{};
    swapChain3->GetDesc(&desc);
    s_bufferCount = desc.BufferCount;

    // Heap para render target views (uno por buffer)
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = s_bufferCount;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(s_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&s_rtvHeap))))
    {
        swapChain3->Release(); return;
    }

    // Heap para SRV — ImGui necesita uno para sus texturas internas
    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = 1;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(s_device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&s_srvHeap))))
    {
        swapChain3->Release(); return;
    }

    // Command allocator y lista para ImGui
    if (FAILED(s_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&s_cmdAllocator))))
    {
        swapChain3->Release(); return;
    }

    if (FAILED(s_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        s_cmdAllocator, nullptr, IID_PPV_ARGS(&s_cmdList))))
    {
        swapChain3->Release(); return;
    }

    s_cmdList->Close();

    // Inicializar ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename = nullptr;

    // Estilo: oscuro y minimalista
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.Alpha = 0.90f;
    style.WindowPadding = { 10.0f, 8.0f };

    // Colores personalizados — acento ámbar
    auto* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_SliderGrab] = ImVec4(0.85f, 0.55f, 0.10f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.70f, 0.15f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.85f, 0.55f, 0.10f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);

    ImGui_ImplWin32_Init(desc.OutputWindow);
    ImGui_ImplDX12_Init(
        s_device,
        s_bufferCount,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        s_srvHeap,
        s_srvHeap->GetCPUDescriptorHandleForHeapStart(),
        s_srvHeap->GetGPUDescriptorHandleForHeapStart()
    );

    swapChain3->Release();
    s_initialized = true;
}

void Overlay::Render() {
    // Toggle visibilidad con INSERT
    if (GetAsyncKeyState(VK_INSERT) & 1)
        s_visible = !s_visible;

    // Actualizar FPS counter siempre, aunque el overlay esté oculto
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    uint64_t now = ((uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime) * 100;
    s_fpsFrames++;
    if (now - s_fpsLastTime >= 500'000'000ULL) {
        s_fpsDisplay = s_fpsFrames * 2.0f;
        s_fpsFrames = 0;
        s_fpsLastTime = now;
    }

    if (!s_visible) return;

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize({ 280.0f, 110.0f }, ImGuiCond_Always);
    ImGui::SetNextWindowPos({ 16.0f, 16.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGui::Begin("FPS Limiter", nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar);

    ImGui::Text("FPS: %.0f", s_fpsDisplay);
    ImGui::SameLine(180.0f);
    ImGui::TextDisabled("INSERT: ocultar");
    ImGui::Separator();

    auto& cfg = Config::Instance();
    if (ImGui::Checkbox("Limitar FPS", &cfg.limiterEnabled))
        cfg.Save("SimpleFPSLimiter.ini");

    ImGui::BeginDisabled(!cfg.limiterEnabled);
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::SliderFloat("##fps", &cfg.targetFPS, 20.0f, 360.0f, "%.0f FPS"))
        if (!ImGui::IsItemActive())
            cfg.Save("SimpleFPSLimiter.ini");
    ImGui::EndDisabled();

    ImGui::End();
    ImGui::Render();

    // Preparar el command list
    s_cmdAllocator->Reset();
    s_cmdList->Reset(s_cmdAllocator, nullptr);

    // Decirle a DX12 qué descriptor heap usar (necesario para las texturas de ImGui)
    ID3D12DescriptorHeap* heaps[] = { s_srvHeap };
    s_cmdList->SetDescriptorHeaps(1, heaps);

    // Enviar los vértices/índices de ImGui al command list
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), s_cmdList);
    s_cmdList->Close();

    // Ejecutar el command list en la GPU — sin esto ImGui no se dibuja
    ID3D12CommandList* lists[] = { s_cmdList };
    s_cmdQueue->ExecuteCommandLists(1, lists);
}

void Overlay::OnResize() {
    if (!s_initialized) return;
    ImGui_ImplDX12_InvalidateDeviceObjects();
    ImGui_ImplDX12_CreateDeviceObjects();
}

void Overlay::Shutdown() {
    if (!s_initialized) return;
    Config::Instance().Save("SimpleFPSLimiter.ini");
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (s_cmdList)      s_cmdList->Release();
    if (s_cmdAllocator) s_cmdAllocator->Release();
    if (s_srvHeap)      s_srvHeap->Release();
    if (s_rtvHeap)      s_rtvHeap->Release();
    s_cmdQueue = nullptr; // no hacemos Release — no es nuestro, es del juego
    s_device = nullptr; // ídem
    s_initialized = false;
}