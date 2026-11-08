// Modulation helper: how do you get from the current key to another one?
#pragma once

#include "theory.h"

struct Pivot {
    mt::Note root;
    uint8_t  qual;     // 0 major, 1 minor, 2 diminished
    uint8_t  fromDeg;  // degree in the source key
    uint8_t  toDeg;    // degree in the target key
};

struct ModInfo {
    int         distance;  // fifths, -5..+6
    const char* relation;  // short label for the header
    int         nPivot;
    Pivot       pivots[7];
    mt::Note    toTonic;
    bool        toMinor;
    mt::Note    domRoot;  // V7 of the target
    mt::Note    iiRoot;   // ii7 of the target
};

void modAnalyse(int fromSlot, int toSlot, bool toMinor, ModInfo& out);
