#include "ui.h"

#include <M5Unified.h>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "guitar.h"
#include "modulation.h"
#include "metronome.h"
#include "theory.h"

using namespace mt;

namespace {

constexpr int kW = 240;
constexpr int kH = 135;

constexpr float kDeg = 0.0174532925f;

// wheel geometry
constexpr int kTabH   = 12;  // top page tabs
constexpr int kCx     = 68;
constexpr int kCy     = 68;
constexpr int kROut   = 53;
constexpr int kRMajIn = 38;
constexpr int kRMinIn = 24;
constexpr int kRHub   = 22;
constexpr int kPanelX = 132;

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// GruvBox Dark
constexpr uint16_t kBg       = rgb(40, 40, 40);    // bg0
constexpr uint16_t kFg       = rgb(235, 219, 178); // fg1
constexpr uint16_t kDim      = rgb(146, 131, 116); // gray
constexpr uint16_t kLine     = rgb(80, 73, 69);    // bg2
constexpr uint16_t kSelBg    = rgb(60, 56, 54);    // bg1
constexpr uint16_t kHubBg    = rgb(29, 32, 33);    // bg0 hard
constexpr uint16_t kInk      = rgb(40, 40, 40);    // text on bright fills
constexpr uint16_t kChordDim = rgb(168, 153, 132);// fg4
constexpr uint16_t kAccent   = rgb(250, 189, 47);  // yellow

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
                sel ? hueColor(i * 30.0f, 0.80f, 0.86f) : hueColor(i * 30.0f, 0.42f, 0.74f));
        fillSeg(d, kCx, kCy, kRMinIn, kRMajIn, a0, a1,
                sel ? hueColor(i * 30.0f, 0.75f, 0.55f) : hueColor(i * 30.0f, 0.38f, 0.42f));

        if (sel) {
            outlineSeg(d, kCx, kCy, kRMajIn, kROut, a0, a1, TFT_WHITE);
        }

        const float am = (a0 + a1) * 0.5f * kDeg;

        char maj[4];
        noteName(majorTonic(i), maj);
        textMid(d, kCx + (int)(45.5f * cosf(am)), kCy + (int)(45.5f * sinf(am)), maj, &fonts::Font0,
                sel ? TFT_WHITE : kInk);

        char min[4];
        noteName(relativeMinorOf(majorTonic(i)), min);
        for (char* p = min; *p; ++p) {
            if (*p >= 'A' && *p <= 'G') *p = (char)(*p - 'A' + 'a');
        }
        textMid(d, kCx + (int)(31.0f * cosf(am)), kCy + (int)(31.0f * sinf(am)), min, &fonts::Font0,
                sel ? TFT_WHITE : rgb(213, 196, 161));
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
    textMid(d, kCx, kCy - 4, name, &fonts::Font4, kFg);

    const int sig = signature(st.slot);
    char      sbuf[8];
    if (sig > 0) {
        snprintf(sbuf, sizeof sbuf, "%d#", sig);
    } else if (sig < 0) {
        snprintf(sbuf, sizeof sbuf, "%db", -sig);
    } else {
        snprintf(sbuf, sizeof sbuf, "0");
    }
    textMid(d, kCx, kCy + 11, sbuf, &fonts::Font0, kDim);
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
    textTL(d, kPanelX + 2, 13, header, &fonts::Font2, kFg);

    // the tab bar costs 12 px, so the scale line moved to the scale page; the
    // chord tones on the right of each row still spell the key out
    for (int i = 0; i < 7; ++i) {
        const int  y   = 32 + i * 13;
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
        textML(d, kPanelX + 25, y + 6, cn, &fonts::FreeSans9pt7b, sel ? kFg : rgb(235, 219, 178));

        char notes[12];
        int  m = 0;
        for (int k = 0; k < 3; ++k) {
            char nm[4];
            noteName(sc.notes[(i + k * 2) % 7], nm);
            m += snprintf(notes + m, sizeof notes - m, "%s%s", k ? " " : "", nm);
            if (m >= (int)sizeof notes - 4) break;
        }
        textTL(d, kPanelX + 58, y + 1, notes, &fonts::Font0, kChordDim);
    }

    d->drawFastVLine(kPanelX - 4, 4, 118, kLine);
}

