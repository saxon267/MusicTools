#include "ui.h"

#include <M5Unified.h>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "guitar.h"
#include "theory.h"

using namespace mt;

namespace {

constexpr int kW = 240;
constexpr int kH = 135;

constexpr float kDeg = 0.0174532925f;

// wheel geometry
constexpr int kCx     = 68;
constexpr int kCy     = 62;
constexpr int kROut   = 58;
constexpr int kRMajIn = 41;
constexpr int kRMinIn = 26;
constexpr int kRHub   = 25;
constexpr int kPanelX = 132;

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kBg       = rgb(14, 17, 22);
constexpr uint16_t kFg       = rgb(232, 236, 241);
constexpr uint16_t kDim      = rgb(132, 144, 158);
constexpr uint16_t kLine     = rgb(40, 47, 58);
constexpr uint16_t kSelBg    = rgb(38, 46, 58);
constexpr uint16_t kHubBg    = rgb(20, 24, 30);
constexpr uint16_t kInk      = rgb(16, 18, 22);
constexpr uint16_t kChordDim = rgb(120, 132, 148);
constexpr uint16_t kAccent   = rgb(150, 178, 208);

M5Canvas* gCanvas = nullptr;

uint16_t hueColor(float h, float s, float v)
{
    float c  = v * s;
    float hp = fmodf(h, 360.0f) / 60.0f;
    if (hp < 0) hp += 6.0f;
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float r = 0, g = 0, b = 0;
    switch ((int)hp) {
        case 0: r = c; g = x; break;
        case 1: r = x; g = c; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        default: r = c; g = 0; b = x; break;
    }
    float m = v - c;
    return rgb((uint8_t)((r + m) * 255.0f + 0.5f), (uint8_t)((g + m) * 255.0f + 0.5f),
               (uint8_t)((b + m) * 255.0f + 0.5f));
}

void fillSeg(LovyanGFX* d, int cx, int cy, int r0, int r1, float a0, float a1, uint16_t col)
{
    const int steps = 12;
    float     pox = 0, poy = 0, pix = 0, piy = 0;
    for (int i = 0; i <= steps; ++i) {
        float a  = (a0 + (a1 - a0) * (float)i / steps) * kDeg;
        float ca = cosf(a), sa = sinf(a);
        float ox = cx + r1 * ca, oy = cy + r1 * sa;
        float ix = cx + r0 * ca, iy = cy + r0 * sa;
        if (i > 0) {
            d->fillTriangle((int32_t)pox, (int32_t)poy, (int32_t)pix, (int32_t)piy, (int32_t)ox,
                            (int32_t)oy, col);
            d->fillTriangle((int32_t)pix, (int32_t)piy, (int32_t)ix, (int32_t)iy, (int32_t)ox,
                            (int32_t)oy, col);
        }
        pox = ox;
        poy = oy;
        pix = ix;
        piy = iy;
    }
}

void outlineSeg(LovyanGFX* d, int cx, int cy, int r0, int r1, float a0, float a1, uint16_t col)
{
    const int steps = 12;
    float     pox = 0, poy = 0, pix = 0, piy = 0;
    for (int i = 0; i <= steps; ++i) {
        float a  = (a0 + (a1 - a0) * (float)i / steps) * kDeg;
        float ca = cosf(a), sa = sinf(a);
        float ox = cx + r1 * ca, oy = cy + r1 * sa;
        float ix = cx + r0 * ca, iy = cy + r0 * sa;
        if (i > 0) {
            d->drawLine((int32_t)pox, (int32_t)poy, (int32_t)ox, (int32_t)oy, col);
            d->drawLine((int32_t)pix, (int32_t)piy, (int32_t)ix, (int32_t)iy, col);
        }
        pox = ox;
        poy = oy;
        pix = ix;
        piy = iy;
    }
    float a = a0 * kDeg, b = a1 * kDeg;
    d->drawLine((int32_t)(cx + r0 * cosf(a)), (int32_t)(cy + r0 * sinf(a)),
                (int32_t)(cx + r1 * cosf(a)), (int32_t)(cy + r1 * sinf(a)), col);
    d->drawLine((int32_t)(cx + r0 * cosf(b)), (int32_t)(cy + r0 * sinf(b)),
                (int32_t)(cx + r1 * cosf(b)), (int32_t)(cy + r1 * sinf(b)), col);
}

void textMid(LovyanGFX* d, int x, int y, const char* s, const lgfx::IFont* f, uint16_t col)
{
    d->setFont(f);
    d->setTextDatum(textdatum_t::middle_center);
    d->setTextColor(col);
    d->drawString(s, x, y);
}

void textTL(LovyanGFX* d, int x, int y, const char* s, const lgfx::IFont* f, uint16_t col)
{
    d->setFont(f);
    d->setTextDatum(textdatum_t::top_left);
    d->setTextColor(col);
    d->drawString(s, x, y);
}

void textML(LovyanGFX* d, int x, int y, const char* s, const lgfx::IFont* f, uint16_t col)
{
    d->setFont(f);
    d->setTextDatum(textdatum_t::middle_left);
    d->setTextColor(col);
    d->drawString(s, x, y);
}

// ---------------------------------------------------------------------------
// page 0: circle of fifths
// ---------------------------------------------------------------------------

void drawWheel(LovyanGFX* d, const UiState& st)
{
    for (int i = 0; i < kSlots; ++i) {
        const bool  sel = (i == st.slot);
        const float a0  = -105.0f + 30.0f * i + 0.7f;
        const float a1  = a0 + 30.0f - 1.4f;

        fillSeg(d, kCx, kCy, kRMajIn, kROut, a0, a1,
                sel ? hueColor(i * 30.0f, 0.95f, 0.96f) : hueColor(i * 30.0f, 0.45f, 0.90f));
        fillSeg(d, kCx, kCy, kRMinIn, kRMajIn, a0, a1,
                sel ? hueColor(i * 30.0f, 0.85f, 0.60f) : hueColor(i * 30.0f, 0.42f, 0.50f));

        if (sel) {
            outlineSeg(d, kCx, kCy, kRMajIn, kROut, a0, a1, TFT_WHITE);
        }

        const float am = (a0 + a1) * 0.5f * kDeg;

        char maj[4];
        noteName(majorTonic(i), maj);
        textMid(d, kCx + (int)(49.5f * cosf(am)), kCy + (int)(49.5f * sinf(am)), maj, &fonts::Font0,
                sel ? TFT_WHITE : kInk);

        char min[4];
        noteName(relativeMinorOf(majorTonic(i)), min);
        for (char* p = min; *p; ++p) {
            if (*p >= 'A' && *p <= 'G') *p = (char)(*p - 'A' + 'a');
        }
        textMid(d, kCx + (int)(33.5f * cosf(am)), kCy + (int)(33.5f * sinf(am)), min, &fonts::Font0,
                sel ? TFT_WHITE : rgb(228, 232, 238));
    }

    d->fillCircle(kCx, kCy, kRHub, kHubBg);

    Note tonic = majorTonic(st.slot);
    if (st.minorView) tonic = relativeMinorOf(tonic);

    char name[6];
    noteName(tonic, name);
    if (st.minorView) {
        int n = 0;
        while (name[n]) ++n;
        name[n]     = 'm';
        name[n + 1] = '\0';
    }
    textMid(d, kCx, kCy - 5, name, &fonts::Font4, kFg);

    const int sig = signature(st.slot);
    char      sbuf[8];
    if (sig > 0) {
        snprintf(sbuf, sizeof sbuf, "%d#", sig);
    } else if (sig < 0) {
        snprintf(sbuf, sizeof sbuf, "%db", -sig);
    } else {
        snprintf(sbuf, sizeof sbuf, "0");
    }
    textMid(d, kCx, kCy + 13, sbuf, &fonts::Font0, kDim);
}

void drawPanel(LovyanGFX* d, const UiState& st)
{
    Note tonic = majorTonic(st.slot);
    if (st.minorView) tonic = relativeMinorOf(tonic);
    Scale sc = scaleOf(tonic, st.minorView);
    Chord chords[7];
    diatonicChords(tonic, st.minorView, chords);

    char tname[6];
    noteName(tonic, tname);

    char header[20];
    snprintf(header, sizeof header, "%s%s %s", tname, st.minorView ? "m" : "",
             st.minorView ? "MINOR" : "MAJOR");
    textTL(d, kPanelX + 2, 3, header, &fonts::Font2, kFg);

    const int sig = signature(st.slot);
    char      sbuf[8];
    if (sig > 0) {
        snprintf(sbuf, sizeof sbuf, "%d#", sig);
    } else if (sig < 0) {
        snprintf(sbuf, sizeof sbuf, "%db", -sig);
    } else {
        snprintf(sbuf, sizeof sbuf, "0");
    }

    char scaleTxt[32];
    int  n = snprintf(scaleTxt, sizeof scaleTxt, "%s ", sbuf);
    for (int i = 0; i < 7; ++i) {
        char nm[4];
        noteName(sc.notes[i], nm);
        n += snprintf(scaleTxt + n, sizeof scaleTxt - n, "%s%s", i ? " " : "", nm);
        if (n >= (int)sizeof scaleTxt - 4) break;
    }
    // Sharp keys spell out to 22+ characters; drop the separators first and the
    // signature last so the line can never run under the panel edge.
    d->setFont(&fonts::Font0);
    const int maxW = kW - (kPanelX + 2) - 6;
    if (d->textWidth(scaleTxt) > maxW) {
        n = snprintf(scaleTxt, sizeof scaleTxt, "%s ", sbuf);
        for (int i = 0; i < 7; ++i) {
            char nm[4];
            noteName(sc.notes[i], nm);
            n += snprintf(scaleTxt + n, sizeof scaleTxt - n, "%s", nm);
        }
        if (d->textWidth(scaleTxt) > maxW) {
            snprintf(scaleTxt, sizeof scaleTxt, "%s", sbuf);
        }
    }
    textTL(d, kPanelX + 2, 19, scaleTxt, &fonts::Font0, kDim);

    for (int i = 0; i < 7; ++i) {
        const int  y   = 31 + i * 13;
        const bool sel = (i == st.selDegree);

        if (sel) {
            d->fillRoundRect(kPanelX + 1, y - 1, 102, 12, 3, kSelBg);
        }

        textTL(d, kPanelX + 3, y + 1, romanNumeral(i, st.minorView), &fonts::Font0,
               sel ? kFg : kDim);

        if (chords[i].q == Quality::Dim) {
            int w = d->textWidth(romanNumeral(i, st.minorView));
            d->drawCircle(kPanelX + 4 + w + 2, y + 3, 1, sel ? kFg : kDim);
        }

        char cn[8];
        chordName(chords[i], cn);
        textML(d, kPanelX + 22, y + 6, cn, &fonts::FreeSans9pt7b, sel ? kFg : rgb(210, 218, 228));

        char notes[12];
        int  m = 0;
        for (int k = 0; k < 3; ++k) {
            char nm[4];
            noteName(sc.notes[(i + k * 2) % 7], nm);
            m += snprintf(notes + m, sizeof notes - m, "%s%s", k ? " " : "", nm);
            if (m >= (int)sizeof notes - 4) break;
        }
        textTL(d, kPanelX + 54, y + 1, notes, &fonts::Font0, kChordDim);
    }

    d->drawFastVLine(kPanelX - 4, 4, 118, kLine);
}

// ---------------------------------------------------------------------------
// page 1: chord query
// ---------------------------------------------------------------------------

void drawChordPage(LovyanGFX* d, const UiState& st)
{
    if (!st.chordValid) {
        textTL(d, 6, 4, "?", &fonts::Font4, kDim);
        textTL(d, 6, 40, "unknown chord", &fonts::Font0, kDim);
        textTL(d, 6, 52, "keep typing", &fonts::Font0, kDim);
        textTL(d, 6, 88, "ENTER: play", &fonts::Font0, rgb(96, 106, 120));
        return;
    }

    const ChordType& t = kChordTypes[st.chordType];

    // symbol, shrinking to the smaller font for long names such as G#m7b5
    d->setFont(&fonts::Font4);
    const bool big = d->textWidth(st.chordText) <= 104;
    textTL(d, 6, 3, st.chordText, big ? &fonts::Font4 : &fonts::Font2, kFg);

    textTL(d, 6, big ? 36 : 26, t.label, &fonts::Font0, kDim);

    Note notes[4];
    spellChord(st.chordRoot, st.chordType, notes);

    char nb[28];
    int  n = 0;
    for (int i = 0; i < t.n; ++i) {
        char nm[4];
        noteName(notes[i], nm);
        n += snprintf(nb + n, sizeof nb - n, "%s%s", i ? "  " : "", nm);
    }
    textTL(d, 6, 48, nb, &fonts::Font0, kFg);

    char ib[24];
    n = 0;
    for (int i = 0; i < t.n; ++i) {
        n += snprintf(ib + n, sizeof ib - n, "%s%s", i ? "  " : "", intervalName(t.semis[i]));
    }
    textTL(d, 6, 58, ib, &fonts::Font0, kDim);

    Note target;
    if (dominantTarget(st.chordRoot, st.chordType, target)) {
        char tn[4];
        noteName(target, tn);
        char rb[20];
        snprintf(rb, sizeof rb, "V7 of %s", tn);
        textTL(d, 6, 72, rb, &fonts::Font0, kAccent);
    }

    textTL(d, 6, 88, "ENTER: play", &fonts::Font0, rgb(96, 106, 120));
    textTL(d, 6, 99, "BACKSPACE: edit", &fonts::Font0, rgb(84, 94, 108));

    // right column: the same chord seen from every key
    d->drawFastVLine(120, 3, 118, kLine);
    textTL(d, 124, 3, "IN KEYS", &fonts::Font0, kDim);

    KeyHit hits[12];
    const int nh = findChordInKeys(st.chordRoot, st.chordType, hits, 12);

    if (nh == 0) {
        textTL(d, 124, 20, "not diatonic", &fonts::Font0, kDim);
        textTL(d, 124, 30, "in any key", &fonts::Font0, kDim);
        return;
    }

    const int shown = nh > 7 ? 7 : nh;
    for (int i = 0; i < shown; ++i) {
        const int y = 16 + i * 13;

        Note tonic = majorTonic(hits[i].slot);
        if (hits[i].minor) tonic = relativeMinorOf(tonic);
        char kn[4];
        noteName(tonic, kn);

        textTL(d, 124, y, kn, &fonts::Font0, kFg);
        textTL(d, 142, y, hits[i].minor ? "min" : "maj", &fonts::Font0, kDim);

        const char* rom = romanNumeral(hits[i].degree, hits[i].minor);
        const int   rx  = 168;
        textTL(d, rx, y, rom, &fonts::Font0, kFg);
        int w = d->textWidth(rom);
        if (hits[i].quality == 2) {
            d->drawCircle(rx + w + 2, y + 3, 1, kFg);
            w += 4;
        }
        if (t.n == 4) {
            textTL(d, rx + w + 2, y, "7", &fonts::Font0, kFg);
        }
    }

    if (nh > shown) {
        char more[16];
        snprintf(more, sizeof more, "+%d more", nh - shown);
        textTL(d, 124, 16 + shown * 13, more, &fonts::Font0, kDim);
    }
}

// ---------------------------------------------------------------------------
// page 2: guitar fingerings
// ---------------------------------------------------------------------------

void drawChordBox(LovyanGFX* d, int x, int y, const Fingering& f)
{
    const int dx = 10, dy = 13, nFrets = 5;
    int       mn = 99, firstSounding = -1;
    for (int i = 0; i < 6; ++i) {
        if (f.fret[i] >= 0 && firstSounding < 0) firstSounding = i;
        if (f.fret[i] > 0 && f.fret[i] < mn) mn = f.fret[i];
    }
    if (firstSounding < 0) return;

    const int first  = (mn > 3) ? mn : 1;
    const int bottom = y + nFrets * dy;

    for (int i = 0; i < 6; ++i) {
        d->drawFastVLine(x + i * dx, y, nFrets * dy, rgb(64, 72, 86));
    }
    for (int j = 0; j <= nFrets; ++j) {
        d->drawFastHLine(x, y + j * dy, 5 * dx, rgb(64, 72, 86));
    }

    if (first == 1) {
        d->fillRect(x - 1, y - 1, 5 * dx + 2, 3, rgb(150, 160, 176));
    } else {
        char b[8];
        snprintf(b, sizeof b, "%dfr", first);
        textTL(d, x - 15, y + 1, b, &fonts::Font0, kDim);
    }

    // open / muted markers above the nut
    for (int i = 0; i < 6; ++i) {
        const int cx = x + i * dx;
        if (f.fret[i] == 0) {
            d->drawCircle(cx, y - 8, 2, kDim);
        } else if (f.fret[i] < 0) {
            d->drawLine(cx - 2, y - 10, cx + 2, y - 6, kDim);
            d->drawLine(cx + 2, y - 10, cx - 2, y - 6, kDim);
        }
    }

    // barre: the lowest sounding string sits on the lowest fret and nothing is
    // fretted below it, which is exactly how a barre is held
    bool allAbove = true;
    int  atFret  = 0;
    for (int i = 0; i < 6; ++i) {
        if (f.fret[i] >= 0) {
            if (f.fret[i] < mn) allAbove = false;
            if (f.fret[i] == mn) ++atFret;
        }
    }
    if (allAbove && atFret >= 2 && f.fret[firstSounding] == mn) {
        int last = firstSounding;
        for (int i = firstSounding; i < 6; ++i) {
            if (f.fret[i] >= 0) last = i;
        }
        const int cy = y + (mn - first) * dy + dy / 2;
        d->fillRoundRect(x + firstSounding * dx - 3, cy - 3, (last - firstSounding) * dx + 7, 7, 3, kFg);
    }

    for (int i = 0; i < 6; ++i) {
        if (f.fret[i] <= 0) continue;
        const int cx = x + i * dx;
        const int cy = y + (f.fret[i] - first) * dy + dy / 2;
        d->fillCircle(cx, cy, 3, kFg);
    }
}

void drawGuitarPage(LovyanGFX* d, const UiState& st)
{
    if (!st.chordValid) {
        textTL(d, 6, 4, "?", &fonts::Font4, kDim);
        textTL(d, 6, 40, "unknown chord", &fonts::Font0, kDim);
        return;
    }

    const ChordType& t = kChordTypes[st.chordType];

    // shapes are recomputed only when the chord actually changes
    static int      cachePc   = -1;
    static int      cacheType = -1;
    static Fingering shapes[6];
    static int      nShapes = 0;

    if (mt::pitchClass(st.chordRoot) != cachePc || st.chordType != cacheType) {
        cachePc   = mt::pitchClass(st.chordRoot);
        cacheType = st.chordType;
        nShapes   = guitarShapes(st.chordRoot, st.chordType, shapes, 6);
    }

    textTL(d, 6, 2, st.chordText, &fonts::Font2, kFg);
    d->setFont(&fonts::Font2);
    const int sw = d->textWidth(st.chordText);
    textTL(d, 6 + sw + 6, 7, t.label, &fonts::Font0, kDim);

    const int perPage = 3;
    const int pages   = (nShapes + perPage - 1) / perPage;
    const int page    = pages > 0 ? (st.gtrPage % pages) : 0;
    if (nShapes > 0) {
        char vb[16];
        snprintf(vb, sizeof vb, "%d-%d/%d", page * perPage + 1,
                 (page * perPage + perPage > nShapes) ? nShapes : page * perPage + perPage, nShapes);
        d->setFont(&fonts::Font0);
        textTL(d, kW - 5 - d->textWidth(vb), 6, vb, &fonts::Font0, kDim);
    }

    if (nShapes == 0) {
        textTL(d, 30, 50, "no playable shape", &fonts::Font0, kDim);
        textTL(d, 30, 62, "for this chord", &fonts::Font0, kDim);
        return;
    }

    const int xs[3] = {30, 98, 166};
    for (int k = 0; k < perPage; ++k) {
        const int i = page * perPage + k;
        if (i >= nShapes) break;
        drawChordBox(d, xs[k], 30, shapes[i]);
    }

    // chord tones under the diagrams
    Note notes[4];
    spellChord(st.chordRoot, st.chordType, notes);
    char nb[28];
    int  n = 0;
    for (int i = 0; i < t.n; ++i) {
        char nm[4];
        noteName(notes[i], nm);
        n += snprintf(nb + n, sizeof nb - n, "%s%s", i ? "  " : "", nm);
    }
    d->setFont(&fonts::Font0);
    textTL(d, kW / 2 - 3 * (int)strlen(nb), 104, nb, &fonts::Font0, kDim);
}

// ---------------------------------------------------------------------------

void drawFooter(LovyanGFX* d, const UiState& st)
{
    static const char* kHints[3] = {
        "TAB chord  Fn+<> key  ^v  ENTER",
        "TAB guitar  Fn+<> root  ^v type",
        "TAB circle  Fn+<> voice  ^v type",
    };
    d->drawFastHLine(4, 124, kW - 8, kLine);
    textTL(d, 5, 126, kHints[st.page % 3], &fonts::Font0, rgb(104, 116, 132));

    char bat[8];
    if (st.battery < 0) {
        snprintf(bat, sizeof bat, "--");
    } else {
        snprintf(bat, sizeof bat, "%d%%", st.battery);
    }
    d->setFont(&fonts::Font0);
    int w = d->textWidth(bat);
    textTL(d, kW - 5 - w, 126, bat, &fonts::Font0,
           st.audioBusy ? rgb(120, 200, 140) : rgb(104, 116, 132));
}

}  // namespace

void uiBegin()
{
    if (!gCanvas) {
        gCanvas = new M5Canvas(&M5.Display);
        gCanvas->setColorDepth(16);
        if (!gCanvas->createSprite(kW, kH)) {
            delete gCanvas;
            gCanvas = nullptr;
        }
    }
}

bool uiHasCanvas()
{
    return gCanvas != nullptr;
}

void uiRender(const UiState& st)
{
    LovyanGFX* d = gCanvas ? (LovyanGFX*)gCanvas : (LovyanGFX*)&M5.Display;

    d->fillScreen(kBg);
    if (st.page == 0) {
        drawWheel(d, st);
        drawPanel(d, st);
    } else if (st.page == 1) {
        drawChordPage(d, st);
    } else {
        drawGuitarPage(d, st);
    }
    drawFooter(d, st);

    if (gCanvas) {
        gCanvas->pushSprite(0, 0);
    }
}


