/**
 * overlay.cpp
 *
 * Mystic AI's freeze-frame drag-select capture and the box-anchored results
 * panel.
 *
 * Flow: a hotkey freezes the current frame; the player drags a box over the
 * text they want; on release the crop is sent to the Claude daemon and read
 * aloud, with a small action panel anchored right at the box so the mouse
 * barely moves. A "Book" button on that panel saves the selection as a fixed
 * region; the Read-Book keybind then re-reads it with no drag.
 *
 * Why a worker thread for the capture: CaptureBackBufferRegion() blocks until
 * the next RT_PostRender hook. Calling it from the render thread (where this
 * UI runs) would deadlock that frame, so the grab is offloaded; the render
 * thread turns the pixels into a texture once they arrive.
 *
 * Threading: the keybind thread only posts a command (g_cmd); every state
 * change happens on the render thread. The capture worker hands its pixels
 * over under g_capMtx.
 */

#include "shared.h"
#include "claude-vision.h"
#include "screen-capture.h"
#include "icons.h"
#include "copytext.h"
#include "imgui/imgui.h"
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <cstdint>
#include <cstring>

// Prompts sent to Claude with the captured image.
static const char* DRAG_PROMPT =
    "This image is a region of the Guild Wars 2 screen that the player selected. "
    "Read the text in it - a tooltip, dialogue, quest text, book page, mail, item "
    "listing, or trading-post prices - in full, top to bottom. If text is clearly "
    "cut off at an edge, mention that briefly.";

static const char* BOOK_PROMPT =
    "This image is a page from an in-game Guild Wars 2 book. Read the page text in "
    "full, top to bottom, naturally, as if reading the book aloud.";

namespace {

// --- Render-thread state machine -------------------------------------------
enum Mode { MODE_IDLE, MODE_CAPTURING, MODE_SELECTING, MODE_REVIEW, MODE_BOOKPANEL };
int  g_mode        = MODE_IDLE;
bool g_needRelease = false;   // release the frozen texture at the next frame's top

// --- Command posted by the keybind thread, drained by the render thread ----
enum Cmd { CMD_NONE, CMD_TOGGLE, CMD_BOOK };
std::atomic<int> g_cmd{CMD_NONE};

// --- WndProc hook <-> render thread ----------------------------------------
// g_uiActive: the render thread publishes "Mystic AI owns Esc right now" so the
// WndProc can swallow the key before GW2 sees it. g_escConsumed: the WndProc
// posts a swallowed Esc for the render thread to act on next frame.
std::atomic<bool> g_uiActive{false};
std::atomic<bool> g_escConsumed{false};

// --- Capture worker handoff (guarded by g_capMtx) --------------------------
std::mutex           g_capMtx;
std::vector<uint8_t> g_capPixels;     // full frame, 24-bit BGR
int                  g_capW = 0, g_capH = 0;
bool                 g_capReady  = false;
bool                 g_capFailed = false;

// --- Frozen-frame texture (render thread only) -----------------------------
ID3D11ShaderResourceView* g_srv  = nullptr;
int                       g_srvW = 0, g_srvH = 0;

// --- Drag state (render thread only) ---------------------------------------
bool   g_dragging = false;
ImVec2 g_dragA{0, 0}, g_dragB{0, 0};

// Finalised selection: g_boxDisp in display coords for drawing, g_sel* in
// frame pixel coords for the crop and for saving as the book region.
ImVec4 g_boxDisp{0, 0, 0, 0};
int    g_selX = 0, g_selY = 0, g_selW = 0, g_selH = 0;
bool   g_haveSel = false;

// The drag-selected crop, kept through the Review session so the action
// buttons (Read / TP / Wiki / Research / Copy) can re-send it.
std::vector<uint8_t> g_lastCrop;
int                  g_lastCropW = 0, g_lastCropH = 0;

// --- Box-anchored panel ----------------------------------------------------
ImVec4 g_anchor{0, 0, 0, 0};   // box the panel anchors to (display coords)
bool   g_reposPanel = false;   // snap the panel to the anchor on its next frame
bool   g_panelOpen  = true;

inline float Mn(float a, float b) { return a < b ? a : b; }
inline float Mx(float a, float b) { return a > b ? a : b; }

// ---------------------------------------------------------------------------
// D3D11 helpers
// ---------------------------------------------------------------------------

// Borrow the D3D11 device from the Nexus swap chain. Caller releases it.
ID3D11Device* BorrowDevice()
{
    if (!APIDefs || !APIDefs->SwapChain) return nullptr;
    auto* sc = (IDXGISwapChain*)APIDefs->SwapChain;
    ID3D11Texture2D* bb = nullptr;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb)) || !bb)
        return nullptr;
    ID3D11Device* dev = nullptr;
    bb->GetDevice(&dev);
    bb->Release();
    return dev;
}

