/**
 * render.cpp
 *
 * ImGui render callbacks for Mystic Trading.
 * Styled after Fast Swap with accent bars, row tints, and clean tables.
 *
 * - RenderDashboard: full portal view with all boards side by side
 * - RenderFlipList: slim draggable flip column for docking next to TP
 * - RenderOptions: addon settings in Nexus options
 */

#include "shared.h"
#include "imgui/imgui.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <algorithm>
#include <fstream>
#include <string>

// ============================================================================
// Style Constants (Fast Swap inspired)
// ============================================================================

static const float ACCENT_WIDTH     = 3.0f;
static const float ROW_HEIGHT       = 24.0f;
static const float ICON_SIZE        = 20.0f;
static const float SECTION_SPACING  = 8.0f;

// Rarity colors matching the portal
static ImVec4 GetRarityColor(Rarity r)
{
    switch (r)
    {
    case Rarity::Fine:        return ImVec4(0.384f, 0.643f, 0.855f, 1.0f);
    case Rarity::Masterwork:  return ImVec4(0.102f, 0.576f, 0.024f, 1.0f);
    case Rarity::Rare:        return ImVec4(0.988f, 0.816f, 0.043f, 1.0f);
    case Rarity::Exotic:      return ImVec4(1.000f, 0.643f, 0.020f, 1.0f);
    case Rarity::Ascended:    return ImVec4(0.984f, 0.243f, 0.553f, 1.0f);
    case Rarity::Legendary:   return ImVec4(0.298f, 0.075f, 0.616f, 1.0f);
    default:                  return ImVec4(0.667f, 0.667f, 0.667f, 1.0f);
    }
}

// Section header accent colors (matching portal board borders)
static const ImVec4 COLOR_DELIVERY  = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
static const ImVec4 COLOR_FLIPS     = ImVec4(0.925f, 0.306f, 0.596f, 1.0f); // #ec4899
static const ImVec4 COLOR_SELLING   = ImVec4(0.133f, 0.773f, 0.369f, 1.0f); // #22c55e
static const ImVec4 COLOR_BUYING    = ImVec4(0.918f, 0.702f, 0.031f, 1.0f); // #eab308
static const ImVec4 COLOR_WALLET    = ImVec4(0.545f, 0.361f, 0.965f, 1.0f); // #8b5cf6
static const ImVec4 COLOR_BANK      = ImVec4(0.937f, 0.267f, 0.267f, 1.0f); // #ef4444
static const ImVec4 COLOR_MATS      = ImVec4(0.976f, 0.451f, 0.086f, 1.0f); // #f97316

// Coin colors
static const ImVec4 COLOR_GOLD   = ImVec4(1.0f, 0.843f, 0.0f, 1.0f);
static const ImVec4 COLOR_SILVER = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
static const ImVec4 COLOR_COPPER = ImVec4(0.72f, 0.45f, 0.2f, 1.0f);

// Row background (alternating, Fast Swap style)
static const ImVec4 ROW_BG_A = ImVec4(0.22f, 0.26f, 0.32f, 0.35f);
static const ImVec4 ROW_BG_B = ImVec4(0.22f, 0.26f, 0.32f, 0.15f);

// ============================================================================
// Helpers
// ============================================================================

static bool CaseInsensitiveFind(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) return true;
    std::string h = haystack, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

// Draw a colored accent bar on the left side of the current row
static void DrawAccentBar(ImVec4 color)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(pos.x - ACCENT_WIDTH - 2, pos.y),
        ImVec2(pos.x - 2, pos.y + ROW_HEIGHT),
        ImGui::ColorConvertFloat4ToU32(color)
    );
}

// Draw alternating row background
static void DrawRowBg(int index)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    ImVec4 bg = (index % 2 == 0) ? ROW_BG_A : ROW_BG_B;
    ImGui::GetWindowDrawList()->AddRectFilled(
        pos,
        ImVec2(pos.x + width, pos.y + ROW_HEIGHT),
        ImGui::ColorConvertFloat4ToU32(bg)
    );
}

