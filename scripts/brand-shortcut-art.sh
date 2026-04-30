#!/bin/bash
# Brand the GW2 shortcut grid art with profile-name banners.
# Adds a top color band + bold white text on the 3 main variants
# (portrait p.png, landscape .png, hero _hero.png). Skips _logo.png
# since that's the transparent overlay Steam composites onto hero.
#
# Backs up originals to <file>.orig if not already backed up,
# so re-running is safe (always brands from the pristine copy).

set -euo pipefail

GRID="$HOME/.local/share/Steam/userdata/64793831/config/grid"
FONT="/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf"

# Args: appid, label, band_color
brand_one_appid() {
    local appid="$1"
    local label="$2"
    local band="$3"

    echo "=== branding $appid as '$label' (band=$band) ==="

    for variant in "p.png" ".png" "_hero.png"; do
        local f="${GRID}/${appid}${variant}"
        local orig="${f}.orig"

        # Save pristine original on first brand
        if [ ! -f "$orig" ]; then
            cp -f "$f" "$orig"
        fi

        # Always brand from the pristine .orig (idempotent re-runs)
        local dims W H
        dims=$(magick identify -format '%w %h' "$orig")
        W=${dims% *}
        H=${dims#* }
        # Band height = 12% of image height
        local BH=$((H * 12 / 100))
        # Text point size = 55% of band height
        local PT=$((BH * 55 / 100))

        magick "$orig" \
            \( -size "${W}x${BH}" "xc:${band}" \
               -gravity center \
               -fill white \
               -font "$FONT" \
               -pointsize "$PT" \
               -annotate 0 "$label" \) \
            -gravity north \
            -composite \
            "$f"

        echo "  branded ${variant} (${W}x${H}, band ${BH}px @ ${PT}pt) — $(stat -c %s "$f") bytes"
    done
}

# Apple TV: blue band
brand_one_appid 2879321470 "Apple TV" "rgba(0,80,150,0.92)"
# Steam Deck: orange band (SteamOS-ish)
brand_one_appid 3111887265 "Steam Deck" "rgba(180,60,15,0.92)"

echo ""
echo "=== final art ==="
ls -la "$GRID"/2879321470* "$GRID"/3111887265*
