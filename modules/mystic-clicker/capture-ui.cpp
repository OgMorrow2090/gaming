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
#include "imgui/imgui.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>

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
};

static CaptureTarget s_Targets[] = {
    {"Deposit Materials", "Inventory deposit button",     &g_DepositX,      &g_DepositY},
    {"Sort/Compact",      "Inventory sort button",        &g_SortX,         &g_SortY},
    {"Bouncy Open",         "Right-click bouncy chest (combo step 1)", &g_ChestX, &g_ChestY},
    {"Bouncy Accept",       "Click Accept dialog for bouncy chest (combo step 2)", &g_BouncyAcceptX, &g_BouncyAcceptY},
    {"Bouncy Meta Complete","Meta completion dialog — slight offset from Bouncy Accept (combo step 3)", &g_BouncyMetaCompleteX, &g_BouncyMetaCompleteY},
    {"Exit Instance",     "Leave instance button",        &g_ExitInstanceX, &g_ExitInstanceY},
    {"Accept 2",          "Accept combo slot 2 (originally Yes Dialog)", &g_YesDialogX, &g_YesDialogY},
    {"Mystic Forge",      "Forge throw button",           &g_MysticForgeX,  &g_MysticForgeY},
    {"Mystic Refill",     "Forge refill button",          &g_MysticRefillX, &g_MysticRefillY},
    {"Vendor Buy",        "Buy from vendor",              &g_VendorX,       &g_VendorY},
    {"Sell Junk",         "Sell junk to vendor",          &g_SellJunkX,     &g_SellJunkY},
    {"Trading Post",      "TP collect button",            &g_TradingPostX,  &g_TradingPostY},
    {"TP Remove",         "Cancel TP listing",            &g_TpRemoveX,     &g_TpRemoveY},
    {"Craft",             "Craft single item button",     &g_CraftX,        &g_CraftY},
    {"Craft All",         "Craft all items button",       &g_CraftAllX,     &g_CraftAllY},
    {"Wizard Vault",      "Wizard Vault collect button",  &g_WizardVaultX,  &g_WizardVaultY},
    {"Char Swap",         "Char Swap addon (overlay)",    &g_CharSwapX,     &g_CharSwapY},
    {"TP Buy/Sell",       "Trading Post buy/sell button", &g_TpBuySellX,    &g_TpBuySellY},
    {"Wizard Vault Complete", "Wizard Vault Complete button (combo 2nd click)", &g_WizardVaultCompleteX, &g_WizardVaultCompleteY},
    {"Accept 1",          "Accept combo slot 1 (originally Chest Accept)", &g_AcceptX,       &g_AcceptY},
    {"Accept 3",          "Accept combo slot 3", &g_GeneralAcceptX, &g_GeneralAcceptY},
    {"Accept 4",          "Accept combo slot 4", &g_GeneralAccept2X, &g_GeneralAccept2Y},
    {"Accept 5",          "Accept combo slot 5", &g_GeneralAccept3X, &g_GeneralAccept3Y},
    {"Accept 6",          "Accept combo slot 6", &g_GeneralAccept4X, &g_GeneralAccept4Y},
    {"Accept 7",          "Accept combo slot 7", &g_Accept7X,      &g_Accept7Y},
    {"Accept 8",          "Accept combo slot 8", &g_Accept8X,      &g_Accept8Y},
    {"Accept 9",          "Accept combo slot 9", &g_Accept9X,      &g_Accept9Y},
    {"Accept 10",         "Accept combo slot 10", &g_Accept10X,    &g_Accept10Y},
    {"Accept 11",         "Accept combo slot 11", &g_Accept11X,    &g_Accept11Y},
    {"Accept 12",         "Accept combo slot 12", &g_Accept12X,    &g_Accept12Y},
    {"Accept 13",         "Accept combo slot 13", &g_Accept13X,    &g_Accept13Y},
    {"Accept 14",         "Accept combo slot 14", &g_Accept14X,    &g_Accept14Y},
    {"Accept Yes (Final)","Final Yes click — fires LAST in Accept Combo (post-confirmation prompt)", &g_Accept15X, &g_Accept15Y},
    {"Accept 16",         "Accept combo slot 16", &g_Accept16X,    &g_Accept16Y},
    {"Accept 17",         "Accept combo slot 17", &g_Accept17X,    &g_Accept17Y},
    {"Accept 18",         "Accept combo slot 18", &g_Accept18X,    &g_Accept18Y},
    {"Accept 19",         "Accept combo slot 19", &g_Accept19X,    &g_Accept19Y},
    {"Accept 20",         "Accept combo slot 20", &g_Accept20X,    &g_Accept20Y},
    {"Merchant",           "Portable Merchant item in inventory (R1+DPad Right Double)", &g_MerchantX, &g_MerchantY},
    {"Wizard Vault Daily Tab",  "WV Daily tab button (combo ensures Daily selected before clicks)", &g_WizardVaultDailyTabX, &g_WizardVaultDailyTabY},
    {"Wizard Vault Weekly Tab", "WV Weekly tab button (combo switches tab after daily)", &g_WizardVaultWeeklyTabX, &g_WizardVaultWeeklyTabY},
    {"Wizard Vault Special Tab", "WV Special tab button (combo runs same flow after weekly)", &g_WizardVaultSpecialTabX, &g_WizardVaultSpecialTabY},
    {"LFG Search",        "Search tab inside LFG panel (combo 2nd click)", &g_LfgSearchX, &g_LfgSearchY},
    {"Teleport to Friend", "Portable 'Teleport to Friend' icon in inventory (combo double-click target)", &g_TeleportFriendX, &g_TeleportFriendY},
    {"Trading Post Icon",  "Portable Trading Post icon in inventory (combo double-click target)", &g_TradingPostIconX, &g_TradingPostIconY},
    {"Bank Icon",          "Portable Bank icon in inventory (combo double-click target)", &g_BankIconX, &g_BankIconY},
    {"Wizard Gobbler",     "Wizard Gobbler icon in inventory (R1+DPad Left Full)", &g_WizardGobblerX, &g_WizardGobblerY},
    {"Wizard Portal Scroll","Wizard Portal Scroll icon in inventory (R1+DPad Left Hold)", &g_WizardPortalScrollX, &g_WizardPortalScrollY},
    {"Lounge Pass",        "Lounge Pass icon in inventory (R1+DPad Left Double)", &g_LoungePassX, &g_LoungePassY},
    {"Chat Waypoint",      "Pinged waypoint link in chat — single-click opens map (R1+DPad Down Full combo 1st)", &g_ChatWaypointX, &g_ChatWaypointY},
    {"Map Waypoint",       "Waypoint on opened map — double-click to travel (R1+DPad Down Full combo 2nd)", &g_MapWaypointX, &g_MapWaypointY},
    {"Party/Squad Bar",    "Party/Squad member bar — right-click target (R1+DPad Down Hold combo 1st)", &g_PartySquadBarX, &g_PartySquadBarY},
    {"Leave Party Button", "Leave Party/Squad button in right-click menu (R1+DPad Down Hold combo 2nd)", &g_LeavePartyX, &g_LeavePartyY},
    {"Mail Take All",     "Take All button in Mail panel (combo 2nd click)", &g_MailTakeAllX, &g_MailTakeAllY},
    {"Craft Filter",      "Crafting filter button (combo 1st click)", &g_CraftFilterX, &g_CraftFilterY},
    {"Craft Collapse",    "Crafting collapse-all button (combo 2nd click)", &g_CraftCollapseX, &g_CraftCollapseY},
    {"Craft Close",       "Close button on Crafting window", &g_CraftCloseX, &g_CraftCloseY},
    {"Guild Hall",        "Guild Hall button in Guild panel", &g_GuildHallX,    &g_GuildHallY},
};
static constexpr int NUM_TARGETS = sizeof(s_Targets) / sizeof(s_Targets[0]);

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
        g_CaptureWinW = 380.0f;
        g_CaptureWinH = 600.0f;
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
        ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Mystic Clicker - Capture", &g_ShowCaptureWindow, 0))
    {
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
                ImGui::BeginChild("##countdown_banner", ImVec2(-1, 60), true);

                char countText[64];
                sprintf_s(countText, "Capturing: %s", s_Targets[s_CountdownTarget].name);
                ImGui::Text("%s", countText);

                ImGui::SetWindowFontScale(2.0f);
                ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                ImGui::Text("%d", remaining);
                ImGui::SetWindowFontScale(1.0f);

                ImGui::Text("Move cursor to target position...");

                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::Spacing();

                if (ImGui::Button("Cancel Capture", ImVec2(-1, 25)))
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
            ImGui::BeginChild("##confirm_banner", ImVec2(-1, 35), true);
            ImGui::Text("%s", s_ConfirmMessage);
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // Instructions
        if (!s_CountdownActive)
        {
            ImGui::Text("Click target, move cursor in %ds.", COUNTDOWN_SECONDS);
            ImGui::Separator();
            ImGui::Spacing();
        }

        // Build sorted display order: uncaptured first, then captured; within each
        // group A-Z by name. Rebuilt every frame — NUM_TARGETS is small so the
        // insertion sort cost is negligible.
        int order[NUM_TARGETS];
        for (int i = 0; i < NUM_TARGETS; ++i) order[i] = i;
        for (int i = 1; i < NUM_TARGETS; ++i)
        {
            int cur = order[i];
            bool curSet = (*s_Targets[cur].posX != 0 || *s_Targets[cur].posY != 0);
            int j = i - 1;
            while (j >= 0)
            {
                int prev = order[j];
                bool prevSet = (*s_Targets[prev].posX != 0 || *s_Targets[prev].posY != 0);
                bool swap = false;
                if (!curSet && prevSet) swap = true;
                else if (curSet == prevSet && strcmp(s_Targets[cur].name, s_Targets[prev].name) < 0) swap = true;
                if (!swap) break;
                order[j + 1] = prev;
                --j;
            }
            order[j + 1] = cur;
        }

        // Target buttons — always visible, disabled during countdown
        for (int idx = 0; idx < NUM_TARGETS; idx++)
        {
            int i = order[idx];
            CaptureTarget& t = s_Targets[i];

            char label[128];
            if (*t.posX != 0 || *t.posY != 0)
                sprintf_s(label, "%s  (%d, %d)", t.name, *t.posX, *t.posY);
            else
                sprintf_s(label, "%s  (not set)", t.name);

            // Highlight the currently capturing target
            if (s_CountdownActive && s_CountdownTarget == i)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.5f, 0.0f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.5f, 0.0f, 0.9f));
                ImGui::Button(label, ImVec2(-1, 30));
                ImGui::PopStyleColor(2);
            }
            else if (s_CountdownActive)
            {
                // Disabled during countdown
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
                ImGui::Button(label, ImVec2(-1, 30));
                ImGui::PopStyleVar();
            }
            else
            {
                if (ImGui::Button(label, ImVec2(-1, 30)))
                {
                    s_CountdownActive = true;
                    s_CountdownTarget = i;
                    s_CountdownStart = std::chrono::steady_clock::now();
                    s_ShowConfirmation = false;

                    char logBuf[128];
                    sprintf_s(logBuf, "Starting %ds countdown for %s", COUNTDOWN_SECONDS, t.name);
                    APIDefs->Log(LOGL_INFO, "MysticClicker", logBuf);
                }

                // Right-click clears the captured position for this target.
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
                    ImGui::SetTooltip("%s\n(right-click to clear)", t.description);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Close", ImVec2(-1, 25)))
        {
            g_ShowCaptureWindow = false;
            s_CountdownActive = false;
            s_CountdownTarget = -1;
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