// Render coin amount inline
static void RenderCoins(Coins c, bool compact = false)
{
    if (c.gold > 0)
    {
        ImGui::TextColored(COLOR_GOLD, "%dg", c.gold);
        ImGui::SameLine(0, 2);
    }
    if (!compact || c.gold > 0 || c.silver > 0)
    {
        ImGui::TextColored(COLOR_SILVER, "%02ds", c.silver);
        ImGui::SameLine(0, 2);
    }
    ImGui::TextColored(COLOR_COPPER, "%02dc", c.copper);
}

// Try to render item icon, returns true if rendered
static bool RenderItemIcon(int itemId)
{
    if (!APIDefs) return false;

    char idBuf[64];
    snprintf(idBuf, sizeof(idBuf), "MT_ICON_%d", itemId);
    Texture_t* tex = APIDefs->Textures_Get(idBuf);

    if (tex && tex->Resource)
    {
        ImGui::Image((ImTextureID)tex->Resource, ImVec2(ICON_SIZE, ICON_SIZE));
        return true;
    }

    // Placeholder square
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRect(pos, ImVec2(pos.x + ICON_SIZE, pos.y + ICON_SIZE),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.4f, 0.4f, 0.5f)));
    ImGui::Dummy(ImVec2(ICON_SIZE, ICON_SIZE));
    return false;
}

// Copy button with tooltip and flash feedback
static bool RenderCopyButton(const char* id, const std::string& textToCopy)
{
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 1.0f, 0.4f, 0.4f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

    bool clicked = ImGui::SmallButton("[C]");

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::PopID();

    if (clicked)
        CopyToClipboard(textToCopy);

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Copy \"%s\" to clipboard", textToCopy.c_str());

    return clicked;
}

// Section header with colored accent and item count
static bool RenderSectionHeader(const char* label, ImVec4 accentColor, int count = -1, Coins* total = nullptr)
{
    // Draw accent bar for header
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(pos.x, pos.y),
        ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + 2),
        ImGui::ColorConvertFloat4ToU32(accentColor)
    );
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);

    bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);

    // Show count and total on same line as header
    if (count >= 0 || total)
    {
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        if (count >= 0)
        {
            ImGui::TextDisabled("(%d)", count);
            if (total) ImGui::SameLine();
        }
        if (total) RenderCoins(*total);
    }

    return open;
}

// ============================================================================
// Row Renderers
// ============================================================================

static void RenderFlipRow(const FlipItem& item, int index, bool compact)
{
    ImGui::PushID(item.id);
    DrawRowBg(index);
    DrawAccentBar(COLOR_FLIPS);

    // Icon
    RenderItemIcon(item.id);
    ImGui::SameLine();

    // Copy button
    RenderCopyButton("cp", item.name);
    ImGui::SameLine();

    // Name with rarity color
    ImGui::TextColored(GetRarityColor(item.rarity), "%s", item.name.c_str());

    if (compact)
    {
        // Compact mode: profit + ROI right-aligned
        ImGui::SameLine(ImGui::GetWindowWidth() - 160);
        RenderCoins(item.profit, true);
        ImGui::SameLine(0, 4);
        if (item.roi >= 50.0f)
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "%.0f%%", item.roi);
        else if (item.roi >= 25.0f)
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.0f, 1.0f), "%.0f%%", item.roi);
        else
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%.0f%%", item.roi);
    }
    else
    {
        // Full mode: detailed row
        ImGui::Indent(ICON_SIZE + 28);

        ImGui::TextDisabled("Buy:");
        ImGui::SameLine();
        RenderCoins(item.buyPrice);
        ImGui::SameLine(0, 12);

        ImGui::TextDisabled("Sell:");
        ImGui::SameLine();
        RenderCoins(item.sellPrice);
        ImGui::SameLine(0, 12);

        ImGui::TextDisabled("Profit:");
        ImGui::SameLine();
        RenderCoins(item.profit);
        ImGui::SameLine(0, 4);

        if (item.roi >= 50.0f)
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "(%.0f%%)", item.roi);
        else
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.0f, 1.0f), "(%.0f%%)", item.roi);

        ImGui::TextDisabled("S:%d  D:%d  Sold:%d  Bought:%d",
            item.supply, item.demand, item.sold, item.bought);

        ImGui::Unindent(ICON_SIZE + 28);
    }

    ImGui::Spacing();
    ImGui::PopID();
}

