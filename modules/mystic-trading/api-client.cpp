/**
 * api-client.cpp
 *
 * Standalone GW2 API client and GW2BLTC scraper for Mystic Trading.
 * Talks directly to api.guildwars2.com and gw2bltc.com — no portal dependency.
 *
 * Replicates the same logic as addams.family/web/backend/games.py
 */

#include "shared.h"
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>
#include <map>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")

static std::atomic<bool> g_Running{false};
static std::thread g_FetchThread;
static std::string g_ConfigPath;

static const char* GW2_API = "https://api.guildwars2.com/v2";
static const char* BLTC_SEARCH = "https://www.gw2bltc.com/en/tp/search";

void SetApiConfig(const char* addonPath)
{
    g_ConfigPath = std::string(addonPath) + "\\mystic-trading.cfg";
    CreateDirectoryA(addonPath, nullptr);
}

// ============================================================================
// HTTP
// ============================================================================

static std::string HttpGet(const std::string& url, const std::string& bearerToken = "")
{
    std::string result;

    HINTERNET hInternet = InternetOpenA("MysticTrading/0.1", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) return result;

    std::string headers;
    if (!bearerToken.empty())
        headers = "Authorization: Bearer " + bearerToken + "\r\n";

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (url.find("https://") == 0)
        flags |= INTERNET_FLAG_SECURE;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(),
        headers.empty() ? nullptr : headers.c_str(),
        headers.empty() ? 0 : (DWORD)headers.length(),
        flags, 0);

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

