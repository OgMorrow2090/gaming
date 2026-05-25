#!/usr/bin/env python3
"""
generate-icons.py - generates Mystic AI's embedded GW2-style button icons.

Produces 64x64 RGBA PNGs in this directory; resources.rc embeds them in the
DLL so Mystic AI ships as a single self-contained file. Re-run after editing
to regenerate. Requires Pillow (`pip install pillow`).

The icons are simple geometric glyphs in a warm GW2-gold palette on a soft
dark rounded backing - styled to sit naturally in the Nexus UI.
"""

import math
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SS   = 4              # supersample factor (rendered big, downscaled for AA)
SZ   = 64             # final icon size
N    = SZ * SS        # working-canvas size

GOLD        = (222, 186, 108, 255)
GOLD_BRIGHT = (255, 234, 156, 255)
GOLD_DEEP   = (150, 120,  60, 255)
DARK        = ( 14,  11,   6, 255)
BACK_FILL   = ( 40,  34,  23, 225)
BACK_EDGE   = (150, 122,  64, 255)
PAGE_FILL   = ( 56,  48,  33, 255)


def px(v):
    """Icon-space coordinate (0..64) -> working-canvas coordinate."""
    return v * SS


def W(v):
    """Stroke width in icon-space pixels -> canvas pixels (min 1)."""
    return max(1, int(round(v * SS)))


def canvas():
    """A fresh transparent canvas with the soft dark rounded backing."""
    img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    m, r = px(5), px(14)
    d.rounded_rectangle([m, m, N - 1 - m, N - 1 - m], radius=r,
                        fill=BACK_FILL, outline=BACK_EDGE, width=W(1))
    return img, d


def save(img, name):
    img.resize((SZ, SZ), Image.Resampling.LANCZOS).save(os.path.join(HERE, name))


def star4(d, cx, cy, outer, inner, col):
    """A 4-point sparkle: spikes on the axes, inner corners on the diagonals."""
    pts = []
    for k in range(4):
        aa = math.radians(90 * k)
        pts.append((cx + outer * math.cos(aa), cy + outer * math.sin(aa)))
        ad = math.radians(90 * k + 45)
        pts.append((cx + inner * math.cos(ad), cy + inner * math.sin(ad)))
    d.polygon(pts, fill=col)


def icon_qa(bright=False):
    """The Mystic AI mark - a large sparkle with a small companion."""
    img, d = canvas()
    col = GOLD_BRIGHT if bright else GOLD
    star4(d, px(28), px(34), px(19), px(6.5), col)
    star4(d, px(45), px(20), px(9),  px(3.0), col)
    return img


def icon_read():
    """A speaker with sound waves - the read-aloud action."""
    img, d = canvas()
    speaker = [(px(17), px(27)), (px(27), px(27)), (px(38), px(17)),
               (px(38), px(47)), (px(27), px(37)), (px(17), px(37))]
    d.polygon(speaker, fill=GOLD)
    for radius in (9, 15):
        bb = [px(38 - radius), px(32 - radius), px(38 + radius), px(32 + radius)]
        d.arc(bb, -52, 52, fill=GOLD, width=W(3))
    return img


def icon_tp():
    """A stack of coins - the Trading Post price action."""
    img, d = canvas()
    cw, ch = px(15), px(6)
    for cy in (px(40), px(32), px(24)):
        d.ellipse([px(32) - cw, cy - ch, px(32) + cw, cy + ch],
                  fill=GOLD, outline=DARK, width=W(1))
    return img


def icon_wiki():
    """An open book - the Add-to-Wiki action."""
    img, d = canvas()
    left  = [(px(32), px(19)), (px(32), px(45)), (px(13), px(41)), (px(13), px(23))]
    right = [(px(32), px(19)), (px(32), px(45)), (px(51), px(41)), (px(51), px(23))]
    d.polygon(left,  fill=GOLD)
    d.polygon(right, fill=GOLD)
    d.line([(px(32), px(17)), (px(32), px(47))], fill=DARK, width=W(2))
    for i in range(3):
        y = px(26 + i * 5)
        d.line([(px(17), y + px(1.2)), (px(29), y)], fill=DARK, width=W(1))
        d.line([(px(35), y), (px(47), y + px(1.2))], fill=DARK, width=W(1))
    return img


def icon_research():
    """A magnifying glass - the AI research action."""
    img, d = canvas()
    cx, cy, r = px(27), px(27), px(13)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=GOLD, width=W(5))
    d.line([(px(37), px(37)), (px(48), px(48))], fill=GOLD, width=W(7))
    d.ellipse([px(46), px(46), px(50), px(50)], fill=GOLD)
    return img


def icon_copy():
    """Two stacked pages - the copy-to-clipboard (OCR) action."""
    img, d = canvas()
    d.rounded_rectangle([px(18), px(16), px(40), px(44)], radius=px(3),
                        outline=GOLD, width=W(2))
    d.rounded_rectangle([px(26), px(24), px(48), px(52)], radius=px(3),
                        fill=PAGE_FILL, outline=GOLD, width=W(2))
    for i in range(3):
        y = px(32 + i * 5)
        d.line([(px(31), y), (px(43), y)], fill=GOLD, width=W(1))
    return img


def icon_stop():
    """A solid block - stop the read / stop speaking."""
    img, d = canvas()
    d.rounded_rectangle([px(21), px(21), px(43), px(43)], radius=px(4), fill=GOLD)
    return img


def icon_book():
    """A closed book - the saved book region."""
    img, d = canvas()
    d.rounded_rectangle([px(17), px(14), px(47), px(50)], radius=px(3),
                        fill=GOLD, outline=DARK, width=W(1))
    d.rectangle([px(17), px(15), px(24), px(49)], fill=GOLD_DEEP)
    d.line([(px(24), px(14)), (px(24), px(50))], fill=DARK, width=W(1))
    for x in (44, 45.5, 47):
        d.line([(px(x), px(18)), (px(x), px(46))], fill=DARK, width=W(1))
    return img


def icon_pin():
    """A thumbtack - the pin-this-panel-open toggle."""
    img, d = canvas()
    # Head: rounded cap at top
    d.rounded_rectangle([px(20), px(15), px(44), px(27)], radius=px(3),
                        fill=GOLD, outline=DARK, width=W(1))
    # Neck: narrower band beneath the head
    d.rectangle([px(28), px(27), px(36), px(33)], fill=GOLD_DEEP)
    # Shaft: tapered to a point
    pts = [(px(26), px(33)), (px(38), px(33)), (px(32), px(50))]
    d.polygon(pts, fill=GOLD, outline=DARK)
    return img


def main():
    save(icon_qa(False),  "qa.png")
    save(icon_qa(True),   "qa_hover.png")
    save(icon_read(),     "read.png")
    save(icon_tp(),       "tp.png")
    save(icon_wiki(),     "wiki.png")
    save(icon_research(), "research.png")
    save(icon_copy(),     "copy.png")
    save(icon_stop(),     "stop.png")
    save(icon_book(),     "book.png")
    save(icon_pin(),      "pin.png")
    print("Mystic AI: generated 10 icons in", HERE)


if __name__ == "__main__":
    main()