void ReleaseFrozenFrame()
{
    if (g_srv) { g_srv->Release(); g_srv = nullptr; }
    g_srvW = g_srvH = 0;
    std::lock_guard<std::mutex> lk(g_capMtx);
    g_capPixels.clear();
    g_capPixels.shrink_to_fit();
    g_capReady = g_capFailed = false;
    g_capW = g_capH = 0;
}

// Turn a 24-bit BGR frame into a shader-resource view for ImGui to draw.
bool BuildFrozenTexture(const std::vector<uint8_t>& bgr, int w, int h)
{
    if (w <= 0 || h <= 0 || bgr.size() < (size_t)w * h * 3) return false;
    ID3D11Device* dev = BorrowDevice();
    if (!dev) return false;

    std::vector<uint8_t> bgra((size_t)w * h * 4);
    for (size_t i = 0, n = (size_t)w * h; i < n; ++i)
    {
        bgra[i * 4 + 0] = bgr[i * 3 + 0];
        bgra[i * 4 + 1] = bgr[i * 3 + 1];
        bgra[i * 4 + 2] = bgr[i * 3 + 2];
        bgra[i * 4 + 3] = 255;
    }

    D3D11_TEXTURE2D_DESC d{};
    d.Width            = (UINT)w;
    d.Height           = (UINT)h;
    d.MipLevels        = 1;
    d.ArraySize        = 1;
    d.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage            = D3D11_USAGE_IMMUTABLE;
    d.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA srd{};
    srd.pSysMem     = bgra.data();
    srd.SysMemPitch = (UINT)w * 4;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(dev->CreateTexture2D(&d, &srd, &tex)) || !tex)
    {
        dev->Release();
        return false;
    }
    HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, &g_srv);
    tex->Release();
    dev->Release();
    if (FAILED(hr) || !g_srv) { g_srv = nullptr; return false; }

    g_srvW = w;
    g_srvH = h;
    return true;
}

// ---------------------------------------------------------------------------
// Capture worker — grabs the whole frame off the render thread.
// ---------------------------------------------------------------------------
void CaptureWorker()
{
    int w = 2560, h = 1440;
    if (GameWindow)
    {
        RECT r;
        if (GetClientRect(GameWindow, &r)) { w = r.right - r.left; h = r.bottom - r.top; }
    }
    std::vector<uint8_t> px;
    bool ok = (w > 0 && h > 0) && CaptureBackBufferRegion(0, 0, w, h, px, 1500);

    std::lock_guard<std::mutex> lk(g_capMtx);
    if (ok)
    {
        g_capPixels = std::move(px);
        g_capW = w; g_capH = h;
        g_capReady = true;
    }
    else
    {
        g_capFailed = true;
    }
}

// ---------------------------------------------------------------------------
// State transitions (render thread)
// ---------------------------------------------------------------------------

// Leave any active mode. The frozen texture is released at the top of the next
// frame, never mid-frame — this frame's draw list may still reference it.
void ExitToIdle(bool stopRead)
{
    g_mode        = MODE_IDLE;
    g_dragging    = false;
    g_haveSel     = false;
    g_needRelease = true;
    g_lastCrop.clear();
    g_lastCrop.shrink_to_fit();
    CopyText::Reset();
    if (stopRead) ClaudeVision::Stop();
}

void StartCapture()
{
    {
        std::lock_guard<std::mutex> lk(g_capMtx);
        g_capPixels.clear();
        g_capReady = g_capFailed = false;
        g_capW = g_capH = 0;
    }
    g_dragging = false;
    g_haveSel  = false;
    g_mode     = MODE_CAPTURING;
    std::thread(CaptureWorker).detach();
}

