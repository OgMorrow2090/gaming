#pragma once

#include <windows.h>
#include "Nexus.h"

// ImGui forward declaration
struct ImGuiContext;

// --- Globals (set in AddonLoad) ---
extern AddonAPI_t*       APIDefs;
extern AddonDefinition_t AddonDef;
extern HMODULE           SelfModule;   // this DLL's handle — for resource-embedded icons
extern HWND              GameWindow;
extern ImGuiContext*     ImGuiCtx;

// --- Keybind identifiers ---
constexpr const char* MYSTIC_AI_CAPTURE   = "MYSTIC_AI_CAPTURE";   // freeze-frame drag-select read
constexpr const char* MYSTIC_AI_READ_BOOK = "MYSTIC_AI_READ_BOOK"; // read the saved book region
constexpr const char* MYSTIC_AI_READ      = "MYSTIC_AI_READ";      // voice the current panel content
constexpr const char* MYSTIC_AI_TP_REGION = "MYSTIC_AI_TP_REGION"; // overview the saved TP region

// --- Keybind handler (keybinds.cpp) ---
void ProcessKeybind(const char* aIdentifier, bool aIsRelease);

// --- Render + overlay (overlay.cpp) ---
void RenderMysticAI();    // RT_Render callback
void ToggleCapture();     // start, or cancel, the freeze-frame drag-select capture
void ReadBookRegion();    // capture + read the saved book region (no drag)
void ReadPanelAloud();    // voice the current panel content (the Read action)
void ReadTpRegion();      // capture + overview the saved TP region (no drag)
void ShutdownOverlay();   // release the frozen-frame texture — called from AddonUnload
UINT MysticAIWndProc(HWND aHwnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam);  // Nexus WndProc hook — consumes Esc

// --- Config (config.cpp) ---
void SetConfigPath(const char* addonDir);
void LoadSettings();
void SaveSettings();
void CheckResolutionChange();

// Per-resolution settings, persisted to mystic-ai-<W>x<H>.cfg.
extern float g_FontScale;     // panel text size, 0.6 .. 2.5
extern float g_ButtonScale;   // action-button size, 0.6 .. 2.5
extern float g_PanelW;        // panel width  — drag-resized, persisted
extern float g_PanelH;        // panel height — drag-resized, persisted
extern float g_PanelOpacity;  // results panel background opacity, 0.2 .. 1.0
// Saved "book region" — a fixed screen rectangle the Read-Book keybind re-reads
// with no drag. Pixel coords in the game window's client space. W/H == 0 = unset.
extern int   g_BookRegionX, g_BookRegionY, g_BookRegionW, g_BookRegionH;
// Saved "TP region" — the TP-region keybind overviews it with no drag. Unlike
// the book region this is CURSOR-RELATIVE: X/Y are the box's offset (client
// pixels, may be negative) from the cursor, W/H its size. The keybind adds the
// live cursor position, so it reads whatever item the cursor is on. W/H == 0
// = unset.
extern int   g_TpRegionX, g_TpRegionY, g_TpRegionW, g_TpRegionH;
