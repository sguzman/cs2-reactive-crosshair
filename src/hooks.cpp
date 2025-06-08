// src/hooks.cpp

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include <thread>

#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// ──────────────────────────────────────────────────────
// Offsets (from your csgo.json):
static constexpr uintptr_t dwLocalPlayer   = 0x00DEB99C;   // 14596508
static constexpr uintptr_t dwEntityList    = 0x04E0102C;   // 81793068
static constexpr uintptr_t m_iCrosshairId  = 0x00011838;   //  71736
static constexpr uintptr_t m_iTeamNum      = 0x000000F4;   //    244
// ──────────────────────────────────────────────────────

// Typedef for the original IDXGISwapChain::Present
using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
static PresentFn          oPresent = nullptr;
static ID3D11Device*      pDevice  = nullptr;
static ID3D11DeviceContext* pContext = nullptr;

// Checks whether the current crosshair is over an enemy
bool CheckCrosshairEnemy() {
    uintptr_t base        = (uintptr_t)GetModuleHandleA("client.dll");
    if (!base) return false;

    uintptr_t localPlayer = *(uintptr_t*)(base + dwLocalPlayer);
    if (!localPlayer) return false;

    int crossId = *(int*)(localPlayer + m_iCrosshairId);
    if (crossId <= 0 || crossId > 64) return false;

    uintptr_t entity = *(uintptr_t*)(base + dwEntityList + crossId * 0x10);
    if (!entity) return false;

    int localTeam = *(int*)(localPlayer + m_iTeamNum);
    int entTeam   = *(int*)(entity   + m_iTeamNum);
    return (entTeam != 0 && entTeam != localTeam);
}

// Our hooked Present
HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!pDevice) {
        // First-time init: grab D3D11 device/context and init ImGui
        pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice);
        pDevice->GetImmediateContext(&pContext);

        ImGui::CreateContext();
        ImGui_ImplWin32_Init(FindWindowA("Valve001", nullptr));
        ImGui_ImplDX11_Init(pDevice, pContext);
    }

    // Start new ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Draw the crosshair
    bool aiming = CheckCrosshairEnemy();
    ImU32 clr = aiming
      ? IM_COL32(255, 0,   0,   255)   // red
      : IM_COL32(255, 255, 255, 255);  // white

    ImVec2 sz = ImGui::GetIO().DisplaySize;
    ImVec2 c{ sz.x * 0.5f, sz.y * 0.5f };
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddLine({c.x - 10, c.y}, {c.x + 10, c.y}, clr, 2.0f);
    dl->AddLine({c.x, c.y - 10}, {c.x, c.y + 10}, clr, 2.0f);

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Call original Present
    return oPresent(pSwapChain, SyncInterval, Flags);
}

// Sets up and enables our hook on IDXGISwapChain::Present
void SetupHooks() {
    if (MH_Initialize() != MH_OK) return;

    // Create a dummy device/swapchain to grab the vtable
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = GetForegroundWindow();
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    ID3D11Device*       tmpDev   = nullptr;
    IDXGISwapChain*     tmpChain = nullptr;
    D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &tmpChain, &tmpDev, nullptr, nullptr);

    // Grab the Present function pointer (vtable index 8)
    void** vtable = *reinterpret_cast<void***>(tmpChain);
    void* presentAddr = vtable[8];

    // Hook it!
    MH_CreateHook(
      presentAddr,
      reinterpret_cast<LPVOID>(hkPresent),
      reinterpret_cast<void**>(&oPresent)
    );
    MH_EnableHook(presentAddr);

    // Clean up our dummy
    tmpChain->Release();
    tmpDev->Release();
}

// Removes and uninitializes all hooks
void UnhookAll() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

// DllMain to spawn our hook thread
BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        std::thread(SetupHooks).detach();
    }
    else if (reason == DLL_PROCESS_DETACH) {
        UnhookAll();
    }
    return TRUE;
}

