/**
 * capture-ui.cpp
 *
 * ImGui capture mode window for Mystic Clicker.
 * Opens a window with buttons for each capture target.
 * Click a button -> 5 second countdown -> captures mouse position.
 *
 * Author: OgMorrow2090
 * Repository: https://github.com/OgMorrow2090/guildwars2
 */

#include "shared.h"
#include "imgui/imgui.h"
#include <cstdio>
#include <chrono>
#include <thread>

// Capture window state
bool g_ShowCaptureWindow = false;

// Countdown state
static bool s_CountdownActive = false;
static int s_CountdownTarget = -1;  // Which capture slot
static std::chrono::steady_clock::time_point s_CountdownStart;
static constexpr int COUNTDOWN_SECONDS = 5;

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
    {"Bouncy Chest",      "Right-click to open",          &g_ChestX,        &g_ChestY},
    {"Exit Instance",     "Leave instance button",        &g_ExitInstanceX, &g_ExitInstanceY},
    {"Yes Dialog",        "Confirm dialog button",        &g_YesDialogX,    &g_YesDialogY},
    {"Mystic Forge",      "Forge throw button",           &g_MysticForgeX,  &g_MysticForgeY},
    {"Mystic Refill",     "Forge refill button",          &g_MysticRefillX, &g_MysticRefillY},
    {"Vendor Buy",        "Buy from vendor",              &g_VendorX,       &g_VendorY},
    {"Sell Junk",         "Sell junk to vendor",          &g_SellJunkX,     &g_SellJunkY},
    {"Trading Post",      "TP collect button",            &g_TradingPostX,  &g_TradingPostY},
    {"TP Remove",         "Cancel TP listing",            &g_TpRemoveX,     &g_TpRemoveY},
};
static constexpr int NUM_TARGETS = sizeof(s_Targets) / sizeof(s_Targets[0]);

/**
 * PerformCapture - Called when countdown reaches zero
 * Captures the current cursor position for the selected target.
 */
static void PerformCapture(int targetIdx)
{
    if (targetIdx < 0 || targetIdx >= NUM_TARGETS) return;

    CaptureTarget& target = s_Targets[targetIdx];

    POINT pt;
    if (GetCursorPos(&pt))
    {
        if (GameWindow != nullptr)
        {
            ScreenToClient(GameWindow, &pt);
        }

        *target.posX = pt.x;
        *target.posY = pt.y;

        char buffer[128];
        sprintf_s(buffer, "%s captured: (%d, %d)", target.name, pt.x, pt.y);
        APIDefs->Log(LOGL_INFO, "MysticClicker", buffer);
        APIDefs->GUI_SendAlert(buffer);

        SaveButtonPositions();
    }
    else
    {
        APIDefs->GUI_SendAlert("Failed to capture position!");
    }

    s_CountdownActive = false;
    s_CountdownTarget = -1;
}

/**
 * RenderCaptureWindow - ImGui render callback
 *
 * Draws the capture mode window with buttons for each target
 * and a countdown overlay when capturing.
 */
void RenderCaptureWindow()
{
    if (!g_ShowCaptureWindow) return;

    // Check countdown
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
            // Draw countdown overlay
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
                                           ImGui::GetIO().DisplaySize.y * 0.5f),
                                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(300, 120));
            ImGui::Begin("##Countdown", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

            ImGui::PushFont(nullptr);
            char countText[64];
            sprintf_s(countText, "Capturing: %s", s_Targets[s_CountdownTarget].name);
            ImVec2 textSize = ImGui::CalcTextSize(countText);
            ImGui::SetCursorPosX((300 - textSize.x) * 0.5f);
            ImGui::Text("%s", countText);

            ImGui::Spacing();

            char numText[16];
            sprintf_s(numText, "%d", remaining);
            ImVec2 numSize = ImGui::CalcTextSize(numText);
            ImGui::SetCursorPosX((300 - numSize.x) * 0.5f);
            ImGui::SetWindowFontScale(3.0f);
            ImGui::Text("%d", remaining);
            ImGui::SetWindowFontScale(1.0f);

            ImGui::Spacing();
            ImGui::SetCursorPosX((300 - 80) * 0.5f);
            if (ImGui::Button("Cancel", ImVec2(80, 25)))
            {
                s_CountdownActive = false;
                s_CountdownTarget = -1;
            }

            ImGui::PopFont();
            ImGui::End();
            return;
        }
    }

    // Main capture window
    ImGui::SetNextWindowSize(ImVec2(350, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Mystic Clicker - Capture", &g_ShowCaptureWindow, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Click a button, then move cursor to target.");
        ImGui::Text("Position captures after %d seconds.", COUNTDOWN_SECONDS);
        ImGui::Separator();
        ImGui::Spacing();

        for (int i = 0; i < NUM_TARGETS; i++)
        {
            CaptureTarget& t = s_Targets[i];

            // Show current position
            char label[128];
            if (*t.posX != 0 || *t.posY != 0)
            {
                sprintf_s(label, "%s  (%d, %d)", t.name, *t.posX, *t.posY);
            }
            else
            {
                sprintf_s(label, "%s  (not set)", t.name);
            }

            if (ImGui::Button(label, ImVec2(-1, 30)))
            {
                // Start countdown
                s_CountdownActive = true;
                s_CountdownTarget = i;
                s_CountdownStart = std::chrono::steady_clock::now();
                g_ShowCaptureWindow = false;  // Hide window during countdown

                char logBuf[128];
                sprintf_s(logBuf, "Starting %ds countdown for %s", COUNTDOWN_SECONDS, t.name);
                APIDefs->Log(LOGL_INFO, "MysticClicker", logBuf);
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", t.description);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Close", ImVec2(-1, 25)))
        {
            g_ShowCaptureWindow = false;
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
}
