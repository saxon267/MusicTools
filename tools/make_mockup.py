"""240x135 UI mockup generator for a Cardputer circle-of-fifths reference firmware.

Renders at 4x supersampling, downsamples to the real 240x135 panel grid, then
writes a nearest-neighbour upscale so the pixel budget can be judged at a glance.
"""
from __future__ import annotations

import colorsys
import math
import os

from PIL import Image, ImageDraw, ImageFont

W, H = 240, 135
SS = 4  # supersample factor
FONT_DIR = r"C:\Windows\Fonts"
OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs")

BG = (14, 17, 22)
FG = (232, 236, 241)
DIM = (128, 140, 155)
LINE = (40, 47, 58)

# circle of fifths, starting at C on top
MAJOR = ["C", "G", "D", "A", "E", "B", "F#", "C#", "G#", "D#", "A#", "F"]
MINOR = ["am", "em", "bm", "f#m", "c#m", "g#m", "d#m", "a#m", "fm", "cm", "gm", "dm"]
ROMAN = ["I", "ii", "iii", "IV", "V", "vi", "vii\u00b0"]
CHORDS = ["C", "Dm", "Em", "F", "G", "Am", "B\u00b0"]
NOTES = ["C E G", "D F A", "E G B", "F A C", "G B D", "A C E", "B D F"]


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    name = "arialbd.ttf" if bold else "arial.ttf"
    return ImageFont.truetype(os.path.join(FONT_DIR, name), int(size * SS))


def hsv(h: float, s: float, v: float) -> tuple[int, int, int]:
    r, g, b = colorsys.hsv_to_rgb(h / 360.0, s, v)
    return int(r * 255), int(g * 255), int(b * 255)


def ctext(d, xy, text, f, fill):
    x, y = xy
    box = d.textbbox((0, 0), text, font=f)
    d.text((x - (box[2] - box[0]) / 2 - box[0], y - (box[3] - box[1]) / 2 - box[1]), text, font=f, fill=fill)


def seg(d, cx, cy, r0, r1, a0, a1, fill, selected=False):
    pts, steps = [], 14
    for i in range(steps + 1):
        a = math.radians(a0 + (a1 - a0) * i / steps)
        pts.append((cx + r1 * math.cos(a), cy + r1 * math.sin(a)))
    for i in range(steps + 1):
        a = math.radians(a1 + (a0 - a1) * i / steps)
        pts.append((cx + r0 * math.cos(a), cy + r0 * math.sin(a)))
    d.polygon(pts, fill=fill)
    if selected:
        d.line(pts[: steps + 2], fill=(255, 255, 255), width=2 * SS)
        d.line(pts[steps + 1 :], fill=(255, 255, 255), width=2 * SS)


def render_ring_layout() -> Image.Image:
    img = Image.new("RGB", (W * SS, H * SS), BG)
    d = ImageDraw.Draw(img)
    cx, cy = 68 * SS, 63 * SS
    r_maj0, r_maj1 = 42 * SS, 59 * SS
    r_min0, r_min1 = 26 * SS, 42 * SS
    f_maj, f_min = font(10, True), font(6, True)
    sel = 0

    for i in range(12):
        a0 = -105 + 30 * i
        a1 = a0 + 30
        s_maj = i == sel
        s_min = i == sel  # relative minor lives in the same slot
        seg(d, cx, cy, r_maj0, r_maj1, a0 + 0.7, a1 - 0.7,
            hsv(i * 30, 0.85 if s_maj else 0.45, 0.80 if s_maj else 0.92), s_maj)
        seg(d, cx, cy, r_min0, r_min1, a0 + 0.7, a1 - 0.7,
            hsv(i * 30, 0.7 if s_min else 0.32, 0.62 if s_min else 0.50), s_min)

        am = math.radians((a0 + a1) / 2)
        rm, rmi = 50.5 * SS, 34 * SS
        ctext(d, (cx + rm * math.cos(am), cy + rm * math.sin(am)), MAJOR[i], f_maj,
              (16, 18, 22) if not s_maj else (255, 255, 255))
        ctext(d, (cx + rmi * math.cos(am), cy + rmi * math.sin(am)), MINOR[i], f_min,
              (245, 245, 245) if not s_min else (255, 255, 255))

    # hub
    d.ellipse([cx - 25 * SS, cy - 25 * SS, cx + 25 * SS, cy + 25 * SS], fill=(20, 24, 30))
    ctext(d, (cx, cy - 5 * SS), "C", font(21, True), FG)
    ctext(d, (cx, cy + 12 * SS), "0# / 0b", font(7), DIM)

    # right panel
    px = 132
    d.line([(px - 4) * SS, 4 * SS, (px - 4) * SS, 122 * SS], fill=LINE, width=1 * SS)
    d.text((px * SS, 5 * SS), "C MAJOR", font=font(11, True), fill=FG)
    d.text((px * SS, 19 * SS), "0#   C D E F G A B", font=font(7), fill=DIM)

    top, row_h = 31, 13.2
    for r in range(7):
        y = (top + r * row_h) * SS
        if r == 4:
            d.rounded_rectangle([px * SS, y, (px + 100) * SS, y + (row_h - 1.8) * SS],
                                radius=2 * SS, fill=(38, 46, 58))
        d.text(((px + 1) * SS, y + 1.4 * SS), ROMAN[r], font=font(7), fill=FG if r == 4 else DIM)
        d.text(((px + 23) * SS, y + 0.4 * SS), CHORDS[r], font=font(10, True),
               fill=FG if r == 4 else (210, 218, 228))
        d.text(((px + 54) * SS, y + 2.6 * SS), NOTES[r], font=font(6), fill=(120, 132, 148))

    # footer
    d.line([(4 * SS, 124 * SS), ((W - 4) * SS, 124 * SS)], fill=LINE, width=1 * SS)
    d.text((5 * SS, 126 * SS), "Fn+\u2190\u2192 key   \u2191\u2193 chord   ENTER play", font=font(6), fill=(120, 132, 148))
    d.text((204 * SS, 126 * SS), "72%", font=font(7), fill=(104, 116, 132))
    return img.resize((W, H), Image.LANCZOS)


