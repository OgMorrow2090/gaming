# Development Setup Guide

Complete guide to setting up a development environment for Guild Wars 2 Nexus addon development.

## Prerequisites

### Required Software

| Software | Version | Download |
|----------|---------|----------|
| Visual Studio 2022 | Latest | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| Guild Wars 2 | Latest | [guildwars2.com](https://www.guildwars2.com/) |
| Nexus Addon Loader | Latest | [GitHub Releases](https://github.com/RaidcoreGG/Nexus/releases) |
| Git | Latest | [git-scm.com](https://git-scm.com/) |

### Visual Studio Workloads

Install these workloads via Visual Studio Installer:

- **Desktop development with C++**
  - MSVC v143 build tools
  - Windows 11 SDK (or Windows 10 SDK)
  - C++ CMake tools (optional)

## Project Setup

### 1. Clone Repository

```bash
git clone https://github.com/OgMorrow2090/gaming.git
cd gaming
```

### 2. Get Nexus API Header

Download `Nexus.h` from the official repository:

```bash
# Create include directory
mkdir include

# Download Nexus.h
curl -o include/Nexus.h https://raw.githubusercontent.com/RaidcoreGG/RCGG-lib-nexus-api/main/Nexus.h
```

Or manually download from: https://github.com/RaidcoreGG/RCGG-lib-nexus-api

### 3. Create Visual Studio Project

1. Open Visual Studio 2022
2. Create new project: **Dynamic-Link Library (DLL)**
3. Project name: `InventoryHotkeys`
4. Location: `gaming/src/`
5. Solution name: `GW2Addons`

### 4. Configure Project Properties

Right-click project → Properties:

**General:**
- Configuration Type: `Dynamic Library (.dll)`
- Windows SDK Version: `10.0` (latest)
- Platform Toolset: `Visual Studio 2022 (v143)`
- C++ Language Standard: `ISO C++17 Standard`

**C/C++ → General:**
- Additional Include Directories: `$(ProjectDir)..\include`

**C/C++ → Preprocessor:**
- Preprocessor Definitions: `WIN32;_WINDOWS;NDEBUG` (Release) or `WIN32;_WINDOWS;_DEBUG` (Debug)

**Linker → General:**
- Output File: `$(OutDir)InventoryHotkeys.dll`

**Linker → Input:**
- Additional Dependencies: (none needed for basic addon)

### 5. Create Source Files

Create these files in your project:

#### `entry.cpp`

```cpp
#include <windows.h>
#include "Nexus.h"

// Global API pointer
AddonAPI* APIDefs = nullptr;
AddonDefinition AddonDef{};

// Forward declarations
void AddonLoad(AddonAPI* aApi);
void AddonUnload();
void ProcessKeybind(const char* aIdentifier, bool aIsRelease);

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// Required export - Nexus calls this to get addon info
extern "C" __declspec(dllexport) AddonDefinition* GetAddonDef()
{
    AddonDef.Signature = -54321;  // Unique negative number
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = "Inventory Hotkeys";
    AddonDef.Version.Major = 0;
    AddonDef.Version.Minor = 1;
    AddonDef.Version.Build = 0;
    AddonDef.Version.Revision = 0;
    AddonDef.Author = "ITINYK";
    AddonDef.Description = "Hotkeys for Deposit Materials and Sort Inventory.";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = EAddonFlags::None;
    AddonDef.Provider = EUpdateProvider::GitHub;
    AddonDef.UpdateLink = "https://github.com/OgMorrow2090/gaming";
    
    return &AddonDef;
}

void AddonLoad(AddonAPI* aApi)
{
    APIDefs = aApi;
    
    // Register keybinds
    APIDefs->RegisterKeybindWithString("KB_DEPOSIT_MATERIALS", ProcessKeybind, "CTRL+D");
    APIDefs->RegisterKeybindWithString("KB_SORT_INVENTORY", ProcessKeybind, "CTRL+SHIFT+S");
    
    APIDefs->Log(ELogLevel_INFO, "InventoryHotkeys", "Addon loaded!");
}

void AddonUnload()
{
    // Cleanup keybinds
    APIDefs->DeregisterKeybind("KB_DEPOSIT_MATERIALS");
    APIDefs->DeregisterKeybind("KB_SORT_INVENTORY");
    
    APIDefs->Log(ELogLevel_INFO, "InventoryHotkeys", "Addon unloaded!");
}

void ProcessKeybind(const char* aIdentifier, bool aIsRelease)
{
    if (aIsRelease) return;
    
    if (strcmp(aIdentifier, "KB_DEPOSIT_MATERIALS") == 0)
    {
        APIDefs->Log(ELogLevel_INFO, "InventoryHotkeys", "Deposit Materials triggered!");
        // TODO: Implement click simulation
    }
    else if (strcmp(aIdentifier, "KB_SORT_INVENTORY") == 0)
    {
        APIDefs->Log(ELogLevel_INFO, "InventoryHotkeys", "Sort Inventory triggered!");
        // TODO: Implement click simulation
    }
}
```

### 6. Build Project

1. Set configuration to `Release` and `x64`
2. Build → Build Solution (Ctrl+Shift+B)
3. Output DLL will be in `src/x64/Release/InventoryHotkeys.dll`

## Testing

### Install Nexus

1. Download `d3d11.dll` from [Nexus Releases](https://github.com/RaidcoreGG/Nexus/releases)
2. Place in Guild Wars 2 installation folder (next to `Gw2-64.exe`)
3. Launch game and accept Nexus agreement

### Install Your Addon

1. Create folder: `Guild Wars 2/addons/InventoryHotkeys/`
2. Copy your built DLL to this folder
3. Launch game
4. Press `Ctrl+O` to open Nexus menu
5. Check addon is listed and enabled

### Debug Logging

View logs in-game:
1. Open Nexus menu (`Ctrl+O`)
2. Go to Log tab
3. Filter by "InventoryHotkeys" channel

Log file location: `Guild Wars 2/addons/Nexus.log`

## Troubleshooting

### Addon Not Loading

- Verify DLL is in `addons/` folder or subfolder
- Check `GetAddonDef` is exported (use Dependency Walker)
- Verify APIVersion matches current Nexus version
- Check Nexus.log for error messages

### Build Errors

- Ensure Windows SDK is installed
- Verify include path to `Nexus.h`
- Check for missing semicolons/brackets
- Ensure building for x64 (not x86)

### Runtime Crashes

- Check null pointers before using API results
- Verify ImGui context is set if using ImGui
- Don't block in keybind handlers
- Use try-catch for external calls

## Next Steps

1. Implement input simulation in `ProcessKeybind()`
2. Calculate button positions based on screen size
3. Add MumbleLink integration for UI scaling
4. Create options panel with ImGui

## Resources

| Resource | URL |
|----------|-----|
| Nexus Wiki | https://github.com/RaidcoreGG/Nexus/wiki |
| Nexus API Reference | https://github.com/RaidcoreGG/Nexus/wiki/API |
| Example Addon | https://github.com/RaidcoreGG/GW2-Compass |
| Raidcore Discord | https://discord.gg/Mvk7W7gjE4 |

---

*Development Setup Guide - Guild Wars 2 Addons*
