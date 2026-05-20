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
#include <cstdlib>
#include <cstdio>
#include <string>

// Prompts sent to Claude with the captured image.
static const char* DRAG_PROMPT =
    "This image is a region of the Guild Wars 2 screen that the player selected. "
    "Read the text in it - a tooltip, dialogue, quest text, book page, mail, item "
    "listing, or trading-post prices - in full, top to bottom.";

static const char* BOOK_PROMPT =
    "This image is a page from an in-game Guild Wars 2 book. Read the page text in "
    "full, top to bottom, naturally, as if reading the book aloud.";

namespace {

// --- Render-thread state machine -------------------------------------------
enum Mode { MODE_IDLE, MODE_CAPTURING, MODE_SELECTING, MODE_REVIEW };
int  g_mode        = MODE_IDLE;
bool g_needRelease = false;   // release the frozen texture at the next frame's top

// --- Command posted by the keybind thread, drained by the render thread ----
// CMD_BOOK toggles book-watch mode; CMD_READ voices the panel.
enum Cmd { CMD_NONE, CMD_TOGGLE, CMD_BOOK, CMD_READ };
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
// buttons (Wiki / Research / Copy) can re-send it.
std::vector<uint8_t> g_lastCrop;
int                  g_lastCropW = 0, g_lastCropH = 0;

// The sentence the Read button voices on demand — the SPOKEN: line of an
// overview result, or the whole result for a plain transcription. Refreshed
// every frame while the panel is Done; empty disables the Read button.
std::string g_spokenText;

// Smart-Copy state. A "@COPY|" result sets the Windows clipboard exactly once
// (g_copyHandled remembers the result string already acted on, so the Done
// branch doesn't re-copy every frame); g_copyMsg / g_copyMsgCol is the line
// the panel then shows.
std::string g_copyHandled;
std::string g_copyMsg;
ImVec4      g_copyMsgCol{1, 1, 1, 1};

// --- Settings auto-expand --------------------------------------------------
// The panel auto-grows its height to fit its content every frame (see
// DrawPanel), so the Settings header no longer needs to bump a stored size —
// opening it just adds its controls and the window grows to fit them. These
// are kept only as a record of whether the header is open across frames.
bool  g_settingsOpen   = false;
float g_settingsExtraH = 0.0f;

// --- Box-anchored panel ----------------------------------------------------
ImVec4 g_anchor{0, 0, 0, 0};   // box the panel anchors to (display coords)
bool   g_reposPanel = false;   // snap the panel to the anchor on its next frame
bool   g_panelOpen  = true;

// --- Book read --------------------------------------------------------------
// The Read-Book keybind is a plain one-shot: each press captures the saved
// book region, sends it to the daemon to read aloud, and is done. The user
// presses it again on the next page. The previous "auto-advance watch" that
// re-fired on page-turn detection was removed in 1.1.14 — once a book finished
// the watch kept sampling everything that subsequently appeared in that
// region, which was the opposite of what people actually want.

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
    g_spokenText.clear();
    g_copyHandled.clear();
    g_copyMsg.clear();
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

// Read-Book keybind. Plain one-shot: capture the saved book region, send to the
// daemon to read aloud, done. The user presses the keybind again on the next
// page. No panel ever opens for a book read — the daemon speaks the page, so
// putting the transcribed text on screen would just repeat it. With no saved
// region we show a Nexus toast so the press is never silent.
void StartBookRead()
{
    if (g_BookRegionW <= 0 || g_BookRegionH <= 0)
    {
        if (APIDefs)
            APIDefs->GUI_SendAlert("Mystic AI: no book region saved yet. Drag-select a "
                                   "book page, then click the Book button to save it.");
        return;
    }
    ClaudeVision::RequestRegion("Book", BOOK_PROMPT,
                                g_BookRegionX, g_BookRegionY, g_BookRegionW, g_BookRegionH);
}

// Drain the keybind command. The Read command just voices the panel and never
// disturbs a mode. A keybind press while a mode is active cancels it; from
// idle it starts a fresh capture (Capture) or fires one book read (Read-Book).
// A press while a read / narration is in flight stops it.
void ProcessCommand()
{
    int c = g_cmd.exchange(CMD_NONE);
    if (c == CMD_NONE) return;

    // Read — voice the panel's cached content; leaves the mode untouched.
    if (c == CMD_READ)
    {
        ClaudeVision::Speak(g_spokenText);
        return;
    }

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
    if (c == CMD_TOGGLE)         StartCapture();
    else if (c == CMD_BOOK)      StartBookRead();
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
    ClaudeVision::RequestPixels("Overview", "@action:overview", std::move(crop), cw, ch);
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

// ---------------------------------------------------------------------------
// Trading Post coin panel
//
// action_trading_post in gw2-claude-daemon.py returns, on a successful price
// lookup, a marker line "@TP|buy=..|sell=..|vendor=..|name=.." followed by the
// spoken prose. We parse the marker and draw buy / sell / vendor rows with
// gold-silver-copper coin discs — the look of the portal GW2 pages. Anything
// without the marker (errors, "not tradeable") falls back to wrapped text.
// ---------------------------------------------------------------------------
struct TpData
{
    long long   buy = -1, sell = -1, vendor = -1;  // copper; -1 = none / unknown
    std::string name;
};

// Read an integer "key=NNN" field out of the marker line; -1 if absent.
long long TpField(const std::string& line, const char* key)
{
    size_t p = line.find(key);
    if (p == std::string::npos) return -1;
    return strtoll(line.c_str() + p + strlen(key), nullptr, 10);
}

bool ParseTpMarker(const std::string& result, TpData& out)
{
    if (result.rfind("@TP|", 0) != 0) return false;
    size_t nl = result.find('\n');
    std::string line = (nl == std::string::npos) ? result : result.substr(0, nl);
    out.buy    = TpField(line, "buy=");
    out.sell   = TpField(line, "sell=");
    out.vendor = TpField(line, "vendor=");
    size_t np  = line.find("name=");
    out.name   = (np != std::string::npos) ? line.substr(np + 5) : "";
    return true;
}

// GW2 coin tints.
const ImU32 COIN_GOLD   = IM_COL32(232, 192, 106, 255);
const ImU32 COIN_SILVER = IM_COL32(204, 209, 217, 255);
const ImU32 COIN_COPPER = IM_COL32(202, 134,  92, 255);

// Draw one coin disc at the cursor, then advance the cursor past it. Sized to
// the current font so it tracks the Text-size slider.
void CoinDisc(ImU32 fill)
{
    float d  = ImGui::GetFontSize() * 0.86f;
    float lh = ImGui::GetTextLineHeight();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 c(p.x + d * 0.5f, p.y + lh * 0.5f);
    float  r = d * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(c, r, fill, 20);
    dl->AddCircle(c, r, IM_COL32(0, 0, 0, 170), 20, 1.4f);
    dl->AddCircleFilled(ImVec2(c.x - r * 0.30f, c.y - r * 0.32f), r * 0.32f,
                        IM_COL32(255, 255, 255, 95), 12);
    ImGui::Dummy(ImVec2(d, lh));
}

// One priced row: "Label   12<gold> 34<silver> 56<copper>", or `noneText` when
// the value is unknown (-1). `labelW` aligns every row's coins to one column.
void DrawCoinRow(const char* label, ImU32 labelCol, long long copper,
                 float labelW, const char* noneText)
{
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(labelCol), "%s", label);
    ImGui::SameLine(labelW);
    if (copper < 0) { ImGui::TextDisabled("%s", noneText); return; }

    long long g = copper / 10000;
    long long s = (copper % 10000) / 100;
    long long c = copper % 100;
    if (g > 0)
    {
        ImGui::Text("%lld", g); ImGui::SameLine(0, 3);
        CoinDisc(COIN_GOLD);    ImGui::SameLine(0, 9);
    }
    if (g > 0 || s > 0)
    {
        ImGui::Text("%lld", s); ImGui::SameLine(0, 3);
        CoinDisc(COIN_SILVER);  ImGui::SameLine(0, 9);
    }
    ImGui::Text("%lld", c); ImGui::SameLine(0, 3);
    CoinDisc(COIN_COPPER);
}

// The TP result: item name, then buy / sell / vendor coin rows.
void DrawTpPanel(const TpData& tp)
{
    if (!tp.name.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.45f, 1.0f), "%s", tp.name.c_str());
    ImGui::Spacing();
    float labelW = ImGui::CalcTextSize("Vendor").x + ImGui::GetFontSize() * 1.1f;
    DrawCoinRow("Buy",    IM_COL32(120, 200, 120, 255), tp.buy,    labelW,
                "no buy orders");
    DrawCoinRow("Sell",   IM_COL32(232, 170,  92, 255), tp.sell,   labelW,
                "no sell listings");
    DrawCoinRow("Vendor", IM_COL32(170, 174, 184, 255), tp.vendor, labelW,
                "not sellable");
}

