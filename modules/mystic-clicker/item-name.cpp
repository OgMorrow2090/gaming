/**
 * item-name.cpp
 *
 * Copy-Item-Name macro: hover an item in inventory, press the hotkey, the
 * item's display name lands on the Windows clipboard. Lets the user paste it
 * into GW2's "type the item name to confirm destroy" dialog rather than
 * typing long names on a controller keyboard.
 *
 * Flow on dispatch (detached thread):
 *   1. Save current cursor pos + clipboard (for restore on failure).
 *   2. Wait for any chord-held modifiers to release (chord may have been
 *      Ctrl+Shift+something).
 *   3. Esc → small wait → Enter to land in a fresh empty chat input. Esc-
 *      then-Enter avoids accidentally sending whatever the user had typed
 *      previously in chat.
 *   4. Restore cursor to its captured position, send Ctrl+LeftClick via
 *      SendInput. GW2 inserts the item chat link `[&AgEAAAAA]` into chat.
 *   5. Ctrl+A → Ctrl+C → Esc. Clipboard now holds the chat link.
 *   6. Parse: base64-decode the bytes between `[&` and `]`. Item link starts
 *      with 0x02; bytes [2..4] (little-endian) are the 24-bit item ID.
 *   7. Cache lookup at addons/MysticClicker/item-name-cache.cfg. On miss,
 *      HTTP GET https://api.guildwars2.com/v2/items/<id>?lang=en, parse
 *      `"name":"..."`, persist to cache.
 *   8. Write item name to clipboard. On any failure: restore original
 *      clipboard + GUI alert.
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
// Public entry point — OCR-based capture
//
// Old approach (chat-link → API) kept hitting input-pipeline conflicts:
// Enter sometimes sent pending chat text, Ctrl+A collided with MYSTIC_FORGE_COMBO,
// modifier timing was flaky on Wine + Sunshine. Replaced with a screen-capture
// + Tesseract pipeline that needs zero input simulation. User hovers an item,
// presses the hotkey, the tooltip is OCR'd in place, the rarity-colored item
// name is isolated and copied to the clipboard. Inventory stays open.
//
// Requires `tesseract` package on bazzite (rpm-ostree install tesseract +
// reboot, ~5 MB layered). See modules/mystic-clicker/ocr.cpp for the
// reusable OCR primitives.
// ============================================================================

void SimulateCopyItemName()
{
    std::thread([] {
        // Wait for chord modifiers to release before any keyboard simulation
        // would have run. We don't drive the keyboard here, but if other
        // chords overlap, briefly waiting prevents capturing a transitional
        // tooltip state.
        int waited = 0;
        while (ModifiersHeld() != 0 && waited < 1500) { Sleep(50); waited += 50; }

        // Generous capture rectangle around the cursor — covers the GW2
        // tooltip whether it appears above-right (typical) or above-left
        // (when item is on right edge of inventory). 700x500 is enough for
        // even long item names + a few stat lines worth of context.
        const int CAPTURE_W = 700;
        const int CAPTURE_H = 500;

        OcrOptions opts;
        opts.colorTarget = OcrColorTarget::AnyRaritySaturated;
        opts.psm = 6;  // assume uniform block of text
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

        // Pick the most likely item-name line from tesseract's output. The
        // color filter has already zapped white text, so what remains is
        // (mostly) the rarity-colored name. But OCR may produce blank lines
        // and misread fragments — take the longest non-empty line.
        std::string best;
        std::stringstream ss(ocr.text);
        std::string line;
        while (std::getline(ss, line))
        {
            // Trim trailing CR / spaces.
            while (!line.empty() &&
                   (line.back() == '\r' || line.back() == ' ' ||
                    line.back() == '\t' || line.back() == '\n'))
                line.pop_back();
            // Trim leading spaces.
            size_t start = 0;
            while (start < line.size() &&
                   (line[start] == ' ' || line[start] == '\t'))
                start++;
            line = line.substr(start);

            if (line.size() > best.size())
                best = line;
        }

        if (best.empty())
        {
            APIDefs->Log(LOGL_WARNING, "MysticClicker",
                "Copy Item Name: OCR returned no readable text — hover an item with its tooltip visible");
            APIDefs->GUI_SendAlert("Copy Item Name: no item text detected");
            return;
        }

        WriteClipboardUtf8(best);

        char msg[360];
        snprintf(msg, sizeof(msg),
                 "Copy Item Name: \"%s\" copied to clipboard", best.c_str());
        APIDefs->Log(LOGL_INFO, "MysticClicker", msg);
    }).detach();
}
