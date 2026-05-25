/**
 * icons.cpp
 *
 * Loads the embedded GW2-style icon PNGs (declared in resources.rc) into Nexus
 * textures so the UI can draw them with ImGui::ImageButton. Keeping the icons
 * embedded as Win32 resources means Mystic AI ships as a single self-contained
 * DLL — nothing extra to deploy.
 */

#include "shared.h"
#include "icons.h"
#include "resource.h"
#include <cstdint>

namespace {

struct IconDef { const char* identifier; uint32_t resourceId; };

const IconDef ICONS[] = {
    { Icons::QA,       IDR_MAI_QA       },
    { Icons::QA_HOVER, IDR_MAI_QA_HOVER },
    { Icons::READ,     IDR_MAI_READ     },
    { Icons::TP,       IDR_MAI_TP       },
    { Icons::WIKI,     IDR_MAI_WIKI     },
    { Icons::RESEARCH, IDR_MAI_RESEARCH },
    { Icons::COPY,     IDR_MAI_COPY     },
    { Icons::STOP,     IDR_MAI_STOP     },
    { Icons::BOOK,     IDR_MAI_BOOK     },
    { Icons::PIN,      IDR_MAI_PIN      },
};

}  // namespace

namespace Icons {

void Load()
{
    if (!APIDefs || !APIDefs->Textures_GetOrCreateFromResource) return;
    for (const IconDef& ic : ICONS)
        APIDefs->Textures_GetOrCreateFromResource(ic.identifier, ic.resourceId, SelfModule);
}

Texture_t* Get(const char* identifier)
{
    if (!APIDefs || !APIDefs->Textures_Get) return nullptr;
    return APIDefs->Textures_Get(identifier);
}

}  // namespace Icons
