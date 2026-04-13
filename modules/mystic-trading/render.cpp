/**
 * render.cpp
 *
 * ImGui render callbacks for Mystic Trading.
 *
 * - RenderDashboard: full view — Delivery, Currencies, then rest (all collapsed by default)
 * - RenderFlipList: slim draggable flip column, lockable size/position
 * - RenderDeliveryBox: auto-shows when delivery items waiting, lockable, no ESC close
 * - RenderOptions: addon settings in Nexus options
 */

#include "shared.h"
#include "imgui/imgui.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <algorithm>
#include <string>
#include <shellapi.h>

// ============================================================================
// Style Constants
// ============================================================================

static const float ACCENT_WIDTH     = 3.0f;
static const float BASE_ROW_HEIGHT  = 24.0f;
static const float BASE_ICON_SIZE   = 20.0f;
static const float SECTION_SPACING  = 8.0f;

// Scaled sizes (use these everywhere)
static float RowHeight() { return BASE_ROW_HEIGHT * g_RowScale; }
static float IconSize()  { return BASE_ICON_SIZE * g_IconScale; }

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

static const ImVec4 COLOR_DELIVERY  = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
static const ImVec4 COLOR_FLIPS     = ImVec4(0.925f, 0.306f, 0.596f, 1.0f);
static const ImVec4 COLOR_SELLING   = ImVec4(0.133f, 0.773f, 0.369f, 1.0f);
static const ImVec4 COLOR_BUYING    = ImVec4(0.918f, 0.702f, 0.031f, 1.0f);
static const ImVec4 COLOR_WALLET    = ImVec4(0.545f, 0.361f, 0.965f, 1.0f);
static const ImVec4 COLOR_BANK      = ImVec4(0.937f, 0.267f, 0.267f, 1.0f);
static const ImVec4 COLOR_MATS      = ImVec4(0.976f, 0.451f, 0.086f, 1.0f);

static const ImVec4 COLOR_GOLD   = ImVec4(1.0f, 0.843f, 0.0f, 1.0f);
static const ImVec4 COLOR_SILVER = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
static const ImVec4 COLOR_COPPER = ImVec4(0.72f, 0.45f, 0.2f, 1.0f);

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

static void DrawAccentBar(ImVec4 color)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(pos.x - ACCENT_WIDTH - 2, pos.y),
        ImVec2(pos.x - 2, pos.y + RowHeight()),
        ImGui::ColorConvertFloat4ToU32(color));
}

static void DrawRowBg(int index)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    ImVec4 bg = (index % 2 == 0) ? ROW_BG_A : ROW_BG_B;
    ImGui::GetWindowDrawList()->AddRectFilled(
        pos, ImVec2(pos.x + width, pos.y + RowHeight()),
        ImGui::ColorConvertFloat4ToU32(bg));
}

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

// Open GW2 wiki page for an item
static void OpenWiki(const std::string& itemName)
{
    std::string url = "https://wiki.guildwars2.com/wiki/";
    for (char c : itemName)
        url += (c == ' ') ? '_' : c;
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static bool RenderItemIcon(int itemId, const std::string& name = "")
{
    if (!APIDefs) return false;
    char idBuf[64];
    snprintf(idBuf, sizeof(idBuf), "MT_ICON_%d", itemId);
    Texture_t* tex = APIDefs->Textures_Get(idBuf);
    ImVec2 iconPos = ImGui::GetCursorScreenPos();
    if (tex && tex->Resource)
    {
        ImGui::Image((ImTextureID)tex->Resource, ImVec2(IconSize(), IconSize()));
    }
    else
    {
        ImGui::GetWindowDrawList()->AddRect(iconPos, ImVec2(iconPos.x + IconSize(), iconPos.y + IconSize()),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.4f, 0.4f, 0.5f)));
        ImGui::Dummy(ImVec2(IconSize(), IconSize()));
    }

    if (!name.empty())
    {
        // Overlay invisible button on icon for click/right-click
        ImGui::SetCursorScreenPos(iconPos);
        ImGui::InvisibleButton("##iconBtn", ImVec2(IconSize(), IconSize()));

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            CopyToClipboard(name);

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click to copy");

        if (ImGui::BeginPopupContextItem("##iconCtx"))
        {
            if (ImGui::MenuItem("Wiki"))
                OpenWiki(name);
            ImGui::EndPopup();
        }
    }

    return (tex && tex->Resource);
}

