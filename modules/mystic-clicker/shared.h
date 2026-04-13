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

// Capture keybind identifiers
constexpr const char* CAPTURE_CRAFT = "CAPTURE_CRAFT";
constexpr const char* CAPTURE_CRAFT_ALL = "CAPTURE_CRAFT_ALL";
constexpr const char* CAPTURE_WIZARD_VAULT = "CAPTURE_WIZARD_VAULT";
constexpr const char* CAPTURE_CHAR_SWAP = "CAPTURE_CHAR_SWAP";

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
void SimulateGenericClick(int slot);
void SimulateClickAt(int x, int y);
void SimulateRightClickAt(int x, int y);


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
