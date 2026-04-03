/**
 * keybinds.cpp
 * 
 * Keybind registration and handler for Mystic Clicker addon.
 * Routes keybind events to appropriate input simulation functions.
 * 
 * Author: OgMorrow2090
 * Repository: https://github.com/OgMorrow2090/guildwars2
 */

#include "shared.h"
#include <cstring>

/**
 * ProcessKeybind - Keybind callback handler
 * 
 * Called by Nexus when a registered keybind is pressed or released.
 * 
 * @param aIdentifier - The keybind identifier
 * @param aIsRelease - true if key was released, false if pressed
 */
void ProcessKeybind(const char* aIdentifier, bool aIsRelease)
{
    // Only act on key press, not release
    if (aIsRelease)
    {
        return;
    }
    
    // Check if resolution changed and load appropriate config
    CheckResolutionChange();
    
    // === ACTION KEYBINDS ===
    if (strcmp(aIdentifier, DEPOSIT_MATERIALS) == 0)
    {
        SimulateDepositMaterialsClick();
    }
    else if (strcmp(aIdentifier, SORT_INVENTORY) == 0)
    {
        SimulateSortInventoryClick();
    }
    else if (strcmp(aIdentifier, OPEN_CHEST) == 0)
    {
        SimulateOpenChestClick();
    }
    else if (strcmp(aIdentifier, DEPOSIT_AND_SORT) == 0)
    {
        SimulateDepositAndSort();
    }
    else if (strcmp(aIdentifier, EXIT_INSTANCE) == 0)
    {
        SimulateExitInstanceClick();
    }
    else if (strcmp(aIdentifier, YES_DIALOG) == 0)
    {
        SimulateYesDialogClick();
    }
    else if (strcmp(aIdentifier, MYSTIC_FORGE) == 0)
    {
        SimulateMysticForgeClick();
    }
    else if (strcmp(aIdentifier, MYSTIC_REFILL) == 0)
    {
        SimulateMysticRefillClick();
    }
    else if (strcmp(aIdentifier, MYSTIC_FORGE_COMBO) == 0)
    {
        SimulateMysticForgeCombo();
    }
    else if (strcmp(aIdentifier, VENDOR_BUY) == 0)
    {
        SimulateVendorClick();
    }
    else if (strcmp(aIdentifier, SELL_JUNK) == 0)
    {
        SimulateSellJunkClick();
    }
    else if (strcmp(aIdentifier, TRADING_POST) == 0)
    {
        SimulateTradingPostClick();
    }
    else if (strcmp(aIdentifier, TP_REMOVE) == 0)
    {
        SimulateTpRemoveClick();
    }
    // === CAPTURE MODE ===
    else if (strcmp(aIdentifier, CAPTURE_MODE) == 0)
    {
        ToggleCaptureWindow();
    }
}