// ---------------------------------------------------------------------------
// page 1: chord query
// ---------------------------------------------------------------------------

void drawChordPage(LovyanGFX* d, const UiState& st)
{
    if (!st.chordValid) {
        textTL(d, 6, 16, "?", &fonts::Font4, kDim);
        textTL(d, 6, 52, "unknown chord", &fonts::Font0, kDim);
        textTL(d, 6, 64, "keep typing", &fonts::Font0, kDim);
        textTL(d, 6, 100, "ENTER: play", &fonts::Font0, rgb(146, 131, 116));
        return;
    }

    const ChordType& t = kChordTypes[st.chordType];

    // symbol, shrinking to the smaller font for long names such as G#m7b5
    d->setFont(&fonts::Font4);
    const bool big = d->textWidth(st.chordText) <= 104;
    textTL(d, 6, 13, st.chordText, big ? &fonts::Font4 : &fonts::Font2, kFg);

    textTL(d, 6, big ? 46 : 38, t.label, &fonts::Font0, kDim);

    Note notes[4];
    spellChord(st.chordRoot, st.chordType, notes);

    char nb[28];
    int  n = 0;
    for (int i = 0; i < t.n; ++i) {
        char nm[4];
        noteName(notes[i], nm);
        n += snprintf(nb + n, sizeof nb - n, "%s%s", i ? "  " : "", nm);
    }
    textTL(d, 6, 58, nb, &fonts::Font0, kFg);

    char ib[24];
    n = 0;
    for (int i = 0; i < t.n; ++i) {
        n += snprintf(ib + n, sizeof ib - n, "%s%s", i ? "  " : "", intervalName(t.semis[i]));
    }
    textTL(d, 6, 68, ib, &fonts::Font0, kDim);

    Note target;
    if (dominantTarget(st.chordRoot, st.chordType, target)) {
        char tn[4];
        noteName(target, tn);
        char rb[20];
        snprintf(rb, sizeof rb, "V7 of %s", tn);
        textTL(d, 6, 82, rb, &fonts::Font0, kAccent);
    }

    textTL(d, 6, 100, "ENTER: play", &fonts::Font0, rgb(146, 131, 116));
    textTL(d, 6, 110, "BACKSPACE: edit", &fonts::Font0, rgb(124, 111, 100));

    // right column: the same chord seen from every key
    d->drawFastVLine(120, 14, 108, kLine);
    textTL(d, 124, 14, "IN KEYS", &fonts::Font0, kDim);

    KeyHit hits[12];
    const int nh = findChordInKeys(st.chordRoot, st.chordType, hits, 12);

    if (nh == 0) {
        textTL(d, 124, 20, "not diatonic", &fonts::Font0, kDim);
        textTL(d, 124, 30, "in any key", &fonts::Font0, kDim);
        return;
    }

    const int shown = nh > 7 ? 7 : nh;
    for (int i = 0; i < shown; ++i) {
        const int y = 26 + i * 13;

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
        textTL(d, 124, 26 + shown * 13, more, &fonts::Font0, kDim);
    }
}

// ---------------------------------------------------------------------------
// page 2: guitar fingerings
// ---------------------------------------------------------------------------

