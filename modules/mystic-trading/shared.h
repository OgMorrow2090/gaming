#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <mutex>
#include "Nexus.h"

// ImGui forward declaration
struct ImGuiContext;

// Global API pointer - set in AddonLoad
extern AddonAPI_t* APIDefs;

// Addon definition
extern AddonDefinition_t AddonDef;

// ImGui context
extern ImGuiContext* ImGuiCtx;

// Keybind identifiers
constexpr const char* KB_TOGGLE_DASHBOARD = "MT_TOGGLE_DASHBOARD";
constexpr const char* KB_TOGGLE_FLIPLIST  = "MT_TOGGLE_FLIPLIST";
constexpr const char* KB_TOGGLE_DELIVERY  = "MT_TOGGLE_DELIVERY";

// Window visibility
extern bool g_ShowDashboard;
extern bool g_ShowFlipList;
extern bool g_ShowDelivery;
extern bool g_LockFlipList;
extern bool g_LockDelivery;

// Keybind handler
void ProcessKeybind(const char* aIdentifier, bool aIsRelease);

// Render callbacks
void RenderDashboard();
void RenderFlipList();
void RenderDeliveryBox();
void RenderOptions();

// Coin display helper
struct Coins {
    int gold;
    int silver;
    int copper;
    int raw;
};

// Item rarity
enum class Rarity {
    Junk,
    Basic,
    Fine,
    Masterwork,
    Rare,
    Exotic,
    Ascended,
    Legendary
};

// Base item data
struct Item {
    int id;
    std::string name;
    std::string icon;
    Rarity rarity;
    int count;
    Coins price;
    Coins totalValue;
};

// Flip item
struct FlipItem {
    int id;
    std::string name;
    std::string icon;
    Rarity rarity;
    Coins buyPrice;
    Coins sellPrice;
    Coins profit;
    float roi;
    int supply;
    int demand;
    int sold;
    int bought;
};

// Transaction (buy/sell order)
struct Transaction {
    int itemId;
    std::string name;
    std::string icon;
    Rarity rarity;
    Coins price;
    int quantity;
    std::string created;
};

// Currency
struct Currency {
    int id;
    std::string name;
    std::string icon;
    int value;
};

// Wallet data
struct WalletData {
    Coins gold;
    std::vector<Currency> currencies;
};

// Trading post data
struct TradingPostData {
    WalletData wallet;
    struct {
        Coins coins;
        std::vector<Item> items;
    } delivery;
    std::vector<Transaction> sells;
    std::vector<Transaction> buys;
    Coins sellValue;
    Coins buyValue;
};

// Full portal data
struct PortalData {
    TradingPostData tradingPost;
    std::vector<FlipItem> flips;
    std::vector<Item> bank;
    std::vector<Item> materials;
    Coins bankTotal;
    Coins matsTotal;
    bool loaded;
};

// Thread-safe portal data access
extern PortalData g_Data;
extern std::mutex g_DataMutex;

// API client
void SetApiConfig(const char* addonPath);
void StartDataFetch();
void StopDataFetch();

// Clipboard
void CopyToClipboard(const std::string& text);

// Icon loading via Nexus texture API
void LoadItemIcon(const std::string& iconUrl, int itemId);

// Config
void LoadConfig();
void SaveConfig();

// Config values
extern int g_RefreshInterval;