// Render clickable item name — left-click copies, right-click opens context menu with Wiki
static void RenderClickableName(const char* id, const std::string& name, Rarity rarity)
{
    ImGui::PushID(id);

    ImVec2 textPos = ImGui::GetCursorScreenPos();
    ImVec2 textSize = ImGui::CalcTextSize(name.c_str());

    ImGui::TextColored(GetRarityColor(rarity), "%s", name.c_str());

    // Overlay invisible button for click detection
    ImGui::SetCursorScreenPos(textPos);
    ImGui::InvisibleButton("##nameBtn", textSize);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        CopyToClipboard(name);

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Click to copy");

    if (ImGui::BeginPopupContextItem("##itemCtx"))
    {
        if (ImGui::MenuItem("Wiki"))
            OpenWiki(name);
        ImGui::EndPopup();
    }

    ImGui::PopID();
}

// Section header — collapsed by default, with expand/collapse
static bool RenderSectionHeader(const char* label, ImVec4 accentColor, int count = -1, Coins* total = nullptr, bool defaultOpen = false)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(pos.x, pos.y),
        ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + 2),
        ImGui::ColorConvertFloat4ToU32(accentColor));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);

    ImGuiTreeNodeFlags flags = defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    bool open = ImGui::CollapsingHeader(label, flags);

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
// Row Renderers — copy icon is to the RIGHT of the value
// ============================================================================

// Check if mouse is hovering over a row region (call after rendering the row)
static bool IsRowHovered(ImVec2 rowStart, float width)
{
    ImVec2 mouse = ImGui::GetIO().MousePos;
    ImVec2 rowEnd = ImVec2(rowStart.x + width, rowStart.y + RowHeight() + 4);
    return mouse.x >= rowStart.x && mouse.x <= rowEnd.x &&
           mouse.y >= rowStart.y && mouse.y <= rowEnd.y;
}

// Render description and stats in a tooltip
static void RenderStatsInTooltip(const std::string& description, const std::vector<ItemStat>& stats)
{
    if (!stats.empty())
    {
        for (auto& s : stats)
        {
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "+%d", s.modifier);
            ImGui::SameLine(0, 4);
            ImGui::Text("%s", s.attribute.c_str());
        }
    }
    if (!description.empty())
    {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(300);
        ImGui::TextDisabled("%s", description.c_str());
        ImGui::PopTextWrapPos();
    }
}

// Tooltip shown on hover over any flip item
static void RenderFlipTooltip(const FlipItem& item)
{
    ImGui::BeginTooltip();
    ImGui::TextColored(GetRarityColor(item.rarity), "%s", item.name.c_str());
    ImGui::Separator();

    RenderStatsInTooltip(item.description, item.stats);
    if (!item.stats.empty() || !item.description.empty())
        ImGui::Spacing();

    ImGui::TextDisabled("Buy Price:");
    ImGui::SameLine(120);
    RenderCoins(item.buyPrice);

    ImGui::TextDisabled("Sell Price:");
    ImGui::SameLine(120);
    RenderCoins(item.sellPrice);

    ImGui::TextDisabled("Profit:");
    ImGui::SameLine(120);
    RenderCoins(item.profit);

    ImGui::TextDisabled("ROI:");
    ImGui::SameLine(120);
    if (item.roi >= 50.0f)
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "%.1f%%", item.roi);
    else if (item.roi >= 25.0f)
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.0f, 1.0f), "%.1f%%", item.roi);
    else
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%.1f%%", item.roi);

    ImGui::Spacing();
    ImGui::TextDisabled("Supply: %d", item.supply);
    ImGui::TextDisabled("Demand: %d", item.demand);
    ImGui::TextDisabled("Sold/day: %d", item.sold);
    ImGui::TextDisabled("Bought/day: %d", item.bought);

    ImGui::EndTooltip();
}