void StartBookRead()
{
    if (g_BookRegionW <= 0 || g_BookRegionH <= 0)
    {
        if (APIDefs)
            APIDefs->GUI_SendAlert("Mystic AI: no book region saved yet. Drag-select a "
                                   "book page, then click the Book button to save it.");
        return;
    }
    g_anchor     = ImVec4((float)g_BookRegionX, (float)g_BookRegionY,
                          (float)g_BookRegionW, (float)g_BookRegionH);
    g_reposPanel = true;
    g_panelOpen  = true;
    g_mode       = MODE_BOOKPANEL;
    ClaudeVision::RequestRegion("Book", BOOK_PROMPT,
                                g_BookRegionX, g_BookRegionY, g_BookRegionW, g_BookRegionH);
}

// Drain the keybind command. Any keybind press while a mode is active cancels
// it; otherwise it stops a stray read, or starts a fresh capture / book read.
void ProcessCommand()
{
    int c = g_cmd.exchange(CMD_NONE);
    if (c == CMD_NONE) return;

    if (g_mode != MODE_IDLE)
    {
        ExitToIdle(true);
        return;
    }
    if (ClaudeVision::GetState() == ClaudeVision::State::Waiting
        || ClaudeVision::IsSpeaking())
    {
        ClaudeVision::Stop();
        return;
    }
    if (c == CMD_TOGGLE)    StartCapture();
    else if (c == CMD_BOOK) StartBookRead();
}

// Capturing -> Selecting once the worker delivers the frame.
void AdvanceCapture()
{
    if (g_mode != MODE_CAPTURING) return;

    bool ready = false, failed = false;
    int  w = 0, h = 0;
    {
        std::lock_guard<std::mutex> lk(g_capMtx);
        ready  = g_capReady;
        failed = g_capFailed;
        w = g_capW; h = g_capH;
    }
    if (failed)
    {
        if (APIDefs) APIDefs->GUI_SendAlert("Mystic AI: screen capture failed.");
        ExitToIdle(false);
        return;
    }
    if (!ready) return;   // worker still running — normally ready within a frame or two

    std::vector<uint8_t> px;
    {
        std::lock_guard<std::mutex> lk(g_capMtx);
        px.swap(g_capPixels);
    }
    if (!BuildFrozenTexture(px, w, h))
    {
        if (APIDefs) APIDefs->GUI_SendAlert("Mystic AI: could not build the capture texture.");
        ExitToIdle(false);
        return;
    }
    // Keep the pixels — FinishSelection crops them.
    {
        std::lock_guard<std::mutex> lk(g_capMtx);
        g_capPixels = std::move(px);
        g_capW = w; g_capH = h;
    }
    g_dragging = false;
    g_haveSel  = false;
    g_mode     = MODE_SELECTING;
}

// Crop the frozen frame to the drag box, fire the read, enter Review.
void FinishSelection(ImVec2 disp)
{
    ImVec2 a(Mn(g_dragA.x, g_dragB.x), Mn(g_dragA.y, g_dragB.y));
    ImVec2 b(Mx(g_dragA.x, g_dragB.x), Mx(g_dragA.y, g_dragB.y));

    // A stray click (no real drag) cancels the overlay.
    if (b.x - a.x < 8.0f || b.y - a.y < 8.0f) { ExitToIdle(false); return; }

    // Display coords -> frame pixel coords.
    float sx = disp.x > 0 ? (float)g_srvW / disp.x : 1.0f;
    float sy = disp.y > 0 ? (float)g_srvH / disp.y : 1.0f;
    int x0 = (int)(a.x * sx), y0 = (int)(a.y * sy);
    int x1 = (int)(b.x * sx), y1 = (int)(b.y * sy);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > g_srvW) x1 = g_srvW;
    if (y1 > g_srvH) y1 = g_srvH;
    int cw = x1 - x0, ch = y1 - y0;
    if (cw < 8 || ch < 8) { ExitToIdle(false); return; }

    std::vector<uint8_t> crop((size_t)cw * ch * 3);
    {
        std::lock_guard<std::mutex> lk(g_capMtx);
        if (g_capW <= 0 || (int)g_capPixels.size() < g_capW * g_capH * 3)
        {
            ExitToIdle(false);
            return;
        }
        for (int yy = 0; yy < ch; ++yy)
        {
            const uint8_t* s = &g_capPixels[((size_t)(y0 + yy) * g_capW + x0) * 3];
            uint8_t*       d = &crop[(size_t)yy * cw * 3];
            memcpy(d, s, (size_t)cw * 3);
        }
        g_capPixels.clear();   // free the full frame; the texture stays for the backdrop
        g_capPixels.shrink_to_fit();
    }

    // Remember the selection (pixel coords) so the Book button can save it.
    g_selX = x0; g_selY = y0; g_selW = cw; g_selH = ch;
    g_haveSel = true;

    // Anchor the panel to the box.
    g_boxDisp    = ImVec4(a.x, a.y, b.x - a.x, b.y - a.y);
    g_anchor     = g_boxDisp;
    g_reposPanel = true;
    g_panelOpen  = true;

    g_lastCrop  = crop;        // keep the crop so the action buttons can re-send it
    g_lastCropW = cw;
    g_lastCropH = ch;

    g_dragging = false;
    g_mode = MODE_REVIEW;
    ClaudeVision::RequestPixels("Read", DRAG_PROMPT, std::move(crop), cw, ch);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
