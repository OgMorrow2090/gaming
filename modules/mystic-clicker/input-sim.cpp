/**
 * input-sim.cpp
 * 
 * Input simulation functions for Mystic Clicker addon.
 * Uses Nexus SendWndProcToGameOnly() to send mouse events directly to the game.
 * 
 * Author: OgMorrow2090
 * Repository: https://github.com/OgMorrow2090/guildwars2
 * 
 * Capture positions:
 *   Ctrl+Shift+D = Deposit button
 *   Ctrl+Shift+C = Compact/Sort button
 *   Ctrl+Shift+B = Bouncy Chest
 * 
 * Actions:
 *   Alt+D = Deposit Materials (left-click)
 *   Alt+C = Compact/Sort (left-click)
 *   Alt+B = Open Bouncy Chest (right-click)
 */

#include "shared.h"
#include <cstdio>

// =============================================================================
// Helper: Find Game Window
// =============================================================================

static void EnsureGameWindow()
{
    if (GameWindow != nullptr) return;
    
    GameWindow = FindWindowA("ArenaNet_Dx_Window_Class", nullptr);
    if (GameWindow == nullptr)
        GameWindow = FindWindowA("ArenaNet_Gr_Window_Class", nullptr);
    if (GameWindow == nullptr)
        GameWindow = FindWindowA(nullptr, "Guild Wars 2");
    if (GameWindow == nullptr)
    {
        GameWindow = GetForegroundWindow();
        APIDefs->Log(LOGL_WARNING, "MysticClicker", "Using foreground window");
    }
}

// =============================================================================
// Click Simulation
// =============================================================================

/**
 * SimulateClickAt - Send LEFT mouse click at coordinates
 */
void SimulateClickAt(int x, int y)
{
    EnsureGameWindow();
    if (GameWindow == nullptr)
    {
        APIDefs->Log(LOGL_WARNING, "MysticClicker", "Game window not available");
        return;
    }
    
    LPARAM lParam = MAKELPARAM(x, y);
    
    APIDefs->WndProc_SendToGameOnly(GameWindow, WM_LBUTTONDOWN, MK_LBUTTON, lParam);
    APIDefs->WndProc_SendToGameOnly(GameWindow, WM_LBUTTONUP, 0, lParam);
}

/**
 * SimulateRealClickAt - Send a REAL OS-level left click at client coordinates.
 * Uses SendInput so the click is delivered through the real Windows input
 * pipeline — this means it's seen by Nexus addon overlays (ImGui windows),
 * not just the GW2 game window. Cursor is restored after.
 *
 * IMPORTANT: ImGui detects clicks by observing button state changes across
 * frames. At 60fps a frame is ~17ms, so we need a meaningful sleep between
 * the LEFTDOWN and LEFTUP events or ImGui will never see the "held" state
 * and the click won't register — the cursor hovers (highlight visible) but
 * nothing fires.
 */
void SimulateRealClickAt(int x, int y)
{
    EnsureGameWindow();
    if (GameWindow == nullptr)
    {
        APIDefs->Log(LOGL_WARNING, "MysticClicker", "Game window not available for real click");
        return;
    }

    // Save current cursor position so we can restore it after
    POINT savedCursor;
    GetCursorPos(&savedCursor);

    // Convert client (game-window-relative) coords to screen coords
    POINT target = { x, y };
    ClientToScreen(GameWindow, &target);

    // Move cursor to target
    SetCursorPos(target.x, target.y);

    // Also fire a MOUSEEVENTF_MOVE so any listeners tracking raw input see it
    INPUT move = {};
    move.type = INPUT_MOUSE;
    move.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &move, sizeof(INPUT));

    // Let ImGui catch up on the hover state — needs multiple frames
    Sleep(60);

    // Button down
    INPUT down = {};
    down.type = INPUT_MOUSE;
    down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &down, sizeof(INPUT));

    // Hold long enough for at least 3–4 frames so ImGui registers the press
    Sleep(60);

    // Button up
    INPUT up = {};
    up.type = INPUT_MOUSE;
    up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &up, sizeof(INPUT));

    // Let the release propagate
    Sleep(30);

    // Restore original cursor position
    SetCursorPos(savedCursor.x, savedCursor.y);
}

