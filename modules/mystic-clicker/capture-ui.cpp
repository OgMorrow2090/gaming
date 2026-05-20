/**
 * capture-ui.cpp
 *
 * ImGui capture mode window for Mystic Clicker.
 * Opens a window with buttons for each capture target.
 * Click a button -> 5 second countdown -> captures mouse position.
 * Window stays open throughout — shows countdown, then confirmation.
 *
 * Author: OgMorrow2090
 * Repository: https://github.com/OgMorrow2090/guildwars2
 */

#include "shared.h"
#include "capture-thumbs.h"
#include "imgui/imgui.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>
#include <string>

// Capture window state
bool g_ShowCaptureWindow = false;

// Track last-seen resolution so we can force-reload saved window state when it changes.
static int s_LastResW = 0;
static int s_LastResH = 0;

// Enables click-hold-anywhere dragging for the current window.
// Call after ImGui::Begin(). Kicks in when dragging over empty space (no item hover).
static void EnableDragAnywhere()
{
    if (ImGui::IsWindowFocused() &&
        !ImGui::IsAnyItemHovered() &&
        !ImGui::IsAnyItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (delta.x != 0.0f || delta.y != 0.0f)
        {
            ImVec2 pos = ImGui::GetWindowPos();
            ImGui::SetWindowPos(ImVec2(pos.x + delta.x, pos.y + delta.y), ImGuiCond_Always);
        }
    }
}

// Clamp a window's position to stay at least partially on-screen.
static void ClampPosToScreen(float& x, float& y, float w, float h)
{
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (GameWindow != nullptr)
    {
        RECT rect;
        if (GetClientRect(GameWindow, &rect))
        {
            screenW = rect.right - rect.left;
            screenH = rect.bottom - rect.top;
        }
    }
    // Keep at least 60px visible so user can grab the title bar even if most of the window
    // went off-screen after a resolution change.
    if (x > (float)screenW - 60.0f) x = (float)screenW - 60.0f;
    if (y > (float)screenH - 60.0f) y = (float)screenH - 60.0f;
    if (x < -w + 60.0f) x = -w + 60.0f;
    if (y < 0.0f)       y = 0.0f;
}

// Countdown state
static bool s_CountdownActive = false;
static int s_CountdownTarget = -1;
static std::chrono::steady_clock::time_point s_CountdownStart;
static constexpr int COUNTDOWN_SECONDS = 5;

// Confirmation state
static bool s_ShowConfirmation = false;
static char s_ConfirmMessage[128] = "";
static std::chrono::steady_clock::time_point s_ConfirmStart;
static constexpr int CONFIRM_DISPLAY_SECONDS = 3;

// Capture target names and their position references
struct CaptureTarget {
    const char* name;
    const char* description;
    int* posX;
    int* posY;
    const char* category;
};

// Stable slot ID derived from the display name — used as the thumbnail-file
// suffix and Nexus texture identifier. Spaces -> '_', non-alnum dropped so
// the result is filesystem and identifier safe.
//   "Accept 1"           -> "Accept_1"
//   "Bouncy Meta Complete" -> "Bouncy_Meta_Complete"
static std::string SlotIdFromName(const char* name)
{
    std::string s;
    for (const char* p = name; *p; ++p)
    {
        char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_')
            s += c;
        else if (c == ' ' || c == '-')
            s += '_';
        // everything else dropped
    }
    return s;
}

