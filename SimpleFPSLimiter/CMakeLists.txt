cmake_minimum_required(VERSION 3.20)
project(SimpleFPSLimiter)

set(CMAKE_CXX_STANDARD 17)

add_library(SimpleFPSLimiter SHARED
    src/dllmain.cpp
    src/hooks/dx12_hook.cpp
    src/limiter/fps_limiter.cpp
    src/overlay/overlay.cpp
    src/config/config.cpp

    # ImGui core
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp

    # ImGui backends
    third_party/imgui/backends/imgui_impl_win32.cpp
    third_party/imgui/backends/imgui_impl_dx12.cpp
)

target_include_directories(SimpleFPSLimiter PRIVATE
    src
    third_party/imgui
    third_party/imgui/backends
    third_party/minhook/include
)

# MinHook como librería estática
add_library(minhook STATIC
    third_party/minhook/src/buffer.c
    third_party/minhook/src/hook.c
    third_party/minhook/src/trampoline.c
    third_party/minhook/src/hde/hde32.c
    third_party/minhook/src/hde/hde64.c
)
target_include_directories(minhook PRIVATE third_party/minhook/include)

target_link_libraries(SimpleFPSLimiter
    minhook
    d3d12
    dxgi
    winmm
)

# El output se llama version.dll
set_target_properties(SimpleFPSLimiter PROPERTIES OUTPUT_NAME "version")