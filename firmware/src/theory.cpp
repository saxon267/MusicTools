#include "theory.h"

#include <cstdio>

namespace mt {

const int8_t kSlotFifths[kSlots] = {0, 1, 2, 3, 4, 5, 6, -5, -4, -3, -2, -1};

static const int8_t kMajorSteps[7] = {0, 2, 4, 5, 7, 9, 11};
static const int8_t kMinorSteps[7] = {0, 2, 3, 5, 7, 8, 10};

int letterNaturalPc(int letter)
{
    static const int8_t pcs[7] = {0, 2, 4, 5, 7, 9, 11};
    return pcs[imod(letter, 7)];
}

int pitchClass(Note n)
{
    return imod(letterNaturalPc(n.letter) + n.acc, 12);
}

void noteName(Note n, char* out)
{
    static const char* kLetters = "CDEFGAB";
    char              buf[4]    = {0, 0, 0, 0};
    int               i         = 0;
    buf[i++]                    = kLetters[imod(n.letter, 7)];
    for (int k = 0; k < (n.acc < 0 ? -n.acc : n.acc); ++k) {
        buf[i++] = n.acc < 0 ? 'b' : '#';
    }
    buf[i] = '\0';
    for (int k = 0; k <= i; ++k) {
        out[k] = buf[k];
    }
}

Note majorTonic(int slot)
{
    int  f      = kSlotFifths[imod(slot, kSlots)];
    Note n;
    n.letter = (int8_t)imod(f * 4, 7);
    n.acc    = (int8_t)(f >= 6 ? 1 : (f <= -2 ? -1 : 0));
    return n;
}

int signature(int slot)
{
    return kSlotFifths[imod(slot, kSlots)];
}

Scale scaleOf(Note tonic, bool minor)
{
    const int8_t* steps = minor ? kMinorSteps : kMajorSteps;
    Scale         s;
    int           base = pitchClass(tonic);
    for (int d = 0; d < 7; ++d) {
        int letter = imod(tonic.letter + d, 7);
        int target = imod(base + steps[d], 12);
        int diff   = target - letterNaturalPc(letter);
        while (diff > 6) diff -= 12;
        while (diff < -6) diff += 12;
        s.notes[d].letter = (int8_t)letter;
        s.notes[d].acc    = (int8_t)diff;
    }
    return s;
}

void diatonicChords(Note tonic, bool minor, Chord out[7])
{
    static const Quality kMajorQ[7] = {Quality::Maj, Quality::Min, Quality::Min,
                                       Quality::Maj, Quality::Maj, Quality::Min,
                                       Quality::Dim};
    static const Quality kMinorQ[7] = {Quality::Min, Quality::Dim, Quality::Maj,
                                       Quality::Min, Quality::Min, Quality::Maj,
                                       Quality::Maj};
    const Quality* q = minor ? kMinorQ : kMajorQ;
    Scale          s = scaleOf(tonic, minor);
    for (int d = 0; d < 7; ++d) {
        out[d].root = s.notes[d];
        out[d].q    = q[d];
    }
}

const char* qualitySuffix(Quality q)
{
    switch (q) {
        case Quality::Min: return "m";
        case Quality::Dim: return "dim";
        case Quality::Aug: return "aug";
        default: return "";
    }
}

void chordName(const Chord& c, char* out)
{
    char buf[4];
    noteName(c.root, buf);
    snprintf(out, 8, "%s%s", buf, qualitySuffix(c.q));
}

const char* romanNumeral(int degree, bool minor)
{
    static const char* kMajorRoman[7] = {"I", "ii", "iii", "IV", "V", "vi", "vii"};
    static const char* kMinorRoman[7] = {"i", "ii", "III", "iv", "v", "VI", "VII"};
    return minor ? kMinorRoman[imod(degree, 7)] : kMajorRoman[imod(degree, 7)];
}

Note relativeMinorOf(Note majorTonic)
{
    Scale s = scaleOf(majorTonic, false);
    return s.notes[5];
}

Note relativeMajorOf(Note minorTonic)
{
    Scale s = scaleOf(minorTonic, true);
    return s.notes[2];
}

}  // namespace mt
