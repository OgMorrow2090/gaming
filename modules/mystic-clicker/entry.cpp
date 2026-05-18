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
#include "screen-capture.h"
#include "imgui/imgui.h"
#include <cstring>

// Global state
AddonAPI_t* APIDefs = nullptr;
AddonDefinition_t AddonDef{};
HWND GameWindow = nullptr;
ImGuiContext* ImGuiCtx = nullptr;

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
    AddonDef.Version.Major = 3;
    AddonDef.Version.Minor = 6;
    AddonDef.Version.Build = 22;
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
    
    // Set up ImGui context
    ImGuiCtx = (ImGuiContext*)APIDefs->ImguiContext;
    ImGui::SetCurrentContext(ImGuiCtx);
    ImGui::SetAllocatorFunctions(
        (void* (*)(size_t, void*))APIDefs->ImguiMalloc,
        (void (*)(void*, void*))APIDefs->ImguiFree);

    // Register capture window render callback
    APIDefs->GUI_Register(RT_Render, RenderCaptureWindow);
    APIDefs->GUI_RegisterCloseOnEscape("Mystic Clicker - Capture", &g_ShowCaptureWindow);

    // Register the D3D11 back-buffer screen-capture hook (used by OCR macros).
    RegisterScreenCaptureHook();

    // Capture mode keybind
    APIDefs->InputBinds_RegisterWithString(CAPTURE_MODE, ProcessKeybind, "CTRL+SHIFT+C");
    APIDefs->InputBinds_RegisterWithString(RESET_WINDOWS, ProcessKeybind, "CTRL+SHIFT+INSERT");

    // 13 action keybinds
    APIDefs->InputBinds_RegisterWithString(DEPOSIT_MATERIALS, ProcessKeybind, "CTRL+D");
    APIDefs->InputBinds_RegisterWithString(SORT_INVENTORY, ProcessKeybind, "CTRL+S");
    APIDefs->InputBinds_RegisterWithString(OPEN_CHEST, ProcessKeybind, "CTRL+B");
    APIDefs->InputBinds_RegisterWithString(BOUNCY_ACCEPT, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(OPEN_CHEST_COMBO, ProcessKeybind, "CTRL+SHIFT+F12");
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
    APIDefs->InputBinds_RegisterWithString(CRAFT, ProcessKeybind, "CTRL+F1");
    APIDefs->InputBinds_RegisterWithString(CRAFT_ALL, ProcessKeybind, "CTRL+F2");
    APIDefs->InputBinds_RegisterWithString(WIZARD_VAULT, ProcessKeybind, "CTRL+F4");
    APIDefs->InputBinds_RegisterWithString(CHAR_SWAP, ProcessKeybind, "CTRL+SHIFT+F4");
    APIDefs->InputBinds_RegisterWithString(TP_BUY_SELL, ProcessKeybind, "CTRL+SHIFT+F1");
    APIDefs->InputBinds_RegisterWithString(WIZARD_VAULT_COMPLETE, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(WIZARD_VAULT_COMBO, ProcessKeybind, "CTRL+SHIFT+F2");
    APIDefs->InputBinds_RegisterWithString(ACCEPT, ProcessKeybind, "CTRL+SHIFT+F3");
    APIDefs->InputBinds_RegisterWithString(GENERAL_ACCEPT, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(GENERAL_ACCEPT_2, ProcessKeybind, "CTRL+SHIFT+F7");
    APIDefs->InputBinds_RegisterWithString(GENERAL_ACCEPT_3, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(GENERAL_ACCEPT_4, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_7, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_8, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_9, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_10, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_11, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_12, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_13, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_14, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_15, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_16, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_17, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_18, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_19, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(ACCEPT_20, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(MERCHANT_COMBO, ProcessKeybind, "ALT+F11");
    APIDefs->InputBinds_RegisterWithString(LFG_COMBO, ProcessKeybind, "CTRL+SHIFT+F6");
    APIDefs->InputBinds_RegisterWithString(TELEPORT_FRIEND_COMBO, ProcessKeybind, "F6");
    APIDefs->InputBinds_RegisterWithString(TRADING_POST_COMBO_KEY, ProcessKeybind, "F7");
    APIDefs->InputBinds_RegisterWithString(BANK_COMBO, ProcessKeybind, "F8");
    APIDefs->InputBinds_RegisterWithString(PERSONAL_MARKER, ProcessKeybind, "F9");
    APIDefs->InputBinds_RegisterWithString(WIZARD_GOBBLER_COMBO, ProcessKeybind, "SHIFT+F1");
    APIDefs->InputBinds_RegisterWithString(WIZARD_PORTAL_SCROLL_COMBO, ProcessKeybind, "SHIFT+F2");
    APIDefs->InputBinds_RegisterWithString(LOUNGE_PASS_COMBO, ProcessKeybind, "SHIFT+F3");
    APIDefs->InputBinds_RegisterWithString(WAYPOINT_COMBO, ProcessKeybind, "SHIFT+F4");
    APIDefs->InputBinds_RegisterWithString(BOUNCY_META_COMPLETE, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(GENERAL_ACCEPT_COMBO, ProcessKeybind, "CTRL+SHIFT+F11");
    APIDefs->InputBinds_RegisterWithString(MAIL_COMBO, ProcessKeybind, "CTRL+SHIFT+F8");
    APIDefs->InputBinds_RegisterWithString(CRAFT_CLOSE, ProcessKeybind, "CTRL+SHIFT+F9");
    APIDefs->InputBinds_RegisterWithString(CRAFT_COLLAPSE_COMBO, ProcessKeybind, "CTRL+SHIFT+F10");
    APIDefs->InputBinds_RegisterWithString(GUILD_HALL, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(GUILD_HALL_COMBO, ProcessKeybind, "CTRL+SHIFT+F5");
    APIDefs->InputBinds_RegisterWithString(GRACEFUL_QUIT, ProcessKeybind, "ALT+SHIFT+Q");
    APIDefs->InputBinds_RegisterWithString(PATHING_TOGGLE_ALL, ProcessKeybind, "CTRL+F3");
    APIDefs->InputBinds_RegisterWithString(COPY_ITEM_NAME, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CLAUDE_READ_SCREEN, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_CRAFT, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_CRAFT_ALL, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_WIZARD_VAULT, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_CHAR_SWAP, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_TP_BUY_SELL, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_WIZARD_VAULT_COMPLETE, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_GENERAL_ACCEPT, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_GENERAL_ACCEPT_2, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_GENERAL_ACCEPT_3, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_GENERAL_ACCEPT_4, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_7, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_8, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_9, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_10, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_11, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_12, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_13, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_14, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_15, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_16, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_17, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_18, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_19, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_ACCEPT_20, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_MERCHANT, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_WIZARD_VAULT_DAILY_TAB, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_WIZARD_VAULT_WEEKLY_TAB, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_WIZARD_VAULT_SPECIAL_TAB, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_LFG_SEARCH, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_TELEPORT_FRIEND, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_TRADING_POST_ICON, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_BANK_ICON, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_WIZARD_GOBBLER, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_WIZARD_PORTAL_SCROLL, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_LOUNGE_PASS, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_CHAT_WAYPOINT, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_MAP_WAYPOINT, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_BOUNCY_ACCEPT, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_BOUNCY_META_COMPLETE, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_MAIL_TAKE_ALL, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_CRAFT_FILTER, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_CRAFT_COLLAPSE, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_CRAFT_CLOSE, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_GUILD_HALL, ProcessKeybind, "(null)");
    APIDefs->InputBinds_RegisterWithString(CAPTURE_EXIT_GAME, ProcessKeybind, "(null)");

    APIDefs->Log(LOGL_INFO, "MysticClicker", "Addon loaded.");
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
    APIDefs->InputBinds_Deregister(BOUNCY_ACCEPT);
    APIDefs->InputBinds_Deregister(OPEN_CHEST_COMBO);
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
    APIDefs->InputBinds_Deregister(CRAFT);
    APIDefs->InputBinds_Deregister(CRAFT_ALL);
    APIDefs->InputBinds_Deregister(WIZARD_VAULT);
    APIDefs->InputBinds_Deregister(CHAR_SWAP);
    APIDefs->InputBinds_Deregister(TP_BUY_SELL);
    APIDefs->InputBinds_Deregister(WIZARD_VAULT_COMPLETE);
    APIDefs->InputBinds_Deregister(WIZARD_VAULT_COMBO);
    APIDefs->InputBinds_Deregister(ACCEPT);
    APIDefs->InputBinds_Deregister(GENERAL_ACCEPT);
    APIDefs->InputBinds_Deregister(GENERAL_ACCEPT_2);
    APIDefs->InputBinds_Deregister(GENERAL_ACCEPT_3);
    APIDefs->InputBinds_Deregister(GENERAL_ACCEPT_4);
    APIDefs->InputBinds_Deregister(ACCEPT_7);
    APIDefs->InputBinds_Deregister(ACCEPT_8);
    APIDefs->InputBinds_Deregister(ACCEPT_9);
    APIDefs->InputBinds_Deregister(ACCEPT_10);
    APIDefs->InputBinds_Deregister(ACCEPT_11);
    APIDefs->InputBinds_Deregister(ACCEPT_12);
    APIDefs->InputBinds_Deregister(ACCEPT_13);
    APIDefs->InputBinds_Deregister(ACCEPT_14);
    APIDefs->InputBinds_Deregister(ACCEPT_15);
    APIDefs->InputBinds_Deregister(ACCEPT_16);
    APIDefs->InputBinds_Deregister(ACCEPT_17);
    APIDefs->InputBinds_Deregister(ACCEPT_18);
    APIDefs->InputBinds_Deregister(ACCEPT_19);
    APIDefs->InputBinds_Deregister(ACCEPT_20);
    APIDefs->InputBinds_Deregister(MERCHANT_COMBO);
    APIDefs->InputBinds_Deregister(LFG_COMBO);
    APIDefs->InputBinds_Deregister(TELEPORT_FRIEND_COMBO);
    APIDefs->InputBinds_Deregister(TRADING_POST_COMBO_KEY);
    APIDefs->InputBinds_Deregister(BANK_COMBO);
    APIDefs->InputBinds_Deregister(PERSONAL_MARKER);
    APIDefs->InputBinds_Deregister(WIZARD_GOBBLER_COMBO);
    APIDefs->InputBinds_Deregister(WIZARD_PORTAL_SCROLL_COMBO);
    APIDefs->InputBinds_Deregister(LOUNGE_PASS_COMBO);
    APIDefs->InputBinds_Deregister(WAYPOINT_COMBO);
    APIDefs->InputBinds_Deregister(BOUNCY_META_COMPLETE);
    APIDefs->InputBinds_Deregister(GENERAL_ACCEPT_COMBO);
    APIDefs->InputBinds_Deregister(MAIL_COMBO);
    APIDefs->InputBinds_Deregister(CRAFT_CLOSE);
    APIDefs->InputBinds_Deregister(CRAFT_COLLAPSE_COMBO);
    APIDefs->InputBinds_Deregister(GUILD_HALL);
    APIDefs->InputBinds_Deregister(GUILD_HALL_COMBO);
    APIDefs->InputBinds_Deregister(GRACEFUL_QUIT);
    APIDefs->InputBinds_Deregister(PATHING_TOGGLE_ALL);
    APIDefs->InputBinds_Deregister(CLAUDE_READ_SCREEN);
    APIDefs->InputBinds_Deregister(CAPTURE_CRAFT);
    APIDefs->InputBinds_Deregister(CAPTURE_CRAFT_ALL);
    APIDefs->InputBinds_Deregister(CAPTURE_WIZARD_VAULT);
    APIDefs->InputBinds_Deregister(CAPTURE_CHAR_SWAP);
    APIDefs->InputBinds_Deregister(CAPTURE_TP_BUY_SELL);
    APIDefs->InputBinds_Deregister(CAPTURE_WIZARD_VAULT_COMPLETE);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT);
    APIDefs->InputBinds_Deregister(CAPTURE_GENERAL_ACCEPT);
    APIDefs->InputBinds_Deregister(CAPTURE_GENERAL_ACCEPT_2);
    APIDefs->InputBinds_Deregister(CAPTURE_GENERAL_ACCEPT_3);
    APIDefs->InputBinds_Deregister(CAPTURE_GENERAL_ACCEPT_4);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_7);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_8);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_9);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_10);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_11);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_12);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_13);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_14);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_15);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_16);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_17);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_18);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_19);
    APIDefs->InputBinds_Deregister(CAPTURE_ACCEPT_20);
    APIDefs->InputBinds_Deregister(CAPTURE_MERCHANT);
    APIDefs->InputBinds_Deregister(CAPTURE_WIZARD_VAULT_DAILY_TAB);
    APIDefs->InputBinds_Deregister(CAPTURE_WIZARD_VAULT_WEEKLY_TAB);
    APIDefs->InputBinds_Deregister(CAPTURE_WIZARD_VAULT_SPECIAL_TAB);
    APIDefs->InputBinds_Deregister(CAPTURE_LFG_SEARCH);
    APIDefs->InputBinds_Deregister(CAPTURE_TELEPORT_FRIEND);
    APIDefs->InputBinds_Deregister(CAPTURE_TRADING_POST_ICON);
    APIDefs->InputBinds_Deregister(CAPTURE_BANK_ICON);
    APIDefs->InputBinds_Deregister(CAPTURE_WIZARD_GOBBLER);
    APIDefs->InputBinds_Deregister(CAPTURE_WIZARD_PORTAL_SCROLL);
    APIDefs->InputBinds_Deregister(CAPTURE_LOUNGE_PASS);
    APIDefs->InputBinds_Deregister(CAPTURE_CHAT_WAYPOINT);
    APIDefs->InputBinds_Deregister(CAPTURE_MAP_WAYPOINT);
    APIDefs->InputBinds_Deregister(CAPTURE_BOUNCY_ACCEPT);
    APIDefs->InputBinds_Deregister(CAPTURE_BOUNCY_META_COMPLETE);
    APIDefs->InputBinds_Deregister(CAPTURE_MAIL_TAKE_ALL);
    APIDefs->InputBinds_Deregister(CAPTURE_CRAFT_FILTER);
    APIDefs->InputBinds_Deregister(CAPTURE_CRAFT_COLLAPSE);
    APIDefs->InputBinds_Deregister(CAPTURE_CRAFT_CLOSE);
    APIDefs->InputBinds_Deregister(CAPTURE_GUILD_HALL);
    APIDefs->InputBinds_Deregister(CAPTURE_EXIT_GAME);

    APIDefs->GUI_Deregister(RenderCaptureWindow);
    APIDefs->GUI_DeregisterCloseOnEscape("Mystic Clicker - Capture");
    DeregisterScreenCaptureHook();
    APIDefs->InputBinds_Deregister(CAPTURE_MODE);
    APIDefs->InputBinds_Deregister(RESET_WINDOWS);

    APIDefs->Log(LOGL_INFO, "MysticClicker", "Addon unloaded.");
    APIDefs = nullptr;
    GameWindow = nullptr;
}
