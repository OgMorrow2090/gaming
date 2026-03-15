/**
 * keybinds.cpp
 *
 * Keybind handler for Mystic Trading.
 */

#include "shared.h"

void ProcessKeybind(const char* aIdentifier, bool aIsRelease)
{
    if (aIsRelease) return;

    if (strcmp(aIdentifier, KB_TOGGLE_DASHBOARD) == 0)
    {
        g_ShowDashboard = !g_ShowDashboard;
    }
    else if (strcmp(aIdentifier, KB_TOGGLE_FLIPLIST) == 0)
    {
        g_ShowFlipList = !g_ShowFlipList;
    }
}
