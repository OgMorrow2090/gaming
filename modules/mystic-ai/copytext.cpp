/**
 * copytext.cpp
 *
 * The "Copy Text" action: OCR the drag-selected crop with the host-side
 * tesseract daemon (gw2-ocr-daemon) and put the text on the Windows clipboard.
 * No Claude call — this is the cheap, local path, meant for pasting an item
 * name into GW2's destroy-confirmation box.
 *
 * The OCR round-trip blocks (it waits for the daemon), so Request() offloads
 * it to a worker thread; the panel polls GetState().
 */

#include "shared.h"
#include "copytext.h"
#include "ocr.h"
#include <mutex>
#include <thread>
#include <string>
#include <vector>
#include <cstdint>

namespace {

std::mutex      g_mtx;
CopyText::State g_state = CopyText::State::Idle;
std::string     g_result;

// Put UTF-8 text on the Windows clipboard as CF_UNICODETEXT. GW2 reads this
// clipboard when the player pastes (Ctrl+V) — the addon runs inside GW2's
// process, so they share the one Wine clipboard.
void SetClipboard(const std::string& utf8)
{
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wlen > 0)
    {
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (size_t)wlen * sizeof(wchar_t));
        if (h)
        {
            wchar_t* dst = (wchar_t*)GlobalLock(h);
            if (dst)
            {
                MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, dst, wlen);
                GlobalUnlock(h);
                SetClipboardData(CF_UNICODETEXT, h);   // the clipboard owns h now
            }
            else
            {
                GlobalFree(h);
            }
        }
    }
    CloseClipboard();
}

// Tesseract output for an item name is one line plus stray whitespace. Flatten
// newlines/tabs to spaces, trim the ends, and collapse runs of spaces.
std::string Tidy(const std::string& in)
{
    std::string s;
    for (char c : in)
        s += (c == '\r' || c == '\n' || c == '\t') ? ' ' : c;

    size_t a = s.find_first_not_of(' ');
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(' ');
    s = s.substr(a, b - a + 1);

    std::string out;
    bool prevSpace = false;
    for (char c : s)
    {
        if (c == ' ')
        {
            if (!prevSpace) out += c;
            prevSpace = true;
        }
        else
        {
            out += c;
            prevSpace = false;
        }
    }
    return out;
}

void Worker(std::vector<uint8_t> px, int w, int h)
{
    OcrOptions opts;   // defaults — raw OCR, tesseract page-segmentation mode 6
    OcrResult r = OcrPixels(std::move(px), w, h, opts);

    std::lock_guard<std::mutex> lk(g_mtx);
    if (r.success)
    {
        std::string text = Tidy(r.text);
        if (text.empty())
        {
            g_state  = CopyText::State::Error;
            g_result = "no text found in the selection";
        }
        else
        {
            SetClipboard(text);
            g_state  = CopyText::State::Done;
            g_result = text;
            if (APIDefs)
                APIDefs->Log(LOGL_INFO, "MysticAI",
                             ("Copy Text: clipboard set to \"" + text + "\"").c_str());
        }
    }
    else
    {
        g_state  = CopyText::State::Error;
        g_result = r.error.empty() ? "OCR failed" : r.error;
    }
}

}  // namespace

namespace CopyText {

void Request(std::vector<uint8_t> bgrPixels, int w, int h)
{
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (g_state == State::Working) return;   // one OCR at a time
        g_state = State::Working;
        g_result.clear();
    }
    std::thread(Worker, std::move(bgrPixels), w, h).detach();
}

void Reset()
{
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_state != State::Working)   // don't disturb an in-flight OCR
    {
        g_state = State::Idle;
        g_result.clear();
    }
}

State GetState()  { std::lock_guard<std::mutex> lk(g_mtx); return g_state; }

std::string GetResult() { std::lock_guard<std::mutex> lk(g_mtx); return g_result; }

}  // namespace CopyText