def render_strip_layout() -> Image.Image:
    img = Image.new("RGB", (W * SS, H * SS), BG)
    d = ImageDraw.Draw(img)
    d.text((4 * SS, 4 * SS), "CIRCLE OF FIFTHS", font=font(9, True), fill=FG)
    d.text((186 * SS, 5 * SS), "C major \u00b7 0#", font=font(8), fill=DIM)

    x0, y0, cw = 4, 19, 19.3
    for i in range(12):
        x = (x0 + i * cw) * SS
        ons = i == 0
        col = hsv(i * 30, 0.85, 0.88) if ons else hsv(i * 30, 0.40, 0.34)
        d.rounded_rectangle([x, y0 * SS, x + (cw - 1.6) * SS, (y0 + 12) * SS], radius=1.5 * SS, fill=col)
        ctext(d, (x + (cw - 1.6) * SS / 2, (y0 + 6) * SS), MAJOR[i], font(9, True),
              (16, 18, 22) if ons else FG)
        col2 = hsv(i * 30, 0.65, 0.50) if ons else hsv(i * 30, 0.28, 0.22)
        d.rounded_rectangle([x, (y0 + 14) * SS, x + (cw - 1.6) * SS, (y0 + 25) * SS], radius=1.5 * SS, fill=col2)
        ctext(d, (x + (cw - 1.6) * SS / 2, (y0 + 19.5) * SS), MINOR[i], font(6, True),
              (16, 18, 22) if ons else (200, 208, 220))

    # chord grid, two columns
    for r in range(4):
        for c in range(2):
            k = r + c * 4
            if k > 6:
                continue
            x = (4 + c * 118) * SS
            y = (48 + r * 19.5) * SS
            ons = k == 4
            if ons:
                d.rounded_rectangle([x, y, x + 112 * SS, y + 17.5 * SS], radius=2 * SS, fill=(38, 46, 58))
            d.text((x + 2 * SS, y + 1.5 * SS), ROMAN[k], font=font(7), fill=DIM)
            d.text((x + 20 * SS, y + 0.8 * SS), CHORDS[k], font=font(9, True), fill=FG)
            d.text((x + 46 * SS, y + 3 * SS), NOTES[k], font=font(6), fill=(150, 160, 175) if ons else DIM)
    d.text((4 * SS, 126 * SS), "readable, but the circle's neighbour logic disappears", font=font(6), fill=(92, 102, 116))
    return img.resize((W, H), Image.LANCZOS)


def upscale(img: Image.Image, k: int) -> Image.Image:
    return img.resize((W * k, H * k), Image.NEAREST)


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    a, b = render_ring_layout(), render_strip_layout()
    a.save(os.path.join(OUT_DIR, "ui-mockup-ring-1x.png"))
    b.save(os.path.join(OUT_DIR, "ui-mockup-strip-1x.png"))
    a4, b4 = upscale(a, 4), upscale(b, 4)

    pad, head = 16, 34
    sheet = Image.new("RGB", (W * 4 * 2 + pad * 3, H * 4 + pad * 2 + head), (28, 32, 38))
    sheet.paste(a4, (pad, pad + head))
    sheet.paste(b4, (pad * 2 + W * 4, pad + head))
    draw = ImageDraw.Draw(sheet)
    lf = ImageFont.truetype(os.path.join(FONT_DIR, "msyh.ttc"), 18)
    draw.text((pad, 8), "A  12 段圆环 + 级数和弦列表", font=lf, fill=(225, 230, 238))
    draw.text((pad * 2 + W * 4, 8), "B  横向 12 格 + 和弦网格", font=lf, fill=(225, 230, 238))
    sheet.save(os.path.join(OUT_DIR, "ui-mockup.png"))
    print("wrote", OUT_DIR)


if __name__ == "__main__":
    main()


