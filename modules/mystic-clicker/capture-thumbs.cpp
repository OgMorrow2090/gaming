/**
 * capture-thumbs.cpp
 *
 * Per-slot capture thumbnails. See capture-thumbs.h for the contract.
 *
 * Storage:
 *   addons/MysticClicker/thumb-<slot>-<W>x<H>.bmp     (24-bit, top-down)
 *   where <W>x<H> is the game window resolution; the thumbnail itself is
 *   always WIDTH x HEIGHT (see capture-thumbs.h).
 *
 * Why a per-slot version counter: Nexus has no texture-release API, so we
 * can't reuse the same identifier and have it re-read a changed file. Each
 * Capture() bumps the slot's version; Get() then uses identifier
 *   MC_THUMB_<slot>_<W>x<H>_v<n>
 * Nexus loads the new file under the new identifier and the old texture
 * stays cached but orphaned. Per-recapture ~64 KB BGRA — bounded by user
 * behaviour (recaptures-per-session) and the addon lifetime, fine in
 * practice.
 *
 * Author: OgMorrow2090
 * Repository: https://github.com/OgMorrow2090/guildwars2
 */

#include "shared.h"
#include "screen-capture.h"
#include "capture-thumbs.h"

#include <thread>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace {

std::mutex                            g_mtx;
std::unordered_map<std::string, int>  g_version;   // slot -> bump counter

void ClientSize(int& w, int& h)
{
    if (GameWindow != nullptr)
    {
        RECT r;
        if (GetClientRect(GameWindow, &r))
        {
            w = r.right  - r.left;
            h = r.bottom - r.top;
            return;
        }
    }
    w = GetSystemMetrics(SM_CXSCREEN);
    h = GetSystemMetrics(SM_CYSCREEN);
}

std::string ThumbDir()
{
    if (APIDefs && APIDefs->Paths_GetAddonDirectory)
    {
        const char* d = APIDefs->Paths_GetAddonDirectory("MysticClicker");
        if (d) return std::string(d);
    }
    return "";
}

std::string ThumbPath(const std::string& slotId)
{
    int w = 0, h = 0;
    ClientSize(w, h);
    char buf[512];
    sprintf_s(buf, "%s\\thumb-%s-%dx%d.bmp",
              ThumbDir().c_str(), slotId.c_str(), w, h);
    return buf;
}

std::string ThumbIdentifier(const std::string& slotId, int version)
{
    int w = 0, h = 0;
    ClientSize(w, h);
    char buf[256];
    sprintf_s(buf, "MC_THUMB_%s_%dx%d_v%d",
              slotId.c_str(), w, h, version);
    return buf;
}

// Write a 24-bit BGR pixel buffer (top-down, row-major, no padding) as a
// standard Windows BMP. Top-down stored via a negative height in the info
// header — matches the orientation CaptureBackBufferRegion returns, so no
// per-row flipping needed.
void WriteBmp24(const std::string& path,
                const std::vector<uint8_t>& bgr, int w, int h)
{
    const int rowSize  = (w * 3 + 3) & ~3;   // BMP rows are 4-byte aligned
    const int dataSize = rowSize * h;
    const int fileSize = 54 + dataSize;

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return;

    uint8_t hdr[54]{};
    // BITMAPFILEHEADER
    hdr[0] = 'B'; hdr[1] = 'M';
    *(uint32_t*)&hdr[2]  = (uint32_t)fileSize;
    *(uint32_t*)&hdr[10] = 54;                // pixel data offset
    // BITMAPINFOHEADER
    *(uint32_t*)&hdr[14] = 40;
    *(int32_t*) &hdr[18] = w;
    *(int32_t*) &hdr[22] = -h;                // negative = top-down
    *(uint16_t*)&hdr[26] = 1;
    *(uint16_t*)&hdr[28] = 24;
    *(uint32_t*)&hdr[34] = (uint32_t)dataSize;
    f.write((char*)hdr, 54);

    if (rowSize == w * 3)
    {
        f.write((char*)bgr.data(), (std::streamsize)bgr.size());
    }
    else
    {
        std::vector<uint8_t> row(rowSize, 0);
        for (int y = 0; y < h; ++y)
        {
            std::memcpy(row.data(), bgr.data() + (size_t)y * w * 3,
                        (size_t)w * 3);
            f.write((char*)row.data(), rowSize);
        }
    }
}

void CaptureWorker(std::string slot, int clickX, int clickY)
{
    int maxW = 0, maxH = 0;
    ClientSize(maxW, maxH);
    if (maxW < Thumbs::WIDTH || maxH < Thumbs::HEIGHT) return;

    // Centre on the click, then clamp so a full WIDTH x HEIGHT region fits
    // inside the client area.
    int rx = clickX - Thumbs::WIDTH  / 2;
    int ry = clickY - Thumbs::HEIGHT / 2;
    if (rx < 0) rx = 0;
    if (ry < 0) ry = 0;
    if (rx + Thumbs::WIDTH  > maxW) rx = maxW - Thumbs::WIDTH;
    if (ry + Thumbs::HEIGHT > maxH) ry = maxH - Thumbs::HEIGHT;

    std::vector<uint8_t> bgr;
    if (!CaptureBackBufferRegion(rx, ry, Thumbs::WIDTH, Thumbs::HEIGHT, bgr))
        return;

    WriteBmp24(ThumbPath(slot), bgr, Thumbs::WIDTH, Thumbs::HEIGHT);

    std::lock_guard<std::mutex> lk(g_mtx);
    g_version[slot]++;
}

}  // namespace

namespace Thumbs {

void Capture(const char* slotId, int clickX, int clickY)
{
    if (!slotId || !*slotId) return;
    std::thread(CaptureWorker, std::string(slotId), clickX, clickY).detach();
}

Texture_t* Get(const char* slotId)
{
    if (!slotId || !*slotId ||
        !APIDefs || !APIDefs->Textures_GetOrCreateFromFile)
        return nullptr;

    int v;
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        v = g_version[slotId];   // default-constructs to 0 for first call
    }
    std::string slot(slotId);
    std::string id   = ThumbIdentifier(slot, v);
    std::string path = ThumbPath(slot);

    // GetOrCreateFromFile is non-blocking — returns nullptr the very first
    // call, then the loaded texture on subsequent ones once Nexus has read
    // the file off-thread. Cheap to call every frame.
    return APIDefs->Textures_GetOrCreateFromFile(id.c_str(), path.c_str());
}

void DeleteAll()
{
    std::string dir = ThumbDir();
    if (dir.empty()) return;

    // Enumerate thumb-*.bmp in the addon dir and unlink each. Covers every
    // resolution variant in one pass — they all share the prefix.
    std::string pattern = dir + "\\thumb-*.bmp";
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                std::string full = dir + "\\" + fd.cFileName;
                DeleteFileA(full.c_str());
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }

    // Bump every cached slot's version so the next Get() mints a fresh
    // identifier — even if a stale BMP somehow remains, the request is for
    // a new identifier and Nexus will return nullptr until a new file lands.
    std::lock_guard<std::mutex> lk(g_mtx);
    for (auto& kv : g_version) kv.second++;
}

}  // namespace Thumbs
