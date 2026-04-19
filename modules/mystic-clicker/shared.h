#pragma once

#include <windows.h>
#include "Nexus.h"

// ImGui forward declaration
struct ImGuiContext;

// Global API pointer - set in AddonLoad
extern AddonAPI_t* APIDefs;

// Addon definition
extern AddonDefinition_t AddonDef;

// Game window handle
extern HWND GameWindow;

// ImGui context
extern ImGuiContext* ImGuiCtx;

// Capture UI
extern bool g_ShowCaptureWindow;
constexpr const char* CAPTURE_MODE = "CAPTURE_MODE";
void RenderCaptureWindow();
void ToggleCaptureWindow();

// Keybind identifiers - Actions
constexpr const char* DEPOSIT_MATERIALS = "DEPOSIT_MATERIALS";
constexpr const char* SORT_INVENTORY = "SORT_INVENTORY";
constexpr const char* OPEN_CHEST = "OPEN_CHEST";
constexpr const char* DEPOSIT_AND_SORT = "DEPOSIT_AND_SORT";
constexpr const char* EXIT_INSTANCE = "EXIT_INSTANCE";
constexpr const char* YES_DIALOG = "YES_DIALOG";
constexpr const char* MYSTIC_FORGE = "MYSTIC_FORGE";
constexpr const char* MYSTIC_REFILL = "MYSTIC_REFILL";
constexpr const char* MYSTIC_FORGE_COMBO = "MYSTIC_FORGE_COMBO";
constexpr const char* VENDOR_BUY = "VENDOR_BUY";
constexpr const char* SELL_JUNK = "SELL_JUNK";
constexpr const char* TRADING_POST = "TRADING_POST";
constexpr const char* TP_REMOVE = "TP_REMOVE";
constexpr const char* CRAFT = "CRAFT";
constexpr const char* CRAFT_ALL = "CRAFT_ALL";
constexpr const char* WIZARD_VAULT = "WIZARD_VAULT";
constexpr const char* CHAR_SWAP = "CHAR_SWAP";
constexpr const char* TP_BUY_SELL = "TP_BUY_SELL";
constexpr const char* WIZARD_VAULT_COMPLETE = "WIZARD_VAULT_COMPLETE";
constexpr const char* WIZARD_VAULT_COMBO = "WIZARD_VAULT_COMBO";
constexpr const char* ACCEPT = "ACCEPT";                    // "Chest Accept" — post-chest dialog
constexpr const char* GENERAL_ACCEPT = "GENERAL_ACCEPT";    // "General Accept 1"
constexpr const char* GENERAL_ACCEPT_2 = "GENERAL_ACCEPT_2";
constexpr const char* GENERAL_ACCEPT_3 = "GENERAL_ACCEPT_3";
constexpr const char* GENERAL_ACCEPT_4 = "GENERAL_ACCEPT_4";
constexpr const char* ACCEPT_7 = "ACCEPT_7";
constexpr const char* ACCEPT_8 = "ACCEPT_8";
constexpr const char* GENERAL_ACCEPT_COMBO = "GENERAL_ACCEPT_COMBO"; // "Accept Combo" — clicks Accept 1 → 8
constexpr const char* GUILD_HALL = "GUILD_HALL";
constexpr const char* GUILD_HALL_COMBO = "GUILD_HALL_COMBO";
constexpr const char* MAIL_TAKE_ALL = "MAIL_TAKE_ALL";      // capture only (position)
constexpr const char* MAIL_COMBO = "MAIL_COMBO";            // Shift+0 → click Take All
constexpr const char* CRAFT_FILTER = "CRAFT_FILTER";        // capture only
constexpr const char* CRAFT_COLLAPSE = "CRAFT_COLLAPSE";    // capture only
constexpr const char* CRAFT_COLLAPSE_COMBO = "CRAFT_COLLAPSE_COMBO"; // click Filter → click Collapse
constexpr const char* CRAFT_CLOSE = "CRAFT_CLOSE";          // single click to close crafting

// Capture keybind identifiers
constexpr const char* CAPTURE_CRAFT = "CAPTURE_CRAFT";
constexpr const char* CAPTURE_CRAFT_ALL = "CAPTURE_CRAFT_ALL";
constexpr const char* CAPTURE_WIZARD_VAULT = "CAPTURE_WIZARD_VAULT";
constexpr const char* CAPTURE_CHAR_SWAP = "CAPTURE_CHAR_SWAP";
constexpr const char* CAPTURE_TP_BUY_SELL = "CAPTURE_TP_BUY_SELL";
constexpr const char* CAPTURE_WIZARD_VAULT_COMPLETE = "CAPTURE_WIZARD_VAULT_COMPLETE";
constexpr const char* CAPTURE_ACCEPT = "CAPTURE_ACCEPT";
constexpr const char* CAPTURE_GENERAL_ACCEPT = "CAPTURE_GENERAL_ACCEPT";
constexpr const char* CAPTURE_GENERAL_ACCEPT_2 = "CAPTURE_GENERAL_ACCEPT_2";
constexpr const char* CAPTURE_GENERAL_ACCEPT_3 = "CAPTURE_GENERAL_ACCEPT_3";
constexpr const char* CAPTURE_GENERAL_ACCEPT_4 = "CAPTURE_GENERAL_ACCEPT_4";
constexpr const char* CAPTURE_ACCEPT_7 = "CAPTURE_ACCEPT_7";
constexpr const char* CAPTURE_ACCEPT_8 = "CAPTURE_ACCEPT_8";
constexpr const char* CAPTURE_GUILD_HALL = "CAPTURE_GUILD_HALL";
constexpr const char* CAPTURE_MAIL_TAKE_ALL = "CAPTURE_MAIL_TAKE_ALL";
constexpr const char* CAPTURE_CRAFT_FILTER = "CAPTURE_CRAFT_FILTER";
constexpr const char* CAPTURE_CRAFT_COLLAPSE = "CAPTURE_CRAFT_COLLAPSE";
constexpr const char* CAPTURE_CRAFT_CLOSE = "CAPTURE_CRAFT_CLOSE";

