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
        # Band height: portrait 12%, landscape/hero 18% (small images need taller bands so text reads)
        local BH
        if [ "$H" -lt 600 ]; then
            BH=$((H * 18 / 100))
        else
            BH=$((H * 12 / 100))
        fi
        # Text point size = 65% of band height
        local PT=$((BH * 65 / 100))

        if [ "$variant" = "_hero.png" ]; then
            # Hero: corner BADGE in bottom-right (full-width band gets washed out by
            # Steam's left-to-right white gradient on the detail page that makes the
            # logo overlay readable). Bottom-right is outside the gradient zone.
            local BADGE_W=360
            local BADGE_H=100
            local BADGE_PT=46
            magick "$orig" \
                \( -size "${BADGE_W}x${BADGE_H}" xc:transparent \
                   -fill "$band" \
                   -draw "roundrectangle 0,0 $((BADGE_W-1)),$((BADGE_H-1)) 16,16" \
                   -gravity center \
                   -fill white \
                   -font "$FONT" \
                   -pointsize "$BADGE_PT" \
                   -stroke "rgba(0,0,0,0.4)" \
                   -strokewidth 2 \
                   -annotate 0 "$label" \
                   -stroke none \
                   -annotate 0 "$label" \) \
                -gravity southeast \
                -geometry +50+50 \
                -composite \
                "$f"
            echo "  branded ${variant} (${W}x${H}, badge ${BADGE_W}x${BADGE_H} @ ${BADGE_PT}pt, bottom-right) — $(stat -c %s "$f") bytes"
        else
            # Portrait + landscape: top band (works in tile/card views)
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
            echo "  branded ${variant} (${W}x${H}, band ${BH}px @ ${PT}pt, top) — $(stat -c %s "$f") bytes"
        fi
    done
}

# Apple TV: blue band
brand_one_appid 2879321470 "Apple TV" "rgba(0,80,150,0.92)"
# Steam Deck: orange band (SteamOS-ish)
brand_one_appid 3111887265 "Steam Deck" "rgba(180,60,15,0.92)"

echo ""
echo "=== final art ==="
ls -la "$GRID"/2879321470* "$GRID"/3111887265*