static void RenderTransactionRow(const Transaction& tx, int index, ImVec4 accent)
{
    ImGui::PushID(tx.itemId + index * 100000);
    DrawRowBg(index);
    DrawAccentBar(accent);

    RenderItemIcon(tx.itemId);
    ImGui::SameLine();

    RenderCopyButton("cp", tx.name);
    ImGui::SameLine();

    ImGui::TextColored(GetRarityColor(tx.rarity), "%s", tx.name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("x%d", tx.quantity);
    ImGui::SameLine(ImGui::GetWindowWidth() - 140);
    RenderCoins(tx.price);

    ImGui::Spacing();
    ImGui::PopID();
}

static void RenderItemRow(const Item& item, int index, ImVec4 accent)
{
    ImGui::PushID(item.id + index * 100000);
    DrawRowBg(index);
    DrawAccentBar(accent);

    RenderItemIcon(item.id);
    ImGui::SameLine();

    RenderCopyButton("cp", item.name);
    ImGui::SameLine();

    ImGui::TextColored(GetRarityColor(item.rarity), "%s", item.name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("x%d", item.count);
    ImGui::SameLine(ImGui::GetWindowWidth() - 140);
    RenderCoins(item.totalValue);

    ImGui::Spacing();
    ImGui::PopID();
}

// ============================================================================
// FULL DASHBOARD
// ============================================================================

void RenderDashboard()
{
    if (!g_ShowDashboard) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.95f));

    ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Mystic Trading##Dashboard", &g_ShowDashboard, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }

    std::lock_guard<std::mutex> lock(g_DataMutex);

    if (!g_Data.loaded)
    {
        ImGui::TextDisabled("Connecting to portal...");
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }

    float totalWidth = ImGui::GetContentRegionAvail().x;

    // ---- LEFT COLUMN ----
    ImGui::BeginChild("LeftCol", ImVec2(totalWidth * 0.48f, 0), true, ImGuiWindowFlags_NoScrollbar);
    {
        // Delivery Box
        int deliveryCount = (int)g_Data.tradingPost.delivery.items.size();
        if (RenderSectionHeader("Delivery Box", COLOR_DELIVERY, deliveryCount,
            g_Data.tradingPost.delivery.coins.raw > 0 ? &g_Data.tradingPost.delivery.coins : nullptr))
        {
            static char deliverySearch[128] = "";
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##delsearch", "Search...", deliverySearch, sizeof(deliverySearch));

            ImGui::BeginChild("DeliveryList", ImVec2(0, 150), false);
            int idx = 0;
            for (auto& item : g_Data.tradingPost.delivery.items)
            {
                if (!CaseInsensitiveFind(item.name, deliverySearch)) continue;
                RenderItemRow(item, idx++, COLOR_DELIVERY);
            }
            if (idx == 0) ImGui::TextDisabled("Empty");
            ImGui::EndChild();
        }

        ImGui::Spacing();

        // Selling
        int sellCount = (int)g_Data.tradingPost.sells.size();
        if (RenderSectionHeader("Selling", COLOR_SELLING, sellCount, &g_Data.tradingPost.sellValue))
        {
            static char sellSearch[128] = "";
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##sellsearch", "Search...", sellSearch, sizeof(sellSearch));

            ImGui::BeginChild("SellList", ImVec2(0, 200), false);
            int idx = 0;
            for (auto& tx : g_Data.tradingPost.sells)
            {
                if (!CaseInsensitiveFind(tx.name, sellSearch)) continue;
                RenderTransactionRow(tx, idx++, COLOR_SELLING);
            }
            if (idx == 0) ImGui::TextDisabled("No active sell orders");
            ImGui::EndChild();
        }

        ImGui::Spacing();

        // Buying
        int buyCount = (int)g_Data.tradingPost.buys.size();
        if (RenderSectionHeader("Buying", COLOR_BUYING, buyCount, &g_Data.tradingPost.buyValue))
        {
            static char buySearch[128] = "";
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##buysearch", "Search...", buySearch, sizeof(buySearch));

            ImGui::BeginChild("BuyList", ImVec2(0, 200), false);
            int idx = 0;
            for (auto& tx : g_Data.tradingPost.buys)
            {
                if (!CaseInsensitiveFind(tx.name, buySearch)) continue;
                RenderTransactionRow(tx, idx++, COLOR_BUYING);
            }
            if (idx == 0) ImGui::TextDisabled("No active buy orders");
            ImGui::EndChild();
        }

        ImGui::Spacing();

        // Wallet
        if (RenderSectionHeader("Wallet", COLOR_WALLET, -1, &g_Data.tradingPost.wallet.gold))
        {
            static char walletSearch[128] = "";
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##walletsearch", "Search currencies...", walletSearch, sizeof(walletSearch));

            for (auto& c : g_Data.tradingPost.wallet.currencies)
            {
                if (!CaseInsensitiveFind(c.name, walletSearch)) continue;
                ImGui::TextDisabled("%s:", c.name.c_str());
                ImGui::SameLine();
                ImGui::Text("%d", c.value);
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine(0, SECTION_SPACING);

    // ---- RIGHT COLUMN ----
    ImGui::BeginChild("RightCol", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);
    {
        // Flips
        int flipCount = (int)g_Data.flips.size();
        if (RenderSectionHeader("Flips", COLOR_FLIPS, flipCount))
        {
            static char flipSearch[128] = "";
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##flipsearch", "Search flips...", flipSearch, sizeof(flipSearch));

            ImGui::BeginChild("FlipsList", ImVec2(0, 300), false);
            int idx = 0;
            for (auto& flip : g_Data.flips)
            {
                if (!CaseInsensitiveFind(flip.name, flipSearch)) continue;
                RenderFlipRow(flip, idx++, false);
            }
            if (idx == 0) ImGui::TextDisabled("No flips found");
            ImGui::EndChild();
        }

        ImGui::Spacing();

        // Bank
        int bankCount = (int)g_Data.bank.size();
        if (RenderSectionHeader("Bank", COLOR_BANK, bankCount, &g_Data.bankTotal))
        {
            static char bankSearch[128] = "";
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##banksearch", "Search bank...", bankSearch, sizeof(bankSearch));

            ImGui::BeginChild("BankList", ImVec2(0, 200), false);
            int idx = 0;
            for (auto& item : g_Data.bank)
            {
                if (!CaseInsensitiveFind(item.name, bankSearch)) continue;
                RenderItemRow(item, idx++, COLOR_BANK);
            }
            if (idx == 0) ImGui::TextDisabled("No data");
            ImGui::EndChild();
        }

        ImGui::Spacing();

        // Materials
        int matsCount = (int)g_Data.materials.size();
        if (RenderSectionHeader("Materials", COLOR_MATS, matsCount, &g_Data.matsTotal))
        {
            static char matsSearch[128] = "";
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##matssearch", "Search materials...", matsSearch, sizeof(matsSearch));

            ImGui::BeginChild("MatsList", ImVec2(0, 200), false);
            int idx = 0;
            for (auto& item : g_Data.materials)
            {
                if (!CaseInsensitiveFind(item.name, matsSearch)) continue;
                RenderItemRow(item, idx++, COLOR_MATS);
            }
            if (idx == 0) ImGui::TextDisabled("No data");
            ImGui::EndChild();
        }
    }
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ============================================================================
// FLIP COLUMN - Slim, draggable, resizable
// ============================================================================

void RenderFlipList()
{
    if (!g_ShowFlipList) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 2));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.08f, 0.92f));

    ImGui::SetNextWindowSize(ImVec2(340, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Flips##FlipColumn", &g_ShowFlipList, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }

    std::lock_guard<std::mutex> lock(g_DataMutex);

    if (!g_Data.loaded)
    {
        ImGui::TextDisabled("Loading...");
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }

    // Header bar with accent
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + 2),
        ImGui::ColorConvertFloat4ToU32(COLOR_FLIPS));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);

    ImGui::TextColored(COLOR_FLIPS, "Profitable Flips");
    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
    ImGui::TextDisabled("(%zu)", g_Data.flips.size());

    // Search
    static char search[128] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##flipsearch", "Search items...", search, sizeof(search));
    ImGui::Spacing();

    // Scrollable flip list
    ImGui::BeginChild("FlipScroll", ImVec2(0, 0), false);
    int idx = 0;
    for (auto& flip : g_Data.flips)
    {
        if (!CaseInsensitiveFind(flip.name, search)) continue;
        RenderFlipRow(flip, idx++, true);
    }
    if (idx == 0)
        ImGui::TextDisabled("No matching flips");
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ============================================================================
// OPTIONS - Nexus addon settings panel
// ============================================================================

