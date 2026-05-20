---
name: Vendored ImGui in this tree has no GetMainViewport
description: The ImGui copy at include/imgui/ is built without multi-viewport, so ImGui::GetMainViewport() and friends don't compile here. Use GetIO().DisplaySize for centring popups instead.
metadata:
  type: project
---

<!-- markdownlint-disable MD041 -->

The ImGui copy in `include/imgui/` is built without the multi-viewport feature
flag. A handful of otherwise-standard ImGui calls don't exist here, most
commonly `ImGui::GetMainViewport()` (and `->GetCenter()`):

```text
error C2039: 'GetMainViewport': is not a member of 'ImGui'
error C3861: 'GetMainViewport': identifier not found
```

CI catches it cleanly — the build went green on mystic-ai / mystic-trading and
failed only on the module that touched the call — but it's an annoying CI
round-trip if you only need to centre a popup.

## How to apply

Don't reach for viewport-based centring:

```cpp
// DOESN'T COMPILE in this tree
ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
```

Derive the centre from `GetIO().DisplaySize` instead — always populated:

```cpp
ImGuiIO& io = ImGui::GetIO();
ImGui::SetNextWindowPos(
    ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
    ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
```

Same visual result, no API dependency. The same pattern applies to any
window-size or position math that would otherwise reach for the viewport.

If multi-viewport (drag-out-of-game-window panels, per-popup OS windows) is
ever genuinely wanted, rebuild ImGui with the viewport flags in
`include/imgui/imconfig.h` — Nexus doesn't prevent it, but the existing UI
doesn't need it.