static CaptureTarget s_Targets[] = {
    // Mystic Forge
    {"Mystic Forge",      "Forge throw button",           &g_MysticForgeX,  &g_MysticForgeY,  "Mystic Forge"},
    {"Mystic Refill",     "Forge refill button",          &g_MysticRefillX, &g_MysticRefillY, "Mystic Forge"},

    // Trading Post
    {"TP Collect",        "TP collect button",            &g_TradingPostX,  &g_TradingPostY,  "Trading Post"},
    {"TP Buy/Sell",       "Trading Post buy/sell button", &g_TpBuySellX,    &g_TpBuySellY,    "Trading Post"},
    {"TP Cancel Listing", "Cancel TP listing",            &g_TpRemoveX,     &g_TpRemoveY,     "Trading Post"},
    {"TP Portable Icon",  "Portable Trading Post icon in inventory (combo double-click target)", &g_TradingPostIconX, &g_TradingPostIconY, "Trading Post"},

    // Bank & Inventory
    {"Deposit Materials", "Inventory deposit button",     &g_DepositX,      &g_DepositY,      "Bank & Inventory"},
    {"Sort/Compact",      "Inventory sort button",        &g_SortX,         &g_SortY,         "Bank & Inventory"},
    {"Bank Portable Icon",     "Portable Bank icon in inventory (combo double-click target)", &g_BankIconX, &g_BankIconY, "Bank & Inventory"},
    {"Merchant Portable Icon", "Portable Merchant item in inventory (R1+DPad Right Double)",  &g_MerchantX, &g_MerchantY, "Bank & Inventory"},

    // Crafting
    {"Single Craft",      "Craft single item button",     &g_CraftX,        &g_CraftY,        "Crafting"},
    {"Craft All",         "Craft all items button",       &g_CraftAllX,     &g_CraftAllY,     "Crafting"},
    {"Craft Filter",      "Crafting filter button (combo 1st click)", &g_CraftFilterX, &g_CraftFilterY,   "Crafting"},
    {"Craft Collapse All","Crafting collapse-all button (combo 2nd click)", &g_CraftCollapseX, &g_CraftCollapseY, "Crafting"},
    {"Close Crafting",    "Close button on Crafting window", &g_CraftCloseX, &g_CraftCloseY,   "Crafting"},

    // Mail
    {"Mail Take All",     "Take All button in Mail panel (combo 2nd click)", &g_MailTakeAllX, &g_MailTakeAllY, "Mail"},

    // Wizard Vault
    {"Wizard Vault Collect",     "Wizard Vault collect button",  &g_WizardVaultX,  &g_WizardVaultY,  "Wizard Vault"},
    {"Wizard Vault Confirm",     "Wizard Vault Complete button (combo 2nd click)", &g_WizardVaultCompleteX, &g_WizardVaultCompleteY, "Wizard Vault"},
    {"Wizard Vault Daily Tab",   "WV Daily tab button (combo ensures Daily selected before clicks)", &g_WizardVaultDailyTabX, &g_WizardVaultDailyTabY,   "Wizard Vault"},
    {"Wizard Vault Weekly Tab",  "WV Weekly tab button (combo switches tab after daily)",            &g_WizardVaultWeeklyTabX, &g_WizardVaultWeeklyTabY, "Wizard Vault"},
    {"Wizard Vault Special Tab", "WV Special tab button (combo runs same flow after weekly)",        &g_WizardVaultSpecialTabX, &g_WizardVaultSpecialTabY, "Wizard Vault"},

    // Wizard Items
    {"Wizard Gobbler",      "Wizard Gobbler icon in inventory (R1+DPad Left Full)",       &g_WizardGobblerX,      &g_WizardGobblerY,      "Wizard Items"},
    {"Wizard Portal Scroll","Wizard Portal Scroll icon in inventory (R1+DPad Left Hold)", &g_WizardPortalScrollX, &g_WizardPortalScrollY, "Wizard Items"},

    // Travel
    {"Lounge Pass",          "Lounge Pass icon in inventory (R1+DPad Left Double)",                                       &g_LoungePassX,    &g_LoungePassY,    "Travel"},
    {"Teleport to Friend",   "Portable 'Teleport to Friend' icon in inventory (combo double-click target)",               &g_TeleportFriendX, &g_TeleportFriendY, "Travel"},
    {"Chat Waypoint (1st)",  "Pinged waypoint link in chat — single-click opens map (R1+DPad Down Full combo 1st)",       &g_ChatWaypointX,  &g_ChatWaypointY,  "Travel"},
    {"Map Waypoint (2nd)",   "Waypoint on opened map — double-click to travel (R1+DPad Down Full combo 2nd)",             &g_MapWaypointX,   &g_MapWaypointY,   "Travel"},

    // Bouncy Chest
    {"Bouncy Open",         "Right-click bouncy chest (combo step 1)",                                                       &g_ChestX,             &g_ChestY,             "Bouncy Chest"},
    {"Bouncy Accept",       "Click Accept dialog for bouncy chest (combo step 2)",                                           &g_BouncyAcceptX,      &g_BouncyAcceptY,      "Bouncy Chest"},
    {"Bouncy Meta Progress","Meta progress dialog — between Accept and Meta Complete (combo step 3)",                       &g_BouncyMetaProgressX,&g_BouncyMetaProgressY,"Bouncy Chest"},
    {"Bouncy Meta Complete","Meta completion dialog — slight offset from Bouncy Accept (combo step 4)",                     &g_BouncyMetaCompleteX,&g_BouncyMetaCompleteY,"Bouncy Chest"},

    // Generic Accept (numbered slots fire in sequence in SimulateGeneralAcceptCombo)
    {"Accept 1",             "Accept combo slot 1",                           &g_AcceptX,         &g_AcceptY,         "Generic Accept"},
    {"Accept 2",             "Accept combo slot 2",                           &g_YesDialogX,      &g_YesDialogY,      "Generic Accept"},
    {"Accept 3",             "Accept combo slot 3",                           &g_GeneralAcceptX,  &g_GeneralAcceptY,  "Generic Accept"},
    {"Accept 4",             "Accept combo slot 4",                           &g_GeneralAccept2X, &g_GeneralAccept2Y, "Generic Accept"},
    {"Accept 5",             "Accept combo slot 5",                           &g_GeneralAccept3X, &g_GeneralAccept3Y, "Generic Accept"},
    {"Accept 6",             "Accept combo slot 6",                           &g_GeneralAccept4X, &g_GeneralAccept4Y, "Generic Accept"},
    {"Accept 7",             "Accept combo slot 7",                           &g_Accept7X,        &g_Accept7Y,        "Generic Accept"},
    {"Accept 8",             "Accept combo slot 8",                           &g_Accept8X,        &g_Accept8Y,        "Generic Accept"},
    {"Accept 9",             "Accept combo slot 9",                           &g_Accept9X,        &g_Accept9Y,        "Generic Accept"},
    {"Accept 10",            "Accept combo slot 10",                          &g_Accept10X,       &g_Accept10Y,       "Generic Accept"},
    {"Accept 11",            "Accept combo slot 11",                          &g_Accept11X,       &g_Accept11Y,       "Generic Accept"},
    {"Accept 12",            "Accept combo slot 12",                          &g_Accept12X,       &g_Accept12Y,       "Generic Accept"},
    {"Accept 13",            "Accept combo slot 13",                          &g_Accept13X,       &g_Accept13Y,       "Generic Accept"},
    {"Accept 14",            "Accept combo slot 14",                          &g_Accept14X,       &g_Accept14Y,       "Generic Accept"},
    {"Accept 15 (Yes)",      "Accept combo slot 15 — final Yes, fires LAST in Accept Combo", &g_Accept15X, &g_Accept15Y, "Generic Accept"},
    {"Accept 16",            "Accept combo slot 16",                          &g_Accept16X,       &g_Accept16Y,       "Generic Accept"},
    {"Accept 17",            "Accept combo slot 17",                          &g_Accept17X,       &g_Accept17Y,       "Generic Accept"},
    {"Accept 18",            "Accept combo slot 18",                          &g_Accept18X,       &g_Accept18Y,       "Generic Accept"},
    {"Accept 19",            "Accept combo slot 19",                          &g_Accept19X,       &g_Accept19Y,       "Generic Accept"},
    {"Accept 20",            "Accept combo slot 20",                          &g_Accept20X,       &g_Accept20Y,       "Generic Accept"},

    // Misc
    {"Vendor Buy",            "Buy from vendor",            &g_VendorX,       &g_VendorY,       "Misc"},
    {"Sell Junk",             "Sell junk to vendor",        &g_SellJunkX,     &g_SellJunkY,     "Misc"},
    {"LFG Search Tab",        "Search tab inside LFG panel (combo 2nd click)", &g_LfgSearchX, &g_LfgSearchY, "Misc"},
    {"Character Swap",        "Char Swap addon (overlay)",  &g_CharSwapX,     &g_CharSwapY,     "Misc"},
    {"Exit Instance",         "Leave instance button",      &g_ExitInstanceX, &g_ExitInstanceY, "Misc"},
    {"Guild Hall (in panel)", "Guild Hall button in Guild panel", &g_GuildHallX, &g_GuildHallY,  "Misc"},
    {"Exit Game",             "Exit to Character Select button in in-game menu (Esc menu) — used by Graceful Quit", &g_ExitGameX, &g_ExitGameY, "Misc"},
};
static constexpr int NUM_TARGETS = sizeof(s_Targets) / sizeof(s_Targets[0]);