/**
 * SimulateKeyPress - Send a single key press (down + up) to the game window.
 * Used by combo actions that need to press a keyboard key (e.g. Guild panel
 * opens via G) in addition to mouse clicks.
 */
void SimulateKeyPress(WPARAM vkCode)
{
    EnsureGameWindow();
    if (GameWindow == nullptr)
    {
        APIDefs->Log(LOGL_WARNING, "MysticClicker", "Game window not available for key press");
        return;
    }

    // Encode scan code in lParam (bits 16-23). GW2 requires it for some keys
    // (e.g. G for Guild panel) — WM_KEYDOWN without scan code silently dropped.
    UINT scanCode = MapVirtualKey((UINT)vkCode, MAPVK_VK_TO_VSC);
    LPARAM downParam = (LPARAM)((scanCode << 16) | 0x00000001);
    LPARAM upParam   = (LPARAM)((scanCode << 16) | 0xC0000001);

    APIDefs->WndProc_SendToGameOnly(GameWindow, WM_KEYDOWN, vkCode, downParam);
    Sleep(30);
    APIDefs->WndProc_SendToGameOnly(GameWindow, WM_KEYUP,   vkCode, upParam);
}

/**
 * SimulateRightClickAt - Send RIGHT mouse click at coordinates
 */
void SimulateRightClickAt(int x, int y)
{
    EnsureGameWindow();
    if (GameWindow == nullptr)
    {
        APIDefs->Log(LOGL_WARNING, "MysticClicker", "Game window not available");
        return;
    }
    
    LPARAM lParam = MAKELPARAM(x, y);
    
    // Move mouse to position first
    APIDefs->WndProc_SendToGameOnly(GameWindow, WM_MOUSEMOVE, 0, lParam);
    
    // Small delay to let game register position
    Sleep(10);
    
    // Right click
    APIDefs->WndProc_SendToGameOnly(GameWindow, WM_RBUTTONDOWN, MK_RBUTTON, lParam);
    Sleep(10);
    APIDefs->WndProc_SendToGameOnly(GameWindow, WM_RBUTTONUP, 0, lParam);
}

// =============================================================================
// Action Functions
// =============================================================================

