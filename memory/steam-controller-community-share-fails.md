# Steam Community controller-layout sharing — why the friend can't see it

Shared a Deck controller config from GW2's native Steam install ("Export Layout")
but a friend browsing GW2's Community Layouts on his own Deck can't find it.

## Likely causes, in order

1. **Wrong save type at export.** The export dialog defaults to **"New Personal
   Save"**, which only cloud-syncs to the exporter's own account — nobody else
   can ever see it. Must explicitly pick **"New Community Layout"**.
2. **Profile privacy.** Community uploads are gated on Steam profile → Edit
   Profile → Privacy Settings → **"Game details" must be Public**. Friends-only
   or Private hides published configs from other users.
3. **Discovery is bad even when correct.** A freshly published layout has 0
   subscribers, so it sorts to the bottom of an unsearchable list — a friend
   scrolling "Community Layouts" can easily miss it even if step 1–2 were done
   right. The friend also needs the game in his library, opened at least once,
   and to be viewing controller settings **on a Steam Deck** (community configs
   are filtered by controller type).

## Reliable alternative

Skip Community discovery — export/back up the `.vdf` file directly (repo:
`configs/steam-controller/`) and have the recipient manually place it under
`~/.local/share/Steam/steamapps/common/Steam Controller Configs/<their-steamid>/config/`,
then select it via the layout editor's "Templates/Saved" browser. See
[gw2-favorites-cleanup](gw2-favorites-cleanup.md) sibling docs and
`controller-vdf-deploy-checklist.md` for the general VDF-deploy verification
pattern (never trust the embedded title/version — diff by sha256).

Note: this is orthogonal to [[configset-controller-pointer]] — that's about the
non-Steam-shortcut appid problem; this is about the *Steam store game* sharing UX.
