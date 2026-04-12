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

