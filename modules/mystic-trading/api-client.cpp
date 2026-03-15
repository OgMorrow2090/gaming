/**
 * api-client.cpp
 *
 * Background data fetcher for Mystic Trading.
 * Fetches trading post data from the addams.family portal API.
 */

#include "shared.h"
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")

static std::atomic<bool> g_Running{false};
static std::thread g_FetchThread;
static std::string g_ConfigPath;
static std::string g_SessionCookie;

void SetApiConfig(const char* addonPath)
{
    g_ConfigPath = std::string(addonPath) + "\\mystic-trading.cfg";
    CreateDirectoryA(addonPath, nullptr);
}

// ============================================================================
// HTTP
// ============================================================================

static std::string HttpGet(const std::string& url, const std::string& sessionCookie)
{
    std::string result;

    HINTERNET hInternet = InternetOpenA("MysticTrading/0.1", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) return result;

    std::string headers = "Cookie: session=" + sessionCookie + "\r\n";

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (url.find("https://") == 0)
        flags |= INTERNET_FLAG_SECURE;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), headers.c_str(), (DWORD)headers.length(), flags, 0);

    if (hUrl)
    {
        char buffer[8192];
        DWORD bytesRead = 0;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            result += buffer;
        }
        InternetCloseHandle(hUrl);
    }

    InternetCloseHandle(hInternet);
    return result;
}

// ============================================================================
// JSON Parsing Helpers
// ============================================================================

// Find the end of a JSON object starting at the opening brace
static size_t FindObjectEnd(const std::string& json, size_t start)
{
    int depth = 0;
    for (size_t i = start; i < json.length(); i++)
    {
        if (json[i] == '"')
        {
            // Skip strings (handle escaped quotes)
            i++;
            while (i < json.length() && json[i] != '"')
            {
                if (json[i] == '\\') i++;
                i++;
            }
            continue;
        }
        if (json[i] == '{' || json[i] == '[') depth++;
        if (json[i] == '}' || json[i] == ']') depth--;
        if (depth == 0) return i;
    }
    return std::string::npos;
}

// Find a key in JSON and return the value substring
static std::string FindValue(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
        pos++;

    if (pos >= json.length()) return "";

    // String value
    if (json[pos] == '"')
    {
        size_t end = pos + 1;
        while (end < json.length() && json[end] != '"')
        {
            if (json[end] == '\\') end++;
            end++;
        }
        return json.substr(pos + 1, end - pos - 1);
    }

    // Object or array
    if (json[pos] == '{' || json[pos] == '[')
    {
        size_t end = FindObjectEnd(json, pos);
        if (end != std::string::npos)
            return json.substr(pos, end - pos + 1);
        return "";
    }

    // Number, bool, null
    size_t end = json.find_first_of(",}] \t\n\r", pos);
    if (end == std::string::npos) end = json.length();
    return json.substr(pos, end - pos);
}

static int ToInt(const std::string& s, int def = 0)
{
    if (s.empty()) return def;
    return atoi(s.c_str());
}

static float ToFloat(const std::string& s, float def = 0.0f)
{
    if (s.empty()) return def;
    return (float)atof(s.c_str());
}

// Split a JSON array into individual element strings
static std::vector<std::string> SplitArray(const std::string& arr)
{
    std::vector<std::string> results;
    if (arr.empty() || arr[0] != '[') return results;

    size_t pos = 1;
    while (pos < arr.length())
    {
        // Skip whitespace
        while (pos < arr.length() && (arr[pos] == ' ' || arr[pos] == '\t' || arr[pos] == '\n' || arr[pos] == '\r' || arr[pos] == ','))
            pos++;

        if (pos >= arr.length() || arr[pos] == ']') break;

        if (arr[pos] == '{' || arr[pos] == '[')
        {
            size_t end = FindObjectEnd(arr, pos);
            if (end == std::string::npos) break;
            results.push_back(arr.substr(pos, end - pos + 1));
            pos = end + 1;
        }
        else
        {
            // Primitive value
            size_t end = arr.find_first_of(",]", pos);
            if (end == std::string::npos) break;
            results.push_back(arr.substr(pos, end - pos));
            pos = end;
        }
    }
    return results;
}

