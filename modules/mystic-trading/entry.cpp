/**
 * entry.cpp
 *
 * DLL entry point and Nexus addon definition for Mystic Trading.
 * In-game overlay for trading post data from the addams.family portal.
 *
 * Author: OgMorrow2090
 * Repository: https://github.com/OgMorrow2090/guildwars2
 */

#include "shared.h"
#include <cstring>

// ImGui includes
#include "imgui/imgui.h"

// Global state
AddonAPI_t* APIDefs = nullptr;
AddonDefinition_t AddonDef{};
ImGuiContext* ImGuiCtx = nullptr;

bool g_ShowDashboard = false;
bool g_ShowFlipList = false;

PortalData g_Data{};
std::mutex g_DataMutex;

int g_RefreshInterval = 30;

// Forward declarations
void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
    AddonDef.Signature = -54322;
    AddonDef.APIVersion = NEXUS_API_VERSION;

    AddonDef.Name = "Mystic Trading";
    AddonDef.Version.Major = 0;
    AddonDef.Version.Minor = 1;
    AddonDef.Version.Build = 0;
    AddonDef.Version.Revision = 0;
    AddonDef.Author = "OgMorrow2090";
    AddonDef.Description = "In-game trading post overlay with flips, orders, bank, and materials. Standalone — talks directly to GW2 API and GW2BLTC.";

    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;

    AddonDef.Flags = AF_None;

    AddonDef.Provider = UP_GitHub;
    AddonDef.UpdateLink = "https://github.com/OgMorrow2090/guildwars2";

    return &AddonDef;
}

void AddonLoad(AddonAPI_t* aApi)
{
    APIDefs = aApi;

    // Set up ImGui context
    ImGuiCtx = (ImGuiContext*)APIDefs->ImguiContext;
    ImGui::SetCurrentContext(ImGuiCtx);
    ImGui::SetAllocatorFunctions(
        (void* (*)(size_t, void*))APIDefs->ImguiMalloc,
        (void (*)(void*, void*))APIDefs->ImguiFree
    );

    // Load config (portal URL, refresh interval)
    SetApiConfig(APIDefs->Paths_GetAddonDirectory("MysticTrading"));
    LoadConfig();

    // Register render callbacks
    APIDefs->GUI_Register(RT_Render, RenderDashboard);
    APIDefs->GUI_Register(RT_Render, RenderFlipList);
    APIDefs->GUI_Register(RT_OptionsRender, RenderOptions);

    // Register close-on-escape
    APIDefs->GUI_RegisterCloseOnEscape("Mystic Trading Dashboard", &g_ShowDashboard);
    APIDefs->GUI_RegisterCloseOnEscape("Mystic Trading Flips", &g_ShowFlipList);

    // Register keybinds
    APIDefs->InputBinds_RegisterWithString(KB_TOGGLE_DASHBOARD, ProcessKeybind, "ALT+T");
    APIDefs->InputBinds_RegisterWithString(KB_TOGGLE_FLIPLIST, ProcessKeybind, "ALT+F");

    // Start background data fetching
    StartDataFetch();

    APIDefs->Log(LOGL_INFO, "MysticTrading", "Addon loaded - dashboard (ALT+T), flip list (ALT+F).");
}

void AddonUnload()
{
    // Stop background fetch
    StopDataFetch();

    // Save config
    SaveConfig();

    // Deregister render callbacks
    APIDefs->GUI_Deregister(RenderDashboard);
    APIDefs->GUI_Deregister(RenderFlipList);
    APIDefs->GUI_Deregister(RenderOptions);

    // Deregister close-on-escape
    APIDefs->GUI_DeregisterCloseOnEscape("Mystic Trading Dashboard");
    APIDefs->GUI_DeregisterCloseOnEscape("Mystic Trading Flips");

    // Deregister keybinds
    APIDefs->InputBinds_Deregister(KB_TOGGLE_DASHBOARD);
    APIDefs->InputBinds_Deregister(KB_TOGGLE_FLIPLIST);

    APIDefs->Log(LOGL_INFO, "MysticTrading", "Addon unloaded.");
    APIDefs = nullptr;
    ImGuiCtx = nullptr;
}