static size_t FindObjectEnd(const std::string& json, size_t start)
{
    int depth = 0;
    for (size_t i = start; i < json.length(); i++)
    {
        if (json[i] == '"')
        {
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

static std::string FindValue(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
        pos++;
    if (pos >= json.length()) return "";
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
    if (json[pos] == '{' || json[pos] == '[')
    {
        size_t end = FindObjectEnd(json, pos);
        if (end != std::string::npos)
            return json.substr(pos, end - pos + 1);
        return "";
    }
    size_t end = json.find_first_of(",}] \t\n\r", pos);
    if (end == std::string::npos) end = json.length();
    return json.substr(pos, end - pos);
}

static int ToInt(const std::string& s, int def = 0) { return s.empty() ? def : atoi(s.c_str()); }
static float ToFloat(const std::string& s, float def = 0.0f) { return s.empty() ? def : (float)atof(s.c_str()); }

static std::vector<std::string> SplitArray(const std::string& arr)
{
    std::vector<std::string> results;
    if (arr.empty() || arr[0] != '[') return results;
    size_t pos = 1;
    while (pos < arr.length())
    {
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
            size_t end = arr.find_first_of(",]", pos);
            if (end == std::string::npos) break;
            results.push_back(arr.substr(pos, end - pos));
            pos = end;
        }
    }
    return results;
}

static Coins CopperToCoins(int copper)
{
    Coins c{};
    int abs_c = copper < 0 ? -copper : copper;
    c.gold = (abs_c / 10000) * (copper < 0 ? -1 : 1);
    c.silver = (abs_c % 10000) / 100;
    c.copper = abs_c % 100;
    c.raw = copper;
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
// GW2 API Helpers
// ============================================================================

struct ItemInfo {
    std::string name;
    std::string icon;
    std::string rarity;
    std::string type;
    std::string description;
    std::vector<ItemStat> stats;
};

struct PriceInfo {
    int buy = 0;
    int sell = 0;
    int buy_qty = 0;
    int sell_qty = 0;
};

// Fetch item details in batches of 200
static std::map<int, ItemInfo> FetchItems(const std::set<int>& ids, const std::string& apiKey)
{
    std::map<int, ItemInfo> items;
    std::vector<int> idVec(ids.begin(), ids.end());

    for (size_t i = 0; i < idVec.size(); i += 200)
    {
        std::string idsStr;
        for (size_t j = i; j < idVec.size() && j < i + 200; j++)
        {
            if (!idsStr.empty()) idsStr += ",";
            idsStr += std::to_string(idVec[j]);
        }

        std::string url = std::string(GW2_API) + "/items?ids=" + idsStr;
        std::string json = HttpGet(url);
        if (json.empty() || json[0] != '[') continue;

        for (auto& obj : SplitArray(json))
        {
            int id = ToInt(FindValue(obj, "id"));
            if (id <= 0) continue;
            ItemInfo info;
            info.name = FindValue(obj, "name");
            info.icon = FindValue(obj, "icon");
            info.rarity = FindValue(obj, "rarity");
            info.type = FindValue(obj, "type");

            // Extract description (strip <c=@flavor> tags)
            std::string desc = FindValue(obj, "description");
            while (desc.find("<c=") != std::string::npos)
            {
                size_t s = desc.find("<c=");
                size_t e = desc.find(">", s);
                if (e != std::string::npos) desc.erase(s, e - s + 1);
                else break;
            }
            while (desc.find("</c>") != std::string::npos)
                desc.erase(desc.find("</c>"), 4);
            info.description = desc;

            // Extract stat attributes from details.infix_upgrade.attributes
            std::string details = FindValue(obj, "details");
            if (!details.empty())
            {
                std::string infix = FindValue(details, "infix_upgrade");
                if (!infix.empty())
                {
                    std::string attrs = FindValue(infix, "attributes");
                    if (!attrs.empty() && attrs[0] == '[')
                    {
                        for (auto& attrObj : SplitArray(attrs))
                        {
                            ItemStat st;
                            st.attribute = FindValue(attrObj, "attribute");
                            st.modifier = ToInt(FindValue(attrObj, "modifier"));
                            if (!st.attribute.empty())
                                info.stats.push_back(st);
                        }
                    }
                }
            }

            items[id] = info;
        }
    }
    return items;
}

// Fetch TP prices in batches of 200
static std::map<int, PriceInfo> FetchPrices(const std::set<int>& ids)
{
    std::map<int, PriceInfo> prices;
    std::vector<int> idVec(ids.begin(), ids.end());

    for (size_t i = 0; i < idVec.size(); i += 200)
    {
        std::string idsStr;
        for (size_t j = i; j < idVec.size() && j < i + 200; j++)
        {
            if (!idsStr.empty()) idsStr += ",";
            idsStr += std::to_string(idVec[j]);
        }

        std::string url = std::string(GW2_API) + "/commerce/prices?ids=" + idsStr;
        std::string json = HttpGet(url);
        if (json.empty() || json[0] != '[') continue;

        for (auto& obj : SplitArray(json))
        {
            int id = ToInt(FindValue(obj, "id"));
            if (id <= 0) continue;
            PriceInfo p;
            std::string buysJson = FindValue(obj, "buys");
            std::string sellsJson = FindValue(obj, "sells");
            p.buy = ToInt(FindValue(buysJson, "unit_price"));
            p.sell = ToInt(FindValue(sellsJson, "unit_price"));
            p.buy_qty = ToInt(FindValue(buysJson, "quantity"));
            p.sell_qty = ToInt(FindValue(sellsJson, "quantity"));
            prices[id] = p;
        }
    }
    return prices;
}

// ============================================================================
// Trading Post Refresh (direct GW2 API)
// ============================================================================

static void RefreshTradingPost(const std::string& apiKey)
{
    // Fetch all endpoints
    std::string deliveryJson = HttpGet(std::string(GW2_API) + "/commerce/delivery", apiKey);
    std::string sellsJson = HttpGet(std::string(GW2_API) + "/commerce/transactions/current/sells", apiKey);
    std::string buysJson = HttpGet(std::string(GW2_API) + "/commerce/transactions/current/buys", apiKey);
    std::string walletJson = HttpGet(std::string(GW2_API) + "/account/wallet", apiKey);

    // Parse wallet
    int walletCopper = 0;
    std::vector<Currency> currencies;
    if (!walletJson.empty() && walletJson[0] == '[')
    {
        for (auto& obj : SplitArray(walletJson))
        {
            int id = ToInt(FindValue(obj, "id"));
            int value = ToInt(FindValue(obj, "value"));
            if (id == 1)
                walletCopper = value;
            else if (value > 0)
            {
                Currency c;
                c.id = id;
                c.value = value;
                currencies.push_back(c);
            }
        }
    }

    // Fetch currency details (name + icon) from /v2/currencies
    if (!currencies.empty())
    {
        std::string curIds;
        for (auto& c : currencies)
        {
            if (!curIds.empty()) curIds += ",";
            curIds += std::to_string(c.id);
        }
        std::string curJson = HttpGet(std::string(GW2_API) + "/currencies?ids=" + curIds);
        if (!curJson.empty() && curJson[0] == '[')
        {
            for (auto& obj : SplitArray(curJson))
            {
                int id = ToInt(FindValue(obj, "id"));
                std::string name = FindValue(obj, "name");
                std::string icon = FindValue(obj, "icon");
                int order = ToInt(FindValue(obj, "order"), 999);
                for (auto& c : currencies)
                {
                    if (c.id == id)
                    {
                        c.name = name;
                        c.icon = icon;
                        c.order = order;
                        break;
                    }
                }
            }
        }

        // Sort to match portal: pinned currencies first, then by GW2 API order
        // PINNED_CURRENCIES from addams.family portal Games.vue
        std::sort(currencies.begin(), currencies.end(),
            [](const Currency& a, const Currency& b) {
                // Hardcoded pin order: Karma, Spirit Shard, Volatile Magic,
                // Fine/Master/Rare Rift Essence, Magnetite Shard, Legendary Insight
                const int pinIds[] = { 2, 23, 45, 78, 80, 79, 28, 70 };
                int ai = 999, bi = 999;
                for (int i = 0; i < 8; i++) {
                    if (pinIds[i] == a.id) ai = i;
                    if (pinIds[i] == b.id) bi = i;
                }
                if (ai != bi) return ai < bi;
                return a.order < b.order;
            });
    }

    // Parse delivery
    int deliveryCopper = 0;
    std::vector<std::pair<int, int>> deliveryRaw; // id, count
    if (!deliveryJson.empty() && deliveryJson[0] == '{')
    {
        deliveryCopper = ToInt(FindValue(deliveryJson, "coins"));
        std::string itemsArr = FindValue(deliveryJson, "items");
        for (auto& obj : SplitArray(itemsArr))
        {
            int id = ToInt(FindValue(obj, "id"));
            int count = ToInt(FindValue(obj, "count"));
            if (id > 0) deliveryRaw.push_back({id, count});
        }
    }

    // Parse sells
    struct TxRaw { int itemId; int price; int quantity; std::string created; };
    std::vector<TxRaw> sellsRaw, buysRaw;

    auto parseTxArray = [](const std::string& json) -> std::vector<TxRaw> {
        std::vector<TxRaw> txns;
        if (json.empty() || json[0] != '[') return txns;
        for (auto& obj : SplitArray(json))
        {
            TxRaw tx;
            tx.itemId = ToInt(FindValue(obj, "item_id"));
            tx.price = ToInt(FindValue(obj, "price"));
            tx.quantity = ToInt(FindValue(obj, "quantity"), 1);
            tx.created = FindValue(obj, "created");
            if (tx.itemId > 0) txns.push_back(tx);
        }
        return txns;
    };

    sellsRaw = parseTxArray(sellsJson);
    buysRaw = parseTxArray(buysJson);

    // Collect all item IDs
    std::set<int> allIds;
    for (auto& d : deliveryRaw) allIds.insert(d.first);
    for (auto& tx : sellsRaw) allIds.insert(tx.itemId);
    for (auto& tx : buysRaw) allIds.insert(tx.itemId);

    auto itemDetails = FetchItems(allIds, apiKey);

    // Build trading post data
    TradingPostData tp{};
    tp.wallet.gold = CopperToCoins(walletCopper);
    tp.wallet.currencies = currencies;

    // Delivery
    tp.delivery.coins = CopperToCoins(deliveryCopper);
    for (auto& d : deliveryRaw)
    {
        Item item{};
        item.id = d.first;
        item.count = d.second;
        auto it = itemDetails.find(d.first);
        if (it != itemDetails.end())
        {
            item.name = it->second.name;
            item.icon = it->second.icon;
            item.rarity = ParseRarity(it->second.rarity);
            item.description = it->second.description;
            item.stats = it->second.stats;
        }
        tp.delivery.items.push_back(item);
    }

    // Sells
    int sellTotal = 0;
    for (auto& tx : sellsRaw)
    {
        Transaction t{};
        t.itemId = tx.itemId;
        t.price = CopperToCoins(tx.price);
        t.quantity = tx.quantity;
        t.created = tx.created;
        sellTotal += tx.price * tx.quantity;
        auto it = itemDetails.find(tx.itemId);
        if (it != itemDetails.end())
        {
            t.name = it->second.name;
            t.icon = it->second.icon;
            t.rarity = ParseRarity(it->second.rarity);
        }
        tp.sells.push_back(t);
    }
    tp.sellValue = CopperToCoins(sellTotal);

    // Buys
    int buyTotal = 0;
    for (auto& tx : buysRaw)
    {
        Transaction t{};
        t.itemId = tx.itemId;
        t.price = CopperToCoins(tx.price);
        t.quantity = tx.quantity;
        t.created = tx.created;
        buyTotal += tx.price * tx.quantity;
        auto it = itemDetails.find(tx.itemId);
        if (it != itemDetails.end())
        {
            t.name = it->second.name;
            t.icon = it->second.icon;
            t.rarity = ParseRarity(it->second.rarity);
        }
        tp.buys.push_back(t);
    }
    tp.buyValue = CopperToCoins(buyTotal);

    // Update global
    {
        std::lock_guard<std::mutex> lock(g_DataMutex);
        g_Data.tradingPost = tp;
    }
}

// ============================================================================
// Bank / Materials Refresh (direct GW2 API)
// ============================================================================

static void RefreshBankOrMats(const std::string& apiKey, const std::string& endpoint, bool isBank)
{
    std::string json = HttpGet(std::string(GW2_API) + endpoint, apiKey);
    if (json.empty() || json[0] != '[') return;

    // Parse slots
    std::set<int> ids;
    struct Slot { int id; int count; };
    std::vector<Slot> slots;

    for (auto& obj : SplitArray(json))
    {
        if (obj == "null") continue;
        int id = ToInt(FindValue(obj, "id"));
        int count = ToInt(FindValue(obj, "count"), 1);
        if (id <= 0 || (count <= 0 && !isBank)) continue;
        ids.insert(id);
        slots.push_back({id, count});
    }

    auto itemDetails = FetchItems(ids, apiKey);
    auto prices = FetchPrices(ids);

    std::vector<Item> items;
    int totalRaw = 0;
    for (auto& s : slots)
    {
        Item item{};
        item.id = s.id;
        item.count = s.count;
        auto it = itemDetails.find(s.id);
        if (it != itemDetails.end())
        {
            item.name = it->second.name;
            item.icon = it->second.icon;
            item.rarity = ParseRarity(it->second.rarity);
            item.description = it->second.description;
            item.stats = it->second.stats;
        }
        auto pit = prices.find(s.id);
        int sell = pit != prices.end() ? pit->second.sell : 0;
        int buy = pit != prices.end() ? pit->second.buy : 0;
        item.sellPrice = CopperToCoins(sell);
        item.buyPrice = CopperToCoins(buy);
        item.supply = pit != prices.end() ? pit->second.sell_qty : 0;
        item.demand = pit != prices.end() ? pit->second.buy_qty : 0;
        int total = sell * s.count;
        item.totalValue = CopperToCoins(total);
        totalRaw += total;
        items.push_back(item);
    }

    // Sort by total value descending
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return a.totalValue.raw > b.totalValue.raw;
    });

    {
        std::lock_guard<std::mutex> lock(g_DataMutex);
        if (isBank)
        {
            g_Data.bank = items;
            g_Data.bankTotal = CopperToCoins(totalRaw);
        }
        else
        {
            g_Data.materials = items;
            g_Data.matsTotal = CopperToCoins(totalRaw);
        }
    }
}

// ============================================================================
// BLTC Flip Scraping
// ============================================================================

static int ParseCoinSpan(const std::string& html, const std::string& cssClass)
{
    size_t pos = html.find(cssClass);
    if (pos == std::string::npos) return 0;
    pos = html.find(">", pos);
    if (pos == std::string::npos) return 0;
    pos++;
    size_t end = html.find("<", pos);
    if (end == std::string::npos) return 0;
    return atoi(html.substr(pos, end - pos).c_str());
}

static int ParseCoinTd(const std::string& td)
{
    int total = 0;
    total += ParseCoinSpan(td, "cur-t1c") * 10000; // gold
    total += ParseCoinSpan(td, "cur-t1b") * 100;    // silver
    total += ParseCoinSpan(td, "cur-t1a");           // copper
    return total;
}

static std::string HtmlUnescape(const std::string& s)
{
    std::string result = s;
    size_t pos;
    // Named entities
    while ((pos = result.find("&amp;")) != std::string::npos) result.replace(pos, 5, "&");
    while ((pos = result.find("&lt;")) != std::string::npos) result.replace(pos, 4, "<");
    while ((pos = result.find("&gt;")) != std::string::npos) result.replace(pos, 4, ">");
    while ((pos = result.find("&quot;")) != std::string::npos) result.replace(pos, 6, "\"");
    while ((pos = result.find("&apos;")) != std::string::npos) result.replace(pos, 6, "'");
    // Numeric entities (&#039; &#39; &#x27; etc.)
    pos = 0;
    while ((pos = result.find("&#", pos)) != std::string::npos)
    {
        size_t semi = result.find(';', pos);
        if (semi == std::string::npos || semi - pos > 8) { pos++; continue; }
        std::string numStr = result.substr(pos + 2, semi - pos - 2);
        int codepoint = 0;
        if (!numStr.empty() && (numStr[0] == 'x' || numStr[0] == 'X'))
            codepoint = (int)strtol(numStr.c_str() + 1, nullptr, 16);
        else
            codepoint = atoi(numStr.c_str());
        if (codepoint > 0 && codepoint < 128)
        {
            char ch = (char)codepoint;
            result.replace(pos, semi - pos + 1, 1, ch);
        }
        else
            pos = semi + 1;
    }
    return result;
}

// Helper: extract string between two markers
static std::string ExtractBetween(const std::string& s, size_t start, const std::string& before, const std::string& after)
{
    size_t a = s.find(before, start);
    if (a == std::string::npos) return "";
    a += before.length();
    size_t b = s.find(after, a);
    if (b == std::string::npos) return "";
    return s.substr(a, b - a);
}

// Helper: extract int from next <td>...</td> containing only digits
static int ExtractNextNumTd(const std::string& s, size_t& pos)
{
    // Find next <td that contains a bare number
    while (pos < s.length())
    {
        size_t tdStart = s.find("<td", pos);
        if (tdStart == std::string::npos) break;
        size_t gt = s.find(">", tdStart);
        if (gt == std::string::npos) break;
        size_t tdEnd = s.find("</td>", gt);
        if (tdEnd == std::string::npos) break;
        std::string content = s.substr(gt + 1, tdEnd - gt - 1);
        pos = tdEnd + 5;
        // Check if content is just digits (possibly with commas)
        std::string digits;
        for (char c : content)
        {
            if (c >= '0' && c <= '9') digits += c;
        }
        if (!digits.empty() && content.find("<") == std::string::npos)
            return atoi(digits.c_str());
    }
    return 0;
}

static std::vector<FlipItem> ParseBltcPage(const std::string& html)
{
    std::vector<FlipItem> items;

    // Find each row by searching for "icon-item" class
    size_t searchPos = 0;
    while (searchPos < html.length())
    {
        // Find next item icon
        size_t iconPos = html.find("icon-item", searchPos);
        if (iconPos == std::string::npos) break;

        // Extract rarity from class="icon-item rarity-exotic ..."
        std::string rarity = ExtractBetween(html, iconPos, "rarity-", " ");
        if (rarity.empty()) rarity = ExtractBetween(html, iconPos, "rarity-", "\"");
        if (rarity.empty()) { searchPos = iconPos + 10; continue; }

        // Extract icon src
        std::string icon = ExtractBetween(html, iconPos, "src=\"", "\"");

        // Extract data-id
        std::string idStr = ExtractBetween(html, iconPos, "data-id=\"", "\"");
        int itemId = atoi(idStr.c_str());
        if (itemId <= 0) { searchPos = iconPos + 10; continue; }

        // Extract item name from td-name link
        size_t namePos = html.find("td-name", iconPos);
        if (namePos == std::string::npos) { searchPos = iconPos + 10; continue; }
        std::string name = HtmlUnescape(ExtractBetween(html, namePos, ">", "</a>"));
        // The first > is the td, then <a...>, we need the text inside <a>
        size_t aPos = html.find("<a", namePos);
        if (aPos != std::string::npos)
        {
            size_t aGt = html.find(">", aPos);
            if (aGt != std::string::npos)
            {
                size_t aEnd = html.find("</a>", aGt);
                if (aEnd != std::string::npos)
                    name = HtmlUnescape(html.substr(aGt + 1, aEnd - aGt - 1));
            }
        }
        if (name.empty()) { searchPos = iconPos + 10; continue; }

        // Find coin values (cur-t1c = gold, cur-t1b = silver, cur-t1a = copper)
        // There are 4 coin columns after the name: sell, buy, profit, ROI
        std::vector<int> coins;
        size_t coinSearch = namePos;
        for (int col = 0; col < 4 && coinSearch < html.length(); col++)
        {
            // Find next <td that contains coin spans
            size_t tdPos = html.find("<td", coinSearch);
            if (tdPos == std::string::npos) break;
            size_t tdEnd = html.find("</td>", tdPos);
            if (tdEnd == std::string::npos) break;
            std::string td = html.substr(tdPos, tdEnd - tdPos + 5);
            coinSearch = tdEnd + 5;

            if (td.find("cur-t1") != std::string::npos)
                coins.push_back(ParseCoinTd(td));
        }

        if (coins.size() < 3) { searchPos = iconPos + 10; continue; }

        // Parse 6 numeric TDs: supply, demand, sold, offers, bought, bids
        std::vector<int> nums;
        size_t numPos = coinSearch;
        for (int i = 0; i < 6; i++)
            nums.push_back(ExtractNextNumTd(html, numPos));

        int sellC = coins[0], buyC = coins[1], profitC = coins[2];
        float roi = buyC > 0 ? (float)profitC / (float)buyC * 100.0f : 0.0f;

        // Map rarity string
        std::string rarityCapitalized = rarity;
        if (!rarityCapitalized.empty())
            rarityCapitalized[0] = (char)toupper(rarityCapitalized[0]);

        FlipItem flip{};
        flip.id = itemId;
        flip.name = name;
        flip.icon = icon;
        flip.rarity = ParseRarity(rarityCapitalized);
        flip.buyPrice = CopperToCoins(buyC);
        flip.sellPrice = CopperToCoins(sellC);
        flip.profit = CopperToCoins(profitC);
        flip.roi = roi;
        flip.supply = nums.size() > 0 ? nums[0] : 0;
        flip.demand = nums.size() > 1 ? nums[1] : 0;
        flip.sold = nums.size() > 2 ? nums[2] : 0;
        flip.bought = nums.size() > 4 ? nums[4] : 0;

        items.push_back(flip);
        searchPos = numPos > iconPos ? numPos : iconPos + 10;
    }

    return items;
}

static void RefreshFlips()
{
    std::vector<FlipItem> allFlips;

    for (int page = 1; page <= 4; page++)
    {
        char url[512];
        snprintf(url, sizeof(url),
            "%s?profit-pct-min=10&profit-pct-max=100&sold-day-min=20&bought-day-min=20&sort=profit&page=%d",
            BLTC_SEARCH, page);

        std::string html = HttpGet(url);
        if (html.empty()) break;

        auto pageFlips = ParseBltcPage(html);
        if (pageFlips.empty()) break;

        allFlips.insert(allFlips.end(), pageFlips.begin(), pageFlips.end());
    }

    // Fetch item descriptions/stats for flip items
    if (!allFlips.empty())
    {
        std::set<int> flipIds;
        for (auto& f : allFlips) flipIds.insert(f.id);
        auto flipDetails = FetchItems(flipIds, "");
        for (auto& f : allFlips)
        {
            auto it = flipDetails.find(f.id);
            if (it != flipDetails.end())
            {
                f.description = it->second.description;
                f.stats = it->second.stats;
            }
        }

        std::lock_guard<std::mutex> lock(g_DataMutex);
        g_Data.flips = allFlips;
    }

    if (APIDefs)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "Fetched %zu flips from GW2BLTC", allFlips.size());
        APIDefs->Log(LOGL_DEBUG, "MysticTrading", msg);
    }
}

