/**
 * render.cpp
 *
 * ImGui render callbacks for Mystic Trading.
 * - RenderDashboard: full portal view with all boards side by side
 * - RenderFlipList: slim draggable flip column for docking next to TP
 * - RenderOptions: addon settings in Nexus options
 */

#include "shared.h"
#include "imgui/imgui.h"
#include <cstdio>

// Rarity colors matching the portal
static ImVec4 GetRarityColor(Rarity r)
{
    switch (r)
    {
    case Rarity::Fine:        return ImVec4(0.384f, 0.643f, 0.855f, 1.0f); // #62A4DA
    case Rarity::Masterwork:  return ImVec4(0.102f, 0.576f, 0.024f, 1.0f); // #1a9306
    case Rarity::Rare:        return ImVec4(0.988f, 0.816f, 0.043f, 1.0f); // #fcd00b
    case Rarity::Exotic:      return ImVec4(1.000f, 0.643f, 0.020f, 1.0f); // #ffa405
    case Rarity::Ascended:    return ImVec4(0.984f, 0.243f, 0.553f, 1.0f); // #fb3e8d
    case Rarity::Legendary:   return ImVec4(0.298f, 0.075f, 0.616f, 1.0f); // #4C139D
    default:                  return ImVec4(0.667f, 0.667f, 0.667f, 1.0f); // #AAAAAA
    }
}

// Gold color for coin display
static const ImVec4 COLOR_GOLD   = ImVec4(1.0f, 0.843f, 0.0f, 1.0f);
static const ImVec4 COLOR_SILVER = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
static const ImVec4 COLOR_COPPER = ImVec4(0.72f, 0.45f, 0.2f, 1.0f);

// Render coin amount inline
static void RenderCoins(Coins c)
{
    if (c.gold > 0)
    {
        ImGui::TextColored(COLOR_GOLD, "%dg", c.gold);
        ImGui::SameLine();
    }
    ImGui::TextColored(COLOR_SILVER, "%02ds", c.silver);
    ImGui::SameLine();
    ImGui::TextColored(COLOR_COPPER, "%02dc", c.copper);
}

// Copy button - small clipboard icon next to item name
static bool RenderCopyButton(const char* id, const std::string& textToCopy)
{
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.3f));

    bool clicked = ImGui::SmallButton("[C]");

    ImGui::PopStyleColor(3);
    ImGui::PopID();

    if (clicked)
    {
        CopyToClipboard(textToCopy);
    }

    // Tooltip
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Copy \"%s\"", textToCopy.c_str());
    }

    return clicked;
}

// Render a single flip item row (used by both dashboard and flip column)
static void RenderFlipRow(const FlipItem& item, bool compact)
{
    ImGui::PushID(item.id);

    // Copy button
    RenderCopyButton("copy", item.name);
    ImGui::SameLine();

    // Item name with rarity color
    ImVec4 color = GetRarityColor(item.rarity);
    ImGui::TextColored(color, "%s", item.name.c_str());

    if (compact)
    {
        // Compact: profit + ROI on same line
        ImGui::SameLine(ImGui::GetWindowWidth() - 180);
        RenderCoins(item.profit);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "(%.0f%%)", item.roi);
    }
    else
    {
        // Full: show buy/sell/profit/ROI + volumes
        ImGui::Indent(24);

        ImGui::Text("Buy: ");
        ImGui::SameLine();
        RenderCoins(item.buyPrice);
        ImGui::SameLine();
        ImGui::Text("  Sell: ");
        ImGui::SameLine();
        RenderCoins(item.sellPrice);
        ImGui::SameLine();
        ImGui::Text("  Profit: ");
        ImGui::SameLine();
        RenderCoins(item.profit);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), " (%.0f%% ROI)", item.roi);

        ImGui::TextDisabled("S:%d  D:%d  Sold:%d  Bought:%d",
            item.supply, item.demand, item.sold, item.bought);

        ImGui::Unindent(24);
    }

    ImGui::Separator();
    ImGui::PopID();
}