static Coins ParseCoins(const std::string& json)
{
    Coins c{};
    c.gold = ToInt(FindValue(json, "gold"));
    c.silver = ToInt(FindValue(json, "silver"));
    c.copper = ToInt(FindValue(json, "copper"));
    c.raw = ToInt(FindValue(json, "raw"));
    return c;
}

static Rarity ParseRarity(const std::string& str)
{
    if (str == "Fine") return Rarity::Fine;
    if (str == "Masterwork") return Rarity::Masterwork;
    if (str == "Rare") return Rarity::Rare;
    if (str == "Exotic") return Rarity::Exotic;
    if (str == "Ascended") return Rarity::Ascended;
    if (str == "Legendary") return Rarity::Legendary;
    if (str == "Junk") return Rarity::Junk;
    return Rarity::Basic;
}

// ============================================================================
// Endpoint Parsers
// ============================================================================

static std::vector<FlipItem> ParseFlips(const std::string& json)
{
    std::vector<FlipItem> items;
    std::string arr = FindValue(json, "items");
    if (arr.empty()) return items;

    for (auto& obj : SplitArray(arr))
    {
        FlipItem item{};
        item.id = ToInt(FindValue(obj, "id"));
        item.name = FindValue(obj, "name");
        item.icon = FindValue(obj, "icon");
        item.rarity = ParseRarity(FindValue(obj, "rarity"));
        item.roi = ToFloat(FindValue(obj, "roi"));
        item.supply = ToInt(FindValue(obj, "supply"));
        item.demand = ToInt(FindValue(obj, "demand"));
        item.sold = ToInt(FindValue(obj, "sold"));
        item.bought = ToInt(FindValue(obj, "bought"));

        std::string buyJson = FindValue(obj, "buy_price");
        if (!buyJson.empty()) item.buyPrice = ParseCoins(buyJson);

        std::string sellJson = FindValue(obj, "sell_price");
        if (!sellJson.empty()) item.sellPrice = ParseCoins(sellJson);

        std::string profitJson = FindValue(obj, "profit");
        if (!profitJson.empty()) item.profit = ParseCoins(profitJson);

        if (item.id > 0) items.push_back(item);
    }
    return items;
}

static std::vector<Transaction> ParseTransactions(const std::string& json, const std::string& key)
{
    std::vector<Transaction> txns;
    std::string arr = FindValue(json, key);
    if (arr.empty()) return txns;

    for (auto& obj : SplitArray(arr))
    {
        Transaction tx{};
        tx.itemId = ToInt(FindValue(obj, "item_id"));
        tx.name = FindValue(obj, "name");
        tx.icon = FindValue(obj, "icon");
        tx.rarity = ParseRarity(FindValue(obj, "rarity"));
        tx.quantity = ToInt(FindValue(obj, "quantity"));
        tx.created = FindValue(obj, "created");

        std::string priceJson = FindValue(obj, "price");
        if (!priceJson.empty()) tx.price = ParseCoins(priceJson);

        if (tx.itemId > 0) txns.push_back(tx);
    }
    return txns;
}

static std::vector<Item> ParseItems(const std::string& json, const std::string& key)
{
    std::vector<Item> items;
    std::string arr = FindValue(json, key);
    if (arr.empty()) return items;

    for (auto& obj : SplitArray(arr))
    {
        Item item{};
        item.id = ToInt(FindValue(obj, "id"));
        item.name = FindValue(obj, "name");
        item.icon = FindValue(obj, "icon");
        item.rarity = ParseRarity(FindValue(obj, "rarity"));
        item.count = ToInt(FindValue(obj, "count"), 1);

        std::string priceJson = FindValue(obj, "price");
        if (!priceJson.empty()) item.price = ParseCoins(priceJson);

        // Some endpoints use "sell_price" instead of "price"
        if (item.price.raw == 0)
        {
            std::string sellJson = FindValue(obj, "sell_price");
            if (!sellJson.empty()) item.price = ParseCoins(sellJson);
        }

        std::string totalJson = FindValue(obj, "total_value");
        if (!totalJson.empty())
            item.totalValue = ParseCoins(totalJson);
        else
        {
            // Calculate total = price * count
            item.totalValue.raw = item.price.raw * item.count;
            item.totalValue.gold = item.totalValue.raw / 10000;
            item.totalValue.silver = (item.totalValue.raw % 10000) / 100;
            item.totalValue.copper = item.totalValue.raw % 100;
        }

        if (item.id > 0) items.push_back(item);
    }
    return items;
}

