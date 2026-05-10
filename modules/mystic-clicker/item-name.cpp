/**
 * item-name.cpp
 *
 * Copy-Item-Name macro — destroy-dialog flow:
 *   1. User initiates a destroy: GW2 pops the "Type the item's name to confirm"
 *      dialog. Inside the dialog, the item name is rendered in YELLOW.
 *   2. User clicks into the textbox (so it has keyboard focus) and presses the
 *      macro hotkey (L1+DPad-East Long_Press → F10).
 *   3. We OCR a region around the cursor with a yellow-pixel filter, isolating
 *      the item name. Whatever lands on top is written to the clipboard.
 *   4. We send Ctrl+V — the focused textbox receives the paste, item name fills
 *      in, user clicks Confirm.
 *
 * Why "yellow" filter (not the old AnyRaritySaturated):
 *   In the destroy popup the item name is yellow regardless of the item's
 *   rarity, so a tight yellow filter is rock-solid here. (Tooltip-OCR would
 *   want AnyRaritySaturated, but that's a different macro.)
 *
 * Requires `tesseract` package on bazzite (rpm-ostree install tesseract).
 * Reusable OCR primitives live in modules/mystic-clicker/ocr.{h,cpp}.
 */

#include "shared.h"
#include "ocr.h"
#include <wininet.h>
#include <thread>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <mutex>

#pragma comment(lib, "wininet.lib")

// ============================================================================
// MumbleLink — detect whether GW2 chat input currently has focus
//
// Reading the GW2 context's uiState bitfield is the only way to tell whether
// our `Enter` keystroke would (a) open chat fresh (chat was closed) or
// (b) send whatever's sitting in the chat input (chat was already focused).
// Without this check, a previous test attempt that left a chat link in the
// input would get publicly broadcast on the next macro run.
//
// MumbleLink layout: standard 1364-byte struct exposed by GW2 via shared
// memory. The 256-byte `context` blob contains a GW2-specific struct whose
// 49th byte begins the uiState uint32. Bit 5 = TextboxHasFocus.
// ============================================================================

#pragma pack(push, 1)
struct GW2MumbleContext
{
    unsigned char serverAddress[28];
    uint32_t mapId;
    uint32_t mapType;
    uint32_t shardId;
    uint32_t instance;
    uint32_t buildId;
    uint32_t uiState;          // bit 5 = TEXTBOX_HAS_FOCUS
    // (rest of fields ignored)
};
struct MumbleLinkedMem
{
    uint32_t uiVersion;
    uint32_t uiTick;
    float fAvatarPosition[3];
    float fAvatarFront[3];
    float fAvatarTop[3];
    wchar_t name[256];
    float fCameraPosition[3];
    float fCameraFront[3];
    float fCameraTop[3];
    wchar_t identity[256];
    uint32_t context_len;
    unsigned char context[256];
    // wchar_t description[2048];  // unused
};
#pragma pack(pop)

static const uint32_t UI_STATE_TEXTBOX_HAS_FOCUS = 1u << 5;

static bool ChatHasFocus()
{
    if (!APIDefs || !APIDefs->DataLink_Get) return false;
    void* p = APIDefs->DataLink_Get("DL_MUMBLE_LINK");
    if (!p) return false;
    auto* mem = (MumbleLinkedMem*)p;
    if (mem->uiVersion < 2) return false;
    auto* ctx = (GW2MumbleContext*)mem->context;
    return (ctx->uiState & UI_STATE_TEXTBOX_HAS_FOCUS) != 0;
}

static std::string AddonDir()
{
    if (!APIDefs || !APIDefs->Paths_GetAddonDirectory) return "";
    const char* p = APIDefs->Paths_GetAddonDirectory("MysticClicker");
    return p ? std::string(p) : std::string();
}

// ============================================================================
// Clipboard helpers (CF_UNICODETEXT — chat link is ASCII but GW2 paste path is wide)
// ============================================================================

static std::string ReadClipboardUtf8()
{
    std::string out;
    if (!OpenClipboard(nullptr)) return out;

    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h)
    {
        const wchar_t* w = (const wchar_t*)GlobalLock(h);
        if (w)
        {
            int needed = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
            if (needed > 1)
            {
                out.resize(needed - 1);
                WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), needed, nullptr, nullptr);
            }
            GlobalUnlock(h);
        }
    }
    else
    {
        h = GetClipboardData(CF_TEXT);
        if (h)
        {
            const char* p = (const char*)GlobalLock(h);
            if (p) { out = p; GlobalUnlock(h); }
        }
    }
    CloseClipboard();
    return out;
}

