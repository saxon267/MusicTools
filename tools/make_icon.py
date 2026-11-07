"""Render the MusicTools app icon: the circle-of-fifths ring on a dark tile.

    python tools/make_icon.py

Writes docs/icon.png (512), docs/icon-128.png and docs/icon-64.png.
"""
from __future__ import annotations

import colorsys
import math
import os

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs")
SS = 4  # supersample


def hsv(h: float, s: float, v: float) -> tuple[int, int, int]:
    r, g, b = colorsys.hsv_to_rgb((h % 360) / 360.0, s, v)
    return int(r * 255), int(g * 255), int(b * 255)


def seg_points(cx, cy, r0, r1, a0, a1, steps=10):
    pts = []
    for i in range(steps + 1):
        a = math.radians(a0 + (a1 - a0) * i / steps)
        pts.append((cx + r1 * math.cos(a), cy + r1 * math.sin(a)))
    for i in range(steps + 1):
        a = math.radians(a1 + (a0 - a1) * i / steps)
        pts.append((cx + r0 * math.cos(a), cy + r0 * math.sin(a)))
    return pts


def build(size: int) -> Image.Image:
    s = size * SS
    img = Image.new("RGB", (s, s), (14, 17, 22))
    d = ImageDraw.Draw(img)

    # rounded tile: draw the corners back in with the transparent colour later
    cx = cy = s / 2
    r_out = s * 0.42
    r_in = r_out * 0.56

    for i in range(12):
        a0 = -90 - 15 + 30 * i + 1.2
        a1 = a0 + 30 - 2.4
        sel = i == 0
        col = hsv(i * 30, 0.95 if sel else 0.72, 0.98 if sel else 0.86)
        pts = seg_points(cx, cy, r_in, r_out, a0, a1)
        d.polygon(pts, fill=col)
        if sel:
            d.line(pts + [pts[0]], fill=(255, 255, 255), width=max(2, int(s * 0.012)))

    hub = r_in * 0.92
    d.ellipse([cx - hub, cy - hub, cx + hub, cy + hub], fill=(14, 17, 22))
    inner = hub * 0.80
    d.ellipse([cx - inner, cy - inner, cx + inner, cy + inner], outline=(34, 41, 52),
              width=max(1, int(s * 0.005)))

    img = img.convert("RGBA")
    # round the tile corners
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, s - 1, s - 1], radius=int(s * 0.22), fill=255)
    img.putalpha(mask)

    return img.resize((size, size), Image.LANCZOS)


def main() -> None:
    os.makedirs(OUT, exist_ok=True)
    for size, name in ((512, "icon.png"), (128, "icon-128.png"), (64, "icon-64.png")):
        build(size).save(os.path.join(OUT, name))
    print("wrote icons to", OUT)


if __name__ == "__main__":
    main()

