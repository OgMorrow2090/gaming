/**
 * config.cpp
 * 
 * Configuration file handling for Mystic Clicker button positions.
 * Saves/loads positions to resolution-specific config files.
 * e.g., mystic-clicker-1920x1080.cfg, mystic-clicker-2560x1440.cfg
 * 
 * Author: OgMorrow2090
 * Repository: https://github.com/OgMorrow2090/guildwars2
 */

#include "shared.h"
#include <fstream>
#include <string>
#include <cstdio>

// =============================================================================
// Button Position State
// =============================================================================

// Default positions - will be overwritten by config or capture
int g_DepositX = 0;
int g_DepositY = 0;
int g_SortX = 0;
int g_SortY = 0;
int g_ChestX = 0;
int g_ChestY = 0;
int g_BouncyAcceptX = 0;
int g_BouncyAcceptY = 0;
int g_BouncyMetaCompleteX = 0;
int g_BouncyMetaCompleteY = 0;
int g_ExitInstanceX = 0;
int g_ExitInstanceY = 0;
int g_Generic1X = 0;
int g_Generic1Y = 0;
int g_Generic2X = 0;
int g_Generic2Y = 0;
int g_Generic3X = 0;
int g_Generic3Y = 0;
int g_Generic4X = 0;
int g_Generic4Y = 0;
int g_Generic5X = 0;
int g_Generic5Y = 0;
int g_YesDialogX = 0;
int g_YesDialogY = 0;
int g_MysticForgeX = 0;
int g_MysticForgeY = 0;
int g_MysticRefillX = 0;
int g_MysticRefillY = 0;
int g_VendorX = 0;
int g_VendorY = 0;
int g_SellJunkX = 0;
int g_SellJunkY = 0;
int g_TradingPostX = 0;
int g_TradingPostY = 0;
int g_TpRemoveX = 0;
int g_TpRemoveY = 0;
int g_CraftX = 0;
int g_CraftY = 0;
int g_CraftAllX = 0;
int g_CraftAllY = 0;
int g_WizardVaultX = 0;
int g_WizardVaultY = 0;
int g_CharSwapX = 0;
int g_CharSwapY = 0;
int g_TpBuySellX = 0;
int g_TpBuySellY = 0;
int g_WizardVaultCompleteX = 0;
int g_WizardVaultCompleteY = 0;
int g_AcceptX = 0;
int g_AcceptY = 0;
int g_GeneralAcceptX = 0;
int g_GeneralAcceptY = 0;
int g_GeneralAccept2X = 0;
int g_GeneralAccept2Y = 0;
int g_GeneralAccept3X = 0;
int g_GeneralAccept3Y = 0;
int g_GeneralAccept4X = 0;
int g_GeneralAccept4Y = 0;
int g_Accept7X = 0;
int g_Accept7Y = 0;
int g_Accept8X = 0;
int g_Accept8Y = 0;
int g_Accept9X = 0;
int g_Accept9Y = 0;
int g_Accept10X = 0;
int g_Accept10Y = 0;
int g_Accept11X = 0;
int g_Accept11Y = 0;
int g_Accept12X = 0;
int g_Accept12Y = 0;
int g_Accept13X = 0;
int g_Accept13Y = 0;
int g_Accept14X = 0;
int g_Accept14Y = 0;
int g_Accept15X = 0;
int g_Accept15Y = 0;
int g_WizardVaultDailyTabX = 0;
int g_WizardVaultDailyTabY = 0;
int g_WizardVaultWeeklyTabX = 0;
int g_WizardVaultWeeklyTabY = 0;
int g_WizardVaultSpecialTabX = 0;
int g_WizardVaultSpecialTabY = 0;
int g_LfgSearchX = 0;
int g_LfgSearchY = 0;
int g_TeleportFriendX = 0;
int g_TeleportFriendY = 0;
int g_TradingPostIconX = 0;
int g_TradingPostIconY = 0;
int g_BankIconX = 0;
int g_BankIconY = 0;
int g_MailTakeAllX = 0;
int g_MailTakeAllY = 0;
int g_CraftFilterX = 0;
int g_CraftFilterY = 0;
int g_CraftCollapseX = 0;
int g_CraftCollapseY = 0;
int g_CraftCloseX = 0;
int g_CraftCloseY = 0;
int g_GuildHallX = 0;
int g_GuildHallY = 0;

// Per-resolution capture window state (position + size)
float g_CaptureWinX = 0.0f;
float g_CaptureWinY = 0.0f;
float g_CaptureWinW = 0.0f;
float g_CaptureWinH = 0.0f;
bool g_ResetWindowsFlag = false;

// Resolution tracking
static int g_CurrentResWidth = 0;
static int g_CurrentResHeight = 0;

// Config directory path (base path without filename)
static std::string g_ConfigDir;

// =============================================================================
// Resolution Detection
// =============================================================================

void GetCurrentResolution(int& width, int& height)
{
    // Try to get resolution from game window first
    if (GameWindow != NULL)
    {
        RECT rect;
        if (GetClientRect(GameWindow, &rect))
        {
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
            return;
        }
    }
    
    // Fallback to primary monitor resolution
    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);
}

std::string GetConfigPathForResolution(int width, int height)
{
    char buffer[512];
    sprintf_s(buffer, "%s\\mystic-clicker-%dx%d.cfg", g_ConfigDir.c_str(), width, height);
    return std::string(buffer);
}

std::string GetCurrentConfigPath()
{
    int width, height;
    GetCurrentResolution(width, height);
    g_CurrentResWidth = width;
    g_CurrentResHeight = height;
    return GetConfigPathForResolution(width, height);
}

// =============================================================================
// Config File Functions
// =============================================================================