static void WriteClipboardUtf8(const std::string& utf8)
{
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return;

    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
    if (hMem)
    {
        wchar_t* p = (wchar_t*)GlobalLock(hMem);
        if (p)
        {
            MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, p, wlen);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    CloseClipboard();
}

// ============================================================================
// Base64 decode (RFC 4648 standard alphabet, padding tolerant)
// ============================================================================

static std::vector<uint8_t> Base64Decode(const std::string& s)
{
    static const int8_t T[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    };

    std::vector<uint8_t> out;
    int val = 0, bits = 0;
    for (char c : s)
    {
        if (c == '=' || c == ' ' || c == '\r' || c == '\n') continue;
        int8_t v = T[(uint8_t)c];
        if (v < 0) return {};
        val = (val << 6) | v;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back((uint8_t)((val >> bits) & 0xFF));
        }
    }
    return out;
}

// ============================================================================
// Chat link parser
//
// Item link format (GW2 wiki):
//   byte 0: type = 0x02 for item
//   byte 1: stack count (1..255)
//   bytes 2..4: item ID, little-endian, 24-bit
//   bytes 5+: optional skin/upgrade IDs (we don't care)
// ============================================================================

static bool ParseGw2ItemLink(const std::string& clip, uint32_t& outItemId)
{
    size_t start = clip.find("[&");
    if (start == std::string::npos) return false;
    size_t end = clip.find(']', start);
    if (end == std::string::npos) return false;

    std::string b64 = clip.substr(start + 2, end - start - 2);
    auto bytes = Base64Decode(b64);
    if (bytes.size() < 5) return false;
    if (bytes[0] != 0x02) return false;  // not an item link

    outItemId = (uint32_t)bytes[2]
              | ((uint32_t)bytes[3] << 8)
              | ((uint32_t)bytes[4] << 16);
    return outItemId != 0;
}

// ============================================================================
// Item-name cache (id=name lines in addons/MysticClicker/item-name-cache.cfg)
// ============================================================================

static std::mutex g_CacheMutex;
static std::unordered_map<uint32_t, std::string> g_NameCache;
static bool g_CacheLoaded = false;

static std::string CachePath()
{
    std::string dir = AddonDir();
    if (dir.empty()) return "";
    return dir + "\\item-name-cache.cfg";
}

static void LoadCacheIfNeeded()
{
    std::lock_guard<std::mutex> lock(g_CacheMutex);
    if (g_CacheLoaded) return;
    g_CacheLoaded = true;

    std::ifstream f(CachePath());
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line))
    {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        uint32_t id = (uint32_t)std::stoul(line.substr(0, eq));
        std::string name = line.substr(eq + 1);
        if (!name.empty() && name.back() == '\r') name.pop_back();
        if (id && !name.empty()) g_NameCache[id] = name;
    }
}

static std::string LookupCache(uint32_t id)
{
    LoadCacheIfNeeded();
    std::lock_guard<std::mutex> lock(g_CacheMutex);
    auto it = g_NameCache.find(id);
    return it != g_NameCache.end() ? it->second : std::string();
}

static void StoreCache(uint32_t id, const std::string& name)
{
    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_NameCache[id] = name;
    }
    std::ofstream f(CachePath(), std::ios::app);
    if (f.is_open()) f << id << '=' << name << '\n';
}

// ============================================================================
// HTTP fetch
// ============================================================================

static std::string HttpGet(const std::string& url)
{
    std::string result;
    HINTERNET hInternet = InternetOpenA("MysticClicker/0.1", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) return result;

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (url.find("https://") == 0) flags |= INTERNET_FLAG_SECURE;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), nullptr, 0, flags, 0);
    if (hUrl)
    {
        char buf[8192];
        DWORD n = 0;
        while (InternetReadFile(hUrl, buf, sizeof(buf) - 1, &n) && n > 0)
        {
            buf[n] = '\0';
            result.append(buf, n);
        }
        InternetCloseHandle(hUrl);
    }
    InternetCloseHandle(hInternet);
    return result;
}

// Minimal "name" extractor — handles the trivial response shape from
// /v2/items/<id>: {... "name":"Mystic Coin" ...}. Decodes \" and \\ escapes.
static std::string ExtractJsonName(const std::string& json)
{
    const std::string key = "\"name\":\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) return "";
    pos += key.size();

    std::string out;
    while (pos < json.size())
    {
        char c = json[pos++];
        if (c == '\\' && pos < json.size())
        {
            char n = json[pos++];
            switch (n)
            {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            default:  out.push_back(n);  break;  // u-escapes left as-is; rare in item names
            }
            continue;
        }
        if (c == '"') break;
        out.push_back(c);
    }
    return out;
}