void DrawFrozenOverlay(bool selecting)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 disp = io.DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(disp);
    if (selecting) ImGui::SetNextWindowFocus();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##mai_overlay", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (g_srv)
        dl->AddImage((ImTextureID)(intptr_t)g_srv, ImVec2(0, 0), disp);

    const ImU32 dimCol  = IM_COL32(0, 0, 0, 130);
    const ImU32 lineCol = IM_COL32(255, 210, 90, 255);

    if (selecting)
    {
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##mai_canvas", disp, ImGuiButtonFlags_MouseButtonLeft);
        if (ImGui::IsItemActivated())
        {
            g_dragA = io.MousePos;
            g_dragB = io.MousePos;
            g_dragging = true;
        }
        if (g_dragging && ImGui::IsItemActive())
            g_dragB = io.MousePos;

        ImVec2 a(Mn(g_dragA.x, g_dragB.x), Mn(g_dragA.y, g_dragB.y));
        ImVec2 b(Mx(g_dragA.x, g_dragB.x), Mx(g_dragA.y, g_dragB.y));
        bool haveBox = g_dragging && (b.x - a.x > 2.0f) && (b.y - a.y > 2.0f);

        if (haveBox)
        {
            dl->AddRectFilled(ImVec2(0, 0), ImVec2(disp.x, a.y), dimCol);
            dl->AddRectFilled(ImVec2(0, b.y), ImVec2(disp.x, disp.y), dimCol);
            dl->AddRectFilled(ImVec2(0, a.y), ImVec2(a.x, b.y), dimCol);
            dl->AddRectFilled(ImVec2(b.x, a.y), ImVec2(disp.x, b.y), dimCol);
            dl->AddRect(a, b, lineCol, 0.0f, 0, 2.0f);
        }
        else
        {
            dl->AddRectFilled(ImVec2(0, 0), disp, dimCol);
            const char* hint = "Drag a box around the text for Mystic AI to read.   Esc cancels.";
            ImVec2 ts = ImGui::CalcTextSize(hint);
            dl->AddText(ImVec2((disp.x - ts.x) * 0.5f, disp.y * 0.12f),
                        IM_COL32(255, 255, 255, 230), hint);
        }

        if (g_dragging && ImGui::IsItemDeactivated())
        {
            g_dragging = false;
            FinishSelection(disp);
        }
    }
    else   // Review — the frozen frame with the chosen box still highlighted.
    {
        ImVec2 a(g_boxDisp.x, g_boxDisp.y);
        ImVec2 b(g_boxDisp.x + g_boxDisp.z, g_boxDisp.y + g_boxDisp.w);
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(disp.x, a.y), dimCol);
        dl->AddRectFilled(ImVec2(0, b.y), ImVec2(disp.x, disp.y), dimCol);
        dl->AddRectFilled(ImVec2(0, a.y), ImVec2(a.x, b.y), dimCol);
        dl->AddRectFilled(ImVec2(b.x, a.y), ImVec2(disp.x, b.y), dimCol);
        dl->AddRect(a, b, lineCol, 0.0f, 0, 2.0f);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

