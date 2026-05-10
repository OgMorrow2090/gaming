/**
 * ocr.cpp
 *
 * Reusable OCR utility for Mystic Clicker. Captures a region of the screen,
 * optionally pre-filters by color, spawns Linux Tesseract via Wine's
 * `start /unix` to run native OCR, then reads and returns the text.
 *
 * Designed so other macros can call OcrScreenRegion(...) for their own
 * uses (e.g. reading TP value off an item tooltip, reading dialog text,
 * scraping addon-rendered overlays, etc.).
 *
 * Why Linux Tesseract instead of a bundled Windows build:
 *   - bazzite already has libtesseract; we just need /usr/bin/tesseract via
 *     `rpm-ostree install tesseract` (~5 MB layered package, no addon bloat)
 *   - Wine can spawn the Linux ELF via cmd.exe's `start /unix <path>` extension
 *
 * Files used (all under /tmp on bazzite, accessible to Wine via Z:\tmp\):
 *   /tmp/gw2-ocr-input-<TS>.bmp   — screenshot we capture
 *   /tmp/gw2-ocr-input-<TS>.bmp   — same path used as input to tesseract
 *   /tmp/gw2-ocr-output-<TS>.txt  — tesseract output (we read this)
 */

#include "shared.h"
#include "ocr.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstdio>
#include <algorithm>

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
static bool WriteBmp(const std::string& path,
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
// Screen capture: BitBlt the requested rect into a BGR buffer.
// ---------------------------------------------------------------------------

static bool CaptureScreenRegion(int x, int y, int w, int h,
                                std::vector<uint8_t>& outPixels)
{
    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return false;
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, w, h);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hBmp);

    BOOL ok = BitBlt(hdcMem, 0, 0, w, h, hdcScreen, x, y, SRCCOPY);

    bool result = false;
    if (ok)
    {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth       = w;
        bi.bmiHeader.biHeight      = -h;  // top-down
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 24;
        bi.bmiHeader.biCompression = BI_RGB;

        int rowStride = ((w * 3 + 3) / 4) * 4;
        std::vector<uint8_t> tmp(rowStride * h);
        if (GetDIBits(hdcMem, hBmp, 0, h, tmp.data(), &bi, DIB_RGB_COLORS))
        {
            outPixels.resize(w * h * 3);
            for (int row = 0; row < h; row++)
            {
                memcpy(&outPixels[row * w * 3],
                       &tmp[row * rowStride],
                       w * 3);
            }
            result = true;
        }
    }

    SelectObject(hdcMem, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    return result;
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
            // Saturated colored = (max - min) > threshold AND brightness above floor
            int mx = std::max({(int)r,(int)g,(int)b});
            int mn = std::min({(int)r,(int)g,(int)b});
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
// Spawn Linux tesseract via Wine `start /unix`. Blocks until done.
// ---------------------------------------------------------------------------

static bool RunTesseract(const std::string& bmpPath,
                         const std::string& outBase,  // tesseract appends .txt
                         const OcrOptions& opts)
{
    // bmpPath/outBase are Linux paths (e.g. /tmp/gw2-ocr-...). Tesseract is
    // a Linux ELF, so we don't translate paths into Wine-style — we hand the
    // Linux binary Linux paths directly via Wine's `start /unix` extension.
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "cmd.exe /c start /unix /wait /usr/bin/tesseract \"%s\" \"%s\" -l %s --psm %d 2>/tmp/gw2-ocr-stderr.log",
             bmpPath.c_str(), outBase.c_str(), opts.lang.c_str(), opts.psm);

    int rc = system(cmd);
    if (rc != 0)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "RunTesseract: system() returned %d for cmd: %.180s", rc, cmd);
        if (APIDefs) APIDefs->Log(LOGL_WARNING, "MysticClicker", buf);
        return false;
    }
    return true;
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

    std::string bmpPath  = std::string("/tmp/gw2-ocr-input-")  + suffix + ".bmp";
    std::string outBase  = std::string("/tmp/gw2-ocr-output-") + suffix;
    std::string outTxt   = outBase + ".txt";

    std::vector<uint8_t> pixels;
    if (!CaptureScreenRegion(x, y, w, h, pixels))
    {
        out.error = "screen capture failed";
        return out;
    }

    if (opts.colorTarget != OcrColorTarget::None)
    {
        ApplyColorIsolation(pixels, w, h, opts);
    }

    // Wine maps Z: → Linux root, so /tmp/foo === Z:\tmp\foo from the Wine side.
    // Tesseract runs as a Linux process and reads/writes /tmp directly.
    if (!WriteBmp(std::string("Z:") + bmpPath, pixels, w, h))
    {
        out.error = "BMP write failed";
        return out;
    }
    if (opts.keepArtifacts)
    {
        char msg[300];
        snprintf(msg, sizeof(msg), "OCR captured to %s", bmpPath.c_str());
        if (APIDefs) APIDefs->Log(LOGL_DEBUG, "MysticClicker", msg);
    }

    if (!RunTesseract(bmpPath, outBase, opts))
    {
        out.error = "tesseract spawn failed";
        if (!opts.keepArtifacts)
        {
            DeleteFileA((std::string("Z:") + bmpPath).c_str());
        }
        return out;
    }

    // Read the text output file (Linux path /tmp/...txt → Wine Z:\tmp\....txt).
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
