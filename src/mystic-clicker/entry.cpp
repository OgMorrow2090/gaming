/**
 * entry.cpp
 *
 * DLL entry point and Nexus addon definition for Mystic Clicker.
 * Exports GetAddonDef() which Nexus calls to register the addon.
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
    AddonDef.Signature = -54321;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    
    // Addon metadata
    AddonDef.Name = "Mystic Clicker";
    AddonDef.Version.Major = 2;
    AddonDef.Version.Minor = 0;
    AddonDef.Version.Build = 0;
    AddonDef.Version.Revision = 0;
    AddonDef.Author = "OgMorrow2090";
    AddonDef.Description = "One-click hotkeys for inventory, vendors, trading post, Mystic Forge, and more.";
    
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
    
    // Set config path and load saved positions
    SetConfigPath(APIDefs->Paths_GetAddonDirectory("MysticClicker"),
                  APIDefs->Paths_GetAddonDirectory("MysticClicker"));
    LoadButtonPositions();
    
    // Get game window handle for input simulation
    // Try multiple window class names that GW2 might use
    GameWindow = FindWindowA("ArenaNet_Dx_Window_Class", nullptr);
    if (GameWindow == nullptr)
    {
        GameWindow = FindWindowA("ArenaNet_Gr_Window_Class", nullptr);
    }
    if (GameWindow == nullptr)
    {
        // Try finding by window title
        GameWindow = FindWindowA(nullptr, "Guild Wars 2");
    }
    if (GameWindow == nullptr)
    {
        // Last resort: get foreground window (will be set on first keybind press)
        APIDefs->Log(LOGL_WARNING, "MysticClicker", "Game window not found at load - will try on keybind press");
    }
    else
    {
        APIDefs->Log(LOGL_INFO, "MysticClicker", "Game window found successfully!");
    }
    
    // 13 action keybinds
    APIDefs->InputBinds_RegisterWithString(DEPOSIT_MATERIALS, ProcessKeybind, "CTRL+D");
    APIDefs->InputBinds_RegisterWithString(SORT_INVENTORY, ProcessKeybind, "CTRL+S");
    APIDefs->InputBinds_RegisterWithString(OPEN_CHEST, ProcessKeybind, "CTRL+B");
    APIDefs->InputBinds_RegisterWithString(DEPOSIT_AND_SORT, ProcessKeybind, "CTRL+Q");
    APIDefs->InputBinds_RegisterWithString(SELL_JUNK, ProcessKeybind, "CTRL+J");
    APIDefs->InputBinds_RegisterWithString(TRADING_POST, ProcessKeybind, "CTRL+O");
    APIDefs->InputBinds_RegisterWithString(VENDOR_BUY, ProcessKeybind, "CTRL+U");
    APIDefs->InputBinds_RegisterWithString(EXIT_INSTANCE, ProcessKeybind, "CTRL+E");
    APIDefs->InputBinds_RegisterWithString(YES_DIALOG, ProcessKeybind, "CTRL+Y");
    APIDefs->InputBinds_RegisterWithString(MYSTIC_FORGE, ProcessKeybind, "CTRL+F");
    APIDefs->InputBinds_RegisterWithString(MYSTIC_REFILL, ProcessKeybind, "CTRL+R");
    APIDefs->InputBinds_RegisterWithString(MYSTIC_FORGE_COMBO, ProcessKeybind, "CTRL+M");
    APIDefs->InputBinds_RegisterWithString(TP_REMOVE, ProcessKeybind, "CTRL+T");
    
    // 11 capture keybinds
    APIDefs->InputBinds_RegisterWithString(CAPTURE_DEPOSIT, ProcessKeybind, "CTRL+SHIFT+D");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_SORT, ProcessKeybind, "CTRL+SHIFT+S");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_CHEST, ProcessKeybind, "CTRL+SHIFT+B");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_EXIT_INSTANCE, ProcessKeybind, "CTRL+SHIFT+E");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_YES_DIALOG, ProcessKeybind, "CTRL+SHIFT+Y");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_MYSTIC_FORGE, ProcessKeybind, "CTRL+SHIFT+F");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_MYSTIC_REFILL, ProcessKeybind, "CTRL+SHIFT+R");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_VENDOR_BUY, ProcessKeybind, "CTRL+SHIFT+U");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_SELL_JUNK, ProcessKeybind, "CTRL+SHIFT+J");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_TRADING_POST, ProcessKeybind, "CTRL+SHIFT+O");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_TP_REMOVE, ProcessKeybind, "CTRL+SHIFT+T");

    APIDefs->Log(LOGL_INFO, "MysticClicker", "Addon loaded - 24 keybinds.");
}

/**
 * AddonUnload - Called when addon is unloaded by Nexus
 * 
 * Clean up resources and deregister keybinds.
 */
void AddonUnload()
{
    // Deregister action keybinds
    APIDefs->InputBinds_Deregister(DEPOSIT_MATERIALS);
    APIDefs->InputBinds_Deregister(SORT_INVENTORY);
    APIDefs->InputBinds_Deregister(OPEN_CHEST);
    APIDefs->InputBinds_Deregister(DEPOSIT_AND_SORT);
    APIDefs->InputBinds_Deregister(SELL_JUNK);
    APIDefs->InputBinds_Deregister(TRADING_POST);
    APIDefs->InputBinds_Deregister(VENDOR_BUY);
    APIDefs->InputBinds_Deregister(EXIT_INSTANCE);
    APIDefs->InputBinds_Deregister(YES_DIALOG);
    APIDefs->InputBinds_Deregister(MYSTIC_FORGE);
    APIDefs->InputBinds_Deregister(MYSTIC_REFILL);
    APIDefs->InputBinds_Deregister(MYSTIC_FORGE_COMBO);
    APIDefs->InputBinds_Deregister(TP_REMOVE);
    
    // Deregister capture keybinds
    APIDefs->InputBinds_Deregister(CAPTURE_DEPOSIT);
    APIDefs->InputBinds_Deregister(CAPTURE_SORT);
    APIDefs->InputBinds_Deregister(CAPTURE_CHEST);
    APIDefs->InputBinds_Deregister(CAPTURE_EXIT_INSTANCE);
    APIDefs->InputBinds_Deregister(CAPTURE_YES_DIALOG);
    APIDefs->InputBinds_Deregister(CAPTURE_MYSTIC_FORGE);
    APIDefs->InputBinds_Deregister(CAPTURE_MYSTIC_REFILL);
    APIDefs->InputBinds_Deregister(CAPTURE_VENDOR_BUY);
    APIDefs->InputBinds_Deregister(CAPTURE_SELL_JUNK);
    APIDefs->InputBinds_Deregister(CAPTURE_TRADING_POST);
    APIDefs->InputBinds_Deregister(CAPTURE_TP_REMOVE);
    
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Addon unloaded.");
    APIDefs = nullptr;
    GameWindow = nullptr;
}
