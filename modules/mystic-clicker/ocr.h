#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Color-isolation strategy applied to the captured image before OCR.
// Tesseract works far better on high-contrast black-on-white, so when we know
// the text we want is in a specific color (e.g. GW2 rarity-colored item names
// in tooltips) we mask everything else to white and the text to black.
enum class OcrColorTarget {
    None,                    // no filtering — OCR the raw screenshot
    AnyRaritySaturated,      // any saturated non-white pixel — catches all GW2 rarity colors
    Yellow,                  // only yellow-ish pixels (GW2 Rare item names)
};

struct OcrOptions {
    OcrColorTarget colorTarget = OcrColorTarget::None;
    int            saturationFloor = 60;   // for AnyRaritySaturated
    int            brightnessFloor = 140;  // for AnyRaritySaturated
    int            psm = 6;                // tesseract page-segmentation mode
    std::string    lang = "eng";
    bool           keepArtifacts = false;  // leave /tmp BMP+TXT for debugging
};

struct OcrResult {
    bool        success = false;
    std::string text;     // raw tesseract output (with newlines)
    std::string error;    // populated when success=false
};

// Capture a screen rectangle, optionally pre-filter by color, run Tesseract
// (must be installed at /usr/bin/tesseract on bazzite — `rpm-ostree install
// tesseract`), return the text. Reusable from any Mystic Clicker macro.
OcrResult OcrScreenRegion(int x, int y, int w, int h, const OcrOptions& opts);

// Convenience: rectangle of given size centered on the cursor.
OcrResult OcrAroundCursor(int width, int height, const OcrOptions& opts);

// Write a 24bpp BGR pixel buffer (top-down, row-major) as a BI_RGB BMP.
// Exposed here so the Claude-vision capture path (claude-vision.cpp) can reuse
// the same writer instead of duplicating it.
bool WriteBmp(const std::string& path, const std::vector<uint8_t>& pixels,
              int width, int height);