void RenderOptions()
{
    ImGui::Text("Mystic Trading v0.1");
    ImGui::Separator();

    if (!APIDefs)
    {
        ImGui::Text("Addon not loaded.");
        return;
    }

    ImGui::Spacing();

    // GW2 API Key status
    ImGui::Text("GW2 API Key");
    ImGui::TextDisabled("Generate at account.arena.net/applications");
    ImGui::TextDisabled("Needs: account, inventories, tradingpost, wallet");
    {
        std::lock_guard<std::mutex> lock(g_DataMutex);
        if (g_Data.loaded)
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Key: Active - data is loading");
        else
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Key: Waiting for first fetch...");
    }
    ImGui::Spacing();
    ImGui::Text("Enter new key to replace:");
    static char apiKeyBuf[256] = "";
    if (ImGui::InputText("##mt_apikey", apiKeyBuf, sizeof(apiKeyBuf), ImGuiInputTextFlags_Password))
    {
        // just buffer update
    }
    if (ImGui::Button("Save API Key") && apiKeyBuf[0] != '\0')
    {
        std::string configPath = std::string(APIDefs->Paths_GetAddonDirectory("MysticTrading")) + "\\mystic-trading.cfg";
        std::ofstream f(configPath);
        if (f.is_open())
        {
            f << "refresh=" << g_RefreshInterval << "\n";
            f << "api_key=" << apiKeyBuf << "\n";
        }
        APIDefs->Log(LOGL_INFO, "MysticTrading", "GW2 API key saved.");
    }

    ImGui::Spacing();

    // Refresh interval
    ImGui::Text("Refresh Interval (seconds)");
    ImGui::SliderInt("##mt_refresh", &g_RefreshInterval, 10, 300, "%d");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Keybinds:");
    ImGui::BulletText("ALT+T - Toggle full dashboard");
    ImGui::BulletText("ALT+F - Toggle flip column");
    ImGui::Spacing();
    ImGui::TextDisabled("Click [C] next to any item to copy its name.");
    ImGui::TextDisabled("Drag the flip column next to the TP for quick flipping.");
    ImGui::Spacing();
    ImGui::TextDisabled("Data: GW2 API (direct) + GW2BLTC (flips)");
}
