// src/hooks.cpp
#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// Typedef for the original Present
using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
static PresentFn oPresent = nullptr;

// D3D11 globals
static ID3D11Device*       pDevice  = nullptr;
static ID3D11DeviceContext* pContext = nullptr;

// Simple “is crosshair over enemy?” stub
bool CheckCrosshairEnemy() {
    // You must fill in the offsets from hazedumper or CE here
    // e.g. uintptr_t base = (uintptr_t)GetModuleHandleA("client.dll");
    // int crossId = *(int*)(localPlayer + OFFSET_CROSSHAIR_ID);
    // ...
    return false; // placeholder
}

// Hooked Present
HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!pDevice) {
        // First call: initialize ImGui
        pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice);
        pDevice->GetImmediateContext(&pContext);

        ImGui::CreateContext();
        ImGui_ImplWin32_Init(FindWindowA("Valve001", nullptr));
        ImGui_ImplDX11_Init(pDevice, pContext);
    }

    // New frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Draw crosshair
    bool aiming = CheckCrosshairEnemy();
    ImU32 color = aiming
      ? IM_COL32(255, 0,   0,   255)
      : IM_COL32(255, 255, 255, 255);
    ImVec2 sz = ImGui::GetIO().DisplaySize;
    ImVec2 center{ sz.x * 0.5f, sz.y * 0.5f };
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddLine({center.x - 10, center.y}, {center.x + 10, center.y}, color, 2.0f);
    dl->AddLine({center.x, center.y - 10}, {center.x, center.y + 10}, color, 2.0f);

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Call original
    return oPresent(pSwapChain, SyncInterval, Flags);
}

void SetupHooks() {
    // initialize MinHook
    if (MH_Initialize() != MH_OK) return;

    // Create a dummy device/swapchain to grab vtable
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = GetForegroundWindow();
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    ID3D11Device* tmpDev = nullptr;
    IDXGISwapChain* tmpChain = nullptr;
    D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
      nullptr, 0, D3D11_SDK_VERSION,
      &sd, &tmpChain, &tmpDev, nullptr, nullptr);

    // Grab Present from vtable index 8
    auto vtable = *reinterpret_cast<void***>(tmpChain);
    void* presentAddr = vtable[8];

    // Hook it
    MH_CreateHook(presentAddr, hkPresent, reinterpret_cast<void**>(&oPresent));
    MH_EnableHook(presentAddr);

    // cleanup dummy
    tmpChain->Release();
    tmpDev->Release();
}

void UnhookAll() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