// ---------------------------------------------------------------------------
// Overview panel
//
// The default drag-select action (@action:overview) returns, when the
// selection is an item, four lines:
//   @OV|buy=..|sell=..|vendor=..|name=..   coin marker (mirrors @TP|)
//   ABOUT:<one or two sentence description>
//   USES:<one line: what the item is used for>
//   SPOKEN:<a prose sentence to be voiced on demand>
// We show the name, the ABOUT text, the "Used for" line, and buy / sell /
// vendor coin rows — all on screen, silently. The SPOKEN line is cached for the
// Read button. A selection that is not an item returns plain text with no
// marker and falls back to wrapped text (see DrawPanel).
// ---------------------------------------------------------------------------
struct OverviewData
{
    long long   buy = -1, sell = -1, vendor = -1;  // copper; -1 = none / unknown
    std::string name;
    std::string about;    // the ABOUT: line
    std::string uses;     // the USES: line — "Used for" heading
    std::string spoken;   // the SPOKEN: line — voiced by the Read button
};

bool ParseOverviewMarker(const std::string& result, OverviewData& out)
{
    if (result.rfind("@OV|", 0) != 0) return false;

    // First line is the @OV| marker; ABOUT: / SPOKEN: are later lines.
    size_t nl   = result.find('\n');
    std::string line = (nl == std::string::npos) ? result : result.substr(0, nl);
    out.buy    = TpField(line, "buy=");
    out.sell   = TpField(line, "sell=");
    out.vendor = TpField(line, "vendor=");
    size_t np  = line.find("name=");
    out.name   = (np != std::string::npos) ? line.substr(np + 5) : "";

    // Walk the remaining lines for the ABOUT: / USES: / SPOKEN: prefixes.
    size_t pos = (nl == std::string::npos) ? std::string::npos : nl + 1;
    while (pos != std::string::npos && pos < result.size())
    {
        size_t end = result.find('\n', pos);
        std::string l = result.substr(pos, end == std::string::npos
                                               ? std::string::npos : end - pos);
        if (l.rfind("ABOUT:", 0) == 0)
            out.about = l.substr(6);
        else if (l.rfind("USES:", 0) == 0)
            out.uses = l.substr(5);
        else if (l.rfind("SPOKEN:", 0) == 0)
            out.spoken = l.substr(7);
        pos = (end == std::string::npos) ? std::string::npos : end + 1;
    }
    return true;
}

