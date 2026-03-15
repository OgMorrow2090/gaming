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
#include <wininet.h>

#pragma comment(lib, "wininet.lib")

static std::atomic<bool> g_Running{false};
static std::thread g_FetchThread;
static std::string g_ConfigPath;

void SetApiConfig(const char* addonPath)
{
    g_ConfigPath = std::string(addonPath) + "\\mystic-trading.cfg";

    // Ensure directory exists
    CreateDirectoryA(addonPath, nullptr);
}

// Simple HTTP GET using WinInet
static std::string HttpGet(const std::string& url, const std::string& sessionCookie)
{
    std::string result;

    HINTERNET hInternet = InternetOpenA("MysticTrading/0.1", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) return result;

    // Set cookie header
    std::string headers = "Cookie: session=" + sessionCookie + "\r\n";

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), headers.c_str(), (DWORD)headers.length(),
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);

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

// Minimal JSON value parser (no dependency needed for simple API responses)
static int ParseInt(const std::string& json, const std::string& key, int defaultVal = 0)
{
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;
    pos += search.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    return atoi(json.c_str() + pos);
}

static float ParseFloat(const std::string& json, const std::string& key, float defaultVal = 0.0f)
{
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;
    pos += search.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    return (float)atof(json.c_str() + pos);
}

static std::string ParseString(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

static Coins ParseCoins(const std::string& json)
{
    Coins c{};
    c.gold = ParseInt(json, "gold");
    c.silver = ParseInt(json, "silver");
    c.copper = ParseInt(json, "copper");
    c.raw = ParseInt(json, "raw");
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

// Parse a JSON array of flip items
static std::vector<FlipItem> ParseFlips(const std::string& json)
{
    std::vector<FlipItem> items;

    // Find "items" array
    size_t arrStart = json.find("\"items\":[");
    if (arrStart == std::string::npos) return items;
    arrStart = json.find("[", arrStart);

    // Parse each object in the array
    size_t pos = arrStart;
    while (true)
    {
        size_t objStart = json.find("{", pos);
        if (objStart == std::string::npos) break;
        size_t objEnd = json.find("}", objStart);
        if (objEnd == std::string::npos) break;

        // Check for nested objects (coins) - find the real end
        size_t searchPos = objEnd + 1;
        int depth = 0;
        for (size_t i = objStart; i < json.length(); i++)
        {
            if (json[i] == '{') depth++;
            if (json[i] == '}') depth--;
            if (depth == 0) { objEnd = i; break; }
        }

        std::string obj = json.substr(objStart, objEnd - objStart + 1);

        FlipItem item{};
        item.id = ParseInt(obj, "id");
        item.name = ParseString(obj, "name");
        item.icon = ParseString(obj, "icon");
        item.rarity = ParseRarity(ParseString(obj, "rarity"));
        item.roi = ParseFloat(obj, "roi");
        item.supply = ParseInt(obj, "supply");
        item.demand = ParseInt(obj, "demand");
        item.sold = ParseInt(obj, "sold");
        item.bought = ParseInt(obj, "bought");

        // Parse nested coin objects
        size_t buyPos = obj.find("\"buy_price\":{");
        if (buyPos != std::string::npos)
        {
            size_t buyEnd = obj.find("}", buyPos + 13);
            std::string buyStr = obj.substr(buyPos + 12, buyEnd - buyPos - 11);
            item.buyPrice = ParseCoins(buyStr);
        }

        size_t sellPos = obj.find("\"sell_price\":{");
        if (sellPos != std::string::npos)
        {
            size_t sellEnd = obj.find("}", sellPos + 14);
            std::string sellStr = obj.substr(sellPos + 13, sellEnd - sellPos - 12);
            item.sellPrice = ParseCoins(sellStr);
        }

        size_t profitPos = obj.find("\"profit\":{");
        if (profitPos != std::string::npos)
        {
            size_t profitEnd = obj.find("}", profitPos + 10);
            std::string profitStr = obj.substr(profitPos + 9, profitEnd - profitPos - 8);
            item.profit = ParseCoins(profitStr);
        }

        if (item.id > 0)
            items.push_back(item);

        pos = objEnd + 1;
        if (pos >= json.length() || json[pos] == ']') break;
    }

    return items;
}

static void FetchLoop()
{
    while (g_Running)
    {
        // Read session cookie from config
        std::string sessionCookie;
        {
            std::ifstream f(g_ConfigPath);
            std::string line;
            while (std::getline(f, line))
            {
                if (line.find("session=") == 0)
                    sessionCookie = line.substr(8);
                if (line.find("portal_url=") == 0)
                    g_PortalUrl = line.substr(11);
                if (line.find("refresh=") == 0)
                    g_RefreshInterval = atoi(line.substr(8).c_str());
            }
        }

        if (sessionCookie.empty())
        {
            if (APIDefs)
                APIDefs->Log(LOGL_WARNING, "MysticTrading", "No session cookie configured. Set it in addon options.");
            // Wait before retrying
            for (int i = 0; i < 100 && g_Running; i++)
                Sleep(100);
            continue;
        }

        std::string baseUrl = g_PortalUrl + "/api/games/gw2";

        // Fetch trading post data
        std::string tpJson = HttpGet(baseUrl + "/trading-post", sessionCookie);

        // Fetch flips
        std::string flipsJson = HttpGet(baseUrl + "/flips", sessionCookie);

        // Fetch bank
        std::string bankJson = HttpGet(baseUrl + "/bank", sessionCookie);

        // Fetch materials
        std::string matsJson = HttpGet(baseUrl + "/materials", sessionCookie);

        // Parse and update global data
        {
            std::lock_guard<std::mutex> lock(g_DataMutex);

            if (!flipsJson.empty())
                g_Data.flips = ParseFlips(flipsJson);

            // TODO: parse trading-post, bank, materials JSON
            // For now flips is the primary use case

            g_Data.loaded = true;
        }

        if (APIDefs)
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Data refreshed: %zu flips loaded.", g_Data.flips.size());
            APIDefs->Log(LOGL_DEBUG, "MysticTrading", msg);
        }

        // Sleep for refresh interval
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
    }
}

void SaveConfig()
{
    std::ofstream f(g_ConfigPath);
    if (!f.is_open()) return;

    f << "portal_url=" << g_PortalUrl << "\n";
    f << "refresh=" << g_RefreshInterval << "\n";

    // Don't overwrite session cookie if it exists - read it first
    std::ifstream existing(g_ConfigPath);
    std::string line;
    while (std::getline(existing, line))
    {
        if (line.find("session=") == 0)
        {
            f << line << "\n";
            break;
        }
    }
}
