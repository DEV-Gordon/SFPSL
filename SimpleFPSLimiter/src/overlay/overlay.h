#pragma once
#include "pch.h"
#include <dxgi1_4.h>

class Overlay {
public:
    static void Init(IDXGISwapChain* swapChain, ID3D12CommandQueue* cmdQueue);
    static void Render();
    static void Shutdown();
    static void OnResize(); // llamar cuando la ventana cambie de tamaño

    static bool IsInitialized() { return s_initialized; }

private:
    static bool                        s_initialized;
    static bool                        s_visible;
    static ID3D12Device* s_device;
    static ID3D12DescriptorHeap* s_rtvHeap;
    static ID3D12DescriptorHeap* s_srvHeap;
    static ID3D12GraphicsCommandList* s_cmdList;
    static ID3D12CommandAllocator* s_cmdAllocator;
    static ID3D12CommandQueue* s_cmdQueue;
    static UINT                        s_bufferCount;

    // Para medir FPS real
    static float   s_fpsAccum;
    static int     s_fpsFrames;
    static float   s_fpsDisplay;
    static uint64_t s_fpsLastTime;
};