// Position the panel just below the anchored box (or above it if there is no
// room), clamped on-screen — so the buttons sit right where the drag ended.
ImVec2 PanelAnchorPos(float panelW, float estH, ImVec2 disp)
{
    const float pad = 10.0f;
    float x = g_anchor.x;
    float y = g_anchor.y + g_anchor.w + pad;        // below the box
    if (y + estH > disp.y - pad)
        y = g_anchor.y - estH - pad;                // ...or above it
    if (y < pad) y = pad;
    if (x + panelW > disp.x - pad) x = disp.x - panelW - pad;
    if (x < pad) x = pad;
    return ImVec2(x, y);
}

// One action button: a GW2 icon if the texture is loaded, otherwise a coloured
// text button. `col` tints the button so each action reads at a glance; `w`
// fixes the width so the action row fits the panel.
bool ActionButton(const char* iconId, const char* label, const char* tip,
                  ImU32 col, float w, float h)
{
    Texture_t* tex = iconId ? Icons::Get(iconId) : nullptr;
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
    ImVec4 cHov(Mn(c.x * 1.30f, 1.0f), Mn(c.y * 1.30f, 1.0f), Mn(c.z * 1.30f, 1.0f), 1.0f);
    ImVec4 cAct(c.x * 0.78f, c.y * 0.78f, c.z * 0.78f, 1.0f);

    // The vendored ImGui predates the str_id ImageButton overload; PushID on
    // the label keeps each button's ID unique.
    ImGui::PushID(label);
    ImGui::PushStyleColor(ImGuiCol_Button,        c);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, cHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  cAct);
    bool clicked = (tex && tex->Resource)
        ? ImGui::ImageButton((ImTextureID)(intptr_t)tex->Resource, ImVec2(h, h))
        : ImGui::Button(label, ImVec2(w, h));
    ImGui::PopStyleColor(3);
    ImGui::PopID();
    if (tip && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tip);
    return clicked;
}