static std::vector<Currency> ParseCurrencies(const std::string& json)
{
    std::vector<Currency> currencies;
    std::string arr = FindValue(json, "currencies");
    if (arr.empty()) return currencies;

    for (auto& obj : SplitArray(arr))
    {
        Currency c{};
        c.id = ToInt(FindValue(obj, "id"));
        c.name = FindValue(obj, "name");
        c.icon = FindValue(obj, "icon");
        c.value = ToInt(FindValue(obj, "value"));

        if (c.id > 0) currencies.push_back(c);
    }
    return currencies;
}

static TradingPostData ParseTradingPost(const std::string& json)
{
    TradingPostData tp{};

    // Wallet
    std::string walletJson = FindValue(json, "wallet");
    if (!walletJson.empty())
        tp.wallet.gold = ParseCoins(walletJson);

    // Currencies
    tp.wallet.currencies = ParseCurrencies(json);

    // Delivery
    std::string deliveryJson = FindValue(json, "delivery");
    if (!deliveryJson.empty())
    {
        std::string coinsJson = FindValue(deliveryJson, "coins");
        if (!coinsJson.empty())
            tp.delivery.coins = ParseCoins(coinsJson);
        tp.delivery.items = ParseItems(deliveryJson, "items");
    }

    // Buy/sell orders
    tp.sells = ParseTransactions(json, "sells");
    tp.buys = ParseTransactions(json, "buys");

    // Summary totals
    std::string summaryJson = FindValue(json, "summary");
    if (!summaryJson.empty())
    {
        std::string sellValJson = FindValue(summaryJson, "sell_value");
        if (!sellValJson.empty()) tp.sellValue = ParseCoins(sellValJson);

        std::string buyValJson = FindValue(summaryJson, "buy_value");
        if (!buyValJson.empty()) tp.buyValue = ParseCoins(buyValJson);
    }

    return tp;
}

// ============================================================================
// Icon Loading
// ============================================================================

void LoadItemIcon(const std::string& iconUrl, int itemId)
{
    if (!APIDefs || iconUrl.empty()) return;

    // Create a unique identifier for each icon
    char idBuf[64];
    snprintf(idBuf, sizeof(idBuf), "MT_ICON_%d", itemId);

    // Check if already loaded
    Texture_t* existing = APIDefs->Textures_Get(idBuf);
    if (existing) return;

    // Parse URL into remote + endpoint
    // Icons are like: https://render.guildwars2.com/file/HASH/ITEMID.png
    // Nexus expects: remote = "https://render.guildwars2.com", endpoint = "/file/HASH/ITEMID.png"
    size_t protoEnd = iconUrl.find("://");
    if (protoEnd == std::string::npos) return;
    size_t pathStart = iconUrl.find('/', protoEnd + 3);
    if (pathStart == std::string::npos) return;

    std::string remote = iconUrl.substr(0, pathStart);
    std::string endpoint = iconUrl.substr(pathStart);

    APIDefs->Textures_GetOrCreateFromURL(idBuf, remote.c_str(), endpoint.c_str());
}

// ============================================================================
// Fetch Loop
// ============================================================================