// The overview result: item name, a short description, a "Used for" line, then
// buy / sell / vendor coin rows.
void DrawOverviewPanel(const OverviewData& ov)
{
    if (!ov.name.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.45f, 1.0f), "%s", ov.name.c_str());
    if (!ov.about.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", ov.about.c_str());
    }
    if (!ov.uses.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "Used for:");
        ImGui::TextWrapped("%s", ov.uses.c_str());
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    float labelW = ImGui::CalcTextSize("Vendor").x + ImGui::GetFontSize() * 1.1f;
    DrawCoinRow("Buy",    IM_COL32(120, 200, 120, 255), ov.buy,    labelW,
                "no buy orders");
    DrawCoinRow("Sell",   IM_COL32(232, 170,  92, 255), ov.sell,   labelW,
                "no sell listings");
    DrawCoinRow("Vendor", IM_COL32(170, 174, 184, 255), ov.vendor, labelW,
                "not sellable");
}

// ---------------------------------------------------------------------------
// Colour-coded Research panel
//
// action_research in gw2-claude-daemon.py returns its write-up as "@SECT\n"
// followed by labelled sections. Each section header is a line of the form
// "#HEADER#" (ABOUT / HOW TO GET / USES / RECIPES / PRICES / TIPS); the prose
// under it follows on the next lines. We draw each header in its own colour and
// wrap the prose beneath it.
// ---------------------------------------------------------------------------

