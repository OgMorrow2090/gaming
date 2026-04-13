#!/bin/bash
# Force gamescope to use 4K @ 120Hz as its composited output.
# -W/-H sets output resolution, -r sets composition refresh rate.
# EDID doesn't natively advertise 4K@120 (pixel clock limit), but since
# there's no physical display, gamescope's internal framebuffer doesn't
# need to match a "real" mode. Sunshine captures the framebuffer directly.
exec /usr/bin/gamescope "$@" -W 3840 -H 2160 -r 120
