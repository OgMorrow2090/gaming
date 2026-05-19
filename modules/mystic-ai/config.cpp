/**
 * config.cpp
 *
 * Per-resolution settings for Mystic AI, saved to resolution-specific files
 * (mystic-ai-2560x1440.cfg, mystic-ai-1280x800.cfg, ...) in the addon
 * directory — so a 4K profile and a Steam Deck profile keep separate UI scale
 * and book regions, exactly like Mystic Clicker's per-resolution configs.
 */

#include "shared.h"
#include <fstream>
#include <string>
#include <cstdio>
#include <cstdlib>

// --- Settings state (declared in shared.h) ---
float g_FontScale    = 1.0f;
float g_ButtonScale  = 1.0f;
float g_PanelW       = 460.0f;
float g_PanelH       = 340.0f;
float g_PanelOpacity = 0.92f;
int   g_BookRegionX  = 0;
int   g_BookRegionY  = 0;
int   g_BookRegionW  = 0;
int   g_BookRegionH  = 0;
int   g_TpRegionX    = 0;
int   g_TpRegionY    = 0;
int   g_TpRegionW    = 0;
int   g_TpRegionH    = 0;

namespace {

std::string g_ConfigDir;
int         g_ResW = 0;
int         g_ResH = 0;

void CurrentResolution(int& w, int& h)
{
    if (GameWindow != nullptr)
    {
        RECT r;
        if (GetClientRect(GameWindow, &r))
        {
            w = r.right - r.left;
            h = r.bottom - r.top;
            return;
        }
    }
    w = GetSystemMetrics(SM_CXSCREEN);
    h = GetSystemMetrics(SM_CYSCREEN);
}

std::string ConfigPath(int w, int h)
{
    char buf[512];
    sprintf_s(buf, "%s\\mystic-ai-%dx%d.cfg", g_ConfigDir.c_str(), w, h);
    return std::string(buf);
}

// Reset to defaults. The book / TP regions are pixel-coordinate data —
// meaningless across a resolution change — so they always reset with
// everything else.
void ResetDefaults()
{
    g_FontScale    = 1.0f;
    g_ButtonScale  = 1.0f;
    g_PanelW       = 460.0f;
    g_PanelH       = 340.0f;
    g_PanelOpacity = 0.92f;
    g_BookRegionX  = 0;
    g_BookRegionY  = 0;
    g_BookRegionW  = 0;
    g_BookRegionH  = 0;
    g_TpRegionX    = 0;
    g_TpRegionY    = 0;
    g_TpRegionW    = 0;
    g_TpRegionH    = 0;
}

bool ReadConfig(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string line;
    while (std::getline(f, line))
    {
        if (line.rfind("UIScale=", 0) == 0)
        {
            // Migration from the old single combined scale — seed both, clamped.
            float v = (float)atof(line.substr(8).c_str());
            if (v < 0.6f) v = 0.6f; else if (v > 2.5f) v = 2.5f;
            g_FontScale = v; g_ButtonScale = v;
        }
        else if (line.rfind("FontScale=", 0) == 0)
        {
            float v = (float)atof(line.substr(10).c_str());
            if (v >= 0.6f && v <= 2.5f) g_FontScale = v;
        }
        else if (line.rfind("ButtonScale=", 0) == 0)
        {
            float v = (float)atof(line.substr(12).c_str());
            if (v >= 0.6f && v <= 2.5f) g_ButtonScale = v;
        }
        else if (line.rfind("PanelW=", 0) == 0)
        {
            float v = (float)atof(line.substr(7).c_str());
            if (v >= 300.0f && v <= 4000.0f) g_PanelW = v;
        }
        else if (line.rfind("PanelH=", 0) == 0)
        {
            float v = (float)atof(line.substr(7).c_str());
            if (v >= 200.0f && v <= 4000.0f) g_PanelH = v;
        }
        else if (line.rfind("PanelOpacity=", 0) == 0)
        {
            float v = (float)atof(line.substr(13).c_str());
            if (v >= 0.2f && v <= 1.0f) g_PanelOpacity = v;
        }
        else if (line.rfind("BookRegionX=", 0) == 0) g_BookRegionX = atoi(line.substr(12).c_str());
        else if (line.rfind("BookRegionY=", 0) == 0) g_BookRegionY = atoi(line.substr(12).c_str());
        else if (line.rfind("BookRegionW=", 0) == 0) g_BookRegionW = atoi(line.substr(12).c_str());
        else if (line.rfind("BookRegionH=", 0) == 0) g_BookRegionH = atoi(line.substr(12).c_str());
        else if (line.rfind("TpRegionX=", 0) == 0)   g_TpRegionX   = atoi(line.substr(10).c_str());
        else if (line.rfind("TpRegionY=", 0) == 0)   g_TpRegionY   = atoi(line.substr(10).c_str());
        else if (line.rfind("TpRegionW=", 0) == 0)   g_TpRegionW   = atoi(line.substr(10).c_str());
        else if (line.rfind("TpRegionH=", 0) == 0)   g_TpRegionH   = atoi(line.substr(10).c_str());
    }
    return true;
}

}  // namespace