static void FetchLoop()
{
    int fetchCount = 0;

    while (g_Running)
    {
        // Re-read config each cycle
        {
            std::ifstream f(g_ConfigPath);
            std::string line;
            while (std::getline(f, line))
            {
                if (line.find("session=") == 0)
                    g_SessionCookie = line.substr(8);
                if (line.find("portal_url=") == 0)
                    g_PortalUrl = line.substr(11);
                if (line.find("refresh=") == 0)
                    g_RefreshInterval = atoi(line.substr(8).c_str());
            }
        }

        if (g_SessionCookie.empty())
        {
            if (APIDefs)
                APIDefs->Log(LOGL_WARNING, "MysticTrading", "No session cookie configured. Set it in Nexus addon options.");
            for (int i = 0; i < 100 && g_Running; i++)
                Sleep(100);
            continue;
        }

        std::string baseUrl = g_PortalUrl + "/api/games/gw2";

        // Always fetch trading post + flips
        std::string tpJson = HttpGet(baseUrl + "/trading-post", g_SessionCookie);
        std::string flipsJson = HttpGet(baseUrl + "/flips", g_SessionCookie);

        // Fetch bank + materials less frequently (every 5th cycle)
        std::string bankJson, matsJson;
        if (fetchCount % 5 == 0)
        {
            bankJson = HttpGet(baseUrl + "/bank", g_SessionCookie);
            matsJson = HttpGet(baseUrl + "/materials", g_SessionCookie);
        }

        // Parse and update
        {
            std::lock_guard<std::mutex> lock(g_DataMutex);

            if (!tpJson.empty())
                g_Data.tradingPost = ParseTradingPost(tpJson);

            if (!flipsJson.empty())
                g_Data.flips = ParseFlips(flipsJson);

            if (!bankJson.empty())
            {
                g_Data.bank = ParseItems(bankJson, "items");
                std::string totalJson = FindValue(bankJson, "total");
                if (!totalJson.empty()) g_Data.bankTotal = ParseCoins(totalJson);
            }

            if (!matsJson.empty())
            {
                g_Data.materials = ParseItems(matsJson, "items");
                std::string totalJson = FindValue(matsJson, "total");
                if (!totalJson.empty()) g_Data.matsTotal = ParseCoins(totalJson);
            }

            g_Data.loaded = true;

            // Queue icon loads for visible items
            for (auto& flip : g_Data.flips)
                LoadItemIcon(flip.icon, flip.id);
            for (auto& item : g_Data.tradingPost.delivery.items)
                LoadItemIcon(item.icon, item.id);
            for (auto& tx : g_Data.tradingPost.sells)
                LoadItemIcon(tx.icon, tx.itemId);
            for (auto& tx : g_Data.tradingPost.buys)
                LoadItemIcon(tx.icon, tx.itemId);
            for (auto& item : g_Data.bank)
                LoadItemIcon(item.icon, item.id);
            for (auto& item : g_Data.materials)
                LoadItemIcon(item.icon, item.id);
        }

        if (APIDefs)
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "Data refreshed: %zu flips, %zu sells, %zu buys, %zu bank, %zu mats",
                g_Data.flips.size(), g_Data.tradingPost.sells.size(),
                g_Data.tradingPost.buys.size(), g_Data.bank.size(), g_Data.materials.size());
            APIDefs->Log(LOGL_DEBUG, "MysticTrading", msg);
        }

        fetchCount++;

        // Sleep for refresh interval (interruptible)
        for (int i = 0; i < g_RefreshInterval * 10 && g_Running; i++)
            Sleep(100);
    }
}

void StartDataFetch()
{
    g_Running = true;
    g_FetchThread = std::thread(FetchLoop);
}

void StopDataFetch()
{
    g_Running = false;
    if (g_FetchThread.joinable())
        g_FetchThread.join();
}

// ============================================================================
// Clipboard
// ============================================================================

void CopyToClipboard(const std::string& text)
{
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (hMem)
    {
        char* pMem = (char*)GlobalLock(hMem);
        if (pMem)
        {
            memcpy(pMem, text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
    }
    CloseClipboard();
}

// ============================================================================
// Config
// ============================================================================

void LoadConfig()
{
    std::ifstream f(g_ConfigPath);
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line))
    {
        if (line.find("portal_url=") == 0)
            g_PortalUrl = line.substr(11);
        if (line.find("refresh=") == 0)
            g_RefreshInterval = atoi(line.substr(8).c_str());
        if (line.find("session=") == 0)
            g_SessionCookie = line.substr(8);
    }
}

void SaveConfig()
{
    // Read existing session first
    std::string existingSession;
    {
        std::ifstream f(g_ConfigPath);
        std::string line;
        while (std::getline(f, line))
        {
            if (line.find("session=") == 0)
                existingSession = line.substr(8);
        }
    }

    std::ofstream f(g_ConfigPath);
    if (!f.is_open()) return;

    f << "portal_url=" << g_PortalUrl << "\n";
    f << "refresh=" << g_RefreshInterval << "\n";

    // Preserve session cookie
    if (!existingSession.empty())
        f << "session=" << existingSession << "\n";
    else if (!g_SessionCookie.empty())
        f << "session=" << g_SessionCookie << "\n";
}