static void MigrateLegacyConfigs(const std::string& newDir, const char* legacyPath)
{
    if (legacyPath == nullptr) return;

    std::string oldDir(legacyPath);

    // Check if legacy directory exists
    DWORD attrs = GetFileAttributesA(oldDir.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        return;

    // Find all old config files and copy them with new names
    std::string searchPattern = oldDir + "\\inventory-hotkeys-*.cfg";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &fd);

    if (hFind == INVALID_HANDLE_VALUE) return;

    int migrated = 0;
    do
    {
        std::string oldFile = oldDir + "\\" + fd.cFileName;

        // Replace "inventory-hotkeys-" with "mystic-clicker-" in filename
        std::string newName(fd.cFileName);
        size_t pos = newName.find("inventory-hotkeys-");
        if (pos != std::string::npos)
            newName.replace(pos, 18, "mystic-clicker-");

        std::string newFile = newDir + "\\" + newName;

        // Only copy if destination doesn't already exist
        if (GetFileAttributesA(newFile.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            CopyFileA(oldFile.c_str(), newFile.c_str(), TRUE);
            migrated++;
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);

    if (migrated > 0)
    {
        char buffer[128];
        sprintf_s(buffer, "Migrated %d config(s) from InventoryHotkeys", migrated);
        APIDefs->Log(LOGL_INFO, "MysticClicker", buffer);
        APIDefs->GUI_SendAlert(buffer);
    }
}

void SetConfigPath(const char* addonPath, const char* legacyPath)
{
    if (addonPath != nullptr)
    {
        g_ConfigDir = std::string(addonPath);

        // Create directory if it doesn't exist
        CreateDirectoryA(addonPath, NULL);

        // Migrate configs from old InventoryHotkeys directory
        MigrateLegacyConfigs(g_ConfigDir, legacyPath);

        // Log current resolution
        int width, height;
        GetCurrentResolution(width, height);
        char buffer[128];
        sprintf_s(buffer, "Detected resolution: %dx%d", width, height);
        APIDefs->Log(LOGL_INFO, "MysticClicker", buffer);
    }
}

void LoadButtonPositions()
{
    if (g_ConfigDir.empty())
    {
        APIDefs->Log(LOGL_WARNING, "MysticClicker", "Config path not set");
        return;
    }
    
    std::string configPath = GetCurrentConfigPath();
    
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        char buffer[256];
        sprintf_s(buffer, "No config for %dx%d - use Ctrl+Shift+D/S/B to capture positions", 
                  g_CurrentResWidth, g_CurrentResHeight);
        APIDefs->Log(LOGL_INFO, "MysticClicker", buffer);
        APIDefs->GUI_SendAlert(buffer);
        return;
    }
    
    std::string line;
    while (std::getline(file, line))
    {
        if (line.find("DepositX=") == 0)
            g_DepositX = std::stoi(line.substr(9));
        else if (line.find("DepositY=") == 0)
            g_DepositY = std::stoi(line.substr(9));
        else if (line.find("SortX=") == 0)
            g_SortX = std::stoi(line.substr(6));
        else if (line.find("SortY=") == 0)
            g_SortY = std::stoi(line.substr(6));
        else if (line.find("ChestX=") == 0)
            g_ChestX = std::stoi(line.substr(7));
        else if (line.find("ChestY=") == 0)
            g_ChestY = std::stoi(line.substr(7));
        else if (line.find("BouncyAcceptX=") == 0)
            g_BouncyAcceptX = std::stoi(line.substr(14));
        else if (line.find("BouncyAcceptY=") == 0)
            g_BouncyAcceptY = std::stoi(line.substr(14));
        else if (line.find("ExitInstanceX=") == 0)
            g_ExitInstanceX = std::stoi(line.substr(14));
        else if (line.find("ExitInstanceY=") == 0)
            g_ExitInstanceY = std::stoi(line.substr(14));
        else if (line.find("Generic1X=") == 0)
            g_Generic1X = std::stoi(line.substr(10));
        else if (line.find("Generic1Y=") == 0)
            g_Generic1Y = std::stoi(line.substr(10));
        else if (line.find("Generic2X=") == 0)
            g_Generic2X = std::stoi(line.substr(10));
        else if (line.find("Generic2Y=") == 0)
            g_Generic2Y = std::stoi(line.substr(10));
        else if (line.find("Generic3X=") == 0)
            g_Generic3X = std::stoi(line.substr(10));
        else if (line.find("Generic3Y=") == 0)
            g_Generic3Y = std::stoi(line.substr(10));
        else if (line.find("Generic4X=") == 0)
            g_Generic4X = std::stoi(line.substr(10));
        else if (line.find("Generic4Y=") == 0)
            g_Generic4Y = std::stoi(line.substr(10));
        else if (line.find("Generic5X=") == 0)
            g_Generic5X = std::stoi(line.substr(10));
        else if (line.find("Generic5Y=") == 0)
            g_Generic5Y = std::stoi(line.substr(10));
        else if (line.find("YesDialogX=") == 0)
            g_YesDialogX = std::stoi(line.substr(11));
        else if (line.find("YesDialogY=") == 0)
            g_YesDialogY = std::stoi(line.substr(11));
        else if (line.find("MysticForgeX=") == 0)
            g_MysticForgeX = std::stoi(line.substr(13));
        else if (line.find("MysticForgeY=") == 0)
            g_MysticForgeY = std::stoi(line.substr(13));
        else if (line.find("MysticRefillX=") == 0)
            g_MysticRefillX = std::stoi(line.substr(14));
        else if (line.find("MysticRefillY=") == 0)
            g_MysticRefillY = std::stoi(line.substr(14));
        else if (line.find("VendorX=") == 0)
            g_VendorX = std::stoi(line.substr(8));
        else if (line.find("VendorY=") == 0)
            g_VendorY = std::stoi(line.substr(8));
        else if (line.find("SellJunkX=") == 0)
            g_SellJunkX = std::stoi(line.substr(10));
        else if (line.find("SellJunkY=") == 0)
            g_SellJunkY = std::stoi(line.substr(10));
        else if (line.find("TradingPostX=") == 0)
            g_TradingPostX = std::stoi(line.substr(13));
        else if (line.find("TradingPostY=") == 0)
            g_TradingPostY = std::stoi(line.substr(13));
        else if (line.find("TpRemoveX=") == 0)
            g_TpRemoveX = std::stoi(line.substr(10));
        else if (line.find("TpRemoveY=") == 0)
            g_TpRemoveY = std::stoi(line.substr(10));
        else if (line.find("CraftX=") == 0)
            g_CraftX = std::stoi(line.substr(7));
        else if (line.find("CraftY=") == 0)
            g_CraftY = std::stoi(line.substr(7));
        else if (line.find("CraftAllX=") == 0)
            g_CraftAllX = std::stoi(line.substr(10));
        else if (line.find("CraftAllY=") == 0)
            g_CraftAllY = std::stoi(line.substr(10));
        else if (line.find("WizardVaultX=") == 0)
            g_WizardVaultX = std::stoi(line.substr(13));
        else if (line.find("WizardVaultY=") == 0)
            g_WizardVaultY = std::stoi(line.substr(13));
        else if (line.find("CharSwapX=") == 0)
            g_CharSwapX = std::stoi(line.substr(10));
        else if (line.find("CharSwapY=") == 0)
            g_CharSwapY = std::stoi(line.substr(10));
        else if (line.find("TpBuySellX=") == 0)
            g_TpBuySellX = std::stoi(line.substr(11));
        else if (line.find("TpBuySellY=") == 0)
            g_TpBuySellY = std::stoi(line.substr(11));
        else if (line.find("WizardVaultCompleteX=") == 0)
            g_WizardVaultCompleteX = std::stoi(line.substr(21));
        else if (line.find("WizardVaultCompleteY=") == 0)
            g_WizardVaultCompleteY = std::stoi(line.substr(21));
        else if (line.find("AcceptX=") == 0)
            g_AcceptX = std::stoi(line.substr(8));
        else if (line.find("AcceptY=") == 0)
            g_AcceptY = std::stoi(line.substr(8));
        else if (line.find("GeneralAcceptX=") == 0)
            g_GeneralAcceptX = std::stoi(line.substr(15));
        else if (line.find("GeneralAcceptY=") == 0)
            g_GeneralAcceptY = std::stoi(line.substr(15));
        else if (line.find("GeneralAccept2X=") == 0)
            g_GeneralAccept2X = std::stoi(line.substr(16));
        else if (line.find("GeneralAccept2Y=") == 0)
            g_GeneralAccept2Y = std::stoi(line.substr(16));
        else if (line.find("GeneralAccept3X=") == 0)
            g_GeneralAccept3X = std::stoi(line.substr(16));
        else if (line.find("GeneralAccept3Y=") == 0)
            g_GeneralAccept3Y = std::stoi(line.substr(16));
        else if (line.find("GeneralAccept4X=") == 0)
            g_GeneralAccept4X = std::stoi(line.substr(16));
        else if (line.find("GeneralAccept4Y=") == 0)
            g_GeneralAccept4Y = std::stoi(line.substr(16));
        else if (line.find("Accept7X=") == 0)
            g_Accept7X = std::stoi(line.substr(9));
        else if (line.find("Accept7Y=") == 0)
            g_Accept7Y = std::stoi(line.substr(9));
        else if (line.find("Accept8X=") == 0)
            g_Accept8X = std::stoi(line.substr(9));
        else if (line.find("Accept8Y=") == 0)
            g_Accept8Y = std::stoi(line.substr(9));
        else if (line.find("Accept9X=") == 0)
            g_Accept9X = std::stoi(line.substr(9));
        else if (line.find("Accept9Y=") == 0)
            g_Accept9Y = std::stoi(line.substr(9));
        else if (line.find("Accept10X=") == 0)
            g_Accept10X = std::stoi(line.substr(10));
        else if (line.find("Accept10Y=") == 0)
            g_Accept10Y = std::stoi(line.substr(10));
        else if (line.find("Accept11X=") == 0)
            g_Accept11X = std::stoi(line.substr(10));
        else if (line.find("Accept11Y=") == 0)
            g_Accept11Y = std::stoi(line.substr(10));
        else if (line.find("Accept12X=") == 0)
            g_Accept12X = std::stoi(line.substr(10));
        else if (line.find("Accept12Y=") == 0)
            g_Accept12Y = std::stoi(line.substr(10));
        else if (line.find("Accept13X=") == 0)
            g_Accept13X = std::stoi(line.substr(10));
        else if (line.find("Accept13Y=") == 0)
            g_Accept13Y = std::stoi(line.substr(10));
        else if (line.find("Accept14X=") == 0)
            g_Accept14X = std::stoi(line.substr(10));
        else if (line.find("Accept14Y=") == 0)
            g_Accept14Y = std::stoi(line.substr(10));
        else if (line.find("Accept15X=") == 0)
            g_Accept15X = std::stoi(line.substr(10));
        else if (line.find("Accept15Y=") == 0)
            g_Accept15Y = std::stoi(line.substr(10));
        else if (line.find("WizardVaultDailyTabX=") == 0)
            g_WizardVaultDailyTabX = std::stoi(line.substr(21));
        else if (line.find("WizardVaultDailyTabY=") == 0)
            g_WizardVaultDailyTabY = std::stoi(line.substr(21));
        else if (line.find("WizardVaultWeeklyTabX=") == 0)
            g_WizardVaultWeeklyTabX = std::stoi(line.substr(22));
        else if (line.find("WizardVaultWeeklyTabY=") == 0)
            g_WizardVaultWeeklyTabY = std::stoi(line.substr(22));
        else if (line.find("WizardVaultSpecialTabX=") == 0)
            g_WizardVaultSpecialTabX = std::stoi(line.substr(23));
        else if (line.find("WizardVaultSpecialTabY=") == 0)
            g_WizardVaultSpecialTabY = std::stoi(line.substr(23));
        else if (line.find("LfgSearchX=") == 0)
            g_LfgSearchX = std::stoi(line.substr(11));
        else if (line.find("LfgSearchY=") == 0)
            g_LfgSearchY = std::stoi(line.substr(11));
        else if (line.find("TeleportFriendX=") == 0)
            g_TeleportFriendX = std::stoi(line.substr(16));
        else if (line.find("TeleportFriendY=") == 0)
            g_TeleportFriendY = std::stoi(line.substr(16));
        else if (line.find("TradingPostIconX=") == 0)
            g_TradingPostIconX = std::stoi(line.substr(17));
        else if (line.find("TradingPostIconY=") == 0)
            g_TradingPostIconY = std::stoi(line.substr(17));
        else if (line.find("BankIconX=") == 0)
            g_BankIconX = std::stoi(line.substr(10));
        else if (line.find("BankIconY=") == 0)
            g_BankIconY = std::stoi(line.substr(10));
        else if (line.find("BouncyMetaCompleteX=") == 0)
            g_BouncyMetaCompleteX = std::stoi(line.substr(20));
        else if (line.find("BouncyMetaCompleteY=") == 0)
            g_BouncyMetaCompleteY = std::stoi(line.substr(20));
        else if (line.find("MailTakeAllX=") == 0)
            g_MailTakeAllX = std::stoi(line.substr(13));
        else if (line.find("MailTakeAllY=") == 0)
            g_MailTakeAllY = std::stoi(line.substr(13));
        else if (line.find("CraftFilterX=") == 0)
            g_CraftFilterX = std::stoi(line.substr(13));
        else if (line.find("CraftFilterY=") == 0)
            g_CraftFilterY = std::stoi(line.substr(13));
        else if (line.find("CraftCollapseX=") == 0)
            g_CraftCollapseX = std::stoi(line.substr(15));
        else if (line.find("CraftCollapseY=") == 0)
            g_CraftCollapseY = std::stoi(line.substr(15));
        else if (line.find("CraftCloseX=") == 0)
            g_CraftCloseX = std::stoi(line.substr(12));
        else if (line.find("CraftCloseY=") == 0)
            g_CraftCloseY = std::stoi(line.substr(12));
        else if (line.find("GuildHallX=") == 0)
            g_GuildHallX = std::stoi(line.substr(11));
        else if (line.find("GuildHallY=") == 0)
            g_GuildHallY = std::stoi(line.substr(11));
        else if (line.find("CaptureWinX=") == 0)
            g_CaptureWinX = (float)std::stod(line.substr(12));
        else if (line.find("CaptureWinY=") == 0)
            g_CaptureWinY = (float)std::stod(line.substr(12));
        else if (line.find("CaptureWinW=") == 0)
            g_CaptureWinW = (float)std::stod(line.substr(12));
        else if (line.find("CaptureWinH=") == 0)
            g_CaptureWinH = (float)std::stod(line.substr(12));
    }

    file.close();

    char buffer[256];
    sprintf_s(buffer, "Loaded [%dx%d]: Deposit(%d,%d) Sort(%d,%d) Chest(%d,%d)", 
              g_CurrentResWidth, g_CurrentResHeight,
              g_DepositX, g_DepositY, g_SortX, g_SortY, g_ChestX, g_ChestY);
    APIDefs->Log(LOGL_INFO, "MysticClicker", buffer);
}

void SaveButtonPositions()
{
    if (g_ConfigDir.empty())
    {
        APIDefs->Log(LOGL_WARNING, "MysticClicker", "Config path not set");
        APIDefs->GUI_SendAlert("Error: Config path not set!");
        return;
    }
    
    std::string configPath = GetCurrentConfigPath();
    
    std::ofstream file(configPath);
    if (!file.is_open())
    {
        APIDefs->Log(LOGL_WARNING, "MysticClicker", "Failed to save config file");
        APIDefs->GUI_SendAlert("Error: Could not save config file!");
        return;
    }
    
    // Write resolution header as comment
    file << "# Resolution: " << g_CurrentResWidth << "x" << g_CurrentResHeight << "\n";
    file << "DepositX=" << g_DepositX << "\n";
    file << "DepositY=" << g_DepositY << "\n";
    file << "SortX=" << g_SortX << "\n";
    file << "SortY=" << g_SortY << "\n";
    file << "ChestX=" << g_ChestX << "\n";
    file << "ChestY=" << g_ChestY << "\n";
    file << "BouncyAcceptX=" << g_BouncyAcceptX << "\n";
    file << "BouncyAcceptY=" << g_BouncyAcceptY << "\n";
    file << "ExitInstanceX=" << g_ExitInstanceX << "\n";
    file << "ExitInstanceY=" << g_ExitInstanceY << "\n";
    file << "Generic1X=" << g_Generic1X << "\n";
    file << "Generic1Y=" << g_Generic1Y << "\n";
    file << "Generic2X=" << g_Generic2X << "\n";
    file << "Generic2Y=" << g_Generic2Y << "\n";
    file << "Generic3X=" << g_Generic3X << "\n";
    file << "Generic3Y=" << g_Generic3Y << "\n";
    file << "Generic4X=" << g_Generic4X << "\n";
    file << "Generic4Y=" << g_Generic4Y << "\n";
    file << "Generic5X=" << g_Generic5X << "\n";
    file << "Generic5Y=" << g_Generic5Y << "\n";
    file << "YesDialogX=" << g_YesDialogX << "\n";
    file << "YesDialogY=" << g_YesDialogY << "\n";
    file << "MysticForgeX=" << g_MysticForgeX << "\n";
    file << "MysticForgeY=" << g_MysticForgeY << "\n";
    file << "MysticRefillX=" << g_MysticRefillX << "\n";
    file << "MysticRefillY=" << g_MysticRefillY << "\n";
    file << "VendorX=" << g_VendorX << "\n";
    file << "VendorY=" << g_VendorY << "\n";
    file << "SellJunkX=" << g_SellJunkX << "\n";
    file << "SellJunkY=" << g_SellJunkY << "\n";
    file << "TradingPostX=" << g_TradingPostX << "\n";
    file << "TradingPostY=" << g_TradingPostY << "\n";
    file << "TpRemoveX=" << g_TpRemoveX << "\n";
    file << "TpRemoveY=" << g_TpRemoveY << "\n";
    file << "CraftX=" << g_CraftX << "\n";
    file << "CraftY=" << g_CraftY << "\n";
    file << "CraftAllX=" << g_CraftAllX << "\n";
    file << "CraftAllY=" << g_CraftAllY << "\n";
    file << "WizardVaultX=" << g_WizardVaultX << "\n";
    file << "WizardVaultY=" << g_WizardVaultY << "\n";
    file << "CharSwapX=" << g_CharSwapX << "\n";
    file << "CharSwapY=" << g_CharSwapY << "\n";
    file << "TpBuySellX=" << g_TpBuySellX << "\n";
    file << "TpBuySellY=" << g_TpBuySellY << "\n";
    file << "WizardVaultCompleteX=" << g_WizardVaultCompleteX << "\n";
    file << "WizardVaultCompleteY=" << g_WizardVaultCompleteY << "\n";
    file << "AcceptX=" << g_AcceptX << "\n";
    file << "AcceptY=" << g_AcceptY << "\n";
    file << "GeneralAcceptX=" << g_GeneralAcceptX << "\n";
    file << "GeneralAcceptY=" << g_GeneralAcceptY << "\n";
    file << "GeneralAccept2X=" << g_GeneralAccept2X << "\n";
    file << "GeneralAccept2Y=" << g_GeneralAccept2Y << "\n";
    file << "GeneralAccept3X=" << g_GeneralAccept3X << "\n";
    file << "GeneralAccept3Y=" << g_GeneralAccept3Y << "\n";
    file << "GeneralAccept4X=" << g_GeneralAccept4X << "\n";
    file << "GeneralAccept4Y=" << g_GeneralAccept4Y << "\n";
    file << "Accept7X=" << g_Accept7X << "\n";
    file << "Accept7Y=" << g_Accept7Y << "\n";
    file << "Accept8X=" << g_Accept8X << "\n";
    file << "Accept8Y=" << g_Accept8Y << "\n";
    file << "Accept9X=" << g_Accept9X << "\n";
    file << "Accept9Y=" << g_Accept9Y << "\n";
    file << "Accept10X=" << g_Accept10X << "\n";
    file << "Accept10Y=" << g_Accept10Y << "\n";
    file << "Accept11X=" << g_Accept11X << "\n";
    file << "Accept11Y=" << g_Accept11Y << "\n";
    file << "Accept12X=" << g_Accept12X << "\n";
    file << "Accept12Y=" << g_Accept12Y << "\n";
    file << "Accept13X=" << g_Accept13X << "\n";
    file << "Accept13Y=" << g_Accept13Y << "\n";
    file << "Accept14X=" << g_Accept14X << "\n";
    file << "Accept14Y=" << g_Accept14Y << "\n";
    file << "Accept15X=" << g_Accept15X << "\n";
    file << "Accept15Y=" << g_Accept15Y << "\n";
    file << "WizardVaultDailyTabX=" << g_WizardVaultDailyTabX << "\n";
    file << "WizardVaultDailyTabY=" << g_WizardVaultDailyTabY << "\n";
    file << "WizardVaultWeeklyTabX=" << g_WizardVaultWeeklyTabX << "\n";
    file << "WizardVaultWeeklyTabY=" << g_WizardVaultWeeklyTabY << "\n";
    file << "WizardVaultSpecialTabX=" << g_WizardVaultSpecialTabX << "\n";
    file << "WizardVaultSpecialTabY=" << g_WizardVaultSpecialTabY << "\n";
    file << "LfgSearchX=" << g_LfgSearchX << "\n";
    file << "LfgSearchY=" << g_LfgSearchY << "\n";
    file << "TeleportFriendX=" << g_TeleportFriendX << "\n";
    file << "TeleportFriendY=" << g_TeleportFriendY << "\n";
    file << "TradingPostIconX=" << g_TradingPostIconX << "\n";
    file << "TradingPostIconY=" << g_TradingPostIconY << "\n";
    file << "BankIconX=" << g_BankIconX << "\n";
    file << "BankIconY=" << g_BankIconY << "\n";
    file << "BouncyMetaCompleteX=" << g_BouncyMetaCompleteX << "\n";
    file << "BouncyMetaCompleteY=" << g_BouncyMetaCompleteY << "\n";
    file << "MailTakeAllX=" << g_MailTakeAllX << "\n";
    file << "MailTakeAllY=" << g_MailTakeAllY << "\n";
    file << "CraftFilterX=" << g_CraftFilterX << "\n";
    file << "CraftFilterY=" << g_CraftFilterY << "\n";
    file << "CraftCollapseX=" << g_CraftCollapseX << "\n";
    file << "CraftCollapseY=" << g_CraftCollapseY << "\n";
    file << "CraftCloseX=" << g_CraftCloseX << "\n";
    file << "CraftCloseY=" << g_CraftCloseY << "\n";
    file << "GuildHallX=" << g_GuildHallX << "\n";
    file << "GuildHallY=" << g_GuildHallY << "\n";
    file << "CaptureWinX=" << g_CaptureWinX << "\n";
    file << "CaptureWinY=" << g_CaptureWinY << "\n";
    file << "CaptureWinW=" << g_CaptureWinW << "\n";
    file << "CaptureWinH=" << g_CaptureWinH << "\n";

    file.close();
    
    char buffer[256];
    sprintf_s(buffer, "Saved config for %dx%d", g_CurrentResWidth, g_CurrentResHeight);
    APIDefs->Log(LOGL_INFO, "MysticClicker", buffer);
    APIDefs->GUI_SendAlert(buffer);
}

// =============================================================================
// Resolution Change Detection
// =============================================================================

void CheckResolutionChange()
{
    int width, height;
    GetCurrentResolution(width, height);
    
    // Check if resolution changed
    if (width != g_CurrentResWidth || height != g_CurrentResHeight)
    {
        char buffer[256];
        sprintf_s(buffer, "Resolution changed: %dx%d -> %dx%d", 
                  g_CurrentResWidth, g_CurrentResHeight, width, height);
        APIDefs->Log(LOGL_INFO, "MysticClicker", buffer);
        
        // Reset all positions for new resolution
        g_DepositX = 0;
        g_DepositY = 0;
        g_SortX = 0;
        g_SortY = 0;
        g_ChestX = 0;
        g_ChestY = 0;
        g_BouncyAcceptX = 0;
        g_BouncyAcceptY = 0;
        g_ExitInstanceX = 0;
        g_ExitInstanceY = 0;
        g_Generic1X = 0;
        g_Generic1Y = 0;
        g_Generic2X = 0;
        g_Generic2Y = 0;
        g_Generic3X = 0;
        g_Generic3Y = 0;
        g_Generic4X = 0;
        g_Generic4Y = 0;
        g_Generic5X = 0;
        g_Generic5Y = 0;
        g_YesDialogX = 0;
        g_YesDialogY = 0;
        g_MysticForgeX = 0;
        g_MysticForgeY = 0;
        g_MysticRefillX = 0;
        g_MysticRefillY = 0;
        g_VendorX = 0;
        g_VendorY = 0;
        g_SellJunkX = 0;
        g_SellJunkY = 0;
        g_TradingPostX = 0;
        g_TradingPostY = 0;
        g_TpRemoveX = 0;
        g_TpRemoveY = 0;
        g_CraftX = 0;
        g_CraftY = 0;
        g_CraftAllX = 0;
        g_CraftAllY = 0;
        g_WizardVaultX = 0;
        g_WizardVaultY = 0;
        g_CharSwapX = 0;
        g_CharSwapY = 0;
        g_TpBuySellX = 0;
        g_TpBuySellY = 0;
        g_WizardVaultCompleteX = 0;
        g_WizardVaultCompleteY = 0;
        g_AcceptX = 0;
        g_AcceptY = 0;
        g_GeneralAcceptX = 0;
        g_GeneralAcceptY = 0;
        g_GeneralAccept2X = 0;
        g_GeneralAccept2Y = 0;
        g_GeneralAccept3X = 0;
        g_GeneralAccept3Y = 0;
        g_GeneralAccept4X = 0;
        g_GeneralAccept4Y = 0;
        g_Accept7X = 0;
        g_Accept7Y = 0;
        g_Accept8X = 0;
        g_Accept8Y = 0;
        g_Accept9X = 0;
        g_Accept9Y = 0;
        g_Accept10X = 0;
        g_Accept10Y = 0;
        g_Accept11X = 0;
        g_Accept11Y = 0;
        g_Accept12X = 0;
        g_Accept12Y = 0;
        g_Accept13X = 0;
        g_Accept13Y = 0;
        g_Accept14X = 0;
        g_Accept14Y = 0;
        g_Accept15X = 0;
        g_Accept15Y = 0;
        g_WizardVaultDailyTabX = 0;
        g_WizardVaultDailyTabY = 0;
        g_WizardVaultWeeklyTabX = 0;
        g_WizardVaultWeeklyTabY = 0;
        g_WizardVaultSpecialTabX = 0;
        g_WizardVaultSpecialTabY = 0;
        g_LfgSearchX = 0;
        g_LfgSearchY = 0;
        g_TeleportFriendX = 0;
        g_TeleportFriendY = 0;
        g_TradingPostIconX = 0;
        g_TradingPostIconY = 0;
        g_BankIconX = 0;
        g_BankIconY = 0;
        g_BouncyMetaCompleteX = 0;
        g_BouncyMetaCompleteY = 0;
        g_MailTakeAllX = 0;
        g_MailTakeAllY = 0;
        g_CraftFilterX = 0;
        g_CraftFilterY = 0;
        g_CraftCollapseX = 0;
        g_CraftCollapseY = 0;
        g_CraftCloseX = 0;
        g_CraftCloseY = 0;
        g_GuildHallX = 0;
        g_GuildHallY = 0;
        g_CaptureWinX = 0.0f;
        g_CaptureWinY = 0.0f;
        g_CaptureWinW = 0.0f;
        g_CaptureWinH = 0.0f;

        // Load config for new resolution (if exists)
        g_CurrentResWidth = width;
        g_CurrentResHeight = height;
        
        std::string configPath = GetConfigPathForResolution(width, height);
        std::ifstream file(configPath);
        
        if (file.is_open())
        {
            std::string line;
            while (std::getline(file, line))
            {
                if (line.find("DepositX=") == 0)
                    g_DepositX = std::stoi(line.substr(9));
                else if (line.find("DepositY=") == 0)
                    g_DepositY = std::stoi(line.substr(9));
                else if (line.find("SortX=") == 0)
                    g_SortX = std::stoi(line.substr(6));
                else if (line.find("SortY=") == 0)
                    g_SortY = std::stoi(line.substr(6));
                else if (line.find("ChestX=") == 0)
                    g_ChestX = std::stoi(line.substr(7));
                else if (line.find("ChestY=") == 0)
                    g_ChestY = std::stoi(line.substr(7));
                else if (line.find("BouncyAcceptX=") == 0)
                    g_BouncyAcceptX = std::stoi(line.substr(14));
                else if (line.find("BouncyAcceptY=") == 0)
                    g_BouncyAcceptY = std::stoi(line.substr(14));
                else if (line.find("ExitInstanceX=") == 0)
                    g_ExitInstanceX = std::stoi(line.substr(14));
                else if (line.find("ExitInstanceY=") == 0)
                    g_ExitInstanceY = std::stoi(line.substr(14));
                else if (line.find("Generic1X=") == 0)
                    g_Generic1X = std::stoi(line.substr(10));
                else if (line.find("Generic1Y=") == 0)
                    g_Generic1Y = std::stoi(line.substr(10));
                else if (line.find("Generic2X=") == 0)
                    g_Generic2X = std::stoi(line.substr(10));
                else if (line.find("Generic2Y=") == 0)
                    g_Generic2Y = std::stoi(line.substr(10));
                else if (line.find("Generic3X=") == 0)
                    g_Generic3X = std::stoi(line.substr(10));
                else if (line.find("Generic3Y=") == 0)
                    g_Generic3Y = std::stoi(line.substr(10));
                else if (line.find("Generic4X=") == 0)
                    g_Generic4X = std::stoi(line.substr(10));
                else if (line.find("Generic4Y=") == 0)
                    g_Generic4Y = std::stoi(line.substr(10));
                else if (line.find("Generic5X=") == 0)
                    g_Generic5X = std::stoi(line.substr(10));
                else if (line.find("Generic5Y=") == 0)
                    g_Generic5Y = std::stoi(line.substr(10));
                else if (line.find("YesDialogX=") == 0)
                    g_YesDialogX = std::stoi(line.substr(11));
                else if (line.find("YesDialogY=") == 0)
                    g_YesDialogY = std::stoi(line.substr(11));
                else if (line.find("MysticForgeX=") == 0)
                    g_MysticForgeX = std::stoi(line.substr(13));
                else if (line.find("MysticForgeY=") == 0)
                    g_MysticForgeY = std::stoi(line.substr(13));
                else if (line.find("MysticRefillX=") == 0)
                    g_MysticRefillX = std::stoi(line.substr(14));
                else if (line.find("MysticRefillY=") == 0)
                    g_MysticRefillY = std::stoi(line.substr(14));
                else if (line.find("VendorX=") == 0)
                    g_VendorX = std::stoi(line.substr(8));
                else if (line.find("VendorY=") == 0)
                    g_VendorY = std::stoi(line.substr(8));
                else if (line.find("SellJunkX=") == 0)
                    g_SellJunkX = std::stoi(line.substr(10));
                else if (line.find("SellJunkY=") == 0)
                    g_SellJunkY = std::stoi(line.substr(10));
                else if (line.find("TradingPostX=") == 0)
                    g_TradingPostX = std::stoi(line.substr(13));
                else if (line.find("TradingPostY=") == 0)
                    g_TradingPostY = std::stoi(line.substr(13));
                else if (line.find("TpRemoveX=") == 0)
                    g_TpRemoveX = std::stoi(line.substr(10));
                else if (line.find("TpRemoveY=") == 0)
                    g_TpRemoveY = std::stoi(line.substr(10));
                else if (line.find("CraftX=") == 0)
                    g_CraftX = std::stoi(line.substr(7));
                else if (line.find("CraftY=") == 0)
                    g_CraftY = std::stoi(line.substr(7));
                else if (line.find("CraftAllX=") == 0)
                    g_CraftAllX = std::stoi(line.substr(10));
                else if (line.find("CraftAllY=") == 0)
                    g_CraftAllY = std::stoi(line.substr(10));
                else if (line.find("WizardVaultX=") == 0)
                    g_WizardVaultX = std::stoi(line.substr(13));
                else if (line.find("WizardVaultY=") == 0)
                    g_WizardVaultY = std::stoi(line.substr(13));
                else if (line.find("CharSwapX=") == 0)
                    g_CharSwapX = std::stoi(line.substr(10));
                else if (line.find("CharSwapY=") == 0)
                    g_CharSwapY = std::stoi(line.substr(10));
                else if (line.find("TpBuySellX=") == 0)
                    g_TpBuySellX = std::stoi(line.substr(11));
                else if (line.find("TpBuySellY=") == 0)
                    g_TpBuySellY = std::stoi(line.substr(11));
                else if (line.find("WizardVaultCompleteX=") == 0)
                    g_WizardVaultCompleteX = std::stoi(line.substr(21));
                else if (line.find("WizardVaultCompleteY=") == 0)
                    g_WizardVaultCompleteY = std::stoi(line.substr(21));
                else if (line.find("AcceptX=") == 0)
                    g_AcceptX = std::stoi(line.substr(8));
                else if (line.find("AcceptY=") == 0)
                    g_AcceptY = std::stoi(line.substr(8));
                else if (line.find("GeneralAcceptX=") == 0)
                    g_GeneralAcceptX = std::stoi(line.substr(15));
                else if (line.find("GeneralAcceptY=") == 0)
                    g_GeneralAcceptY = std::stoi(line.substr(15));
                else if (line.find("GeneralAccept2X=") == 0)
                    g_GeneralAccept2X = std::stoi(line.substr(16));
                else if (line.find("GeneralAccept2Y=") == 0)
                    g_GeneralAccept2Y = std::stoi(line.substr(16));
                else if (line.find("GeneralAccept3X=") == 0)
                    g_GeneralAccept3X = std::stoi(line.substr(16));
                else if (line.find("GeneralAccept3Y=") == 0)
                    g_GeneralAccept3Y = std::stoi(line.substr(16));
                else if (line.find("GeneralAccept4X=") == 0)
                    g_GeneralAccept4X = std::stoi(line.substr(16));
                else if (line.find("GeneralAccept4Y=") == 0)
                    g_GeneralAccept4Y = std::stoi(line.substr(16));
                else if (line.find("Accept7X=") == 0)
                    g_Accept7X = std::stoi(line.substr(9));
                else if (line.find("Accept7Y=") == 0)
                    g_Accept7Y = std::stoi(line.substr(9));
                else if (line.find("Accept8X=") == 0)
                    g_Accept8X = std::stoi(line.substr(9));
                else if (line.find("Accept8Y=") == 0)
                    g_Accept8Y = std::stoi(line.substr(9));
                else if (line.find("Accept9X=") == 0)
                    g_Accept9X = std::stoi(line.substr(9));
                else if (line.find("Accept9Y=") == 0)
                    g_Accept9Y = std::stoi(line.substr(9));
                else if (line.find("Accept10X=") == 0)
                    g_Accept10X = std::stoi(line.substr(10));
                else if (line.find("Accept10Y=") == 0)
                    g_Accept10Y = std::stoi(line.substr(10));
                else if (line.find("Accept11X=") == 0)
                    g_Accept11X = std::stoi(line.substr(10));
                else if (line.find("Accept11Y=") == 0)
                    g_Accept11Y = std::stoi(line.substr(10));
                else if (line.find("Accept12X=") == 0)
                    g_Accept12X = std::stoi(line.substr(10));
                else if (line.find("Accept12Y=") == 0)
                    g_Accept12Y = std::stoi(line.substr(10));
                else if (line.find("Accept13X=") == 0)
                    g_Accept13X = std::stoi(line.substr(10));
                else if (line.find("Accept13Y=") == 0)
                    g_Accept13Y = std::stoi(line.substr(10));
                else if (line.find("Accept14X=") == 0)
                    g_Accept14X = std::stoi(line.substr(10));
                else if (line.find("Accept14Y=") == 0)
                    g_Accept14Y = std::stoi(line.substr(10));
                else if (line.find("Accept15X=") == 0)
                    g_Accept15X = std::stoi(line.substr(10));
                else if (line.find("Accept15Y=") == 0)
                    g_Accept15Y = std::stoi(line.substr(10));
                else if (line.find("WizardVaultWeeklyTabX=") == 0)
                    g_WizardVaultWeeklyTabX = std::stoi(line.substr(22));
                else if (line.find("WizardVaultWeeklyTabY=") == 0)
                    g_WizardVaultWeeklyTabY = std::stoi(line.substr(22));
                else if (line.find("LfgSearchX=") == 0)
                    g_LfgSearchX = std::stoi(line.substr(11));
                else if (line.find("LfgSearchY=") == 0)
                    g_LfgSearchY = std::stoi(line.substr(11));
                else if (line.find("BouncyMetaCompleteX=") == 0)
                    g_BouncyMetaCompleteX = std::stoi(line.substr(20));
                else if (line.find("BouncyMetaCompleteY=") == 0)
                    g_BouncyMetaCompleteY = std::stoi(line.substr(20));
                else if (line.find("MailTakeAllX=") == 0)
                    g_MailTakeAllX = std::stoi(line.substr(13));
                else if (line.find("MailTakeAllY=") == 0)
                    g_MailTakeAllY = std::stoi(line.substr(13));
                else if (line.find("CraftFilterX=") == 0)
                    g_CraftFilterX = std::stoi(line.substr(13));
                else if (line.find("CraftFilterY=") == 0)
                    g_CraftFilterY = std::stoi(line.substr(13));
                else if (line.find("CraftCollapseX=") == 0)
                    g_CraftCollapseX = std::stoi(line.substr(15));
                else if (line.find("CraftCollapseY=") == 0)
                    g_CraftCollapseY = std::stoi(line.substr(15));
                else if (line.find("CraftCloseX=") == 0)
                    g_CraftCloseX = std::stoi(line.substr(12));
                else if (line.find("CraftCloseY=") == 0)
                    g_CraftCloseY = std::stoi(line.substr(12));
                else if (line.find("GuildHallX=") == 0)
                    g_GuildHallX = std::stoi(line.substr(11));
                else if (line.find("GuildHallY=") == 0)
                    g_GuildHallY = std::stoi(line.substr(11));
                else if (line.find("CaptureWinX=") == 0)
                    g_CaptureWinX = (float)std::stod(line.substr(12));
                else if (line.find("CaptureWinY=") == 0)
                    g_CaptureWinY = (float)std::stod(line.substr(12));
                else if (line.find("CaptureWinW=") == 0)
                    g_CaptureWinW = (float)std::stod(line.substr(12));
                else if (line.find("CaptureWinH=") == 0)
                    g_CaptureWinH = (float)std::stod(line.substr(12));
            }
            file.close();

            sprintf_s(buffer, "Loaded config for %dx%d", width, height);
            APIDefs->Log(LOGL_INFO, "MysticClicker", buffer);
            APIDefs->GUI_SendAlert(buffer);
        }
        else
        {
            sprintf_s(buffer, "New resolution %dx%d - capture positions to configure", width, height);
            APIDefs->GUI_SendAlert(buffer);
        }
    }
}
