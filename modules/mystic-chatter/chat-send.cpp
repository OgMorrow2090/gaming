/**
 * chat-send.cpp
 *
 * Chat message sending for Mystic Chatter addon.
 * Uses clipboard paste with WndProc for Proton compatibility.
 *
 * Flow:
 *   1. Save clipboard
 *   2. Copy message to clipboard
 *   3. Press Enter via WndProc (open chat)
 *   4. Ctrl+V via WndProc (paste from clipboard)
 *   5. Press Enter via WndProc (send)
 *   6. Restore clipboard
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

static LPARAM MakeKeyLParam(UINT scanCode, bool isUp, bool isExtended = false)
{
    LPARAM lParam = 1;
    lParam |= (scanCode & 0xFF) << 16;
    if (isExtended) lParam |= (1 << 24);
    if (isUp) lParam |= (1 << 30) | (1 << 31);
    return lParam;
}

static void SendKeyDown(UINT vk, UINT scanCode)
{
    APIDefs->WndProc_SendToGameOnly(GameWindow, WM_KEYDOWN, vk, MakeKeyLParam(scanCode, false));
}

static void SendKeyUp(UINT vk, UINT scanCode)
{
    APIDefs->WndProc_SendToGameOnly(GameWindow, WM_KEYUP, vk, MakeKeyLParam(scanCode, true));
}

static void SendKeyStroke(UINT vk, UINT scanCode)
{
    SendKeyDown(vk, scanCode);
    Sleep(20);
    SendKeyUp(vk, scanCode);
}

// =============================================================================
// Helper: Clipboard
// =============================================================================

static bool SetClipboardText(const char* text)
{
    if (!OpenClipboard(GameWindow))
    {
        // Retry with nullptr
        if (!OpenClipboard(nullptr))
            return false;
    }

    EmptyClipboard();

    size_t len = strlen(text) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem == nullptr)
    {
        CloseClipboard();
        return false;
    }

    memcpy(GlobalLock(hMem), text, len);
    GlobalUnlock(hMem);
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
    return true;
}

static std::string GetClipboardText()
{
    std::string result;
    if (!OpenClipboard(GameWindow))
    {
        if (!OpenClipboard(nullptr))
            return result;
    }

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData != nullptr)
    {
        char* pszText = static_cast<char*>(GlobalLock(hData));
        if (pszText != nullptr)
        {
            result = pszText;
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}

// =============================================================================
// SendChatMessage - Main Function
// =============================================================================

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

    std::string msg(message);

    std::thread([msg]()
    {
        char logBuf[256];
        snprintf(logBuf, sizeof(logBuf), "Sending: %s", msg.c_str());
        APIDefs->Log(LOGL_INFO, "MysticChatter", logBuf);

        // 0. Release modifier keys that may still be held from the triggering keybind
        //    (Ctrl+Shift+KP combo — Ctrl and Shift are still down when we fire)
        SendKeyUp(VK_CONTROL, 0x1D);
        SendKeyUp(VK_SHIFT, 0x2A);
        SendKeyUp(VK_LCONTROL, 0x1D);
        SendKeyUp(VK_LSHIFT, 0x2A);
        Sleep(100);

        // 1. Save clipboard
        std::string prevClipboard = GetClipboardText();

        // 2. Copy message to clipboard
        if (!SetClipboardText(msg.c_str()))
        {
            APIDefs->Log(LOGL_WARNING, "MysticChatter", "Failed to set clipboard");
            return;
        }

        // 3. Open chat — Enter (scan code 0x1C)
        SendKeyStroke(VK_RETURN, 0x1C);
        Sleep(200);

        // 4. Paste — Ctrl+V via WndProc
        SendKeyDown(VK_CONTROL, 0x1D);
        Sleep(30);
        SendKeyStroke('V', 0x2F);
        Sleep(30);
        SendKeyUp(VK_CONTROL, 0x1D);
        Sleep(150);

        // 5. Send — Enter
        SendKeyStroke(VK_RETURN, 0x1C);
        Sleep(50);

        // 6. Restore clipboard
        if (!prevClipboard.empty())
            SetClipboardText(prevClipboard.c_str());

        APIDefs->Log(LOGL_DEBUG, "MysticChatter", "Message sent");

    }).detach();
}
