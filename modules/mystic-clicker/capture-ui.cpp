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
#include <chrono>

// Capture window state
bool g_ShowCaptureWindow = false;

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
    {"Accept 15",         "Accept combo slot 15", &g_Accept15X,    &g_Accept15Y},
    {"Wizard Vault Weekly Tab", "WV Weekly tab button (combo switches tab after daily)", &g_WizardVaultWeeklyTabX, &g_WizardVaultWeeklyTabY},
    {"LFG Search",        "Search tab inside LFG panel (combo 2nd click)", &g_LfgSearchX, &g_LfgSearchY},
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
    if (!g_ShowCaptureWindow) return;

    // Auto-hide confirmation after 3 seconds
    if (s_ShowConfirmation)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - s_ConfirmStart).count();
        if (elapsed >= CONFIRM_DISPLAY_SECONDS)
            s_ShowConfirmation = false;
    }

    // Main window — always visible
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Mystic Clicker - Capture", &g_ShowCaptureWindow, ImGuiWindowFlags_AlwaysAutoResize))
    {
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

        // Target buttons — always visible, disabled during countdown
        for (int i = 0; i < NUM_TARGETS; i++)
        {
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
    }
    ImGui::End();
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
