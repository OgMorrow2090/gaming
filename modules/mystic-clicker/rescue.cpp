/**
 * rescue.cpp
 *
 * Cross-addon ImGui window rescue. Walks every top-level ImGui window in the
 * shared Nexus context and clamps each one back into the visible viewport so
 * windows that drifted off-screen after a 4K → Deck resolution swap (or any
 * other off-screen state) come back into reach.
 *
 * Triggered by RESET_WINDOWS / MT_RESET_WINDOWS keybind (Ctrl+Shift+Home).
 * Runs from RenderCaptureWindow once per rescue request.
 */

#include "shared.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>

static const float MIN_VISIBLE_PX = 60.0f;

void RescueAllImGuiWindows()
{
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (ctx == nullptr) return;

    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    if (ds.x <= 0.0f || ds.y <= 0.0f) return;

    int rescued = 0;
    for (int i = 0; i < ctx->Windows.Size; ++i)
    {
        ImGuiWindow* w = ctx->Windows[i];
        if (w == nullptr) continue;
        if (w->Name == nullptr || w->Name[0] == '\0') continue;
        if (w->Hidden && !w->Active) continue;
        // Skip non-toplevel windows — child/popup/tooltip are positioned by their parent.
        if (w->Flags & ImGuiWindowFlags_ChildWindow) continue;
        if (w->Flags & ImGuiWindowFlags_Tooltip)     continue;
        if (w->Flags & ImGuiWindowFlags_Popup)       continue;
        if (w->Flags & ImGuiWindowFlags_NoSavedSettings && (w->Flags & ImGuiWindowFlags_NoMove))
            continue;  // overlays/HUD elements that are intentionally pinned

        ImVec2 pos = w->Pos;
        ImVec2 size = w->SizeFull;

        bool changed = false;

        // Clamp size to display first.
        if (size.x > ds.x) { size.x = ds.x; changed = true; }
        if (size.y > ds.y) { size.y = ds.y; changed = true; }

        // Clamp position so at least MIN_VISIBLE_PX of the window stays on-screen
        // on every edge.
        if (pos.x + size.x < MIN_VISIBLE_PX)             { pos.x = MIN_VISIBLE_PX - size.x; changed = true; }
        if (pos.y + size.y < MIN_VISIBLE_PX)             { pos.y = MIN_VISIBLE_PX - size.y; changed = true; }
        if (pos.x > ds.x - MIN_VISIBLE_PX)               { pos.x = ds.x - MIN_VISIBLE_PX;   changed = true; }
        if (pos.y > ds.y - MIN_VISIBLE_PX)               { pos.y = ds.y - MIN_VISIBLE_PX;   changed = true; }

        // Hard floor for negative-coord drift.
        if (pos.x < 0.0f) { pos.x = 0.0f; changed = true; }
        if (pos.y < 0.0f) { pos.y = 0.0f; changed = true; }

        if (changed)
        {
            ImGui::SetWindowPos(w->Name, pos, ImGuiCond_Always);
            ImGui::SetWindowSize(w->Name, size, ImGuiCond_Always);
            rescued++;
        }
    }

    if (rescued > 0)
    {
        char buf[96];
        sprintf_s(buf, "Window rescue: repositioned %d ImGui window(s)", rescued);
        APIDefs->Log(LOGL_INFO, "MysticClicker", buf);
    }
}
