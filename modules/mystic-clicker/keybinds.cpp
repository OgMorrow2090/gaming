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
#include <cstdio>

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

    // TRACE: log every chord so we can see which ones reach the DLL.
    {
        char buf[256];
        sprintf_s(buf, "[TRACE] ProcessKeybind received: %s", aIdentifier ? aIdentifier : "(null)");
        APIDefs->Log(LOGL_INFO, "MysticClicker", buf);
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
    else if (strcmp(aIdentifier, OPEN_CHEST_COMBO) == 0)
    {
        SimulateOpenChestCombo();
    }
    else if (strcmp(aIdentifier, BOUNCY_ACCEPT) == 0)
    {
        SimulateBouncyAcceptClick();
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
    else if (strcmp(aIdentifier, CRAFT) == 0)
    {
        SimulateCraftClick();
    }
    else if (strcmp(aIdentifier, CRAFT_ALL) == 0)
    {
        SimulateCraftAllClick();
    }
    else if (strcmp(aIdentifier, WIZARD_VAULT) == 0)
    {
        SimulateWizardVaultClick();
    }
    else if (strcmp(aIdentifier, CHAR_SWAP) == 0)
    {
        SimulateCharSwapClick();
    }
    else if (strcmp(aIdentifier, TP_BUY_SELL) == 0)
    {
        SimulateTpBuySellClick();
    }
    else if (strcmp(aIdentifier, WIZARD_VAULT_COMPLETE) == 0)
    {
        SimulateWizardVaultCompleteClick();
    }
    else if (strcmp(aIdentifier, WIZARD_VAULT_COMBO) == 0)
    {
        SimulateWizardVaultCombo();
    }
    else if (strcmp(aIdentifier, ACCEPT) == 0)
    {
        SimulateAcceptClick();
    }
    else if (strcmp(aIdentifier, GENERAL_ACCEPT) == 0)
    {
        SimulateGeneralAcceptClick();
    }
    else if (strcmp(aIdentifier, GENERAL_ACCEPT_2) == 0)
    {
        SimulateGeneralAccept2Click();
    }
    else if (strcmp(aIdentifier, GENERAL_ACCEPT_3) == 0)
    {
        SimulateGeneralAccept3Click();
    }
    else if (strcmp(aIdentifier, GENERAL_ACCEPT_4) == 0)
    {
        SimulateGeneralAccept4Click();
    }
    else if (strcmp(aIdentifier, ACCEPT_7) == 0)
    {
        SimulateAccept7Click();
    }
    else if (strcmp(aIdentifier, ACCEPT_8) == 0)
    {
        SimulateAccept8Click();
    }
    else if (strcmp(aIdentifier, ACCEPT_9) == 0)
    {
        SimulateAccept9Click();
    }
    else if (strcmp(aIdentifier, ACCEPT_10) == 0)
    {
        SimulateAccept10Click();
    }
    else if (strcmp(aIdentifier, ACCEPT_11) == 0)
    {
        SimulateAccept11Click();
    }
    else if (strcmp(aIdentifier, ACCEPT_12) == 0)
    {
        SimulateAccept12Click();
    }
    else if (strcmp(aIdentifier, ACCEPT_13) == 0)
    {
        SimulateAccept13Click();
    }
    else if (strcmp(aIdentifier, ACCEPT_14) == 0)
    {
        SimulateAccept14Click();
    }
    else if (strcmp(aIdentifier, ACCEPT_15) == 0)
    {
        SimulateAccept15Click();
    }
    else if (strcmp(aIdentifier, LFG_COMBO) == 0)
    {
        SimulateLfgCombo();
    }
    else if (strcmp(aIdentifier, TELEPORT_FRIEND_COMBO) == 0)
    {
        SimulateTeleportFriendCombo();
    }
    else if (strcmp(aIdentifier, TRADING_POST_COMBO_KEY) == 0)
    {
        SimulateTradingPostCombo();
    }
    else if (strcmp(aIdentifier, BANK_COMBO) == 0)
    {
        SimulateBankCombo();
    }
    else if (strcmp(aIdentifier, BOUNCY_META_COMPLETE) == 0)
    {
        SimulateBouncyMetaCompleteClick();
    }
    else if (strcmp(aIdentifier, GENERAL_ACCEPT_COMBO) == 0)
    {
        SimulateGeneralAcceptCombo();
    }
    else if (strcmp(aIdentifier, MAIL_COMBO) == 0)
    {
        SimulateMailCombo();
    }
    else if (strcmp(aIdentifier, CRAFT_COLLAPSE_COMBO) == 0)
    {
        SimulateCraftCollapseCombo();
    }
    else if (strcmp(aIdentifier, CRAFT_CLOSE) == 0)
    {
        SimulateCraftCloseClick();
    }
    else if (strcmp(aIdentifier, GUILD_HALL) == 0)
    {
        SimulateGuildHallClick();
    }
    else if (strcmp(aIdentifier, GUILD_HALL_COMBO) == 0)
    {
        SimulateGuildHallCombo();
    }
    // === CAPTURE MODE ===
    else if (strcmp(aIdentifier, CAPTURE_MODE) == 0)
    {
        ToggleCaptureWindow();
    }
    else if (strcmp(aIdentifier, RESET_WINDOWS) == 0)
    {
        ResetWindowPositions();
    }
}