// Keybind handler
void ProcessKeybind(const char* aIdentifier, bool aIsRelease);

// Input simulation functions
void SimulateDepositMaterialsClick();
void SimulateSortInventoryClick();
void SimulateOpenChestClick();
void SimulateDepositAndSort();
void SimulateExitInstanceClick();
void SimulateYesDialogClick();
void SimulateMysticForgeClick();
void SimulateMysticRefillClick();
void SimulateMysticForgeCombo();
void SimulateVendorClick();
void SimulateSellJunkClick();
void SimulateTradingPostClick();
void SimulateTpRemoveClick();
void SimulateCraftClick();
void SimulateCraftAllClick();
void SimulateWizardVaultClick();
void SimulateCharSwapClick();
void SimulateTpBuySellClick();
void SimulateWizardVaultCompleteClick();
void SimulateWizardVaultCombo();  // Wizard Vault → delay → Wizard Vault Complete
void SimulateAcceptClick();
void SimulateGeneralAcceptClick();
void SimulateGeneralAccept2Click();
void SimulateGeneralAccept3Click();
void SimulateGeneralAccept4Click();
void SimulateAccept7Click();
void SimulateAccept8Click();
void SimulateGeneralAcceptCombo();       // "Accept Combo" — click Accept 1..8 in sequence
void SimulateMailCombo();               // Shift+0 (open Mail) → delay → click Take All
void SimulateCraftCollapseCombo();      // click Filter → delay → click Collapse
void SimulateCraftCloseClick();
void SimulateGuildHallClick();
void SimulateGuildHallCombo();    // Press G → delay → click Guild Hall button
void SimulateGenericClick(int slot);
void SimulateClickAt(int x, int y);
void SimulateRightClickAt(int x, int y);
void SimulateRealClickAt(int x, int y);  // SendInput-based — works on Nexus overlays
void SimulateKeyPress(WPARAM vkCode);    // Send a single keypress (WM_KEYDOWN+KEYUP) to the game


// Config functions
void SetConfigPath(const char* addonPath, const char* legacyPath);
void LoadButtonPositions();
void SaveButtonPositions();
void CheckResolutionChange();

// Button positions
extern int g_DepositX;
extern int g_DepositY;
extern int g_SortX;
extern int g_SortY;
extern int g_ChestX;
extern int g_ChestY;
extern int g_ExitInstanceX;
extern int g_ExitInstanceY;
extern int g_Generic1X;
extern int g_Generic1Y;
extern int g_Generic2X;
extern int g_Generic2Y;
extern int g_Generic3X;
extern int g_Generic3Y;
extern int g_Generic4X;
extern int g_Generic4Y;
extern int g_Generic5X;
extern int g_Generic5Y;
extern int g_YesDialogX;
extern int g_YesDialogY;
extern int g_MysticForgeX;
extern int g_MysticForgeY;
extern int g_MysticRefillX;
extern int g_MysticRefillY;
extern int g_VendorX;
extern int g_VendorY;
extern int g_SellJunkX;
extern int g_SellJunkY;
extern int g_TradingPostX;
extern int g_TradingPostY;
extern int g_TpRemoveX;
extern int g_TpRemoveY;
extern int g_CraftX;
extern int g_CraftY;
extern int g_CraftAllX;
extern int g_CraftAllY;
extern int g_WizardVaultX;
extern int g_WizardVaultY;
extern int g_CharSwapX;
extern int g_CharSwapY;
extern int g_TpBuySellX;
extern int g_TpBuySellY;
extern int g_WizardVaultCompleteX;
extern int g_WizardVaultCompleteY;
extern int g_AcceptX;
extern int g_AcceptY;
extern int g_GeneralAcceptX;
extern int g_GeneralAcceptY;
extern int g_GeneralAccept2X;
extern int g_GeneralAccept2Y;
extern int g_GeneralAccept3X;
extern int g_GeneralAccept3Y;
extern int g_GeneralAccept4X;
extern int g_GeneralAccept4Y;
extern int g_Accept7X;
extern int g_Accept7Y;
extern int g_Accept8X;
extern int g_Accept8Y;
extern int g_MailTakeAllX;
extern int g_MailTakeAllY;
extern int g_CraftFilterX;
extern int g_CraftFilterY;
extern int g_CraftCollapseX;
extern int g_CraftCollapseY;
extern int g_CraftCloseX;
extern int g_CraftCloseY;
extern int g_GuildHallX;
extern int g_GuildHallY;