void DrawPanel(bool reviewMode)
{
    ImGuiIO& io = ImGui::GetIO();
    ClaudeVision::State cs = ClaudeVision::GetState();
    bool speaking = ClaudeVision::IsSpeaking();

    // Fresh capture: snap the panel to the box at the saved size. Otherwise the
    // size is the player's — the window is freely drag-resizable.
    if (g_reposPanel)
    {
        ImGui::SetNextWindowPos(PanelAnchorPos(g_PanelW, g_PanelH, io.DisplaySize),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(g_PanelW, g_PanelH), ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        g_reposPanel = false;
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f, 200.0f),
                                        ImVec2(99999.0f, 99999.0f));
    ImGui::SetNextWindowBgAlpha(g_PanelOpacity);

    if (ImGui::Begin("Mystic AI###mai_panel", &g_panelOpen,
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse))
    {
        // Persist a drag-resize once the player releases the window edge.
        ImVec2 ws = ImGui::GetWindowSize();
        float dW = ws.x - g_PanelW, dH = ws.y - g_PanelH;
        if ((dW > 1.0f || dW < -1.0f || dH > 1.0f || dH < -1.0f)
            && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            g_PanelW = ws.x;
            g_PanelH = ws.y;
            SaveSettings();
        }

        ImGui::SetWindowFontScale(g_FontScale);

        // --- status / answer ---
        if (cs == ClaudeVision::State::Waiting)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.2f, 1.0f),
                "Reading the screen...  (%ds)", ClaudeVision::GetElapsedMs() / 1000);
        }
        else if (cs == ClaudeVision::State::Done)
        {
            if (!ClaudeVision::GetLabel().empty())
                ImGui::TextDisabled("%s", ClaudeVision::GetLabel().c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", ClaudeVision::GetResult().c_str());
        }
        else if (cs == ClaudeVision::State::Error)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
                "%s", ClaudeVision::GetResult().c_str());
        }
        else
        {
            ImGui::TextDisabled("Idle.");
        }

        // Copy Text (OCR) runs independently of the Claude-backed actions.
        CopyText::State copySt = CopyText::GetState();
        if (copySt == CopyText::State::Working)
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.2f, 1.0f), "Copying text (OCR)...");
        else if (copySt == CopyText::State::Done)
            ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f),
                "Copied to clipboard: %s", CopyText::GetResult().c_str());
        else if (copySt == CopyText::State::Error)
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f),
                "Copy Text: %s", CopyText::GetResult().c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- action buttons, sitting right at the box ---
        float bsz  = 26.0f * g_ButtonScale;
        float btnH = bsz + ImGui::GetStyle().FramePadding.y * 2.0f;
        bool  busy = (cs == ClaudeVision::State::Waiting)
                     || (copySt == CopyText::State::Working);

        // A distinct tint per action so the row reads at a glance.
        const ImU32 COL_READ  = IM_COL32( 48, 112, 158, 235);
        const ImU32 COL_TP    = IM_COL32(152, 122,  36, 235);
        const ImU32 COL_WIKI  = IM_COL32( 54, 124,  74, 235);
        const ImU32 COL_RSCH  = IM_COL32(104,  76, 150, 235);
        const ImU32 COL_COPY  = IM_COL32( 94, 100, 110, 235);
        const ImU32 COL_BOOK  = IM_COL32(152,  98,  46, 235);
        const ImU32 COL_STOP  = IM_COL32(160,  60,  60, 235);
        const ImU32 COL_CLOSE = IM_COL32( 72,  76,  84, 235);

        if (busy)
        {
            ImGui::TextDisabled("Working...");
            if (speaking || cs == ClaudeVision::State::Waiting)
            {
                ImGui::SameLine();
                if (ActionButton(Icons::STOP, "Stop", "Stop the read / stop speaking.",
                                 COL_STOP, 96.0f * g_ButtonScale, btnH))
                    ClaudeVision::Stop();
            }
        }
        else if (reviewMode && g_haveSel && !g_lastCrop.empty())
        {
            // Six actions in a 3-wide, 2-row grid — each button gets a third of
            // the panel, so even "Research" fits with room to spare.
            float sp = ImGui::GetStyle().ItemSpacing.x;
            float bw = (ImGui::GetContentRegionAvail().x - sp * 2.0f) / 3.0f;
            ImGui::SetWindowFontScale(g_FontScale * 0.85f);

            if (ActionButton(Icons::READ, "Read", "Read the selection aloud again.",
                             COL_READ, bw, btnH))
                ClaudeVision::RequestPixels("Read", DRAG_PROMPT,
                                            g_lastCrop, g_lastCropW, g_lastCropH);
            ImGui::SameLine();
            if (ActionButton(Icons::TP, "TP", "Check this item's Trading Post price.",
                             COL_TP, bw, btnH))
                ClaudeVision::RequestPixels("Trading Post", "@action:trading-post",
                                            g_lastCrop, g_lastCropW, g_lastCropH);
            ImGui::SameLine();
            if (ActionButton(Icons::WIKI, "Wiki", "Add this to your GW2 wiki favorites.",
                             COL_WIKI, bw, btnH))
                ClaudeVision::RequestPixels("Wiki", "@action:wiki-fav",
                                            g_lastCrop, g_lastCropW, g_lastCropH);
            if (ActionButton(Icons::RESEARCH, "Research",
                             "Research this with the GW2 wiki and the web.",
                             COL_RSCH, bw, btnH))
                ClaudeVision::RequestPixels("Research", "@action:research",
                                            g_lastCrop, g_lastCropW, g_lastCropH);
            ImGui::SameLine();
            if (ActionButton(Icons::COPY, "Copy",
                             "OCR the selection to the clipboard (no AI) - paste it "
                             "into GW2's destroy-confirm box.",
                             COL_COPY, bw, btnH))
                CopyText::Request(g_lastCrop, g_lastCropW, g_lastCropH);
            ImGui::SameLine();
            if (ActionButton(Icons::BOOK, "Book",
                             "Save this selection as the static book region. The Read "
                             "Book keybind then re-reads it with no drag.",
                             COL_BOOK, bw, btnH))
            {
                g_BookRegionX = g_selX; g_BookRegionY = g_selY;
                g_BookRegionW = g_selW; g_BookRegionH = g_selH;
                SaveSettings();
                if (APIDefs)
                    APIDefs->GUI_SendAlert("Mystic AI: book region saved. Use the "
                                           "Read Book keybind to re-read it.");
            }

            ImGui::SetWindowFontScale(g_FontScale);
        }

        ImGui::Spacing();
        if (ActionButton(nullptr, "Close", "Close this panel.",
                         COL_CLOSE, 96.0f * g_ButtonScale, btnH))
            g_panelOpen = false;

        // --- settings ---
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Settings###mai_settings"))
        {
            bool save = false;
            ImGui::TextUnformatted("Text size");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##mai_font", &g_FontScale, 0.6f, 2.5f, "%.2f");
            save |= ImGui::IsItemDeactivatedAfterEdit();

            ImGui::TextUnformatted("Button size");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##mai_btn", &g_ButtonScale, 0.6f, 2.5f, "%.2f");
            save |= ImGui::IsItemDeactivatedAfterEdit();

            ImGui::TextUnformatted("Panel opacity");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##mai_opacity", &g_PanelOpacity, 0.2f, 1.0f, "%.2f");
            save |= ImGui::IsItemDeactivatedAfterEdit();
            ImGui::TextDisabled("Drag the panel's edge to resize it.");

            if (g_BookRegionW > 0)
            {
                ImGui::Text("Book region: %dx%d at (%d,%d)",
                    g_BookRegionW, g_BookRegionH, g_BookRegionX, g_BookRegionY);
                if (ImGui::Button("Clear book region"))
                {
                    g_BookRegionX = g_BookRegionY = g_BookRegionW = g_BookRegionH = 0;
                    save = true;
                }
            }
            else
            {
                ImGui::TextDisabled("Book region: not set");
            }
            if (save) SaveSettings();
        }
    }
    ImGui::End();

    if (!g_panelOpen)
        ExitToIdle(false);   // panel dismissed — leave any speech running
}

}  // namespace