// Display order for category headers. Anything in s_Targets with a category
// not in this list is silently dropped — keep them in sync.
static const char* const s_Categories[] = {
    "Mystic Forge",
    "Trading Post",
    "Bank & Inventory",
    "Crafting",
    "Mail",
    "Wizard Vault",
    "Wizard Items",
    "Travel",
    "Bouncy Chest",
    "Generic Accept",
    "Misc",
};
static constexpr int NUM_CATEGORIES = sizeof(s_Categories) / sizeof(s_Categories[0]);

// A distinct tint per category, index-aligned with s_Categories above — keep
// the two arrays the same length and order. Each colour tints that category's
// collapsing header (and, faintly, its idle capture buttons) so the categories
// separate at a glance.
static const ImVec4 s_CategoryColors[NUM_CATEGORIES] = {
    ImVec4(0.62f, 0.45f, 0.85f, 1.0f),  // Mystic Forge     - violet
    ImVec4(0.85f, 0.67f, 0.28f, 1.0f),  // Trading Post     - amber
    ImVec4(0.36f, 0.58f, 0.88f, 1.0f),  // Bank & Inventory - blue
    ImVec4(0.88f, 0.52f, 0.27f, 1.0f),  // Crafting         - orange
    ImVec4(0.30f, 0.72f, 0.70f, 1.0f),  // Mail             - teal
    ImVec4(0.86f, 0.42f, 0.68f, 1.0f),  // Wizard Vault     - magenta
    ImVec4(0.50f, 0.50f, 0.86f, 1.0f),  // Wizard Items     - indigo
    ImVec4(0.42f, 0.74f, 0.42f, 1.0f),  // Travel           - green
    ImVec4(0.78f, 0.60f, 0.40f, 1.0f),  // Bouncy Chest     - tan
    ImVec4(0.50f, 0.60f, 0.70f, 1.0f),  // Generic Accept   - slate
    ImVec4(0.58f, 0.58f, 0.60f, 1.0f),  // Misc             - grey
};