// Render a transaction row (buy/sell order)
static void RenderTransactionRow(const Transaction& tx)
{
    ImGui::PushID(tx.itemId);

    RenderCopyButton("copy", tx.name);
    ImGui::SameLine();

    ImVec4 color = GetRarityColor(tx.rarity);
    ImGui::TextColored(color, "%s", tx.name.c_str());
    ImGui::SameLine();
    ImGui::Text(" x%d  ", tx.quantity);
    ImGui::SameLine();
    RenderCoins(tx.price);

    ImGui::Separator();
    ImGui::PopID();
}

// Render a bank/material item row
static void RenderItemRow(const Item& item)
{
    ImGui::PushID(item.id);

    RenderCopyButton("copy", item.name);
    ImGui::SameLine();

    ImVec4 color = GetRarityColor(item.rarity);
    ImGui::TextColored(color, "%s", item.name.c_str());
    ImGui::SameLine();
    ImGui::Text(" x%d  ", item.count);
    ImGui::SameLine();
    RenderCoins(item.totalValue);

    ImGui::Separator();
    ImGui::PopID();
}

// ============================================================================
// FULL DASHBOARD - All boards side by side
// ============================================================================
void RenderDashboard()
{
    if (!g_ShowDashboard) return;

    ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Mystic Trading##Dashboard", &g_ShowDashboard,
        ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    std::lock_guard<std::mutex> lock(g_DataMutex);

    if (!g_Data.loaded)
    {
        ImGui::Text("Loading data from portal...");
        ImGui::End();
        return;
    }

    // Two-column layout: left (TP data) | right (flips + market)
    float totalWidth = ImGui::GetContentRegionAvail().x;

    // ---- LEFT COLUMN: Delivery, Selling, Buying, Wallet ----
    ImGui::BeginChild("LeftColumn", ImVec2(totalWidth * 0.5f, 0), true);
    {
        // Delivery Box
        if (ImGui::CollapsingHeader("Delivery Box", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (g_Data.tradingPost.delivery.coins.raw > 0)
            {
                ImGui::Text("Coins: ");
                ImGui::SameLine();
                RenderCoins(g_Data.tradingPost.delivery.coins);
            }
            for (auto& item : g_Data.tradingPost.delivery.items)
                RenderItemRow(item);
            if (g_Data.tradingPost.delivery.items.empty())
                ImGui::TextDisabled("Empty");
        }

        // Selling
        if (ImGui::CollapsingHeader("Selling", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Total: ");
            ImGui::SameLine();
            RenderCoins(g_Data.tradingPost.sellValue);
            ImGui::Separator();
            for (auto& tx : g_Data.tradingPost.sells)
                RenderTransactionRow(tx);
            if (g_Data.tradingPost.sells.empty())
                ImGui::TextDisabled("No active sell orders");
        }

        // Buying
        if (ImGui::CollapsingHeader("Buying", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Total: ");
            ImGui::SameLine();
            RenderCoins(g_Data.tradingPost.buyValue);
            ImGui::Separator();
            for (auto& tx : g_Data.tradingPost.buys)
                RenderTransactionRow(tx);
            if (g_Data.tradingPost.buys.empty())
                ImGui::TextDisabled("No active buy orders");
        }

        // Wallet
        if (ImGui::CollapsingHeader("Wallet"))
        {
            ImGui::Text("Gold: ");
            ImGui::SameLine();
            RenderCoins(g_Data.tradingPost.wallet.gold);
            for (auto& c : g_Data.tradingPost.wallet.currencies)
            {
                ImGui::Text("%s: %d", c.name.c_str(), c.value);
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ---- RIGHT COLUMN: Flips, Bank, Materials ----
    ImGui::BeginChild("RightColumn", ImVec2(0, 0), true);
    {
        // Flips
        if (ImGui::CollapsingHeader("Flips", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("%zu profitable flips", g_Data.flips.size());
            ImGui::Separator();

            // Search filter
            static char flipSearch[128] = "";
            ImGui::InputText("Search##flips", flipSearch, sizeof(flipSearch));

            ImGui::BeginChild("FlipsList", ImVec2(0, 300), false);
            for (auto& flip : g_Data.flips)
            {
                // Filter by search
                if (flipSearch[0] != '\0')
                {
                    std::string lower = flip.name;
                    std::string query = flipSearch;
                    // Simple case-insensitive search
                    for (auto& c : lower) c = (char)tolower(c);
                    for (auto& c : query) c = (char)tolower(c);
                    if (lower.find(query) == std::string::npos)
                        continue;
                }
                RenderFlipRow(flip, false);
            }
            ImGui::EndChild();
        }

        // Bank
        if (ImGui::CollapsingHeader("Bank"))
        {
            ImGui::Text("Total: ");
            ImGui::SameLine();
            RenderCoins(g_Data.bankTotal);
            ImGui::Separator();

            ImGui::BeginChild("BankList", ImVec2(0, 200), false);
            for (auto& item : g_Data.bank)
                RenderItemRow(item);
            if (g_Data.bank.empty())
                ImGui::TextDisabled("No data");
            ImGui::EndChild();
        }

        // Materials
        if (ImGui::CollapsingHeader("Materials"))
        {
            ImGui::Text("Total: ");
            ImGui::SameLine();
            RenderCoins(g_Data.matsTotal);
            ImGui::Separator();

            ImGui::BeginChild("MatsList", ImVec2(0, 200), false);
            for (auto& item : g_Data.materials)
                RenderItemRow(item);
            if (g_Data.materials.empty())
                ImGui::TextDisabled("No data");
            ImGui::EndChild();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

// ============================================================================
// FLIP COLUMN - Slim, resizable, draggable list for docking next to TP
// ============================================================================
void RenderFlipList()
{
    if (!g_ShowFlipList) return;

    ImGui::SetNextWindowSize(ImVec2(320, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Flips##FlipColumn", &g_ShowFlipList,
        ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    std::lock_guard<std::mutex> lock(g_DataMutex);

    if (!g_Data.loaded)
    {
        ImGui::Text("Loading...");
        ImGui::End();
        return;
    }

    // Search filter
    static char search[128] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##flipsearch", "Search items...", search, sizeof(search));
    ImGui::Separator();

    // Compact flip list - just name + profit + copy button
    for (auto& flip : g_Data.flips)
    {
        // Filter
        if (search[0] != '\0')
        {
            std::string lower = flip.name;
            std::string query = search;
            for (auto& c : lower) c = (char)tolower(c);
            for (auto& c : query) c = (char)tolower(c);
            if (lower.find(query) == std::string::npos)
                continue;
        }
        RenderFlipRow(flip, true);
    }

    ImGui::End();
}

// ============================================================================
// OPTIONS - Addon settings rendered in Nexus options panel
// ============================================================================
void RenderOptions()
{
    ImGui::Text("Mystic Trading Settings");
    ImGui::Separator();

    // Portal URL
    static char urlBuf[256];
    static bool urlInit = false;
    if (!urlInit)
    {
        strncpy(urlBuf, g_PortalUrl.c_str(), sizeof(urlBuf) - 1);
        urlInit = true;
    }
    if (ImGui::InputText("Portal URL", urlBuf, sizeof(urlBuf)))
    {
        g_PortalUrl = urlBuf;
    }

    // Refresh interval
    if (ImGui::SliderInt("Refresh (seconds)", &g_RefreshInterval, 10, 300))
    {
        // Clamped by slider
    }

    // Session cookie (masked)
    static char sessionBuf[512] = "";
    ImGui::InputText("Session Cookie", sessionBuf, sizeof(sessionBuf), ImGuiInputTextFlags_Password);
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        // Save session cookie to config file
        std::string configPath = g_ConfigPath.empty() ? "mystic-trading.cfg" : g_ConfigPath;

        // Read existing config
        std::ifstream inFile(configPath);
        std::string content;
        std::string line;
        bool foundSession = false;
        while (std::getline(inFile, line))
        {
            if (line.find("session=") == 0)
            {
                content += "session=" + std::string(sessionBuf) + "\n";
                foundSession = true;
            }
            else
            {
                content += line + "\n";
            }
        }
        inFile.close();

        if (!foundSession)
            content += "session=" + std::string(sessionBuf) + "\n";

        std::ofstream outFile(configPath);
        outFile << content;
    }

    ImGui::Separator();
    ImGui::TextDisabled("Keybinds: ALT+T = Dashboard, ALT+F = Flip Column");
    ImGui::TextDisabled("Click [C] next to any item to copy its name to clipboard.");
}