void SimulateDepositMaterialsClick()
{
    if (g_DepositX == 0 && g_DepositY == 0)
    {
        APIDefs->GUI_SendAlert("Deposit position not set! Use Ctrl+Shift+D to capture");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Deposit Materials");
    SimulateClickAt(g_DepositX, g_DepositY);
}

void SimulateSortInventoryClick()
{
    if (g_SortX == 0 && g_SortY == 0)
    {
        APIDefs->GUI_SendAlert("Sort position not set! Use Ctrl+Shift+C to capture");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Sort/Compact");
    SimulateClickAt(g_SortX, g_SortY);
}

void SimulateOpenChestClick()
{
    if (g_ChestX == 0 && g_ChestY == 0)
    {
        APIDefs->GUI_SendAlert("Chest position not set! Use Ctrl+Shift+B to capture");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Right-clicking Bouncy Chest");
    SimulateRightClickAt(g_ChestX, g_ChestY);
}

void SimulateDepositAndSort()
{
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Combo: Deposit + Sort");
    
    // First deposit
    if (g_DepositX != 0 || g_DepositY != 0)
    {
        SimulateClickAt(g_DepositX, g_DepositY);
    }
    
    // Small delay then sort
    Sleep(100);
    
    if (g_SortX != 0 || g_SortY != 0)
    {
        SimulateClickAt(g_SortX, g_SortY);
    }
}

void SimulateExitInstanceClick()
{
    if (g_ExitInstanceX == 0 && g_ExitInstanceY == 0)
    {
        APIDefs->GUI_SendAlert("Exit Instance position not set! Use Ctrl+Shift+E to capture");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Exit Instance");
    SimulateClickAt(g_ExitInstanceX, g_ExitInstanceY);
}

void SimulateGenericClick(int slot)
{
    int x = 0, y = 0;
    const char* slotName = "Generic";
    
    switch (slot)
    {
        case 1: x = g_Generic1X; y = g_Generic1Y; slotName = "Generic 1"; break;
        case 2: x = g_Generic2X; y = g_Generic2Y; slotName = "Generic 2"; break;
        case 3: x = g_Generic3X; y = g_Generic3Y; slotName = "Generic 3"; break;
        case 4: x = g_Generic4X; y = g_Generic4Y; slotName = "Generic 4"; break;
        case 5: x = g_Generic5X; y = g_Generic5Y; slotName = "Generic 5"; break;
        default: return;
    }
    
    if (x == 0 && y == 0)
    {
        char buffer[128];
        sprintf_s(buffer, "%s position not set! Capture with Ctrl+Shift+%d", slotName, slot);
        APIDefs->GUI_SendAlert(buffer);
        return;
    }
    
    char buffer[64];
    sprintf_s(buffer, "Clicking %s", slotName);
    APIDefs->Log(LOGL_INFO, "MysticClicker", buffer);
    SimulateClickAt(x, y);
}

void SimulateYesDialogClick()
{
    if (g_YesDialogX == 0 && g_YesDialogY == 0)
    {
        APIDefs->GUI_SendAlert("Yes Dialog position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Yes Dialog");
    SimulateClickAt(g_YesDialogX, g_YesDialogY);
}

void SimulateMysticForgeClick()
{
    if (g_MysticForgeX == 0 && g_MysticForgeY == 0)
    {
        APIDefs->GUI_SendAlert("Mystic Forge position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Mystic Forge");
    SimulateClickAt(g_MysticForgeX, g_MysticForgeY);
}

void SimulateMysticRefillClick()
{
    if (g_MysticRefillX == 0 && g_MysticRefillY == 0)
    {
        APIDefs->GUI_SendAlert("Mystic Refill position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Mystic Refill");
    SimulateClickAt(g_MysticRefillX, g_MysticRefillY);
}

void SimulateVendorClick()
{
    if (g_VendorX == 0 && g_VendorY == 0)
    {
        APIDefs->GUI_SendAlert("Vendor position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Vendor");
    SimulateClickAt(g_VendorX, g_VendorY);
}

void SimulateSellJunkClick()
{
    if (g_SellJunkX == 0 && g_SellJunkY == 0)
    {
        APIDefs->GUI_SendAlert("Sell Junk position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Sell Junk");
    SimulateClickAt(g_SellJunkX, g_SellJunkY);
}

void SimulateTradingPostClick()
{
    if (g_TradingPostX == 0 && g_TradingPostY == 0)
    {
        APIDefs->GUI_SendAlert("Trading Post position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Trading Post");
    SimulateClickAt(g_TradingPostX, g_TradingPostY);
}

void SimulateTpRemoveClick()
{
    if (g_TpRemoveX == 0 && g_TpRemoveY == 0)
    {
        APIDefs->GUI_SendAlert("TP Remove position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking TP Remove");
    SimulateClickAt(g_TpRemoveX, g_TpRemoveY);
}

void SimulateCraftClick()
{
    if (g_CraftX == 0 && g_CraftY == 0)
    {
        APIDefs->GUI_SendAlert("Craft position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Craft");
    SimulateClickAt(g_CraftX, g_CraftY);
}

void SimulateCraftAllClick()
{
    if (g_CraftAllX == 0 && g_CraftAllY == 0)
    {
        APIDefs->GUI_SendAlert("Craft All position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Craft All");
    SimulateClickAt(g_CraftAllX, g_CraftAllY);
}

void SimulateWizardVaultClick()
{
    if (g_WizardVaultX == 0 && g_WizardVaultY == 0)
    {
        APIDefs->GUI_SendAlert("Wizard Vault position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Wizard Vault Collect");
    SimulateClickAt(g_WizardVaultX, g_WizardVaultY);
}

void SimulateCharSwapClick()
{
    if (g_CharSwapX == 0 && g_CharSwapY == 0)
    {
        APIDefs->GUI_SendAlert("Char Swap position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Char Swap (real click for overlay)");
    // Real click via SendInput — Char Swap is a Nexus addon overlay,
    // not the game window, so WndProc messages won't reach it.
    SimulateRealClickAt(g_CharSwapX, g_CharSwapY);
}

void SimulateTpBuySellClick()
{
    if (g_TpBuySellX == 0 && g_TpBuySellY == 0)
    {
        APIDefs->GUI_SendAlert("TP Buy/Sell position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking TP Buy/Sell");
    SimulateClickAt(g_TpBuySellX, g_TpBuySellY);
}

void SimulateWizardVaultCompleteClick()
{
    if (g_WizardVaultCompleteX == 0 && g_WizardVaultCompleteY == 0)
    {
        APIDefs->GUI_SendAlert("Wizard Vault Complete position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Wizard Vault Complete");
    SimulateClickAt(g_WizardVaultCompleteX, g_WizardVaultCompleteY);
}

void SimulateWizardVaultCombo()
{
    // Always open the WV panel first — Shift+I runs regardless of capture state.
    {
        // Log held-modifier state so we can tell if the Nexus chord is still active
        bool ctrlHeld  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shiftHeld = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
        char sbuf[96];
        sprintf_s(sbuf, "WV Combo: key state at start — Ctrl=%d Shift=%d", ctrlHeld, shiftHeld);
        APIDefs->Log(LOGL_INFO, "MysticClicker", sbuf);

        // Release any physically-held modifiers (from the Nexus keybind chord)
        // so GW2 sees a clean Shift+I when we send it.
        INPUT clear[4] = {};
        clear[0].type = INPUT_KEYBOARD;
        clear[0].ki.wVk = VK_LCONTROL;
        clear[0].ki.dwFlags = KEYEVENTF_KEYUP;
        clear[1].type = INPUT_KEYBOARD;
        clear[1].ki.wVk = VK_RCONTROL;
        clear[1].ki.dwFlags = KEYEVENTF_KEYUP;
        clear[2].type = INPUT_KEYBOARD;
        clear[2].ki.wVk = VK_LSHIFT;
        clear[2].ki.dwFlags = KEYEVENTF_KEYUP;
        clear[3].type = INPUT_KEYBOARD;
        clear[3].ki.wVk = VK_RSHIFT;
        clear[3].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(4, clear, sizeof(INPUT));
        Sleep(50);

        APIDefs->Log(LOGL_INFO, "MysticClicker", "WV Combo: sending Shift+I via SendInput");
        INPUT inputs[4] = {};
        WORD shiftScan = (WORD)MapVirtualKey(VK_LSHIFT, MAPVK_VK_TO_VSC);
        WORD iScan     = (WORD)MapVirtualKey(0x49, MAPVK_VK_TO_VSC);

        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_LSHIFT;
        inputs[0].ki.wScan = shiftScan;
        inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE;

        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = 0x49;
        inputs[1].ki.wScan = iScan;
        inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE;

        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 0x49;
        inputs[2].ki.wScan = iScan;
        inputs[2].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = VK_LSHIFT;
        inputs[3].ki.wScan = shiftScan;
        inputs[3].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

        SendInput(4, inputs, sizeof(INPUT));
    }

    if (g_WizardVaultX == 0 && g_WizardVaultY == 0)
    {
        APIDefs->GUI_SendAlert("WV Collect position not set — panel opened, clicks skipped. Capture via Ctrl+Shift+C");
        return;
    }
    if (g_WizardVaultCompleteX == 0 && g_WizardVaultCompleteY == 0)
    {
        APIDefs->GUI_SendAlert("WV Complete position not set — panel opened, clicks skipped. Capture via Ctrl+Shift+C");
        return;
    }

    // Wait for panel animation before clicking
    Sleep(1000);

    APIDefs->Log(LOGL_INFO, "MysticClicker", "Wizard Vault Combo: Collect");
    SimulateClickAt(g_WizardVaultX, g_WizardVaultY);

    Sleep(100);

    APIDefs->Log(LOGL_INFO, "MysticClicker", "Wizard Vault Combo: Complete");
    SimulateClickAt(g_WizardVaultCompleteX, g_WizardVaultCompleteY);
}

void SimulateAcceptClick()
{
    if (g_AcceptX == 0 && g_AcceptY == 0)
    {
        APIDefs->GUI_SendAlert("Chest Accept position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Chest Accept");
    SimulateClickAt(g_AcceptX, g_AcceptY);
}

void SimulateGeneralAcceptClick()
{
    if (g_GeneralAcceptX == 0 && g_GeneralAcceptY == 0)
    {
        APIDefs->GUI_SendAlert("General Accept 1 position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking General Accept 1");
    SimulateClickAt(g_GeneralAcceptX, g_GeneralAcceptY);
}

void SimulateGeneralAccept2Click()
{
    if (g_GeneralAccept2X == 0 && g_GeneralAccept2Y == 0)
    {
        APIDefs->GUI_SendAlert("General Accept 2 position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking General Accept 2");
    SimulateClickAt(g_GeneralAccept2X, g_GeneralAccept2Y);
}

void SimulateMailCombo()
{
    // Open Mail via Shift+0 → wait for panel → click Take All
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Mail Combo: Open panel (Shift+0 via SendInput)");
    EnsureGameWindow();

    // Release any physically-held modifiers from the Nexus chord
    INPUT clear[4] = {};
    clear[0].type = INPUT_KEYBOARD; clear[0].ki.wVk = VK_LCONTROL; clear[0].ki.dwFlags = KEYEVENTF_KEYUP;
    clear[1].type = INPUT_KEYBOARD; clear[1].ki.wVk = VK_RCONTROL; clear[1].ki.dwFlags = KEYEVENTF_KEYUP;
    clear[2].type = INPUT_KEYBOARD; clear[2].ki.wVk = VK_LSHIFT;   clear[2].ki.dwFlags = KEYEVENTF_KEYUP;
    clear[3].type = INPUT_KEYBOARD; clear[3].ki.wVk = VK_RSHIFT;   clear[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, clear, sizeof(INPUT));
    Sleep(50);

    // Shift+0 (main keyboard 0, VK 0x30)
    INPUT inputs[4] = {};
    WORD shiftScan = (WORD)MapVirtualKey(VK_LSHIFT, MAPVK_VK_TO_VSC);
    WORD zeroScan  = (WORD)MapVirtualKey(0x30, MAPVK_VK_TO_VSC);

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_LSHIFT;
    inputs[0].ki.wScan = shiftScan;
    inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 0x30;
    inputs[1].ki.wScan = zeroScan;
    inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE;

    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 0x30;
    inputs[2].ki.wScan = zeroScan;
    inputs[2].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_LSHIFT;
    inputs[3].ki.wScan = shiftScan;
    inputs[3].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));

    if (g_MailTakeAllX == 0 && g_MailTakeAllY == 0)
    {
        APIDefs->GUI_SendAlert("Mail Take All position not set — panel opened, click skipped. Capture via Ctrl+Shift+C");
        return;
    }

    Sleep(1000);
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Mail Combo: Click Take All");
    SimulateClickAt(g_MailTakeAllX, g_MailTakeAllY);
}

void SimulateCraftCollapseCombo()
{
    if (g_CraftFilterX == 0 && g_CraftFilterY == 0)
    {
        APIDefs->GUI_SendAlert("Craft Filter position not set! Capture first");
        return;
    }
    if (g_CraftCollapseX == 0 && g_CraftCollapseY == 0)
    {
        APIDefs->GUI_SendAlert("Craft Collapse position not set! Capture first");
        return;
    }

    APIDefs->Log(LOGL_INFO, "MysticClicker", "Craft Collapse Combo: Filter");
    SimulateClickAt(g_CraftFilterX, g_CraftFilterY);

    Sleep(100);

    APIDefs->Log(LOGL_INFO, "MysticClicker", "Craft Collapse Combo: Collapse");
    SimulateClickAt(g_CraftCollapseX, g_CraftCollapseY);
}

void SimulateCraftCloseClick()
{
    if (g_CraftCloseX == 0 && g_CraftCloseY == 0)
    {
        APIDefs->GUI_SendAlert("Craft Close position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Craft Close");
    SimulateClickAt(g_CraftCloseX, g_CraftCloseY);
}

void SimulateGuildHallClick()
{
    if (g_GuildHallX == 0 && g_GuildHallY == 0)
    {
        APIDefs->GUI_SendAlert("Guild Hall position not set! Capture first");
        return;
    }
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Clicking Guild Hall");
    SimulateClickAt(g_GuildHallX, g_GuildHallY);
}

void SimulateGuildHallCombo()
{
    // Always open the Guild panel first — G press runs regardless of whether
    // the Guild Hall button position is captured yet.
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Guild Hall Combo: Press G");
    SimulateKeyPress(0x47);  // VK code for 'G'

    if (g_GuildHallX == 0 && g_GuildHallY == 0)
    {
        APIDefs->GUI_SendAlert("Guild Hall position not set — panel opened, click skipped. Capture via Ctrl+Shift+C");
        return;
    }

    // Guild panel animation can take up to ~1s before buttons are clickable.
    Sleep(1000);

    char buf[96];
    sprintf_s(buf, "Guild Hall Combo: Click at (%d,%d)", g_GuildHallX, g_GuildHallY);
    APIDefs->Log(LOGL_INFO, "MysticClicker", buf);
    SimulateRealClickAt(g_GuildHallX, g_GuildHallY);
}

void SimulateMysticForgeCombo()
{
    // Check both positions are set
    if (g_MysticForgeX == 0 && g_MysticForgeY == 0)
    {
        APIDefs->GUI_SendAlert("Mystic Forge position not set! Capture first");
        return;
    }
    if (g_MysticRefillX == 0 && g_MysticRefillY == 0)
    {
        APIDefs->GUI_SendAlert("Mystic Refill position not set! Capture first");
        return;
    }
    
    // Click Forge first
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Mystic Forge Combo: Forge");
    SimulateClickAt(g_MysticForgeX, g_MysticForgeY);
    
    // Wait 100ms (same as Deposit+Sort combo)
    Sleep(100);
    
    // Click Refill
    APIDefs->Log(LOGL_INFO, "MysticClicker", "Mystic Forge Combo: Refill");
    SimulateClickAt(g_MysticRefillX, g_MysticRefillY);
}

// =============================================================================
// Position Capture Functions
// =============================================================================

static void CapturePosition(int& outX, int& outY, const char* name)
{
    EnsureGameWindow();
    
    POINT pt;
    if (GetCursorPos(&pt))
    {
        if (GameWindow != nullptr)
        {
            ScreenToClient(GameWindow, &pt);
        }
        
        outX = pt.x;
        outY = pt.y;
        
        char buffer[128];
        sprintf_s(buffer, "%s captured: (%d, %d) - Saved!", name, pt.x, pt.y);
        APIDefs->Log(LOGL_INFO, "MysticClicker", buffer);
        APIDefs->GUI_SendAlert(buffer);
        
        // Auto-save after each capture
        SaveButtonPositions();
    }
    else
    {
        APIDefs->Log(LOGL_WARNING, "MysticClicker", "Failed to get cursor position");
        APIDefs->GUI_SendAlert("Failed to capture position!");
    }
}