/**
 * PerformCapture - Called when countdown reaches zero
 */
static void PerformCapture(int targetIdx)
{
    if (targetIdx < 0 || targetIdx >= NUM_TARGETS) return;

    CaptureTarget& target = s_Targets[targetIdx];

    POINT pt;
    if (GetCursorPos(&pt))
    {
        if (GameWindow != nullptr)
            ScreenToClient(GameWindow, &pt);

        *target.posX = pt.x;
        *target.posY = pt.y;

        sprintf_s(s_ConfirmMessage, "%s captured at (%d, %d)", target.name, pt.x, pt.y);
        APIDefs->Log(LOGL_INFO, "MysticClicker", s_ConfirmMessage);

        SaveButtonPositions();

        // Grab a 128x128 thumbnail of the back buffer around the click point
        // so the capture window can show it on hover — visual reminder of
        // which dialog this slot was used for. Detached worker; see
        // capture-thumbs.cpp.
        Thumbs::Capture(SlotIdFromName(target.name).c_str(), pt.x, pt.y);
    }
    else
    {
        sprintf_s(s_ConfirmMessage, "Failed to capture %s!", target.name);
    }

    s_CountdownActive = false;
    s_CountdownTarget = -1;
    s_ShowConfirmation = true;
    s_ConfirmStart = std::chrono::steady_clock::now();
}

/**
 * RenderCaptureWindow - ImGui render callback
 */
