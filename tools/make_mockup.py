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


def render_chord_page() -> Image.Image:
    """Mirrors the layout constants in firmware/src/ui.cpp drawChordPage()."""
    img = Image.new("RGB", (W * SS, H * SS), BG)
    d = ImageDraw.Draw(img)

    sym, label = "G7", "dom 7"
    notes, ivs = "G  B  D  F", "1  3  5  b7"

    d.text((6 * SS, 3 * SS), sym, font=font(24, True), fill=FG)
    d.text((6 * SS, 36 * SS), label, font=font(8), fill=DIM)
    d.text((6 * SS, 48 * SS), notes, font=font(8), fill=FG)
    d.text((6 * SS, 58 * SS), ivs, font=font(8), fill=DIM)
    d.text((6 * SS, 72 * SS), "V7 of C", font=font(8), fill=(150, 178, 208))
    d.text((6 * SS, 88 * SS), "ENTER: play", font=font(7), fill=(96, 106, 120))
    d.text((6 * SS, 99 * SS), "BACKSPACE: edit", font=font(7), fill=(84, 94, 108))

    d.line([(120 * SS, 3 * SS), (120 * SS, 121 * SS)], fill=LINE, width=1 * SS)
    d.text((124 * SS, 3 * SS), "IN KEYS", font=font(8), fill=DIM)

    rows = [("C", "maj", "V7"), ("A", "min", "VII7")]
    for i, (k, mode, rom) in enumerate(rows):
        y = (16 + i * 13) * SS
        d.text((124 * SS, y), k, font=font(8), fill=FG)
        d.text((142 * SS, y), mode, font=font(8), fill=DIM)
        d.text((168 * SS, y), rom, font=font(8), fill=FG)

    d.line([(4 * SS, 124 * SS), ((W - 4) * SS, 124 * SS)], fill=LINE, width=1 * SS)
    d.text((5 * SS, 126 * SS), "TAB circle  Fn+<> root  ^v type", font=font(8), fill=(104, 116, 132))
    d.text(((W - 27) * SS, 126 * SS), "72%", font=font(8), fill=(104, 116, 132))
    return img.resize((W, H), Image.LANCZOS)


def _draw_box(d, x, y, frets):
    """Mirror of drawChordBox() in firmware/src/ui.cpp."""
    dx, dy, n = 10, 13, 5
    fr = [f for f in frets if f > 0]
    mn = min(fr) if fr else 0
    first = mn if mn > 3 else 1
    first_sounding = next((i for i, f in enumerate(frets) if f >= 0), -1)

    for i in range(6):
        d.line([(x + i * dx) * SS, y * SS, (x + i * dx) * SS, (y + n * dy) * SS],
               fill=(64, 72, 86), width=1 * SS)
    for j in range(n + 1):
        d.line([x * SS, (y + j * dy) * SS, (x + 5 * dx) * SS, (y + j * dy) * SS],
               fill=(64, 72, 86), width=1 * SS)

    if first == 1:
        d.rectangle([(x - 1) * SS, (y - 1) * SS, (x + 5 * dx + 1) * SS, (y + 1.5) * SS], fill=(150, 160, 176))
    else:
        d.text(((x - 15) * SS, (y + 1) * SS), f"{first}fr", font=font(7), fill=DIM)

    for i, f in enumerate(frets):
        cx = (x + i * dx) * SS
        if f == 0:
            d.ellipse([cx - 2 * SS, (y - 10) * SS, cx + 2 * SS, (y - 6) * SS], outline=DIM, width=1 * SS)
        elif f < 0:
            d.line([cx - 2 * SS, (y - 10) * SS, cx + 2 * SS, (y - 6) * SS], fill=DIM, width=1 * SS)
            d.line([cx + 2 * SS, (y - 10) * SS, cx - 2 * SS, (y - 6) * SS], fill=DIM, width=1 * SS)

    sounding = [f for f in frets if f >= 0]
    all_above = all(f >= mn for f in sounding)
    at_fret = sum(1 for f in frets if f == mn)
    if mn > 0 and all_above and at_fret >= 2 and frets[first_sounding] == mn:
        last = max(i for i, f in enumerate(frets) if f >= 0)
        cy = y + (mn - first) * dy + dy / 2
        d.rounded_rectangle([(x + first_sounding * dx - 3) * SS, (cy - 3) * SS,
                             (x + last * dx + 3) * SS, (cy + 3) * SS], radius=3 * SS, fill=FG)

    for i, f in enumerate(frets):
        if f <= 0:
            continue
        cx = (x + i * dx) * SS
        cy = (y + (f - first) * dy + dy / 2) * SS
        d.ellipse([cx - 3 * SS, cy - 3 * SS, cx + 3 * SS, cy + 3 * SS], fill=FG)


