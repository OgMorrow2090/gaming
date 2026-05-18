/**
 * ocr.cpp
 *
 * Reusable OCR utility for Mystic Clicker. Captures a region of the screen,
 * optionally pre-filters by color, asks a host-side daemon to run tesseract,
 * then reads and returns the text.
 *
 * Why a daemon and not direct Wine spawn:
 *   GW2 runs inside Steam Linux Runtime's pressure-vessel sandbox. Wine's
 *   `start /unix` is broken in Proton 11.0 ("Could not translate the
 *   specified Unix filename to a DOS filename"), so we cannot directly
 *   exec /usr/bin/tesseract from inside the sandbox.
 *
 *   Instead, scripts/gw2-ocr-daemon.sh runs as a systemd user unit OUTSIDE
 *   the sandbox. It watches /tmp via inotify, runs tesseract on incoming
 *   BMPs, and writes results back to /tmp. /tmp is bind-mounted into the
 *   sandbox so both sides see the same files — no IPC needed beyond plain
 *   files.
 *
 * Protocol (matched by scripts/gw2-ocr-daemon.sh):
 *   DLL writes  /tmp/gw2-ocr-input-<TS>.bmp        — captured screen region
 *   DLL touches /tmp/gw2-ocr-input-<TS>.ready      — "BMP complete" trigger
 *   daemon writes /tmp/gw2-ocr-output-<TS>.txt     — tesseract output
 *   daemon touches /tmp/gw2-ocr-done-<TS>          — completion marker
 *
 * The .ready trigger exists because the daemon polls /tmp; without it we'd
 * race the DLL's std::ofstream close and OCR a partial BMP.
 *
 * The DLL accesses /tmp via Wine's Z: drive (Z:\ → /). Wait + read are
 * pure file I/O — no Win32 process spawning required.
 */

#include "shared.h"
#include "ocr.h"
#include "screen-capture.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstdio>

// ---------------------------------------------------------------------------
// BMP writer (BI_RGB, 24 bpp). Manual to avoid GDI+ dependency.
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct BmpFileHeader {
    uint16_t bfType;          // 'BM'
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
};
struct BmpInfoHeader {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};
#pragma pack(pop)

// Write a 24bpp BGR pixel buffer (top-down, row-major) as a BI_RGB BMP.
// pixels: width*height*3 bytes, in BGR order, top-left first.
// We write rows bottom-up as required by the BMP format.
// Declared in ocr.h — also reused by claude-vision.cpp.
bool WriteBmp(const std::string& path,
              const std::vector<uint8_t>& pixels,
              int width, int height)
{
    int rowStride = ((width * 3 + 3) / 4) * 4;  // BMP rows are 4-byte aligned
    int padBytes  = rowStride - width * 3;
    uint32_t pixelDataSize = rowStride * height;

    BmpFileHeader fh{};
    fh.bfType    = 0x4D42;  // 'BM'
    fh.bfOffBits = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);
    fh.bfSize    = fh.bfOffBits + pixelDataSize;

    BmpInfoHeader ih{};
    ih.biSize        = sizeof(BmpInfoHeader);
    ih.biWidth       = width;
    ih.biHeight      = height;
    ih.biPlanes      = 1;
    ih.biBitCount    = 24;
    ih.biCompression = 0;
    ih.biSizeImage   = pixelDataSize;

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write((const char*)&fh, sizeof(fh));
    f.write((const char*)&ih, sizeof(ih));

    // Bottom-up: write rows in reverse.
    static const uint8_t pad[3] = { 0, 0, 0 };
    for (int y = height - 1; y >= 0; y--)
    {
        f.write((const char*)&pixels[y * width * 3], width * 3);
        if (padBytes > 0) f.write((const char*)pad, padBytes);
    }
    return f.good();
}

// ---------------------------------------------------------------------------
// Screen capture: read pixels from GW2's D3D11 swap-chain back buffer.
//
// Why not BitBlt(GetDC(nullptr)): GW2 renders via DXVK → Vulkan → gamescope
// on bazzite. The Wine GDI desktop surface is empty as far as the game's
// content is concerned — BitBlt comes back all-black. Reading the back
// buffer through Nexus's IDXGISwapChain is the only thing that works.
// ---------------------------------------------------------------------------

