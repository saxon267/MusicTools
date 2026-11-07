// Chord progressions: a small library of classic changes plus a custom slot.
#pragma once

#include <cstdint>

#include "theory.h"

enum : uint8_t {
    Q_MAJ  = 0,
    Q_MIN  = 1,
    Q_DOM7 = 2,
    Q_MAJ7 = 3,
    Q_MIN7 = 4,
    Q_DIM  = 5,
};

struct ProgStep {
    uint8_t degree;  // 0..6, index into the key's scale
    int8_t  acc;     // extra accidental on the root, -1 / 0 / +1
    uint8_t qual;    // Q_*
};

struct Progression {
    const char*     name;
    bool            minor;  // read the degrees from the relative minor key
    uint8_t         len;
    const ProgStep* steps;
};

struct ProgStyle {
    const char*        name;
    const Progression* items;
    int                count;
};

extern const ProgStyle kProgStyles[];
extern const int       kProgStyleCount;  // includes the CUSTOM entry

// Resolves one step against a key into a chord the synth understands.
void progChord(mt::Note keyTonic, bool keyMinor, const ProgStep& s, mt::Note& root, int& typeIdx);

// Chord type index for a quality byte.
int progTypeIndex(uint8_t qual);

// Roman numeral label such as "bVII" or "ii" for the editor.
void progRoman(const ProgStep& s, bool keyMinor, char* out, int cap);

// The custom progression starts as I - V - vi - IV.
extern ProgStep gCustomSteps[4];
extern const int kCustomLen;