static void RenderFlipRow(const FlipItem& item, int index, bool compact)
{
    ImGui::PushID(item.id);
    ImVec2 rowStart = ImGui::GetCursorScreenPos();
    float rowWidth = ImGui::GetContentRegionAvail().x;
    DrawRowBg(index);
    DrawAccentBar(COLOR_FLIPS);

    RenderItemIcon(item.id, item.name);
    ImGui::SameLine();

    RenderClickableName("cn", item.name, item.rarity);

    if (compact)
    {
        ImGui::Indent(IconSize() + 8);
        ImGui::TextDisabled("B:");
        ImGui::SameLine(0, 2);
        RenderCoins(item.buyPrice, true);
        ImGui::SameLine(0, 6);
        ImGui::TextDisabled("S:");
        ImGui::SameLine(0, 2);
        RenderCoins(item.sellPrice, true);
        ImGui::SameLine(0, 6);
        ImGui::TextDisabled("P:");
        ImGui::SameLine(0, 2);
        RenderCoins(item.profit, true);
        ImGui::SameLine(0, 4);
        if (item.roi >= 50.0f)
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "%.0f%%", item.roi);
        else if (item.roi >= 25.0f)
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.0f, 1.0f), "%.0f%%", item.roi);
        else
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%.0f%%", item.roi);
        ImGui::Unindent(IconSize() + 8);
    }
    else
    {
        ImGui::Indent(IconSize() + 8);
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
        ImGui::Unindent(IconSize() + 8);
    }

    if (IsRowHovered(rowStart, rowWidth))
        RenderFlipTooltip(item);

    ImGui::Spacing();
    ImGui::PopID();
}

static void RenderTransactionRow(const Transaction& tx, int index, ImVec4 accent)
{
    ImGui::PushID(tx.itemId + index * 100000);
    ImVec2 rowStart = ImGui::GetCursorScreenPos();
    float rowWidth = ImGui::GetContentRegionAvail().x;
    DrawRowBg(index);
    DrawAccentBar(accent);

    RenderItemIcon(tx.itemId, tx.name);
    ImGui::SameLine();
    RenderClickableName("cn", tx.name, tx.rarity);
    ImGui::SameLine();
    ImGui::TextDisabled("x%d", tx.quantity);
    ImGui::SameLine(ImGui::GetWindowWidth() - 160);
    RenderCoins(tx.price);

    if (IsRowHovered(rowStart, rowWidth))
    {
        ImGui::BeginTooltip();
        ImGui::TextColored(GetRarityColor(tx.rarity), "%s", tx.name.c_str());
        ImGui::Separator();
        ImGui::TextDisabled("Listed Price:");
        ImGui::SameLine(120);
        RenderCoins(tx.price);
        ImGui::TextDisabled("Quantity:");
        ImGui::SameLine(120);
        ImGui::Text("%d", tx.quantity);
        if (!tx.created.empty())
        {
            ImGui::TextDisabled("Listed:");
            ImGui::SameLine(120);
            ImGui::TextDisabled("%s", tx.created.c_str());
        }
        ImGui::EndTooltip();
    }

    ImGui::Spacing();
    ImGui::PopID();
}

static void RenderItemRow(const Item& item, int index, ImVec4 accent)
{
    ImGui::PushID(item.id + index * 100000);
    ImVec2 rowStart = ImGui::GetCursorScreenPos();
    float rowWidth = ImGui::GetContentRegionAvail().x;
    DrawRowBg(index);
    DrawAccentBar(accent);

    RenderItemIcon(item.id, item.name);
    ImGui::SameLine();
    RenderClickableName("cn", item.name, item.rarity);
    ImGui::SameLine();
    ImGui::TextDisabled("x%d", item.count);
    ImGui::SameLine(ImGui::GetWindowWidth() - 160);
    RenderCoins(item.totalValue);

    if (IsRowHovered(rowStart, rowWidth))
    {
        ImGui::BeginTooltip();
        ImGui::TextColored(GetRarityColor(item.rarity), "%s", item.name.c_str());
        ImGui::Separator();

        RenderStatsInTooltip(item.description, item.stats);
        if (!item.stats.empty() || !item.description.empty())
            ImGui::Spacing();

        ImGui::TextDisabled("Buy Price:");
        ImGui::SameLine(120);
        RenderCoins(item.buyPrice);

        ImGui::TextDisabled("Sell Price:");
        ImGui::SameLine(120);
        RenderCoins(item.sellPrice);

        if (item.count > 1)
        {
            ImGui::TextDisabled("Count:");
            ImGui::SameLine(120);
            ImGui::Text("%d", item.count);
            ImGui::TextDisabled("Total Value:");
            ImGui::SameLine(120);
            RenderCoins(item.totalValue);
        }

        if (item.supply > 0 || item.demand > 0)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Supply: %d", item.supply);
            ImGui::TextDisabled("Demand: %d", item.demand);
        }

        ImGui::EndTooltip();
    }

    ImGui::Spacing();
    ImGui::PopID();
}

