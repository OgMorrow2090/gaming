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
    else if (strcmp(aIdentifier, KB_TOGGLE_DELIVERY) == 0)
    {
        g_ShowDelivery = !g_ShowDelivery;
    }
    else if (strcmp(aIdentifier, KB_RESET_WINDOWS) == 0)
    {
        ArmResetWindows();
    }
}
