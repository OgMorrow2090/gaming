/**
 * chat-send.cpp
 *
 * Chat message sending for Mystic Chatter addon.
 * Uses WndProc_SendToGameOnly (same as Mystic Clicker) for Proton compatibility.
 * Sends WM_KEYDOWN/UP for Enter, WM_CHAR for each text character.
 *
 * Flow:
 *   1. Press Enter (open chat)
 *   2. Send each character via WM_CHAR
 *   3. Press Enter (send message)
 *
 * No clipboard needed — WM_CHAR types directly into the chat input.
 *
 * Author: OgMorrow2090
 * Repository: https://github.com/OgMorrow2090/guildwars2
 */

#include "shared.h"
#include <cstdio>
#include <string>
#include <thread>

// =============================================================================
// Helper: Find Game Window
// =============================================================================

static void EnsureGameWindow()
{
    if (GameWindow != nullptr) return;

    GameWindow = FindWindowA("ArenaNet_Dx_Window_Class", nullptr);
    if (GameWindow == nullptr)
        GameWindow = FindWindowA("ArenaNet_Gr_Window_Class", nullptr);
    if (GameWindow == nullptr)
        GameWindow = FindWindowA(nullptr, "Guild Wars 2");
    if (GameWindow == nullptr)
    {
        GameWindow = GetForegroundWindow();
        APIDefs->Log(LOGL_WARNING, "MysticChatter", "Using foreground window");
    }
}

// =============================================================================
// Helper: Key Simulation via WndProc (Proton compatible)
// =============================================================================

/**
 * Build lParam for WM_KEYDOWN/WM_KEYUP
 * Bits: 0-15 repeat count, 16-23 scan code, 24 extended, 30 prev state, 31 transition
 */
static LPARAM MakeKeyLParam(UINT scanCode, bool isUp, bool isExtended = false)
{
    LPARAM lParam = 1; // repeat count = 1
    lParam |= (scanCode & 0xFF) << 16;
    if (isExtended) lParam |= (1 << 24);
    if (isUp) lParam |= (1 << 30) | (1 << 31);
    return lParam;
}

static void SendKeyDown(UINT vk, UINT scanCode)
{
    LPARAM lParam = MakeKeyLParam(scanCode, false);
    APIDefs->WndProc_SendToGameOnly(GameWindow, WM_KEYDOWN, vk, lParam);
}

static void SendKeyUp(UINT vk, UINT scanCode)
{
    LPARAM lParam = MakeKeyLParam(scanCode, true);
    APIDefs->WndProc_SendToGameOnly(GameWindow, WM_KEYUP, vk, lParam);
}

static void SendKeyStroke(UINT vk, UINT scanCode)
{
    SendKeyDown(vk, scanCode);
    Sleep(15);
    SendKeyUp(vk, scanCode);
}

static void SendChar(char c)
{
    APIDefs->WndProc_SendToGameOnly(GameWindow, WM_CHAR, (WPARAM)c, 0);
}

// =============================================================================
// SendChatMessage - Main Function
// =============================================================================

/**
 * SendChatMessage - Send text to GW2 chat
 *
 * Uses WndProc messages for Proton/Wine compatibility:
 *   WM_KEYDOWN Enter (open chat) -> WM_CHAR per character -> WM_KEYDOWN Enter (send)
 *
 * Runs on a separate thread to allow Sleep() delays without blocking.
 *
 * @param message The text to send (max 199 chars per GW2 limit)
 */
void SendChatMessage(const char* message)
{
    EnsureGameWindow();
    if (GameWindow == nullptr)
    {
        APIDefs->Log(LOGL_WARNING, "MysticChatter", "Game window not available");
        return;
    }

    if (message == nullptr || strlen(message) == 0)
        return;

    if (strlen(message) > 199)
    {
        APIDefs->Log(LOGL_WARNING, "MysticChatter", "Message exceeds 199 char limit");
        return;
    }

    // Copy message for the thread
    std::string msg(message);

    // Run on background thread for Sleep() delays
    std::thread([msg]()
    {
        char logBuf[256];
        snprintf(logBuf, sizeof(logBuf), "Sending: %s", msg.c_str());
        APIDefs->Log(LOGL_INFO, "MysticChatter", logBuf);

        // 1. Open chat — press Enter (VK_RETURN = 0x0D, scan code 0x1C)
        SendKeyStroke(VK_RETURN, 0x1C);
        Sleep(100);

        // 2. Type each character via WM_CHAR
        for (char c : msg)
        {
            SendChar(c);
            Sleep(5);
        }
        Sleep(50);

        // 3. Send — press Enter
        SendKeyStroke(VK_RETURN, 0x1C);

        APIDefs->Log(LOGL_DEBUG, "MysticChatter", "Message sent");

    }).detach();
}
