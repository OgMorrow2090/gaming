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
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <cstdio>

static const float MIN_VISIBLE_PX = 60.0f;

// Cap window size at 70% of viewport so a window stretched to 4K dimensions
// (e.g. 1920×1080) still shrinks to a usable size on a 1280×800 Deck —
// 896×560 in this case. Without this cap, a window only shrinks if it's
// strictly larger than the display, which leaves it filling the whole screen
// (still unusable).
static const float MAX_VIEWPORT_FRACTION = 0.70f;

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

        // Clamp size to a fraction of the viewport so oversized windows shrink
        // to something usable, not just to fill-the-screen.
        const float maxW = ds.x * MAX_VIEWPORT_FRACTION;
        const float maxH = ds.y * MAX_VIEWPORT_FRACTION;
        if (size.x > maxW) { size.x = maxW; changed = true; }
        if (size.y > maxH) { size.y = maxH; changed = true; }

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

    char buf[128];
    sprintf_s(buf, "Window rescue: %d window(s) repositioned/resized (viewport %.0fx%.0f)",
              rescued, ds.x, ds.y);
    APIDefs->Log(LOGL_INFO, "MysticClicker", buf);
}
