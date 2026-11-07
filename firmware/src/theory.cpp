#include "theory.h"

#include <cctype>
#include <cstdio>
#include <cstring>

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
    char              buf[6]    = {0, 0, 0, 0, 0, 0};
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

// ---------------------------------------------------------------------------
// Chord dictionary
// ---------------------------------------------------------------------------

// Kept in a deliberate order: the up/down arrows walk this list, so neighbouring
// entries should be the variations a player actually compares.
const ChordType kChordTypes[] = {
    {"", "major", 3, {0, 4, 7}, {0, 2, 4}},
    {"m", "minor", 3, {0, 3, 7}, {0, 2, 4}},
    {"dim", "dim", 3, {0, 3, 6}, {0, 2, 4}},
    {"aug", "aug", 3, {0, 4, 8}, {0, 2, 4}},
    {"7", "dom 7", 4, {0, 4, 7, 10}, {0, 2, 4, 6}},
    {"maj7", "maj 7", 4, {0, 4, 7, 11}, {0, 2, 4, 6}},
    {"m7", "min 7", 4, {0, 3, 7, 10}, {0, 2, 4, 6}},
    {"m7b5", "half dim 7", 4, {0, 3, 6, 10}, {0, 2, 4, 6}},
    {"dim7", "dim 7", 4, {0, 3, 6, 9}, {0, 2, 4, 6}},
    {"6", "maj 6", 4, {0, 4, 7, 9}, {0, 2, 4, 5}},
    {"m6", "min 6", 4, {0, 3, 7, 9}, {0, 2, 4, 5}},
    {"sus4", "sus 4", 3, {0, 5, 7}, {0, 3, 4}},
    {"sus2", "sus 2", 3, {0, 2, 7}, {0, 1, 4}},
    {"7sus4", "7 sus 4", 4, {0, 5, 7, 10}, {0, 3, 4, 6}},
};

const int kChordTypeCount = (int)(sizeof(kChordTypes) / sizeof(kChordTypes[0]));

int chordTypeFromSuffix(const char* suffix)
{
    for (int i = 0; i < kChordTypeCount; ++i) {
        if (strcmp(suffix, kChordTypes[i].suffix) == 0) return i;
    }
    return -1;
}

void spellChord(Note root, int typeIdx, Note out[4])
{
    const ChordType& t    = kChordTypes[typeIdx];
    const int        base = pitchClass(root);
    for (int i = 0; i < t.n; ++i) {
        int letter = imod(root.letter + t.degrees[i], 7);
        int target = imod(base + t.semis[i], 12);
        int diff   = target - letterNaturalPc(letter);
        while (diff > 6) diff -= 12;
        while (diff < -6) diff += 12;
        out[i].letter = (int8_t)letter;
        out[i].acc    = (int8_t)diff;
    }
}

bool parseChord(const char* text, Note& root, int& typeIdx)
{
    if (!text) return false;
    const char* p = text;
    while (*p == ' ') ++p;

    char c = *p;
    if (c >= 'a' && c <= 'g') c = (char)(c - 'a' + 'A');
    static const char kLetters[] = "CDEFGAB";
    const char*       lp         = strchr(kLetters, c);
    if (!lp) return false;

    root.letter = (int8_t)(lp - kLetters);
    root.acc    = 0;
    ++p;

    if (*p == '#') {
        root.acc = 1;
        ++p;
    } else if (*p == 'b') {
        root.acc = -1;
        ++p;
    }

    char suf[8];
    int  i = 0;
    while (*p && i < 7) {
        suf[i++] = (char)tolower((unsigned char)*p);
        ++p;
    }
    suf[i] = '\0';

    int t = chordTypeFromSuffix(suf);
    if (t < 0) return false;
    typeIdx = t;
    return true;
}

void formatChord(Note root, int typeIdx, char* out, int cap)
{
    char nm[4];
    noteName(root, nm);
    snprintf(out, (size_t)cap, "%s%s", nm, kChordTypes[typeIdx].suffix);
}

Note pcToNote(int pc, bool preferFlats)
{
    static const int8_t kSharpLetter[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    static const int8_t kSharpAcc[12]    = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    static const int8_t kFlatLetter[12]  = {0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6};
    static const int8_t kFlatAcc[12]     = {0, -1, 0, -1, 0, 0, -1, 0, -1, 0, -1, 0};
    pc = imod(pc, 12);
    Note n;
    n.letter = preferFlats ? kFlatLetter[pc] : kSharpLetter[pc];
    n.acc    = preferFlats ? kFlatAcc[pc] : kSharpAcc[pc];
    return n;
}

int findChordInKeys(Note root, int typeIdx, KeyHit* hits, int maxHits)
{
    const ChordType& t   = kChordTypes[typeIdx];
    const int        rpc = pitchClass(root);
    int              count = 0;

    for (int slot = 0; slot < kSlots; ++slot) {
        for (int m = 0; m < 2; ++m) {
            Note tonic = majorTonic(slot);
            if (m) tonic = relativeMinorOf(tonic);
            Scale s = scaleOf(tonic, m != 0);

            for (int d = 0; d < 7; ++d) {
                Note cn[4];
                cn[0] = s.notes[d];
                cn[1] = s.notes[(d + 2) % 7];
                cn[2] = s.notes[(d + 4) % 7];
                if (t.n == 4) cn[3] = s.notes[(d + 6) % 7];

                if (pitchClass(cn[0]) != rpc) continue;

                bool ok = true;
                for (int i = 0; i < t.n; ++i) {
                    if (imod(pitchClass(cn[i]) - rpc, 12) != t.semis[i]) {
                        ok = false;
                        break;
                    }
                }
                if (!ok) continue;

                const int third = imod(pitchClass(cn[1]) - rpc, 12);
                const int fifth = imod(pitchClass(cn[2]) - rpc, 12);
                uint8_t   q     = 0;
                if (third == 3 && fifth == 6) {
                    q = 2;
                } else if (fifth == 8) {
                    q = 3;
                } else if (third == 3) {
                    q = 1;
                }

                if (count < maxHits) {
                    hits[count].slot    = (int8_t)slot;
                    hits[count].minor   = (m != 0);
                    hits[count].degree  = (int8_t)d;
                    hits[count].quality = q;
                    ++count;
                }
            }
        }
    }
    return count;
}

bool dominantTarget(Note root, int typeIdx, Note& target)
{
    // Only shapes that actually pull a fifth down.
    if (typeIdx != 0 && typeIdx != 4 && typeIdx != 13) return false;
    target = pcToNote(imod(pitchClass(root) + 5, 12), root.acc < 0);
    return true;
}

const char* intervalName(int semis)
{
    static const char* kNames[12] = {"1", "b2", "2", "b3", "3", "4",
                                     "b5", "5", "#5", "6", "b7", "7"};
    return kNames[imod(semis, 12)];
}

}  // namespace mt
