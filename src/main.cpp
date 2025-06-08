// src/main.cpp
#include <Windows.h>
#include <thread>
#include "MinHook.h"

// Forward declarations
void SetupHooks();
void UnhookAll();

HMODULE g_hModule = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        std::thread(SetupHooks).detach();
    }
    else if (reason == DLL_PROCESS_DETACH) {
        UnhookAll();
    }
    return TRUE;
}