// Map a section name (the text between the # markers) to its heading colour.
ImVec4 SectionColour(const std::string& header)
{
    if (header == "ABOUT")      return ImVec4(0.80f, 0.80f, 0.84f, 1.0f);  // light grey
    if (header == "HOW TO GET") return ImVec4(0.36f, 0.82f, 0.86f, 1.0f);  // cyan
    if (header == "USES")       return ImVec4(0.45f, 0.85f, 0.45f, 1.0f);  // green
    if (header == "RECIPES")    return ImVec4(0.56f, 0.74f, 0.96f, 1.0f);  // light blue
    if (header == "PRICES")     return ImVec4(0.93f, 0.78f, 0.36f, 1.0f);  // gold
    if (header == "TIPS")       return ImVec4(0.96f, 0.62f, 0.30f, 1.0f);  // orange
    return ImVec4(0.62f, 0.78f, 0.96f, 1.0f);                              // default accent
}

// Render a "@SECT" research result section by section. A "#HEADER#" line is a
// coloured heading; everything else wraps as normal text under it.
void DrawSectionedResult(const std::string& result)
{
    // Skip the leading "@SECT" line.
    size_t pos = result.find('\n');
    pos = (pos == std::string::npos) ? result.size() : pos + 1;

    bool firstSection = true;
    while (pos < result.size())
    {
        size_t end = result.find('\n', pos);
        std::string line = result.substr(pos, end == std::string::npos
                                                  ? std::string::npos : end - pos);
        pos = (end == std::string::npos) ? result.size() : end + 1;

        // A header line is "#NAME#" — a leading '#', a trailing '#', no spaces
        // around the markers themselves.
        bool isHeader = line.size() >= 3 && line.front() == '#'
                        && line.back() == '#';
        if (isHeader)
        {
            std::string name = line.substr(1, line.size() - 2);
            if (!firstSection) ImGui::Spacing();
            firstSection = false;
            ImGui::TextColored(SectionColour(name), "%s", name.c_str());
        }
        else if (!line.empty())
        {
            ImGui::TextWrapped("%s", line.c_str());
        }
    }
}

// Auto-sizing panel — measured content height (render thread only).
//
// The panel auto-grows its height to fit whatever it draws, so the player
// never has to drag it taller. The window uses ImGuiWindowFlags_AlwaysAutoResize
// for the natural fit; the width is pinned to g_PanelW via the size constraints
// and the height is capped at AUTO_CAP_FRAC of the game window.
//
// The variable-height part is the status / answer block, so it is drawn inside
// a child region. g_answerContentH is that block's natural height as measured
// last frame: while it fits the budget the child is given that exact height and
// the whole panel grows to fit it; once it overflows the budget the child is
// pinned to the budget and gains a scrollbar, so the panel stays capped and the
// text scrolls. 0 until the first frame is drawn.
constexpr float AUTO_CAP_FRAC = 0.80f;   // panel height ceiling, fraction of screen
float g_answerContentH = 0.0f;