// ============================================================================
// Input synthesis helpers
// ============================================================================

static void SendKeyScancode(WORD vk)
{
    INPUT in[2] = {};
    WORD scan = (WORD)MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = vk; in[0].ki.wScan = scan;
    in[0].ki.dwFlags = KEYEVENTF_SCANCODE;
    in[1] = in[0];
    in[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

static void SendKeyChord(WORD modVk, WORD vk)
{
    INPUT in[4] = {};
    WORD modScan = (WORD)MapVirtualKey(modVk, MAPVK_VK_TO_VSC);
    WORD scan = (WORD)MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = modVk; in[0].ki.wScan = modScan; in[0].ki.dwFlags = KEYEVENTF_SCANCODE;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = vk;    in[1].ki.wScan = scan;    in[1].ki.dwFlags = KEYEVENTF_SCANCODE;
    in[2] = in[1]; in[2].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    in[3] = in[0]; in[3].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
}

// Send Shift+Home as an extended-key VK chord. Used to select chat input
// contents from current cursor position back to the start. We deliberately
// avoid Ctrl+A (which is the standard "select all") because in this user's
// canonical InputBinds, Ctrl+A is bound to MYSTIC_FORGE_COMBO — pressing it
// would fire the Forge macro instead of selecting chat text, closing the
// inventory and breaking the macro.
static void SendShiftHome()
{
    INPUT in[4] = {};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_LSHIFT;
    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wVk = VK_HOME;
    in[1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;  // Home is in the extended-key block
    in[2] = in[1]; in[2].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;
    in[3] = in[0]; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
}

static void SendCtrlLeftClickAtCursor()
{
    INPUT in[4] = {};
    WORD ctrlScan = (WORD)MapVirtualKey(VK_LCONTROL, MAPVK_VK_TO_VSC);
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_LCONTROL;
    in[0].ki.wScan = ctrlScan;
    in[0].ki.dwFlags = KEYEVENTF_SCANCODE;

    in[1].type = INPUT_MOUSE;
    in[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    in[2].type = INPUT_MOUSE;
    in[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    in[3] = in[0];
    in[3].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

    SendInput(1, &in[0], sizeof(INPUT));
    Sleep(20);
    SendInput(1, &in[1], sizeof(INPUT));
    Sleep(40);
    SendInput(1, &in[2], sizeof(INPUT));
    Sleep(20);
    SendInput(1, &in[3], sizeof(INPUT));
}

// Mirrors input-sim.cpp's static helper. Re-implemented here to avoid
// header churn for one call site.
static void DrainModifiers()
{
    INPUT clear[6] = {};
    clear[0].type = INPUT_KEYBOARD; clear[0].ki.wVk = VK_LCONTROL; clear[0].ki.dwFlags = KEYEVENTF_KEYUP;
    clear[1].type = INPUT_KEYBOARD; clear[1].ki.wVk = VK_RCONTROL; clear[1].ki.dwFlags = KEYEVENTF_KEYUP;
    clear[2].type = INPUT_KEYBOARD; clear[2].ki.wVk = VK_LSHIFT;   clear[2].ki.dwFlags = KEYEVENTF_KEYUP;
    clear[3].type = INPUT_KEYBOARD; clear[3].ki.wVk = VK_RSHIFT;   clear[3].ki.dwFlags = KEYEVENTF_KEYUP;
    clear[4].type = INPUT_KEYBOARD; clear[4].ki.wVk = VK_LMENU;    clear[4].ki.dwFlags = KEYEVENTF_KEYUP;
    clear[5].type = INPUT_KEYBOARD; clear[5].ki.wVk = VK_RMENU;    clear[5].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(6, clear, sizeof(INPUT));
}

static int ModifiersHeld()
{
    int m = 0;
    if (GetAsyncKeyState(VK_LSHIFT)   & 0x8000) m |= 1;
    if (GetAsyncKeyState(VK_RSHIFT)   & 0x8000) m |= 1;
    if (GetAsyncKeyState(VK_LMENU)    & 0x8000) m |= 2;
    if (GetAsyncKeyState(VK_RMENU)    & 0x8000) m |= 2;
    if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) m |= 4;
    if (GetAsyncKeyState(VK_RCONTROL) & 0x8000) m |= 4;
    return m;
}

// ============================================================================
// Public entry point — OCR yellow item name in destroy popup, paste into
// focused textbox.
//
// Pre-conditions caller is responsible for: destroy popup is visible, textbox
// has focus (user clicked it). We OCR around the cursor (which is in / near
// the textbox), isolate yellow pixels (the item name), then Ctrl+V paste.
// ============================================================================

void SimulateCopyItemName()
{
    std::thread([] {
        // Wait for any chord-held modifiers to release. Until they do, our
        // Ctrl+V would land on top of the user's still-held chord and either
        // misfire or get eaten. 1.5s budget is plenty for human release time.
        int waited = 0;
        while (ModifiersHeld() != 0 && waited < 1500) { Sleep(50); waited += 50; }

        // Capture a region big enough to cover the whole destroy popup. At
        // 2560×1440 (Apple TV stream) the dialog is ~1100px wide and the
        // yellow item name sits in the top-right, often 400+px right of the
        // textbox. A 1800×900 region centered on the cursor (textbox) gives
        // 900px on each horizontal side — comfortably covers the entire
        // dialog regardless of where in the textbox the user clicked.
        // Higher pixel count (≈4.9 MB BGR) but tesseract still runs in
        // <500ms on the host-side daemon.
        const int CAPTURE_W = 1800;
        const int CAPTURE_H = 900;

        OcrOptions opts;
        opts.colorTarget = OcrColorTarget::Yellow;  // GW2 destroy-dialog item-name color
        opts.psm = 6;                                // uniform block of text
        opts.lang = "eng";

        OcrResult ocr = OcrAroundCursor(CAPTURE_W, CAPTURE_H, opts);

        if (!ocr.success)
        {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "Copy Item Name: OCR failed — %s", ocr.error.c_str());
            APIDefs->Log(LOGL_WARNING, "MysticClicker", buf);
            APIDefs->GUI_SendAlert("Copy Item Name: OCR failed");
            return;
        }

        // Tesseract typically returns several lines. Some are clean item-name
        // text ("Pact Fleet Axe", "Skin") and some are noise from inventory
        // icons whose yellow accents survived the color filter ("[ oa cat
        // atl po en"). We need to:
        //   1. Reject lines that are mostly garbage (non-letter chars dominate)
        //   2. Keep clean lines (letters + spaces + a few common punctuators)
        //   3. Concatenate them — GW2 wraps long item names mid-string in the
        //      destroy dialog, so the name often spans two lines.
        auto isLetter = [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        };
        auto isAllowed = [&](unsigned char c) {
            return isLetter(c) || c == ' ' || c == '\'' || c == '-';
        };
        auto isCleanLine = [&](const std::string& s) {
            if (s.size() < 3) return false;
            int letters = 0, allowed = 0, total = 0;
            for (char c : s)
            {
                total++;
                if (isLetter((unsigned char)c)) letters++;
                if (isAllowed((unsigned char)c)) allowed++;
            }
            // Need at least 3 letters AND 80% of chars must be allowed
            return letters >= 3 && allowed * 5 >= total * 4;
        };

        std::vector<std::string> cleanLines;
        std::stringstream ss(ocr.text);
        std::string line;
        while (std::getline(ss, line))
        {
            while (!line.empty() &&
                   (line.back() == '\r' || line.back() == ' ' ||
                    line.back() == '\t' || line.back() == '\n'))
                line.pop_back();
            size_t start = 0;
            while (start < line.size() &&
                   (line[start] == ' ' || line[start] == '\t'))
                start++;
            line = line.substr(start);
            if (isCleanLine(line))
                cleanLines.push_back(line);
        }

        std::string best;
        for (size_t i = 0; i < cleanLines.size(); i++)
        {
            if (i > 0) best += " ";
            best += cleanLines[i];
        }

        if (best.empty())
        {
            APIDefs->Log(LOGL_WARNING, "MysticClicker",
                "Copy Item Name: no yellow text found — make sure the destroy popup is visible");
            APIDefs->GUI_SendAlert("Copy Item Name: no item text detected");
            return;
        }

        WriteClipboardUtf8(best);

        // Auto-paste into the focused textbox. The user clicked the textbox
        // before pressing the chord, and our hotkey (F10) doesn't steal focus,
        // so the textbox is still the active control.
        Sleep(40);
        SendKeyChord(VK_LCONTROL, 'V');

        char msg[360];
        snprintf(msg, sizeof(msg),
                 "Copy Item Name: \"%s\" copied + pasted", best.c_str());
        APIDefs->Log(LOGL_INFO, "MysticClicker", msg);
    }).detach();
}