void RenderCaptureWindow()
{
    // Global window rescue — runs even when the capture panel is hidden so
    // Ctrl+Shift+Home pulls every Nexus addon window back on-screen, not just
    // ours. This is the cross-addon fix for windows that drift off-screen
    // after a 4K → Deck resolution swap.
    if (g_ResetWindowsFlag)
    {
        RescueAllImGuiWindows();
        // Mystic Trading and the per-window MC capture state still consume
        // the flag — leave it set; capture-ui's own block (further down) and
        // MT's render path each clear after applying their own state.
    }

    if (!g_ShowCaptureWindow) return;

    // Auto-hide confirmation after 3 seconds
    if (s_ShowConfirmation)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - s_ConfirmStart).count();
        if (elapsed >= CONFIRM_DISPLAY_SECONDS)
            s_ShowConfirmation = false;
    }

    // Per-resolution window position/size management.
    // CheckResolutionChange() is called from the keybind path, but the render loop
    // may run before any keybind press — so detect res change here too.
    int curW, curH;
    if (GameWindow != nullptr)
    {
        RECT r;
        if (GetClientRect(GameWindow, &r)) { curW = r.right - r.left; curH = r.bottom - r.top; }
        else { curW = GetSystemMetrics(SM_CXSCREEN); curH = GetSystemMetrics(SM_CYSCREEN); }
    }
    else
    {
        curW = GetSystemMetrics(SM_CXSCREEN);
        curH = GetSystemMetrics(SM_CYSCREEN);
    }
    bool resChanged = (curW != s_LastResW || curH != s_LastResH);
    if (resChanged)
    {
        CheckResolutionChange();
        s_LastResW = curW;
        s_LastResH = curH;
    }

    // Rescue: Ctrl+Shift+Home moves the window to a guaranteed-visible spot.
    if (g_ResetWindowsFlag)
    {
        g_CaptureWinX = 100.0f;
        g_CaptureWinY = 100.0f;
        g_CaptureWinW = 450.0f * g_ButtonScale;
        g_CaptureWinH = 600.0f * g_ButtonScale;
        g_ResetWindowsFlag = false;
        SaveButtonPositions();
    }

    // Apply saved pos/size. Use Always on resolution change or rescue, FirstUseEver otherwise.
    ImGuiCond posCond = (g_CaptureWinX != 0.0f || g_CaptureWinY != 0.0f) ? ImGuiCond_FirstUseEver : ImGuiCond_FirstUseEver;
    if (resChanged && (g_CaptureWinX != 0.0f || g_CaptureWinY != 0.0f))
    {
        ClampPosToScreen(g_CaptureWinX, g_CaptureWinY, g_CaptureWinW, g_CaptureWinH);
        posCond = ImGuiCond_Always;
    }

    if (g_CaptureWinX != 0.0f || g_CaptureWinY != 0.0f)
        ImGui::SetNextWindowPos(ImVec2(g_CaptureWinX, g_CaptureWinY), posCond);
    if (g_CaptureWinW > 0.0f && g_CaptureWinH > 0.0f)
        ImGui::SetNextWindowSize(ImVec2(g_CaptureWinW, g_CaptureWinH), posCond);
    else
        ImGui::SetNextWindowSize(ImVec2(450.0f * g_ButtonScale, 0), ImGuiCond_FirstUseEver);

    ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f * g_ButtonScale, 200.0f), ImVec2(10000.0f, 10000.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * g_ButtonScale, 4.0f * g_ButtonScale));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f * g_ButtonScale, 3.0f * g_ButtonScale));
    ImGui::SetNextWindowBgAlpha(g_WindowOpacity);
    if (ImGui::Begin("Mystic Clicker - Capture", &g_ShowCaptureWindow, 0))
    {
        ImGui::SetWindowFontScale(g_FontScale * 0.85f);
        EnableDragAnywhere();

        // Show countdown banner if active
        if (s_CountdownActive)
        {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - s_CountdownStart).count();
            int remaining = COUNTDOWN_SECONDS - (int)elapsed;

            if (remaining <= 0)
            {
                PerformCapture(s_CountdownTarget);
            }
            else
            {
                // Countdown banner at top of window
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.8f, 0.5f, 0.0f, 0.9f));
                ImGui::BeginChild("##countdown_banner", ImVec2(-1, 60.0f * g_ButtonScale), true);

                char countText[64];
                sprintf_s(countText, "Capturing: %s", s_Targets[s_CountdownTarget].name);
                ImGui::Text("%s", countText);

                ImGui::SetWindowFontScale(2.0f * g_FontScale * 0.85f);
                ImGui::SameLine(ImGui::GetWindowWidth() - 60.0f * g_ButtonScale);
                ImGui::Text("%d", remaining);
                ImGui::SetWindowFontScale(g_FontScale * 0.85f);

                ImGui::Text("Move cursor to target position...");

                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::Spacing();

                if (ImGui::Button("Cancel Capture", ImVec2(-1, 25.0f * g_ButtonScale)))
                {
                    s_CountdownActive = false;
                    s_CountdownTarget = -1;
                }

                ImGui::Separator();
                ImGui::Spacing();
            }
        }

        // Show confirmation banner if just captured
        if (s_ShowConfirmation && !s_CountdownActive)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.6f, 0.0f, 0.9f));
            ImGui::BeginChild("##confirm_banner", ImVec2(-1, 35.0f * g_ButtonScale), true);
            ImGui::Text("%s", s_ConfirmMessage);
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // Instructions + Close at top (always visible, never clipped by scroll)
        if (!s_CountdownActive)
        {
            if (ImGui::Button("Close Panel", ImVec2(-1, 25.0f * g_ButtonScale)))
            {
                g_ShowCaptureWindow = false;
                s_CountdownActive = false;
                s_CountdownTarget = -1;
            }
            ImGui::Text("Click target, move cursor in %ds.", COUNTDOWN_SECONDS);
            ImGui::Separator();
            ImGui::Spacing();
        }

        for (int c = 0; c < NUM_CATEGORIES; ++c)
        {
            const char*  category = s_Categories[c];
            const ImVec4 cc       = s_CategoryColors[c];

            // Collect indices of targets in this category.
            int catIndices[NUM_TARGETS];
            int catCount = 0;
            for (int i = 0; i < NUM_TARGETS; ++i)
            {
                if (strcmp(s_Targets[i].category, category) == 0)
                    catIndices[catCount++] = i;
            }
            if (catCount == 0) continue;

            // Sort within category: uncaptured first, then alphabetic by name.
            for (int i = 1; i < catCount; ++i)
            {
                int cur = catIndices[i];
                bool curSet = (*s_Targets[cur].posX != 0 || *s_Targets[cur].posY != 0);
                int j = i - 1;
                while (j >= 0)
                {
                    int prev = catIndices[j];
                    bool prevSet = (*s_Targets[prev].posX != 0 || *s_Targets[prev].posY != 0);
                    bool swap = false;
                    if (!curSet && prevSet) swap = true;
                    else if (curSet == prevSet && strcmp(s_Targets[cur].name, s_Targets[prev].name) < 0) swap = true;
                    if (!swap) break;
                    catIndices[j + 1] = prev;
                    --j;
                }
                catIndices[j + 1] = cur;
            }

            // Count uncaptured for the header label.
            int unsetCount = 0;
            bool catHasActive = false;
            for (int i = 0; i < catCount; ++i)
            {
                int idx = catIndices[i];
                if (*s_Targets[idx].posX == 0 && *s_Targets[idx].posY == 0) ++unsetCount;
                if (s_CountdownActive && s_CountdownTarget == idx) catHasActive = true;
            }

            // Header label. The "###cat_%d" suffix gives the header a stable ImGui ID
            // independent of the visible text, so the unset-count change doesn't reset
            // the open/closed state.
            char headerLabel[128];
            if (unsetCount > 0)
                sprintf_s(headerLabel, "%s  (%d unset)###cat_%d", category, unsetCount, c);
            else
                sprintf_s(headerLabel, "%s  (all set)###cat_%d", category, c);

            // Auto-expand the category containing an active countdown so the
            // highlighted button stays visible during the 5s wait.
            if (catHasActive) ImGui::SetNextItemOpen(true);

            // Tint the header with the category's colour so categories
            // separate at a glance.
            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(cc.x, cc.y, cc.z, 0.55f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(cc.x, cc.y, cc.z, 0.78f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(cc.x, cc.y, cc.z, 0.92f));
            bool catOpen = ImGui::CollapsingHeader(headerLabel);
            ImGui::PopStyleColor(3);
            if (!catOpen) continue;

            for (int i = 0; i < catCount; ++i)
            {
                int idx = catIndices[i];
                CaptureTarget& t = s_Targets[idx];

                char label[128];
                if (*t.posX != 0 || *t.posY != 0)
                    sprintf_s(label, "%s (%d,%d)", t.name, *t.posX, *t.posY);
                else
                    sprintf_s(label, "%s (-)", t.name);

                float btnH = 30.0f * g_ButtonScale;
                if (s_CountdownActive && s_CountdownTarget == idx)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.5f, 0.0f, 0.9f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.5f, 0.0f, 0.9f));
                    ImGui::Button(label, ImVec2(-1, btnH));
                    ImGui::PopStyleColor(2);
                }
                else if (s_CountdownActive)
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
                    ImGui::Button(label, ImVec2(-1, btnH));
                    ImGui::PopStyleVar();
                }
                else
                {
                    // Faint category tint so an expanded category's buttons
                    // still read as one group.
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(cc.x, cc.y, cc.z, 0.20f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(cc.x, cc.y, cc.z, 0.42f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(cc.x, cc.y, cc.z, 0.60f));
                    bool pressed = ImGui::Button(label, ImVec2(-1, btnH));
                    ImGui::PopStyleColor(3);
                    if (pressed)
                    {
                        s_CountdownActive = true;
                        s_CountdownTarget = idx;
                        s_CountdownStart = std::chrono::steady_clock::now();
                        s_ShowConfirmation = false;

                        char logBuf[128];
                        sprintf_s(logBuf, "Starting %ds countdown for %s", COUNTDOWN_SECONDS, t.name);
                        APIDefs->Log(LOGL_INFO, "MysticClicker", logBuf);
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && (*t.posX != 0 || *t.posY != 0))
                    {
                        *t.posX = 0;
                        *t.posY = 0;
                        SaveButtonPositions();
                        sprintf_s(s_ConfirmMessage, "%s cleared", t.name);
                        APIDefs->Log(LOGL_INFO, "MysticClicker", s_ConfirmMessage);
                        s_ShowConfirmation = true;
                        s_ConfirmStart = std::chrono::steady_clock::now();
                    }

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(t.description);
                        ImGui::TextDisabled("(right-click to clear)");

                        // Show the captured thumbnail with a small red + at
                        // the centre — that centre is the click point, give
                        // or take edge-of-screen clamping. Only shown for
                        // captured slots; uncaptured slots have no file yet.
                        bool isSet = (*t.posX != 0 || *t.posY != 0);
                        if (isSet)
                        {
                            std::string sid = SlotIdFromName(t.name);
                            Texture_t* tex  = Thumbs::Get(sid.c_str());
                            if (tex && tex->Resource && tex->Width > 0)
                            {
                                ImGui::Spacing();
                                // Display at 2x where it fits, but shrink to
                                // stay within ~70% wide / 60% tall of the game
                                // window — keeps the tooltip readable on a 4K
                                // bazzite display and prevents 1536-wide
                                // overflow on the 1280-wide Deck.
                                ImGuiIO& io = ImGui::GetIO();
                                float scale = 2.0f;
                                float maxW  = io.DisplaySize.x * 0.70f;
                                float maxH  = io.DisplaySize.y * 0.60f;
                                if ((float)tex->Width  * scale > maxW)
                                    scale = maxW / (float)tex->Width;
                                if ((float)tex->Height * scale > maxH)
                                    scale = maxH / (float)tex->Height;
                                if (scale < 0.5f) scale = 0.5f;
                                ImVec2 sz((float)tex->Width  * scale,
                                          (float)tex->Height * scale);
                                ImVec2 p0 = ImGui::GetCursorScreenPos();
                                ImGui::Image((ImTextureID)tex->Resource, sz);

                                // Fat filled red dot at the captured click
                                // point with a white outline for contrast —
                                // visible against any GW2 dialog backdrop. The
                                // dot marks the cursor position at capture
                                // time, which is the location the click will
                                // fire at — not necessarily the button's
                                // geometric centre.
                                ImDrawList* dl = ImGui::GetWindowDrawList();
                                ImVec2 c(p0.x + sz.x * 0.5f, p0.y + sz.y * 0.5f);
                                const float radius   = 22.0f;
                                const ImU32 colFill  = IM_COL32(255,  60,  60, 240);
                                const ImU32 colRing  = IM_COL32(255, 255, 255, 230);
                                dl->AddCircleFilled(c, radius, colFill, 32);
                                dl->AddCircle      (c, radius, colRing, 32, 2.0f);
                            }
                            else
                            {
                                // Either captured before this version of the
                                // addon (no file on disk yet) or Nexus is
                                // still loading. The note is mostly the
                                // former — telling the player to recapture.
                                ImGui::Spacing();
                                ImGui::TextDisabled("(recapture to refresh the thumbnail)");
                            }
                        }
                        ImGui::EndTooltip();
                    }
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Settings###settings"))
        {
            ImGui::Text("Text size");
            float prevFontScale = g_FontScale;
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##fontscale", &g_FontScale, 0.6f, 2.5f, "%.2f");
            if (g_FontScale != prevFontScale)
            {
                SaveButtonPositions();
            }

            ImGui::Text("Button size");
            float prevButtonScale = g_ButtonScale;
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##buttonscale", &g_ButtonScale, 0.6f, 2.5f, "%.2f");
            if (g_ButtonScale != prevButtonScale)
            {
                SaveButtonPositions();
            }

            ImGui::Text("Opacity");
            float prevOpacity = g_WindowOpacity;
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##windowopacity", &g_WindowOpacity, 0.2f, 1.0f, "%.2f");
            if (g_WindowOpacity != prevOpacity)
            {
                SaveButtonPositions();
            }

            // Clean slate: zero every slot's X/Y, delete every saved
            // thumbnail BMP, save the config. Two-step via a modal popup so
            // it's not triggered by a stray click.
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.20f, 0.20f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.25f, 0.25f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.80f, 0.30f, 0.30f, 1.00f));
            if (ImGui::Button("Clear all captures...",
                              ImVec2(-1, 30.0f * g_ButtonScale)))
                ImGui::OpenPopup("ClearAllConfirm");
            ImGui::PopStyleColor(3);

            {
                // Vendored ImGui in this tree doesn't expose GetMainViewport;
                // derive the screen centre from GetIO().DisplaySize instead.
                ImGuiIO& io2 = ImGui::GetIO();
                ImGui::SetNextWindowPos(
                    ImVec2(io2.DisplaySize.x * 0.5f, io2.DisplaySize.y * 0.5f),
                    ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            }
            if (ImGui::BeginPopupModal("ClearAllConfirm", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted(
                    "Clear all captured positions and thumbnails?");
                ImGui::TextDisabled("Cannot be undone — every slot returns "
                                    "to the unset state.");
                ImGui::Spacing();
                if (ImGui::Button("Yes, clear all", ImVec2(160.0f, 0)))
                {
                    for (int i = 0; i < NUM_TARGETS; ++i)
                    {
                        *s_Targets[i].posX = 0;
                        *s_Targets[i].posY = 0;
                    }
                    SaveButtonPositions();
                    Thumbs::DeleteAll();

                    sprintf_s(s_ConfirmMessage,
                              "Cleared all %d capture slots and thumbnails",
                              NUM_TARGETS);
                    APIDefs->Log(LOGL_INFO, "MysticClicker", s_ConfirmMessage);
                    s_ShowConfirmation = true;
                    s_ConfirmStart = std::chrono::steady_clock::now();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120.0f, 0)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }

        // Persist position/size if moved/resized. Debounce to 1 second after
        // the last change to avoid disk-thrashing during a drag.
        ImVec2 curPos  = ImGui::GetWindowPos();
        ImVec2 curSize = ImGui::GetWindowSize();
        const float EPS = 0.5f;
        static double s_LastChangeTime = 0.0;
        static bool s_SavePending = false;
        if (fabsf(curPos.x  - g_CaptureWinX) > EPS ||
            fabsf(curPos.y  - g_CaptureWinY) > EPS ||
            fabsf(curSize.x - g_CaptureWinW) > EPS ||
            fabsf(curSize.y - g_CaptureWinH) > EPS)
        {
            g_CaptureWinX = curPos.x;
            g_CaptureWinY = curPos.y;
            g_CaptureWinW = curSize.x;
            g_CaptureWinH = curSize.y;
            s_LastChangeTime = ImGui::GetTime();
            s_SavePending = true;
        }
        if (s_SavePending && (ImGui::GetTime() - s_LastChangeTime) > 1.0)
        {
            SaveButtonPositions();
            s_SavePending = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void ResetWindowPositions()
{
    g_ResetWindowsFlag = true;
}

/**
 * ToggleCaptureWindow - Keybind handler
 */
void ToggleCaptureWindow()
{
    g_ShowCaptureWindow = !g_ShowCaptureWindow;
    s_CountdownActive = false;
    s_CountdownTarget = -1;
    s_ShowConfirmation = false;
}