void DrawPanel()
{
    ImGuiIO& io = ImGui::GetIO();
    ClaudeVision::State cs = ClaudeVision::GetState();
    bool speaking = ClaudeVision::IsSpeaking();

    // Height ceiling — beyond this the panel stays capped and the answer
    // scrolls instead of pushing the window off-screen.
    float capH = io.DisplaySize.y * AUTO_CAP_FRAC;

    // Fresh capture: snap the panel to the box. The height is estimated from
    // last frame's measured answer block (or g_PanelH on the very first open),
    // so PanelAnchorPos can place the box above/below sensibly; AlwaysAutoResize
    // then settles the real height on the same frame.
    if (g_reposPanel)
    {
        float estH = g_answerContentH > 0.0f
                     ? Mn(g_answerContentH + 220.0f, capH) : g_PanelH;
        ImGui::SetNextWindowPos(PanelAnchorPos(g_PanelW, estH, io.DisplaySize),
                                ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        g_reposPanel = false;
    }

    // Pin the width to g_PanelW and cap the height. With AlwaysAutoResize the
    // window fits its content within these bounds — height auto-grows, width is
    // fixed, neither axis is drag-resizable.
    ImGui::SetNextWindowSizeConstraints(ImVec2(g_PanelW, 120.0f),
                                        ImVec2(g_PanelW, capH));
    ImGui::SetNextWindowBgAlpha(g_PanelOpacity);

    if (ImGui::Begin("Mystic AI###mai_panel", &g_panelOpen,
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize))
    {
        ImGui::SetWindowFontScale(g_FontScale);

        float bsz  = 26.0f * g_ButtonScale;
        float btnH = bsz + ImGui::GetStyle().FramePadding.y * 2.0f;
        bool  busy = (cs == ClaudeVision::State::Waiting);

        // A distinct tint per action so the row reads at a glance.
        const ImU32 COL_READ  = IM_COL32( 48, 112, 158, 235);
        const ImU32 COL_WIKI  = IM_COL32( 54, 124,  74, 235);
        const ImU32 COL_RSCH  = IM_COL32(104,  76, 150, 235);
        const ImU32 COL_COPY  = IM_COL32( 94, 100, 110, 235);
        const ImU32 COL_BOOK  = IM_COL32(152,  98,  46, 235);
        const ImU32 COL_LEGY  = IM_COL32(150, 112,  40, 235);
        const ImU32 COL_STOP  = IM_COL32(160,  60,  60, 235);

        // --- action buttons, sitting right at the box ---
        // Drawn ABOVE the result so the mouse lands on the buttons, not the
        // text, the moment the panel opens.
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
        else if (g_haveSel && !g_lastCrop.empty())
        {
            // The action row, in a 3-wide grid: Read, Wiki, Research on the
            // first row; Copy, Book, Legendary on the second.
            float sp = ImGui::GetStyle().ItemSpacing.x;
            float bw = (ImGui::GetContentRegionAvail().x - sp * 2.0f) / 3.0f;
            ImGui::SetWindowFontScale(g_FontScale * 0.85f);

            // Row 1 — Read / Wiki / Research.
            // Read voices the panel's cached SPOKEN line on demand — fire-and-
            // forget, so the overview keeps showing while speech plays.
            if (ActionButton(Icons::READ, "Read", "Read this aloud.",
                             COL_READ, bw, btnH))
                ClaudeVision::Speak(g_spokenText);
            ImGui::SameLine();
            if (ActionButton(Icons::WIKI, "Wiki", "Queue this for your GW2 wiki "
                             "favorites - added on the next game launch.",
                             COL_WIKI, bw, btnH))
                ClaudeVision::RequestPixels("Wiki", "@action:wiki-fav",
                                            g_lastCrop, g_lastCropW, g_lastCropH);
            ImGui::SameLine();
            if (ActionButton(Icons::RESEARCH, "Research",
                             "Research this with the GW2 wiki and the web.",
                             COL_RSCH, bw, btnH))
                ClaudeVision::RequestPixels("Research", "@action:research",
                                            g_lastCrop, g_lastCropW, g_lastCropH);

            // Row 2 — Copy / Book / Legendary.
            if (ActionButton(Icons::COPY, "Copy",
                             "Copy this item's name to the clipboard - paste it "
                             "into GW2's destroy-confirm box.",
                             COL_COPY, bw, btnH))
                ClaudeVision::RequestPixels("Copy", "@action:copy-name",
                                            g_lastCrop, g_lastCropW, g_lastCropH);
            ImGui::SameLine();
            if (ActionButton(Icons::BOOK, "Book",
                             "Save this selection as the static book region. The "
                             "Read Book keybind then re-reads it with no drag.",
                             COL_BOOK, bw, btnH))
            {
                g_BookRegionX = g_selX; g_BookRegionY = g_selY;
                g_BookRegionW = g_selW; g_BookRegionH = g_selH;
                SaveSettings();
                if (APIDefs)
                    APIDefs->GUI_SendAlert("Mystic AI: book region saved. Use the "
                                           "Read Book keybind to re-read it.");
            }
            ImGui::SameLine();
            // No Legendary icon asset — ActionButton renders a text button when
            // the icon id is null.
            if (ActionButton(nullptr, "Legendary", "Queue this item for the "
                             "CraftyLegend favourites - added on the next game launch.",
                             COL_LEGY, bw, btnH))
                ClaudeVision::RequestPixels("Legendary", "@action:legendary-fav",
                                            g_lastCrop, g_lastCropW, g_lastCropH);

            ImGui::SetWindowFontScale(g_FontScale);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- status / answer ---
        // The answer is the only variable-height part of the panel, so it is
        // drawn inside a child region. The budget is the height left under the
        // cap once the buttons above and the Settings block below are
        // accounted for. While last frame's measured answer fits the budget the
        // child is sized to it exactly (so the panel grows to fit); once it
        // overflows the child is pinned to the budget and scrolls. When the
        // Settings header is open its controls draw below the child, so more
        // height is reserved for them — keeping the open block clear of the cap.
        // g_settingsExtraH already carries the g_FontScale factor.
        float reserveBelow = 64.0f * g_FontScale
                             + (g_settingsOpen ? g_settingsExtraH : 0.0f);
        float budget = capH - ImGui::GetCursorPosY() - reserveBelow;
        if (budget < 80.0f) budget = 80.0f;
        bool  overflow   = g_answerContentH > budget;
        float childH     = overflow
                           ? budget
                           : (g_answerContentH > 0.0f ? g_answerContentH : budget);
        ImGui::BeginChild("##mai_answer", ImVec2(0.0f, childH), false,
                          overflow ? 0 : ImGuiWindowFlags_NoScrollbar);
        float answerY0 = ImGui::GetCursorPosY();

        if (cs == ClaudeVision::State::Waiting)
        {
            // Animated dots, no digits — a counting number looked like a
            // countdown. The daemon normally replies within a few seconds.
            int dots = (ClaudeVision::GetElapsedMs() / 400) % 3 + 1;
            char wait[40];
            snprintf(wait, sizeof(wait), "Reading the screen%.*s",
                     dots, "...");
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.2f, 1.0f), "%s", wait);
        }
        else if (cs == ClaudeVision::State::Done)
        {
            if (!ClaudeVision::GetLabel().empty())
                ImGui::TextDisabled("%s", ClaudeVision::GetLabel().c_str());
            ImGui::Separator();

            // Dispatch on the result marker. "@OV|" is the silent drag-select
            // overview; "@TP|" the Trading Post coin panel; "@SECT" the
            // colour-coded Research write-up; "@COPY|" the item-name clipboard
            // grab; a Wiki / Legendary favourite returns a plain confirmation
            // line shown as a tick; anything else is plain wrapped text.
            const std::string& res   = ClaudeVision::GetResult();
            const std::string  label = ClaudeVision::GetLabel();
            OverviewData ov;
            TpData       tp;
            if (ParseOverviewMarker(res, ov))
            {
                DrawOverviewPanel(ov);
                g_spokenText = ov.spoken;   // Read button voices this
            }
            else if (ParseTpMarker(res, tp))
            {
                DrawTpPanel(tp);
            }
            else if (res.rfind("@SECT", 0) == 0)
            {
                // Colour-coded Research — render section by section. The whole
                // result is still what the Read button voices.
                DrawSectionedResult(res);
                g_spokenText = res;
            }
            else if (res.rfind("@COPY|", 0) == 0)
            {
                // Set the clipboard exactly once per result, not every frame.
                if (g_copyHandled != res)
                {
                    g_copyHandled = res;
                    std::string name = res.substr(6);   // text after "@COPY|"
                    if (!name.empty())
                    {
                        CopyText::SetClipboard(name);
                        g_copyMsg    = "Copied: " + name;
                        g_copyMsgCol = ImVec4(0.5f, 0.9f, 0.5f, 1.0f);
                    }
                    else
                    {
                        g_copyMsg    = "Couldn't find an item name.";
                        g_copyMsgCol = ImVec4(1.0f, 0.66f, 0.34f, 1.0f);
                    }
                }
                if (!g_copyMsg.empty())
                    ImGui::TextColored(g_copyMsgCol, "%s", g_copyMsg.c_str());
            }
            else if (label == "Wiki" || label == "Legendary")
            {
                // A favourite action — show the daemon's confirmation as a
                // green tick message; it is not spoken. The vendored ImGui font
                // is ASCII-only, so the tick is a plain "+ ". Guard against a
                // short or empty reply so the panel never shows a bare tick.
                ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "+");
                ImGui::SameLine();
                ImGui::TextWrapped("%s", res.empty() ? "Done." : res.c_str());
            }
            else
            {
                ImGui::TextWrapped("%s", res.c_str());
                g_spokenText = res;         // Read button voices the transcription
            }
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

        // Measure how tall the answer drew this frame (cursor delta within the
        // child) so next frame can size — or cap and scroll — the child to fit.
        g_answerContentH = ImGui::GetCursorPosY() - answerY0
                           + ImGui::GetStyle().WindowPadding.y * 2.0f;
        ImGui::EndChild();

        // --- settings ---
        // The Settings header (and its open controls) draw below the answer
        // child; the panel auto-resizes to fit them, so opening Settings just
        // grows the window — no stored size to bump any more.
        ImGui::Spacing();
        bool settingsOpen = ImGui::CollapsingHeader("Settings###mai_settings");
        g_settingsExtraH  = settingsOpen ? 210.0f * g_FontScale : 0.0f;
        g_settingsOpen    = settingsOpen;
        if (settingsOpen)
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
            ImGui::TextDisabled("The panel grows to fit its text automatically.");

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
        ExitToIdle(true);    // panel dismissed — stop any in-flight read / speech
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
    // inventory it had open behind it. The hook only swallows Esc while a
    // Mystic AI window is actually on screen (see the g_uiActive publish
    // below), so a consumed Esc always means "close that window".
    if (g_escConsumed.exchange(false) && g_mode != MODE_IDLE)
        ExitToIdle(true);

    AdvanceCapture();

    if (g_mode == MODE_SELECTING)
    {
        DrawFrozenOverlay(true);
    }
    else if (g_mode == MODE_REVIEW)
    {
        DrawFrozenOverlay(false);
        DrawPanel();
    }

    // Publish for the WndProc hook whether Mystic AI should own Esc this frame.
    // ONLY while a Mystic AI window is on screen (the freeze overlay or the
    // results panel). A book auto-advance watch, an in-flight read or narration
    // all run with NO visible UI — owning Esc then would swallow the key from
    // GW2 and every other addon (e.g. Event Timers' Esc-to-close) for as long
    // as the watch stayed armed. Stop a book watch with a second Read-Book
    // press instead; it is no longer tied to Esc.
    g_uiActive.store(g_mode != MODE_IDLE);
}

void ToggleCapture()  { g_cmd.store(CMD_TOGGLE); }

void ReadBookRegion() { g_cmd.store(CMD_BOOK); }

void ReadPanelAloud() { g_cmd.store(CMD_READ); }

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
