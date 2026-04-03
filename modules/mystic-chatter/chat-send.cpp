/**
 * chat-send.cpp
 *
 * Chat message sending for Mystic Chatter addon.
 * Uses clipboard paste approach (same as Blish HUD Chat Shorts):
 *   1. Save current clipboard
 *   2. Copy message text to clipboard
 *   3. Open chat (Enter)
 *   4. Paste (Ctrl+V)
 *   5. Send (Enter)
 *   6. Restore clipboard
 *
 * Author: OgMorrow2090
 * Repository: https://github.com/OgMorrow2090/guildwars2
 */

#include "shared.h"
#include <cstdio>
#include <string>
#include <thread>
#include <chrono>

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
// Helper: Keyboard Simulation via SendInput
// =============================================================================

static void KeyPress(WORD vk)
{
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = 0;
    SendInput(1, &input, sizeof(INPUT));
}

static void KeyRelease(WORD vk)
{
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

static void KeyStroke(WORD vk)
{
    KeyPress(vk);
    Sleep(10);
    KeyRelease(vk);
}

static void KeyCombo(WORD modifier, WORD key)
{
    KeyPress(modifier);
    Sleep(10);
    KeyStroke(key);
    Sleep(10);
    KeyRelease(modifier);
}

// =============================================================================
// Helper: Clipboard
// =============================================================================

static bool SetClipboardText(const char* text)
{
    if (!OpenClipboard(nullptr))
        return false;

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
    if (!OpenClipboard(nullptr))
        return result;

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

/**
 * SendChatMessage - Send text to GW2 chat
 *
 * Uses clipboard paste for reliability (same approach as Blish HUD):
 *   Enter -> Clipboard paste -> Enter
 *
 * Runs on a separate thread to avoid blocking the game.
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

    // Copy message to a string for the thread
    std::string msg(message);

    // Run on background thread to avoid blocking game input
    std::thread([msg]()
    {
        char logBuf[256];
        snprintf(logBuf, sizeof(logBuf), "Sending: %s", msg.c_str());
        APIDefs->Log(LOGL_INFO, "MysticChatter", logBuf);

        // 1. Save current clipboard content
        std::string prevClipboard = GetClipboardText();

        // 2. Copy message to clipboard
        if (!SetClipboardText(msg.c_str()))
        {
            APIDefs->Log(LOGL_WARNING, "MysticChatter", "Failed to set clipboard");
            return;
        }

        // 3. Open chat (press Enter)
        KeyStroke(VK_RETURN);
        Sleep(50);

        // 4. Paste (Ctrl+V)
        KeyCombo(VK_CONTROL, 'V');
        Sleep(50);

        // 5. Send (press Enter)
        KeyStroke(VK_RETURN);
        Sleep(30);

        // 6. Restore previous clipboard
        if (!prevClipboard.empty())
            SetClipboardText(prevClipboard.c_str());

        APIDefs->Log(LOGL_DEBUG, "MysticChatter", "Message sent");

    }).detach();
}
