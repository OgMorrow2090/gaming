/**
 * entry.cpp
 *
 * DLL entry point and Nexus addon definition for Mystic AI — the Claude-vision
 * screen reader for Guild Wars 2: freeze-frame drag-select capture, spoken
 * answers, and a saved book region. Companion to the host-side
 * gw2-claude-daemon.
 *
 * Mystic AI is a sibling of Mystic Clicker; the Claude-vision feature was spun
 * out of that addon into this dedicated module.
 *
 * Author: OgMorrow2090
 * Repository: https://github.com/OgMorrow2090/guildwars2
 */

#include "shared.h"
#include "screen-capture.h"
#include "icons.h"
#include "imgui/imgui.h"

// Global state
AddonAPI_t*       APIDefs    = nullptr;
AddonDefinition_t AddonDef{};
HMODULE           SelfModule = nullptr;
HWND              GameWindow = nullptr;
ImGuiContext*     ImGuiCtx   = nullptr;

void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();

/**
 * DLL entry point. Capture this DLL's module handle — Textures_GetOrCreateFrom-
 * Resource needs it to find the embedded icon PNGs.
 */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    (void)lpReserved;
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        SelfModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

/**
 * GetAddonDef - required Nexus export. Nexus calls this to register the addon.
 */
extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
    // Signature must be unique per addon — Mystic Clicker is -54321, Mystic
    // Trading is -54322.
    AddonDef.Signature  = static_cast<uint32_t>(-54323);
    AddonDef.APIVersion = NEXUS_API_VERSION;

    AddonDef.Name             = "Mystic AI";
    AddonDef.Version.Major    = 1;
    AddonDef.Version.Minor    = 1;
    AddonDef.Version.Build    = 9;
    AddonDef.Version.Revision = 0;
    AddonDef.Author      = "OgMorrow2090";
    AddonDef.Description = "Freeze-frame screen reader for GW2 - drag-select any "
                           "text and Claude reads tooltips, books, dialog and "
                           "trading-post prices aloud.";

    AddonDef.Load   = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags  = AF_None;

    AddonDef.Provider   = UP_GitHub;
    AddonDef.UpdateLink = "https://github.com/OgMorrow2090/guildwars2";

    return &AddonDef;
}

/**
 * AddonLoad - initialise state, register render callback, hooks and keybinds.
 */
void AddonLoad(AddonAPI_t* aApi)
{
    APIDefs = aApi;

    // Per-resolution settings.
    SetConfigPath(APIDefs->Paths_GetAddonDirectory("MysticAI"));
    LoadSettings();

    // Game window — used for resolution detection. Try the known class names,
    // then the title.
    GameWindow = FindWindowA("ArenaNet_Dx_Window_Class", nullptr);
    if (GameWindow == nullptr)
        GameWindow = FindWindowA("ArenaNet_Gr_Window_Class", nullptr);
    if (GameWindow == nullptr)
        GameWindow = FindWindowA(nullptr, "Guild Wars 2");

    // ImGui context (shared with Nexus).
    ImGuiCtx = (ImGuiContext*)APIDefs->ImguiContext;
    ImGui::SetCurrentContext(ImGuiCtx);
    ImGui::SetAllocatorFunctions(
        (void* (*)(size_t, void*))APIDefs->ImguiMalloc,
        (void  (*)(void*,  void*))APIDefs->ImguiFree);

    // Render callback, the D3D11 back-buffer capture hook, and the WndProc hook
    // (lets Esc close Mystic AI without GW2 also closing its book / inventory).
    APIDefs->GUI_Register(RT_Render, RenderMysticAI);
    RegisterScreenCaptureHook();
    APIDefs->WndProc_Register(MysticAIWndProc);

    // Embedded GW2-style icons + a Nexus Quick Access shortcut that fires the
    // capture keybind when clicked.
    Icons::Load();
    APIDefs->QuickAccess_Add("MYSTIC_AI_SHORTCUT", Icons::QA, Icons::QA_HOVER,
                             MYSTIC_AI_CAPTURE,
                             "Mystic AI - drag-select to read the screen");

    // Keybinds — unbound by default so the player binds them (controller-
    // friendly: bind the capture key to a button, the region reads to
    // double-presses).
    APIDefs->InputBinds_RegisterWithString(MYSTIC_AI_CAPTURE,   ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(MYSTIC_AI_READ_BOOK, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(MYSTIC_AI_READ,      ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(MYSTIC_AI_TP_REGION, ProcessKeybind, "(null)");

    APIDefs->Log(LOGL_INFO, "MysticAI", "Mystic AI loaded.");
}

/**
 * AddonUnload - deregister everything and release resources.
 */
void AddonUnload()
{
    APIDefs->InputBinds_Deregister(MYSTIC_AI_CAPTURE);
    APIDefs->InputBinds_Deregister(MYSTIC_AI_READ_BOOK);
    APIDefs->InputBinds_Deregister(MYSTIC_AI_READ);
    APIDefs->InputBinds_Deregister(MYSTIC_AI_TP_REGION);
    APIDefs->QuickAccess_Remove("MYSTIC_AI_SHORTCUT");
    APIDefs->WndProc_Deregister(MysticAIWndProc);
    APIDefs->GUI_Deregister(RenderMysticAI);
    DeregisterScreenCaptureHook();
    ShutdownOverlay();

    APIDefs->Log(LOGL_INFO, "MysticAI", "Mystic AI unloaded.");
    APIDefs    = nullptr;
    GameWindow = nullptr;
}