void SetConfigPath(const char* addonDir)
{
    if (addonDir == nullptr) return;
    g_ConfigDir = std::string(addonDir);
    CreateDirectoryA(addonDir, nullptr);
}

void LoadSettings()
{
    if (g_ConfigDir.empty()) return;
    CurrentResolution(g_ResW, g_ResH);
    ResetDefaults();
    if (ReadConfig(ConfigPath(g_ResW, g_ResH)) && APIDefs)
    {
        char buf[256];
        sprintf_s(buf, "Mystic AI: loaded settings for %dx%d", g_ResW, g_ResH);
        APIDefs->Log(LOGL_INFO, "MysticAI", buf);
    }
}

void SaveSettings()
{
    if (g_ConfigDir.empty()) return;
    if (g_ResW == 0 || g_ResH == 0) CurrentResolution(g_ResW, g_ResH);

    std::ofstream f(ConfigPath(g_ResW, g_ResH));
    if (!f.is_open())
    {
        if (APIDefs)
            APIDefs->Log(LOGL_WARNING, "MysticAI", "Mystic AI: could not save settings");
        return;
    }
    f << "# Resolution: " << g_ResW << "x" << g_ResH << "\n";
    f << "FontScale="    << g_FontScale    << "\n";
    f << "ButtonScale="  << g_ButtonScale  << "\n";
    f << "PanelW="       << g_PanelW       << "\n";
    f << "PanelH="       << g_PanelH       << "\n";
    f << "PanelOpacity=" << g_PanelOpacity << "\n";
    f << "BookRegionX="  << g_BookRegionX  << "\n";
    f << "BookRegionY="  << g_BookRegionY  << "\n";
    f << "BookRegionW="  << g_BookRegionW  << "\n";
    f << "BookRegionH="  << g_BookRegionH  << "\n";
    f << "TpRegionX="    << g_TpRegionX    << "\n";
    f << "TpRegionY="    << g_TpRegionY    << "\n";
    f << "TpRegionW="    << g_TpRegionW    << "\n";
    f << "TpRegionH="    << g_TpRegionH    << "\n";
}

void CheckResolutionChange()
{
    int w, h;
    CurrentResolution(w, h);
    if (w == g_ResW && h == g_ResH) return;

    if (APIDefs)
    {
        char buf[256];
        sprintf_s(buf, "Mystic AI: resolution %dx%d -> %dx%d", g_ResW, g_ResH, w, h);
        APIDefs->Log(LOGL_INFO, "MysticAI", buf);
    }
    g_ResW = w;
    g_ResH = h;
    ResetDefaults();
    ReadConfig(ConfigPath(w, h));
}