void drawChordBox(LovyanGFX* d, int x, int y, const Fingering& f)
{
    const int dx = 10, dy = 12, nFrets = 5;
    int       mn = 99, firstSounding = -1;
    for (int i = 0; i < 5; ++i) {
        if (f.fret[i] >= 0 && firstSounding < 0) firstSounding = i;
        if (f.fret[i] > 0 && f.fret[i] < mn) mn = f.fret[i];
    }
    if (firstSounding < 0) return;

    const int first  = (mn > 3) ? mn : 1;
    const int bottom = y + nFrets * dy;

    for (int i = 0; i < 5; ++i) {
        d->drawFastVLine(x + i * dx, y, nFrets * dy, rgb(80, 73, 69));
    }
    for (int j = 0; j <= nFrets; ++j) {
        d->drawFastHLine(x, y + j * dy, 5 * dx, rgb(80, 73, 69));
    }

    if (first == 1) {
        d->fillRect(x - 1, y - 1, 5 * dx + 2, 3, rgb(168, 153, 132));
    } else {
        char b[8];
        snprintf(b, sizeof b, "%dfr", first);
        textTL(d, x - 15, y + 1, b, &fonts::Font0, kDim);
    }

    // open / muted markers above the nut
    for (int i = 0; i < 5; ++i) {
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
    for (int i = 0; i < 5; ++i) {
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

    for (int i = 0; i < 5; ++i) {
        if (f.fret[i] <= 0) continue;
        const int cx = x + i * dx;
        const int cy = y + (f.fret[i] - first) * dy + dy / 2;
        d->fillCircle(cx, cy, 3, kFg);
    }
}

void drawGuitarPage(LovyanGFX* d, const UiState& st)
{
    if (!st.chordValid) {
        textTL(d, 6, 16, "?", &fonts::Font4, kDim);
        textTL(d, 6, 52, "unknown chord", &fonts::Font0, kDim);
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

    textTL(d, 6, 13, st.chordText, &fonts::Font2, kFg);
    d->setFont(&fonts::Font2);
    const int sw = d->textWidth(st.chordText);
    textTL(d, 6 + sw + 6, 18, t.label, &fonts::Font0, kDim);

    const int perPage = 3;
    const int pages   = (nShapes + perPage - 1) / perPage;
    const int page    = pages > 0 ? (st.gtrPage % pages) : 0;
    if (nShapes > 0) {
        char vb[16];
        snprintf(vb, sizeof vb, "%d-%d/%d", page * perPage + 1,
                 (page * perPage + perPage > nShapes) ? nShapes : page * perPage + perPage, nShapes);
        d->setFont(&fonts::Font0);
        textTL(d, kW - 5 - d->textWidth(vb), 17, vb, &fonts::Font0, kDim);
    }

    if (nShapes == 0) {
        textTL(d, 30, 56, "no playable shape", &fonts::Font0, kDim);
        textTL(d, 30, 68, "for this chord", &fonts::Font0, kDim);
        return;
    }

    const int xs[3] = {30, 98, 166};
    for (int k = 0; k < perPage; ++k) {
        const int i = page * perPage + k;
        if (i >= nShapes) break;
        drawChordBox(d, xs[k], 38, shapes[i]);
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
    textTL(d, kW / 2 - 3 * (int)strlen(nb), 108, nb, &fonts::Font0, kDim);
}


// ---------------------------------------------------------------------------
// page 3: scale dictionary
// ---------------------------------------------------------------------------

void drawScalePage(LovyanGFX* d, const UiState& st)
{
    if (st.scaleType < 0 || st.scaleType >= kScaleTypeCount) return;
    const ScaleType& t = kScaleTypes[st.scaleType];

    Note notes[8];
    spellScale(st.scaleRoot, st.scaleType, notes);

    char rn[4];
    noteName(st.scaleRoot, rn);
    char header[24];
    snprintf(header, sizeof header, "%s %s", rn, t.name);
    textTL(d, 6, 13, header, &fonts::Font2, kFg);

    char idx[8];
    snprintf(idx, sizeof idx, "%d/%d", st.scaleType + 1, kScaleTypeCount);
    d->setFont(&fonts::Font0);
    textTL(d, kW - 5 - d->textWidth(idx), 17, idx, &fonts::Font0, kDim);

    // scale tones in order, with their degrees underneath
    const int colW = 220 / t.n;
    for (int i = 0; i < t.n; ++i) {
        const int cx = 10 + colW * i + colW / 2;
        char      nm[4];
        noteName(notes[i], nm);
        // every pitch class owns a hue, so the scale reads as a slice of a rainbow
        textMid(d, cx, 44, nm, &fonts::Font2, hueColor(pitchClass(notes[i]) * 30.0f, 0.50f, 0.85f));

        char deg[4];
        int  k = 0;
        for (int a = 0; a < notes[i].acc; ++a) deg[k++] = '#';
        for (int a = 0; a < -notes[i].acc; ++a) deg[k++] = 'b';
        deg[k++] = (char)('1' + t.degrees[i]);
        deg[k]   = '\0';
        textMid(d, cx, 56, deg, &fonts::Font0, kDim);
    }

    // twelve pitch classes, the ones inside the scale lit up
    int flats = 0, sharps = 0;
    for (int i = 0; i < t.n; ++i) {
        if (notes[i].acc > 0) {
            sharps += notes[i].acc;
        } else {
            flats -= notes[i].acc;
        }
    }
    const bool prefFlat = flats > sharps;

    for (int pc = 0; pc < 12; ++pc) {
        const int x = 10 + pc * 18;
        int       deg = -1;
        for (int i = 0; i < t.n; ++i) {
            if (pitchClass(notes[i]) == pc) deg = i;
        }
        if (deg >= 0) {
            d->fillRoundRect(x, 62, 16, 26, 2, hueColor(pc * 30.0f, 0.62f, 0.62f));
        } else {
            d->drawRoundRect(x, 62, 16, 26, 2, hueColor(pc * 30.0f, 0.35f, 0.26f));
        }

        // inside the scale use the spelled tone (F#, not Gb), outside fall back
        char b[4];
        if (deg >= 0) {
            noteName(notes[deg], b);
        } else {
            noteName(pcToNote(pc, prefFlat), b);
        }
        textMid(d, x + 8, 70, b, &fonts::Font0, deg >= 0 ? kInk : rgb(124, 111, 100));

        if (deg >= 0) {
            char dg[4];
            int  k = 0;
            for (int a = 0; a < notes[deg].acc; ++a) dg[k++] = '#';
            for (int a = 0; a < -notes[deg].acc; ++a) dg[k++] = 'b';
            dg[k++] = (char)('1' + t.degrees[deg]);
            dg[k]   = '\0';
            textMid(d, x + 8, 81, dg, &fonts::Font0, deg >= 0 ? kInk : kAccent);
        }
    }

    // interval formula
    char f[32];
    int  k = 0;
    for (int i = 0; i < t.n; ++i) {
        const int step = (i + 1 < t.n) ? (t.semis[i + 1] - t.semis[i]) : (12 - t.semis[i]);
        k += snprintf(f + k, sizeof f - k, "%s%d", i ? " " : "", step);
    }
    textMid(d, kW / 2, 96, f, &fonts::Font0, kDim);
}

// ---------------------------------------------------------------------------
// page 4: metronome
// ---------------------------------------------------------------------------

void drawMetroPage(LovyanGFX* d, const UiState& st)
{
    const MetroPattern& p = kMetroPatterns[st.metroPattern];

    char bpm[8];
    snprintf(bpm, sizeof bpm, "%d", st.bpm);
    textTL(d, 12, 14, bpm, &fonts::Font4, kFg);
    d->setFont(&fonts::Font4);
    textTL(d, 12 + d->textWidth(bpm) + 8, 26, "BPM", &fonts::Font0, kDim);

    d->setFont(&fonts::Font2);
    textTL(d, kW - 12 - d->textWidth(p.name), 16, p.name, &fonts::Font2, kAccent);

    // beat lamps
    const int spacing = 26;
    const int startX  = kW / 2 - (p.beats - 1) * spacing / 2;
    for (int i = 0; i < p.beats; ++i) {
        const int  x     = startX + i * spacing;
        const bool on    = (st.beat == i);
        const bool first = (p.accents & (1u << i)) != 0;
        if (on) {
            d->fillCircle(x, 58, 7, first ? rgb(251, 73, 52) : kAccent);
        } else {
            d->drawCircle(x, 58, 6, first ? rgb(106, 58, 47) : rgb(80, 73, 69));
        }
    }

    textTL(d, 12, 76, st.metroOn ? "PLAYING" : "STOPPED", &fonts::Font0,
           st.metroOn ? rgb(184, 187, 38) : kDim);
    textTL(d, 12, 90, "ENTER: start / stop", &fonts::Font0, rgb(146, 131, 116));
    textTL(d, 12, 100, "Fn+<> tempo   Fn+^v style", &fonts::Font0, rgb(146, 131, 116));
}

// ---------------------------------------------------------------------------
// page 5: progression player
// ---------------------------------------------------------------------------

const char* progSuffix(uint8_t qual)
{
    static const char* kSuffix[6] = {"", "m", "7", "maj7", "m7", "dim"};
    return kSuffix[qual % 6];
}

void progChordLabel(mt::Note keyTonic, bool keyMinor, const ProgStep& s, char* out, int cap)
{
    mt::Note root;
    int      ti = 0;
    progChord(keyTonic, keyMinor, s, root, ti);
    char nm[4];
    mt::noteName(root, nm);
    snprintf(out, (size_t)cap, "%s%s", nm, progSuffix(s.qual));
}

void drawProgPage(LovyanGFX* d, const UiState& st)
{
    // view 0: style list
    if (st.progView == 0) {
        textTL(d, 8, 14, "STYLE", &fonts::Font0, kDim);
        for (int i = 0; i < kProgStyleCount; ++i) {
            const int  y   = 26 + i * 13;
            const bool sel = (i == st.progStyle);
            if (sel) d->fillRoundRect(6, y - 1, 228, 12, 3, kSelBg);
            textTL(d, 12, y + 1, kProgStyles[i].name, &fonts::Font0, sel ? kFg : kDim);
        }
        return;
    }

    // view 1: the progressions inside a style
    if (st.progView == 1) {
        const ProgStyle& s = kProgStyles[st.progStyle];
        textTL(d, 8, 13, s.name, &fonts::Font2, kFg);
        for (int i = 0; i < s.count; ++i) {
            const int  y   = 38 + i * 13;
            const bool sel = (i == st.progSel);
            if (sel) d->fillRoundRect(6, y - 1, 228, 12, 3, kSelBg);
            textTL(d, 12, y + 1, s.items[i].name, &fonts::Font0, sel ? kFg : kDim);
        }
        return;
    }

    if (!st.progSteps || st.progLen <= 0) {
        textTL(d, 12, 40, "no progression", &fonts::Font0, kDim);
        return;
    }

    mt::Note tonic = mt::majorTonic(st.slot);
    if (st.progMinor) tonic = mt::relativeMinorOf(tonic);

    char key[16];
    char kn[4];
    mt::noteName(tonic, kn);
    snprintf(key, sizeof key, "%s %s", kn, st.progMinor ? "minor" : "major");
    char head[40];
    snprintf(head, sizeof head, "%s  %s", kProgStyles[st.progStyle].name, st.progName ? st.progName : "");
    textTL(d, 6, 12, head, &fonts::Font0, kDim);
    d->setFont(&fonts::Font0);
    textTL(d, kW - 6 - d->textWidth(key), 12, key, &fonts::Font0, kAccent);

    // chord slots, scrolled so the current one is always visible
    const int visible = 4;
    int       first   = 0;
    if (st.progLen > visible) {
        first = st.progPos - 1;
        if (first < 0) first = 0;
        if (first > st.progLen - visible) first = st.progLen - visible;
    }
    const uint8_t beats = kMetroPatterns[st.metroPattern].beats;

    for (int k = 0; k < visible; ++k) {
        const int i = first + k;
        if (i >= st.progLen) break;
        const int  x   = 6 + k * 58;
        const bool cur = (st.progPlaying && i == st.progPos);
        const bool sel = (st.progEdit && i == st.progSlot);

        if (cur) {
            d->fillRoundRect(x, 24, 54, 54, 4, kSelBg);
        } else if (sel) {
            d->drawRoundRect(x, 24, 54, 54, 4, kAccent);
        }

        // beat lamps inside the current step
        if (cur && beats > 1) {
            const int dotGap = 7;
            const int dotX   = x + 27 - (beats - 1) * dotGap / 2;
            for (int b = 0; b < beats; ++b) {
                const bool on = (b == st.beat);
                if (on) {
                    d->fillCircle(dotX + b * dotGap, 32, 2, kAccent);
                } else {
                    d->drawCircle(dotX + b * dotGap, 32, 2, rgb(80, 73, 69));
                }
            }
        }

        char label[10];
        progChordLabel(tonic, st.progMinor, st.progSteps[i], label, sizeof label);
        textMid(d, x + 27, 52, label, &fonts::Font2, cur ? kFg : rgb(235, 219, 178));

        char rom[8];
        progRoman(st.progSteps[i], st.progMinor, rom, sizeof rom);
        textMid(d, x + 27, 68, rom, &fonts::Font0, cur ? kAccent : kDim);
    }

    if (st.progEdit) {
        textTL(d, 6, 96, "Fn+<> chord   ENTER play", &fonts::Font0, rgb(146, 131, 116));
    } else {
        textTL(d, 6, 96, st.progPlaying ? "PLAYING" : "STOPPED", &fonts::Font0,
               st.progPlaying ? rgb(184, 187, 38) : kDim);
        textTL(d, 70, 96, "ENTER play   BKSP back", &fonts::Font0, rgb(146, 131, 116));
    }

    char pos[16];
    snprintf(pos, sizeof pos, "%d/%d", st.progPos + 1, st.progLen);
    d->setFont(&fonts::Font0);
    textTL(d, kW - 6 - d->textWidth(pos), 96, pos, &fonts::Font0, kDim);
}


// ---------------------------------------------------------------------------
// page 6: modulation helper
// ---------------------------------------------------------------------------

const char* modChordName(const mt::Note& root, uint8_t qual)
{
    static const char* kSuffix[3] = {"", "m", "dim"};
    char               nm[4];
    mt::noteName(root, nm);
    static char buf[8];
    snprintf(buf, sizeof buf, "%s%s", nm, kSuffix[qual % 3]);
    return buf;
}

void drawModPage(LovyanGFX* d, const UiState& st)
{
    ModInfo info;
    modAnalyse(st.slot, st.minorView, st.modTo, st.modMinor, info);

    char sn[6];
    char tn[6];
    snprintf(sn, sizeof sn, "%s%s", modChordName(info.fromTonic, 0), st.minorView ? "m" : "");
    snprintf(tn, sizeof tn, "%s%s", modChordName(info.toTonic, 0), st.modMinor ? "m" : "");

    // the two keys as chips, the focused one is filled
    const bool srcOn = (st.modFocus == 0);
    d->fillRoundRect(6, 12, 40, 18, 3, srcOn ? kSelBg : kBg);
    d->drawRoundRect(6, 12, 40, 18, 3, srcOn ? kAccent : kLine);
    textMid(d, 26, 21, sn, &fonts::Font2, srcOn ? kAccent : kDim);

    textMid(d, 52, 21, ">", &fonts::Font0, kDim);

    d->fillRoundRect(60, 12, 40, 18, 3, srcOn ? kBg : kSelBg);
    d->drawRoundRect(60, 12, 40, 18, 3, srcOn ? kLine : kAccent);
    textMid(d, 80, 21, tn, &fonts::Font2, srcOn ? kDim : kAccent);

    d->setFont(&fonts::Font0);
    textTL(d, kW - 6 - d->textWidth(info.relation), 18, info.relation, &fonts::Font0, kDim);

    // twelve slots of the circle, the path between the two keys is filled
    const int dir = (info.distance >= 0) ? 1 : -1;
    const int steps = (info.distance >= 0) ? info.distance : -info.distance;
    for (int i = 0; i < 12; ++i) {
        const int x = 22 + i * 17;
        if (i == st.slot) {
            d->fillCircle(x, 40, 4, kAccent);
        } else if (i == st.modTo) {
            d->fillCircle(x, 40, 4, kFg);
        } else {
            bool path = false;
            for (int k = 1; k < steps; ++k) {
                if ((st.slot + dir * k + 12) % 12 == i) path = true;
            }
            if (path) {
                d->fillCircle(x, 40, 2, kLine);
            } else {
                d->drawCircle(x, 40, 2, kLine);
            }
        }
    }

    // shared chords with the degree in each key
    char fromHdr[8];
    char toHdr[8];
    snprintf(fromHdr, sizeof fromHdr, "in %s", sn);
    snprintf(toHdr, sizeof toHdr, "in %s", tn);
    textTL(d, 8, 50, "SHARED", &fonts::Font0, kDim);
    d->setFont(&fonts::Font0);
    textTL(d, 172 - d->textWidth(fromHdr), 50, fromHdr, &fonts::Font0, kDim);
    textTL(d, 232 - d->textWidth(toHdr), 50, toHdr, &fonts::Font0, kDim);

    if (info.nPivot == 0) {
        textTL(d, 8, 63, "none: use the dominant", &fonts::Font0, rgb(124, 111, 100));
    }
    const int shown = info.nPivot > 3 ? 3 : info.nPivot;
    for (int i = 0; i < shown; ++i) {
        const Pivot& pv = info.pivots[i];
        const int    y  = 63 + i * 12;

        textTL(d, 8, y - 2, modChordName(pv.root, pv.qual), &fonts::Font2, kFg);

        const char* fr = mt::romanNumeral(pv.fromDeg, st.minorView);
        const char* tr = mt::romanNumeral(pv.toDeg, st.modMinor);
        d->setFont(&fonts::Font0);
        textTL(d, 172 - d->textWidth(fr), y + 2, fr, &fonts::Font0, kDim);
        textTL(d, 232 - d->textWidth(tr), y + 2, tr, &fonts::Font0, kAccent);
    }

    char line[24];
    snprintf(line, sizeof line, "%s7 > %s", modChordName(info.domRoot, 0), tn);
    textTL(d, 8, 104, "V7", &fonts::Font0, kDim);
    textTL(d, 40, 104, line, &fonts::Font0, kAccent);

    snprintf(line, sizeof line, "%sm7 %s7 > %s", modChordName(info.iiRoot, 0), modChordName(info.domRoot, 0), tn);
    textTL(d, 8, 116, "ii-V", &fonts::Font0, kDim);
    textTL(d, 40, 116, line, &fonts::Font0, kFg);
}

// ---------------------------------------------------------------------------
// Transient bar shown for a moment after the volume keys are used.
void drawVolumeBar(LovyanGFX* d, const UiState& st)
{
    if (!st.showVolume) return;

    d->fillRoundRect(8, 98, 224, 20, 5, rgb(60, 56, 54));
    d->drawRoundRect(8, 98, 224, 20, 5, rgb(80, 73, 69));
    textTL(d, 14, 104, "VOL", &fonts::Font0, kDim);

    const int barX = 46, barW = 130, barY = 104, barH = 8;
    d->fillRoundRect(barX, barY, barW, barH, 4, rgb(40, 40, 40));
    const int w = (barW * st.volume) / 100;
    if (w > 0) d->fillRoundRect(barX, barY, w, barH, 4, kAccent);

    char b[8];
    snprintf(b, sizeof b, "%d%%", st.volume);
    d->setFont(&fonts::Font0);
    textTL(d, kW - 12 - d->textWidth(b), 104, b, &fonts::Font0, kFg);
}

void drawTabs(LovyanGFX* d, const UiState& st)
{
    static const char* kNames[7] = {"CIRC", "CHORD", "SHAPE", "SCALE", "TEMPO", "PROG", "MOD"};
    for (int i = 0; i < 7; ++i) {
        const int  x  = (i * 240) / 7;
        const bool on = (i == st.page);
        if (on) {
            d->fillRoundRect(x + 1, 2, 32, 9, 2, kSelBg);
        }
        d->setFont(&fonts::Font0);
        const int w = d->textWidth(kNames[i]);
        textTL(d, x + (34 - w) / 2, 3, kNames[i], &fonts::Font0,
               on ? kFg : rgb(124, 111, 100));
    }
    d->drawFastHLine(0, kTabH, kW, kLine);
}

void drawFooter(LovyanGFX* d, const UiState& st)
{
    static const char* kHints[7] = {
        "TAB chord  Fn+<> key  ^v  ENTER",
        "TAB shape  Fn+<> root  ^v type",
        "TAB scale  Fn+<> voice  ^v type",
        "TAB tempo  Fn+<> root ^v ENTER",
        "TAB prog  Fn+<> tempo  ^v style",
        "TAB mod  Fn+<> move  ^v pick",
        "Fn+<> key  ^v mode  ENTER switch",
    };
    d->drawFastHLine(4, 124, kW - 8, kLine);
    textTL(d, 5, 126, kHints[st.page % 7], &fonts::Font0, rgb(146, 131, 116));

    char bat[8];
    if (st.battery < 0) {
        snprintf(bat, sizeof bat, "--");
    } else {
        snprintf(bat, sizeof bat, "%d%%", st.battery);
    }
    d->setFont(&fonts::Font0);
    int w = d->textWidth(bat);
    textTL(d, kW - 5 - w, 126, bat, &fonts::Font0,
           st.audioBusy ? rgb(184, 187, 38) : rgb(146, 131, 116));
}

}  // namespace

void uiSplash()
{
    LovyanGFX* d = gCanvas ? (LovyanGFX*)gCanvas : (LovyanGFX*)&M5.Display;
    const int  cx = kW / 2, cy = 44;

    d->fillScreen(kBg);
    for (int i = 0; i < 12; ++i) {
        const float a0 = -105.0f + 30.0f * i + 1.4f;
        const float a1 = a0 + 30.0f - 2.8f;
        fillSeg(d, cx, cy, 22, 40, a0, a1, hueColor(i * 30.0f, 0.70f, 0.82f));
    }
    outlineSeg(d, cx, cy, 22, 40, -103.6f, -76.4f, TFT_WHITE);  // C at the top
    d->fillCircle(cx, cy, 20, kBg);
    d->drawCircle(cx, cy, 16, rgb(60, 56, 54));

    d->setFont(&fonts::Font2);
    d->setTextDatum(textdatum_t::middle_center);
    d->setTextColor(kFg);
    d->drawString("MusicTools", cx, 97);

    d->setFont(&fonts::Font0);
    d->setTextColor(rgb(146, 131, 116));
    d->drawString("CHORDS SCALES TEMPO", cx, 114);

    if (gCanvas) gCanvas->pushSprite(0, 0);
}

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
    drawTabs(d, st);
    if (st.page == 0) {
        drawWheel(d, st);
        drawPanel(d, st);
    } else if (st.page == 1) {
        drawChordPage(d, st);
    } else if (st.page == 2) {
        drawGuitarPage(d, st);
    } else if (st.page == 3) {
        drawScalePage(d, st);
    } else if (st.page == 4) {
        drawMetroPage(d, st);
    } else if (st.page == 5) {
        drawProgPage(d, st);
    } else {
        drawModPage(d, st);
    }
    drawVolumeBar(d, st);
    drawFooter(d, st);

    if (gCanvas) {
        gCanvas->pushSprite(0, 0);
    }
}

