// ============================================================================
// DELIVERY BOX — auto-shows, lockable, no ESC close
// ============================================================================

void RenderDeliveryBox()
{
    std::lock_guard<std::mutex> lock(g_DataMutex);

    if (!g_Data.loaded) return;

    if (!g_ShowDelivery) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 2));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.08f, g_WindowOpacity));

    ImGui::SetNextWindowSize(ImVec2(340, 300), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    if (g_LockDelivery)
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;

    if (!ImGui::Begin("Delivery Box##DeliveryWin", &g_ShowDelivery, flags))
    {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }
    ImGui::SetWindowFontScale(g_FontScale);

    // Right-click context menu for lock/unlock
    if (ImGui::BeginPopupContextWindow("##DeliveryCtx"))
    {
        if (ImGui::MenuItem(g_LockDelivery ? "Unlock" : "Lock"))
            g_LockDelivery = !g_LockDelivery;
        ImGui::EndPopup();
    }

    // Gold waiting
    if (g_Data.tradingPost.delivery.coins.raw > 0)
    {
        ImGui::TextDisabled("Gold:");
        ImGui::SameLine();
        RenderCoins(g_Data.tradingPost.delivery.coins);
    }

    ImGui::Spacing();

    // Items
    if (!g_Data.tradingPost.delivery.items.empty())
    {
        ImGui::BeginChild("DeliveryScroll", ImVec2(0, 0), false);
        int idx = 0;
        for (auto& item : g_Data.tradingPost.delivery.items)
            RenderItemRow(item, idx++, COLOR_DELIVERY);
        ImGui::EndChild();
    }
    else
    {
        ImGui::TextDisabled("No items waiting");
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ============================================================================
// FULL DASHBOARD — Delivery first, Currencies, then rest. Collapsed by default.
// ============================================================================

void RenderDashboard()
{
    if (!g_ShowDashboard) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, g_WindowOpacity));

    ImGui::SetNextWindowSize(ImVec2(600, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Mystic Trading##Dashboard", &g_ShowDashboard, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }
    ImGui::SetWindowFontScale(g_FontScale);

    std::lock_guard<std::mutex> lock(g_DataMutex);

    if (!g_Data.loaded)
    {
        ImGui::TextDisabled("Fetching data from GW2 API...");
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }

    // ---- Single column: Wallet, Bank, Materials ----

    // 1. Wallet — expanded by default, collapsible
    if (RenderSectionHeader("Wallet", COLOR_WALLET, -1, &g_Data.tradingPost.wallet.gold, true))
    {
        static char walletSearch[128] = "";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##walletsearch", "Search currencies...", walletSearch, sizeof(walletSearch));

        for (auto& c : g_Data.tradingPost.wallet.currencies)
        {
            if (!CaseInsensitiveFind(c.name, walletSearch)) continue;

            if (!c.icon.empty())
            {
                char iconId[64];
                snprintf(iconId, sizeof(iconId), "MT_CUR_%d", c.id);
                Texture_t* tex = APIDefs ? APIDefs->Textures_Get(iconId) : nullptr;
                if (tex && tex->Resource)
                {
                    ImGui::Image((ImTextureID)tex->Resource, ImVec2(IconSize() * 0.8f, IconSize() * 0.8f));
                    ImGui::SameLine();
                }
            }

            ImGui::TextDisabled("%s:", c.name.c_str());
            ImGui::SameLine();
            ImGui::Text("%d", c.value);
        }
    }

    ImGui::Spacing();

    // 2. Bank (collapsed by default)
    int bankCount = (int)g_Data.bank.size();
    if (RenderSectionHeader("Bank", COLOR_BANK, bankCount, &g_Data.bankTotal))
    {
        static char bankSearch[128] = "";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##banksearch", "Search bank...", bankSearch, sizeof(bankSearch));

        ImGui::BeginChild("BankList", ImVec2(0, 250), false);
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

    // 3. Materials (collapsed by default)
    int matsCount = (int)g_Data.materials.size();
    if (RenderSectionHeader("Materials", COLOR_MATS, matsCount, &g_Data.matsTotal))
    {
        static char matsSearch[128] = "";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##matssearch", "Search materials...", matsSearch, sizeof(matsSearch));

        ImGui::BeginChild("MatsList", ImVec2(0, 250), false);
        int idx = 0;
        for (auto& item : g_Data.materials)
        {
            if (!CaseInsensitiveFind(item.name, matsSearch)) continue;
            RenderItemRow(item, idx++, COLOR_MATS);
        }
        if (idx == 0) ImGui::TextDisabled("No data");
        ImGui::EndChild();
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ============================================================================
// FLIP COLUMN — lockable size/position, closes on ESC
// ============================================================================

void RenderFlipList()
{
    if (!g_ShowFlipList) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 2));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.08f, g_WindowOpacity));

    ImGui::SetNextWindowSize(ImVec2(340, 600), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    if (g_LockFlipList)
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;

    if (!ImGui::Begin("Flips##FlipColumn", &g_ShowFlipList, flags))
    {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }
    ImGui::SetWindowFontScale(g_FontScale);

    // Right-click context menu for lock/unlock + flip limit
    if (ImGui::BeginPopupContextWindow("##FlipCtx"))
    {
        if (ImGui::MenuItem(g_LockFlipList ? "Unlock" : "Lock"))
            g_LockFlipList = !g_LockFlipList;
        ImGui::Separator();
        ImGui::TextDisabled("Show:");
        if (ImGui::MenuItem("20 items", nullptr, g_FlipLimit == 20)) g_FlipLimit = 20;
        if (ImGui::MenuItem("50 items", nullptr, g_FlipLimit == 50)) g_FlipLimit = 50;
        if (ImGui::MenuItem("100 items", nullptr, g_FlipLimit == 100)) g_FlipLimit = 100;
        ImGui::EndPopup();
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

    // Search
    static char search[128] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##flipsearch", "Search items...", search, sizeof(search));
    ImGui::Spacing();

    // Scrollable flip list (limited by g_FlipLimit)
    ImGui::BeginChild("FlipScroll", ImVec2(0, 0), false);
    int idx = 0;
    for (auto& flip : g_Data.flips)
    {
        if (idx >= g_FlipLimit) break;
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
// OPTIONS
// ============================================================================

void RenderOptions()
{
    ImGui::Text("Mystic Trading v0.4.0 (build 20260319)");
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

    // Flip limit dropdown
    ImGui::Text("Flip Items to Show");
    {
        const char* flipOptions[] = { "20", "50", "100" };
        int currentIdx = 0;
        if (g_FlipLimit == 50) currentIdx = 1;
        else if (g_FlipLimit == 100) currentIdx = 2;
        if (ImGui::Combo("##mt_fliplimit", &currentIdx, flipOptions, 3))
        {
            if (currentIdx == 0) g_FlipLimit = 20;
            else if (currentIdx == 1) g_FlipLimit = 50;
            else g_FlipLimit = 100;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // UI Scale settings
    ImGui::Text("Appearance");
    ImGui::Text("Text Size");
    ImGui::SliderFloat("##mt_fontscale", &g_FontScale, 0.7f, 2.0f, "%.1f");
    ImGui::Text("Icon Size");
    ImGui::SliderFloat("##mt_iconscale", &g_IconScale, 0.5f, 3.0f, "%.1f");
    ImGui::Text("Row Height");
    ImGui::SliderFloat("##mt_rowscale", &g_RowScale, 0.7f, 2.0f, "%.1f");
    ImGui::Text("Window Transparency");
    ImGui::SliderFloat("##mt_opacity", &g_WindowOpacity, 0.3f, 1.0f, "%.0f%%");
    if (ImGui::Button("Reset to Default"))
    {
        g_FontScale = 1.0f;
        g_IconScale = 1.0f;
        g_RowScale = 1.0f;
        g_WindowOpacity = 0.92f;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Keybinds:");
    ImGui::BulletText("ALT+T - Wallet, Bank & Materials");
    ImGui::BulletText("ALT+F - Toggle flip list");
    ImGui::BulletText("ALT+D - Toggle delivery box (auto-shows when items waiting)");
    ImGui::Spacing();
    ImGui::TextDisabled("Copy icon next to values copies item name.");
    ImGui::TextDisabled("Right-click windows to lock/unlock position.");
    ImGui::Spacing();
    ImGui::TextDisabled("Data: GW2 API (direct) + GW2BLTC (flips)");
}
