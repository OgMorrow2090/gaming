/**
 * entry.cpp
 *
 * DLL entry point and Nexus addon definition for Mystic Chatter.
 * Exports GetAddonDef() which Nexus calls to register the addon.
 *
 * Sends predefined text messages to GW2 chat via clipboard paste.
 * Flow: Open chat (Enter) -> Paste text (Ctrl+V) -> Send (Enter)
 *
 * Author: OgMorrow2090
 * Repository: https://github.com/OgMorrow2090/guildwars2
 */

#include "shared.h"
#include <cstring>

// Global state
AddonAPI_t* APIDefs = nullptr;
AddonDefinition_t AddonDef{};
HWND GameWindow = nullptr;

// Forward declarations
void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();

/**
 * DLL Entry Point
 * Called by Windows when the DLL is loaded/unloaded.
 */
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

/**
 * GetAddonDef - Required Nexus Export
 *
 * Nexus calls this function to get addon metadata and callbacks.
 * Must be exported as a C function (no name mangling).
 */
extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
    // Signature must be unique and negative for non-Raidcore addons
    AddonDef.Signature = -54323;
    AddonDef.APIVersion = NEXUS_API_VERSION;

    // Addon metadata
    AddonDef.Name = "Mystic Chatter";
    AddonDef.Version.Major = 1;
    AddonDef.Version.Minor = 0;
    AddonDef.Version.Build = 0;
    AddonDef.Version.Revision = 0;
    AddonDef.Author = "OgMorrow2090";
    AddonDef.Description = "Quick chat macros - send predefined messages with a single keybind.";

    // Callbacks
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;

    // Optional features
    AddonDef.Flags = AF_None;

    // Auto-update from GitHub releases
    AddonDef.Provider = UP_GitHub;
    AddonDef.UpdateLink = "https://github.com/OgMorrow2090/guildwars2";

    return &AddonDef;
}

/**
 * AddonLoad - Called when addon is loaded by Nexus
 *
 * Initialize addon state and register keybinds.
 */
void AddonLoad(AddonAPI_t* aApi)
{
    APIDefs = aApi;

    // Get game window handle for input simulation
    GameWindow = FindWindowA("ArenaNet_Dx_Window_Class", nullptr);
    if (GameWindow == nullptr)
        GameWindow = FindWindowA("ArenaNet_Gr_Window_Class", nullptr);
    if (GameWindow == nullptr)
        GameWindow = FindWindowA(nullptr, "Guild Wars 2");
    if (GameWindow == nullptr)
        APIDefs->Log(LOGL_WARNING, "MysticChatter", "Game window not found at load - will try on keybind press");
    else
        APIDefs->Log(LOGL_INFO, "MysticChatter", "Game window found successfully!");

    // 14 chat macro keybinds
    APIDefs->InputBinds_RegisterWithString(CHAT_TY,    ProcessKeybind, "CTRL+SHIFT+KP1");
    APIDefs->InputBinds_RegisterWithString(CHAT_NP,    ProcessKeybind, "CTRL+SHIFT+KP2");
    APIDefs->InputBinds_RegisterWithString(CHAT_HI,    ProcessKeybind, "CTRL+SHIFT+KP3");
    APIDefs->InputBinds_RegisterWithString(CHAT_PARTY, ProcessKeybind, "CTRL+SHIFT+KP4");
    APIDefs->InputBinds_RegisterWithString(CHAT_SAY,   ProcessKeybind, "CTRL+SHIFT+KP5");
    APIDefs->InputBinds_RegisterWithString(CHAT_MAP,   ProcessKeybind, "CTRL+SHIFT+KP6");
    APIDefs->InputBinds_RegisterWithString(CHAT_GUILD, ProcessKeybind, "CTRL+SHIFT+KP7");
    APIDefs->InputBinds_RegisterWithString(CHAT_SQUAD, ProcessKeybind, "CTRL+SHIFT+KP8");
    APIDefs->InputBinds_RegisterWithString(CHAT_QHEAL, ProcessKeybind, "CTRL+SHIFT+KP9");
    APIDefs->InputBinds_RegisterWithString(CHAT_AHEAL, ProcessKeybind, "CTRL+SHIFT+KP0");
    APIDefs->InputBinds_RegisterWithString(CHAT_PLUS,  ProcessKeybind, "CTRL+SHIFT+KPADD");
    APIDefs->InputBinds_RegisterWithString(CHAT_MINUS, ProcessKeybind, "CTRL+SHIFT+KPSUB");
    APIDefs->InputBinds_RegisterWithString(CHAT_YES,   ProcessKeybind, "CTRL+SHIFT+KPDEC");
    APIDefs->InputBinds_RegisterWithString(CHAT_NO,    ProcessKeybind, "CTRL+SHIFT+KPMUL");

    APIDefs->Log(LOGL_INFO, "MysticChatter", "Addon loaded - 14 chat macros.");
}

/**
 * AddonUnload - Called when addon is unloaded by Nexus
 *
 * Clean up resources and deregister keybinds.
 */
void AddonUnload()
{
    APIDefs->InputBinds_Deregister(CHAT_TY);
    APIDefs->InputBinds_Deregister(CHAT_NP);
    APIDefs->InputBinds_Deregister(CHAT_HI);
    APIDefs->InputBinds_Deregister(CHAT_PARTY);
    APIDefs->InputBinds_Deregister(CHAT_SAY);
    APIDefs->InputBinds_Deregister(CHAT_MAP);
    APIDefs->InputBinds_Deregister(CHAT_GUILD);
    APIDefs->InputBinds_Deregister(CHAT_SQUAD);
    APIDefs->InputBinds_Deregister(CHAT_QHEAL);
    APIDefs->InputBinds_Deregister(CHAT_AHEAL);
    APIDefs->InputBinds_Deregister(CHAT_PLUS);
    APIDefs->InputBinds_Deregister(CHAT_MINUS);
    APIDefs->InputBinds_Deregister(CHAT_YES);
    APIDefs->InputBinds_Deregister(CHAT_NO);

    APIDefs->Log(LOGL_INFO, "MysticChatter", "Addon unloaded.");
    APIDefs = nullptr;
    GameWindow = nullptr;
}
