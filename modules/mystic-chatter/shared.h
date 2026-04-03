#pragma once

#include <windows.h>
#include "Nexus.h"

// Global API pointer - set in AddonLoad
extern AddonAPI_t* APIDefs;

// Addon definition
extern AddonDefinition_t AddonDef;

// Game window handle
extern HWND GameWindow;

// Keybind identifiers - Chat macros
constexpr const char* CHAT_TY      = "CHAT_TY";
constexpr const char* CHAT_NP      = "CHAT_NP";
constexpr const char* CHAT_HI      = "CHAT_HI";
constexpr const char* CHAT_PARTY   = "CHAT_PARTY";
constexpr const char* CHAT_SAY     = "CHAT_SAY";
constexpr const char* CHAT_MAP     = "CHAT_MAP";
constexpr const char* CHAT_GUILD   = "CHAT_GUILD";
constexpr const char* CHAT_SQUAD   = "CHAT_SQUAD";
constexpr const char* CHAT_QHEAL   = "CHAT_QHEAL";
constexpr const char* CHAT_AHEAL   = "CHAT_AHEAL";
constexpr const char* CHAT_PLUS    = "CHAT_PLUS";
constexpr const char* CHAT_MINUS   = "CHAT_MINUS";
constexpr const char* CHAT_YES     = "CHAT_YES";
constexpr const char* CHAT_NO      = "CHAT_NO";

// Keybind handler
void ProcessKeybind(const char* aIdentifier, bool aIsRelease);

// Chat sending function
void SendChatMessage(const char* message);