def render_guitar_page() -> Image.Image:
    img = Image.new("RGB", (W * SS, H * SS), BG)
    d = ImageDraw.Draw(img)

    d.text((6 * SS, 2 * SS), "G7", font=font(15, True), fill=FG)
    d.text((36 * SS, 7 * SS), "dom 7", font=font(8), fill=DIM)
    d.text((206 * SS, 6 * SS), "1-3/6", font=font(8), fill=DIM)

    shapes = [[3, 2, 0, 0, 0, 1], [3, 5, 3, 4, 3, 3], [3, 2, 3, 0, 0, 3]]
    for k, s in enumerate(shapes):
        _draw_box(d, 30 + k * 68, 30, s)

    d.text((96 * SS, 104 * SS), "G  B  D  F", font=font(8), fill=DIM)
    d.line([(4 * SS, 124 * SS), ((W - 4) * SS, 124 * SS)], fill=LINE, width=1 * SS)
    d.text((5 * SS, 126 * SS), "TAB circle  Fn+<> voice  ^v type", font=font(8), fill=(104, 116, 132))
    d.text(((W - 27) * SS, 126 * SS), "72%", font=font(8), fill=(104, 116, 132))
    return img.resize((W, H), Image.LANCZOS)
def render_scale_page() -> Image.Image:
    """Mirrors drawScalePage() in firmware/src/ui.cpp (D dorian as the sample)."""
    img = Image.new("RGB", (W * SS, H * SS), BG)
    d = ImageDraw.Draw(img)

    d.text((6 * SS, 2 * SS), "D DORIAN", font=font(15, True), fill=FG)
    d.text((212 * SS, 7 * SS), "2/14", font=font(8), fill=DIM)

    notes = ["D", "E", "F", "G", "A", "B", "C"]
    degs = ["1", "2", "b3", "4", "5", "6", "b7"]
    colw = 220 / 7
    for i, (n, g) in enumerate(zip(notes, degs)):
        cx = (10 + colw * i + colw / 2) * SS
        ctext(d, (cx, 33 * SS), n, font(13, True), FG)
        ctext(d, (cx, 46 * SS), g, font(8), DIM)

    inside = {2, 4, 5, 7, 9, 11, 0}   # D E F G A B C
    names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    for pc in range(12):
        x = (10 + pc * 18) * SS
        if pc in inside:
            d.rounded_rectangle([x, 56 * SS, x + 16 * SS, 82 * SS], radius=2 * SS, fill=(50, 64, 88))
        else:
            d.rounded_rectangle([x, 56 * SS, x + 16 * SS, 82 * SS], radius=2 * SS, outline=(44, 51, 62), width=1 * SS)
        col = FG if pc in inside else (88, 98, 112)
        ctext(d, (x + 8 * SS, 64 * SS), names[pc], font(8), col)
        if pc in inside:
            ctext(d, (x + 8 * SS, 75 * SS), degs[notes.index(names[pc])] if names[pc] in notes else "", font(7), (150, 178, 208))

    ctext(d, (120 * SS, 94 * SS), "2 1 2 2 1 2 2", font(8), DIM)

    d.line([(4 * SS, 124 * SS), ((W - 4) * SS, 124 * SS)], fill=LINE, width=1 * SS)
    d.text((5 * SS, 126 * SS), "TAB circle  Fn+<> root  ^v type", font=font(8), fill=(104, 116, 132))
    d.text(((W - 27) * SS, 126 * SS), "72%", font=font(8), fill=(104, 116, 132))
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

    c = render_chord_page()
    c.save(os.path.join(OUT_DIR, "ui-mockup-chord-1x.png"))
    upscale(c, 4).save(os.path.join(OUT_DIR, "ui-mockup-chord.png"))

    g = render_guitar_page()
    g.save(os.path.join(OUT_DIR, "ui-mockup-guitar-1x.png"))
    upscale(g, 4).save(os.path.join(OUT_DIR, "ui-mockup-guitar.png"))

    sp = render_scale_page()
    sp.save(os.path.join(OUT_DIR, "ui-mockup-scale-1x.png"))
    upscale(sp, 4).save(os.path.join(OUT_DIR, "ui-mockup-scale.png"))
    print("wrote", OUT_DIR)


if __name__ == "__main__":
    main()





