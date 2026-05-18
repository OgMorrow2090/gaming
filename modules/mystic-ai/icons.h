#pragma once

// GW2-style button icons for Mystic AI. The PNGs are embedded in the DLL as
// Win32 resources (see resources.rc / resource.h) so the addon stays a single
// self-contained file. Icons::Load() registers them with Nexus at addon load;
// Icons::Get() returns the Nexus texture for drawing with ImGui::ImageButton.

struct Texture_t;

namespace Icons {

// Texture identifiers — passed to Nexus and to QuickAccess_Add.
constexpr const char* QA        = "MAI_TEX_QA";
constexpr const char* QA_HOVER  = "MAI_TEX_QA_HOVER";
constexpr const char* READ      = "MAI_TEX_READ";
constexpr const char* TP        = "MAI_TEX_TP";
constexpr const char* WIKI      = "MAI_TEX_WIKI";
constexpr const char* RESEARCH  = "MAI_TEX_RESEARCH";
constexpr const char* COPY      = "MAI_TEX_COPY";
constexpr const char* STOP      = "MAI_TEX_STOP";
constexpr const char* BOOK      = "MAI_TEX_BOOK";

// Register every embedded icon with Nexus. Call once from AddonLoad.
void Load();

// Return the Nexus texture for an identifier, or nullptr if not ready yet.
Texture_t* Get(const char* identifier);

}  // namespace Icons