// ---------------------------------------------------------------------------
// Public entry points (declared in shared.h)
// ---------------------------------------------------------------------------

void RenderMysticAI()
{
    // Release a retired texture now — safely, before this frame draws anything.
    if (g_needRelease)
    {
        ReleaseFrozenFrame();
        g_needRelease = false;
    }

    ClaudeVision::Poll();
    ProcessCommand();

    // Esc — the WndProc hook (MysticAIWndProc) swallows the key and posts it
    // here, so the panel/overlay closes without GW2 also closing the book or
    // inventory it had open behind it.
    if (g_escConsumed.exchange(false))
    {
        if (g_mode != MODE_IDLE)
            ExitToIdle(true);
        else if (ClaudeVision::GetState() == ClaudeVision::State::Waiting
                 || ClaudeVision::IsSpeaking())
            ClaudeVision::Stop();
    }

    AdvanceCapture();

    if (g_mode == MODE_SELECTING)
    {
        DrawFrozenOverlay(true);
    }
    else if (g_mode == MODE_REVIEW)
    {
        DrawFrozenOverlay(false);
        DrawPanel(true);
    }
    else if (g_mode == MODE_BOOKPANEL)
    {
        DrawPanel(false);
    }

    // Publish for the WndProc hook whether Mystic AI should own Esc this frame.
    g_uiActive.store(g_mode != MODE_IDLE
                     || ClaudeVision::IsSpeaking()
                     || ClaudeVision::GetState() == ClaudeVision::State::Waiting);
}

void ToggleCapture()  { g_cmd.store(CMD_TOGGLE); }

void ReadBookRegion() { g_cmd.store(CMD_BOOK); }

void ShutdownOverlay()
{
    // Called from AddonUnload, after the render callback is deregistered — no
    // pending draw can reference the texture any more.
    ReleaseFrozenFrame();
}

// Nexus WndProc hook. While Mystic AI's panel or overlay is up (or it is still
// reading aloud), swallow Esc: post it for the render thread and return 0 so
// GW2 never sees the key — its book / inventory / window stays open. When
// Mystic AI is idle, Esc passes straight through unchanged.
UINT MysticAIWndProc(HWND, UINT aMsg, WPARAM aWParam, LPARAM)
{
    if (aMsg == WM_KEYDOWN && aWParam == VK_ESCAPE && g_uiActive.load())
    {
        g_escConsumed.store(true);
        return 0;
    }
    return aMsg;
}