static bool CaptureScreenRegion(int x, int y, int w, int h,
                                std::vector<uint8_t>& outPixels)
{
    return CaptureBackBufferRegion(x, y, w, h, outPixels, 300);
}

// ---------------------------------------------------------------------------
// Optional yellow-color pass: any pixel that ISN'T close to the configured
// hue gets whitened. Tesseract loves high-contrast black-on-white, so after
// this pass the surviving colored pixels become the only OCR-able marks.
//
// In GW2, item-name color follows item rarity (yellow=Rare, green=Masterwork,
// orange=Exotic, etc.). Tooltip body text is white/gray. By zapping
// "near-white" / "near-grey" pixels, we end up isolating just the rarity-
// colored item name — which is what the user wants.
//
// The default filter here biases toward "saturated, not-near-white" which
// catches all rarity colors. Caller can override via OcrOptions.colorTarget
// if they want a specific hue (e.g. only yellow).
// ---------------------------------------------------------------------------

static void ApplyColorIsolation(std::vector<uint8_t>& pixels, int w, int h,
                                const OcrOptions& opts)
{
    int n = w * h;
    for (int i = 0; i < n; i++)
    {
        uint8_t b = pixels[i*3 + 0];
        uint8_t g = pixels[i*3 + 1];
        uint8_t r = pixels[i*3 + 2];

        bool keep = false;
        if (opts.colorTarget == OcrColorTarget::AnyRaritySaturated)
        {
            // Saturated colored = (max - min) > threshold AND brightness above floor.
            // Manual min/max because windows.h defines `max`/`min` as macros that
            // collide with std::max/std::min initializer-list overloads.
            int mx = r; if (g > mx) mx = g; if (b > mx) mx = b;
            int mn = r; if (g < mn) mn = g; if (b < mn) mn = b;
            int sat = mx - mn;
            keep = (sat >= opts.saturationFloor) && (mx >= opts.brightnessFloor);
        }
        else if (opts.colorTarget == OcrColorTarget::Yellow)
        {
            // Yellow: high R + high G, low B
            keep = (r >= 180) && (g >= 150) && (b <= 100);
        }
        // OcrColorTarget::None falls through with keep=false → all kept after the
        // inversion below.
        if (opts.colorTarget == OcrColorTarget::None) keep = true;

        if (!keep)
        {
            pixels[i*3+0] = 255;
            pixels[i*3+1] = 255;
            pixels[i*3+2] = 255;
        }
        else
        {
            // High-contrast: any kept pixel becomes pure black.
            pixels[i*3+0] = 0;
            pixels[i*3+1] = 0;
            pixels[i*3+2] = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// Wait for the host-side daemon to finish OCR.
//
// We've already written the BMP at this point; the daemon's inotify watcher
// will pick it up, run tesseract, and touch a "done" marker. We poll for
// that marker. If the marker never appears within the budget, the daemon
// is probably not running — log a clear hint about how to start it.
// ---------------------------------------------------------------------------

static bool WaitForOcrDone(const std::string& doneMarker, int timeoutMs)
{
    int waited = 0;
    while (waited < timeoutMs)
    {
        if (GetFileAttributesA(doneMarker.c_str()) != INVALID_FILE_ATTRIBUTES)
            return true;
        Sleep(50);
        waited += 50;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------

OcrResult OcrScreenRegion(int x, int y, int w, int h, const OcrOptions& opts)
{
    OcrResult out{};

    // Generate a unique-ish suffix so concurrent calls don't collide.
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    char suffix[32];
    snprintf(suffix, sizeof(suffix), "%lld", (long long)now);

    std::string bmpPath   = std::string("/tmp/gw2-ocr-input-")  + suffix + ".bmp";
    std::string readyMark = std::string("/tmp/gw2-ocr-input-")  + suffix + ".ready";
    std::string outTxt    = std::string("/tmp/gw2-ocr-output-") + suffix + ".txt";
    std::string doneMark  = std::string("/tmp/gw2-ocr-done-")   + suffix;

    std::vector<uint8_t> pixels;
    if (!CaptureScreenRegion(x, y, w, h, pixels))
    {
        out.error = "screen capture failed";
        return out;
    }

    if (opts.colorTarget != OcrColorTarget::None)
    {
        // Forensics: dump the RAW (pre-filter) image too so we can verify
        // (a) the capture region included what we expected and (b) inspect
        // the actual screen-space color of the target text. Path mirrors the
        // filtered BMP but with `-raw` suffix.
        std::string rawPath = std::string("Z:/tmp/gw2-ocr-input-")
                            + suffix + "-raw.bmp";
        WriteBmp(rawPath, pixels, w, h);
        ApplyColorIsolation(pixels, w, h, opts);
    }

    // Wine maps Z: → Linux root, so /tmp/foo from the host == Z:\tmp\foo from
    // inside Wine. The host-side daemon picks up the BMP via inotify on /tmp.
    std::string bmpFullPath = std::string("Z:") + bmpPath;
    if (!WriteBmp(bmpFullPath, pixels, w, h))
    {
        out.error = "BMP write failed: " + bmpFullPath;
        if (APIDefs)
        {
            char msg[400];
            snprintf(msg, sizeof(msg),
                     "BMP write failed for path: %s", bmpFullPath.c_str());
            APIDefs->Log(LOGL_WARNING, "MysticAI", msg);
        }
        return out;
    }
    // Always log what we just wrote — helps diagnose path-translation issues
    // when the daemon doesn't see the file. Will downgrade to DEBUG once stable.
    if (APIDefs)
    {
        char msg[400];
        snprintf(msg, sizeof(msg),
                 "OCR wrote BMP: %s (%d×%d, %zu bytes BGR)",
                 bmpFullPath.c_str(), w, h, pixels.size());
        APIDefs->Log(LOGL_INFO, "MysticAI", msg);
    }

    // Touch the .ready marker AFTER the BMP is fully written. The daemon polls
    // /tmp every 100ms looking for these markers; without it we'd race its
    // poll vs. our std::ofstream close and OCR a partial BMP.
    {
        std::ofstream ready(std::string("Z:") + readyMark);
        // ofstream's destructor closes/flushes — file exists once we leave scope.
    }

    // Wait for the daemon's done marker. 3s budget covers daemon poll latency
    // (≤100ms) + tesseract runtime (~150-300ms on a 700×500 region) + FS sync.
    if (!WaitForOcrDone(std::string("Z:") + doneMark, 3000))
    {
        out.error = "OCR timed out — gw2-ocr-daemon.service not running on bazzite?";
        if (APIDefs) APIDefs->Log(LOGL_WARNING, "MysticAI",
            "OCR timed out. Check daemon: systemctl --user status gw2-ocr-daemon. "
            "Forensics: BMP and ready marker preserved on disk for inspection.");
        // Intentionally DON'T delete on timeout — leave forensics for debugging.
        // The daemon will clean stale entries on next successful run.
        return out;
    }

    // Read the text output file the daemon wrote.
    std::ifstream f(std::string("Z:") + outTxt);
    if (f.is_open())
    {
        std::stringstream ss;
        ss << f.rdbuf();
        out.text = ss.str();
        out.success = true;
    }
    else
    {
        out.error = "failed to read tesseract output: " + outTxt;
    }

    if (!opts.keepArtifacts)
    {
        DeleteFileA((std::string("Z:") + bmpPath).c_str());
        DeleteFileA((std::string("Z:") + outTxt).c_str());
        DeleteFileA((std::string("Z:") + doneMark).c_str());
    }

    return out;
}

OcrResult OcrAroundCursor(int width, int height, const OcrOptions& opts)
{
    POINT cursor;
    GetCursorPos(&cursor);
    int x = cursor.x - width / 2;
    int y = cursor.y - height / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    return OcrScreenRegion(x, y, width, height, opts);
}