// ============================================================================
// Icon Loading
// ============================================================================

void LoadItemIcon(const std::string& iconUrl, int itemId)
{
    if (!APIDefs || iconUrl.empty()) return;

    char idBuf[64];
    snprintf(idBuf, sizeof(idBuf), "MT_ICON_%d", itemId);

    Texture_t* existing = APIDefs->Textures_Get(idBuf);
    if (existing) return;

    size_t protoEnd = iconUrl.find("://");
    if (protoEnd == std::string::npos) return;
    size_t pathStart = iconUrl.find('/', protoEnd + 3);
    if (pathStart == std::string::npos) return;

    std::string remote = iconUrl.substr(0, pathStart);
    std::string endpoint = iconUrl.substr(pathStart);

    APIDefs->Textures_GetOrCreateFromURL(idBuf, remote.c_str(), endpoint.c_str());
}

static void LoadCurrencyIcon(const std::string& iconUrl, int currencyId)
{
    if (!APIDefs || iconUrl.empty()) return;

    char idBuf[64];
    snprintf(idBuf, sizeof(idBuf), "MT_CUR_%d", currencyId);

    Texture_t* existing = APIDefs->Textures_Get(idBuf);
    if (existing) return;

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
    int cycle = 0;

    while (g_Running)
    {
        // Read API key from config
        std::string apiKey;
        {
            std::ifstream f(g_ConfigPath);
            std::string line;
            while (std::getline(f, line))
            {
                if (line.find("api_key=") == 0)
                    apiKey = line.substr(8);
                if (line.find("refresh=") == 0)
                    g_RefreshInterval = atoi(line.substr(8).c_str());
            }
        }

        if (apiKey.empty())
        {
            if (APIDefs)
                APIDefs->Log(LOGL_WARNING, "MysticTrading", "No GW2 API key configured. Set it in Nexus addon options.");
            for (int i = 0; i < 100 && g_Running; i++)
                Sleep(100);
            continue;
        }

        // Always refresh trading post (30s)
        RefreshTradingPost(apiKey);

        // Refresh flips every 5th cycle (~2.5 min at 30s interval)
        if (cycle % 5 == 0)
            RefreshFlips();

        // Refresh bank + materials every 10th cycle (~5 min)
        if (cycle % 10 == 0)
        {
            RefreshBankOrMats(apiKey, "/account/bank", true);
            RefreshBankOrMats(apiKey, "/account/materials", false);
        }

        // Queue icon loads
        {
            std::lock_guard<std::mutex> lock(g_DataMutex);
            g_Data.loaded = true;

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
            for (auto& c : g_Data.tradingPost.wallet.currencies)
                LoadCurrencyIcon(c.icon, c.id);
        }

        if (APIDefs)
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "Cycle %d: %zu flips, %zu sells, %zu buys, %zu bank, %zu mats",
                cycle, g_Data.flips.size(), g_Data.tradingPost.sells.size(),
                g_Data.tradingPost.buys.size(), g_Data.bank.size(), g_Data.materials.size());
            APIDefs->Log(LOGL_DEBUG, "MysticTrading", msg);
        }

        cycle++;

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
        if (line.find("refresh=") == 0)
            g_RefreshInterval = atoi(line.substr(8).c_str());
        if (line.find("font_scale=") == 0)
            g_FontScale = (float)atof(line.substr(11).c_str());
        if (line.find("icon_scale=") == 0)
            g_IconScale = (float)atof(line.substr(11).c_str());
        if (line.find("row_scale=") == 0)
            g_RowScale = (float)atof(line.substr(10).c_str());
        if (line.find("flip_limit=") == 0)
            g_FlipLimit = atoi(line.substr(11).c_str());
        if (line.find("lock_flip_list=") == 0)
            g_LockFlipList = (atoi(line.substr(15).c_str()) != 0);
        if (line.find("lock_delivery=") == 0)
            g_LockDelivery = (atoi(line.substr(14).c_str()) != 0);
    }
}

void SaveConfig()
{
    // Read existing API key
    std::string existingKey;
    {
        std::ifstream f(g_ConfigPath);
        std::string line;
        while (std::getline(f, line))
        {
            if (line.find("api_key=") == 0)
                existingKey = line.substr(8);
        }
    }

    std::ofstream f(g_ConfigPath);
    if (!f.is_open()) return;
    f << "refresh=" << g_RefreshInterval << "\n";
    f << "font_scale=" << g_FontScale << "\n";
    f << "icon_scale=" << g_IconScale << "\n";
    f << "row_scale=" << g_RowScale << "\n";
    f << "flip_limit=" << g_FlipLimit << "\n";
    f << "lock_flip_list=" << (g_LockFlipList ? 1 : 0) << "\n";
    f << "lock_delivery=" << (g_LockDelivery ? 1 : 0) << "\n";
    if (!existingKey.empty())
        f << "api_key=" << existingKey << "\n";
}
