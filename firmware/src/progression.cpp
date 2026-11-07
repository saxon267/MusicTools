#include "progression.h"

#include <cstdio>
#include <cstring>

namespace {

// I-V-vi-IV and friends. Degrees index the key's own scale, so the chords come
// out correctly spelled for whatever key the circle is sitting on.
const ProgStep kPop1[] = {{0, 0, Q_MAJ}, {4, 0, Q_MAJ}, {5, 0, Q_MIN}, {3, 0, Q_MAJ}};
const ProgStep kPop2[] = {{5, 0, Q_MIN}, {3, 0, Q_MAJ}, {0, 0, Q_MAJ}, {4, 0, Q_MAJ}};
const ProgStep kPop3[] = {{0, 0, Q_MAJ}, {5, 0, Q_MIN}, {3, 0, Q_MAJ}, {4, 0, Q_MAJ}};
const ProgStep kPop4[] = {{3, 0, Q_MAJ}, {0, 0, Q_MAJ}, {4, 0, Q_MAJ}, {5, 0, Q_MIN}};

const ProgStep kRock1[] = {{0, 0, Q_MAJ}, {6, -1, Q_MAJ}, {3, 0, Q_MAJ}};
const ProgStep kRock2[] = {{0, 0, Q_MIN}, {5, 0, Q_MAJ}, {2, 0, Q_MAJ}, {6, 0, Q_MAJ}};
const ProgStep kRock3[] = {{0, 0, Q_MAJ}, {3, 0, Q_MAJ}, {4, 0, Q_MAJ}};

const ProgStep kJazz1[] = {{1, 0, Q_MIN}, {4, 0, Q_DOM7}, {0, 0, Q_MAJ7}};
const ProgStep kJazz2[] = {{0, 0, Q_MAJ7}, {5, 0, Q_MIN7}, {1, 0, Q_MIN7}, {4, 0, Q_DOM7}};
const ProgStep kJazz3[] = {{2, 0, Q_MIN7}, {5, 0, Q_MIN7}, {1, 0, Q_MIN7}, {4, 0, Q_DOM7}};
const ProgStep kJazz4[] = {{0, 0, Q_MIN}, {3, 0, Q_MIN}, {4, 0, Q_DOM7}, {0, 0, Q_MIN}};

const ProgStep kBlues12[] = {
    {0, 0, Q_DOM7}, {0, 0, Q_DOM7}, {0, 0, Q_DOM7}, {0, 0, Q_DOM7},
    {3, 0, Q_DOM7}, {3, 0, Q_DOM7}, {0, 0, Q_DOM7}, {0, 0, Q_DOM7},
    {4, 0, Q_DOM7}, {3, 0, Q_DOM7}, {0, 0, Q_DOM7}, {4, 0, Q_DOM7},
};
const ProgStep kBlues8[] = {
    {0, 0, Q_DOM7}, {0, 0, Q_DOM7}, {3, 0, Q_DOM7}, {0, 0, Q_DOM7},
    {4, 0, Q_DOM7}, {3, 0, Q_DOM7}, {0, 0, Q_DOM7}, {4, 0, Q_DOM7},
};

const ProgStep kClass1[] = {{0, 0, Q_MAJ}, {3, 0, Q_MAJ}, {4, 0, Q_MAJ}, {0, 0, Q_MAJ}};
const ProgStep kClass2[] = {{0, 0, Q_MIN}, {3, 0, Q_MIN}, {4, 0, Q_MAJ}, {0, 0, Q_MIN}};
const ProgStep kClass3[] = {{0, 0, Q_MAJ}, {4, 0, Q_MAJ}, {5, 0, Q_MIN}, {2, 0, Q_MIN},
                            {3, 0, Q_MAJ}, {0, 0, Q_MAJ}, {3, 0, Q_MAJ}, {4, 0, Q_MAJ}};

const ProgStep kFolk1[] = {{0, 0, Q_MIN}, {6, 0, Q_MAJ}, {5, 0, Q_MAJ}, {6, 0, Q_MAJ}};
const ProgStep kFolk2[] = {{0, 0, Q_MIN}, {5, 0, Q_MAJ}, {2, 0, Q_MAJ}, {6, 0, Q_MAJ}};
const ProgStep kFolk3[] = {{0, 0, Q_MAJ}, {1, 0, Q_MIN}, {0, 0, Q_MAJ}, {4, 0, Q_MAJ}};

const Progression kPopItems[] = {
    {"I-V-vi-IV", false, 4, kPop1},
    {"vi-IV-I-V", false, 4, kPop2},
    {"I-vi-IV-V", false, 4, kPop3},
    {"IV-I-V-vi", false, 4, kPop4},
};
const Progression kRockItems[] = {
    {"I-bVII-IV", false, 3, kRock1},
    {"i-bVI-bIII-bVII", true, 4, kRock2},
    {"I-IV-V", false, 3, kRock3},
};
const Progression kJazzItems[] = {
    {"ii-V-I", false, 3, kJazz1},
    {"I-vi-ii-V", false, 4, kJazz2},
    {"iii-vi-ii-V", false, 4, kJazz3},
    {"i-iv-V7-i", true, 4, kJazz4},
};
const Progression kBluesItems[] = {
    {"12-bar blues", false, 12, kBlues12},
    {"8-bar blues", false, 8, kBlues8},
};
const Progression kClassItems[] = {
    {"I-IV-V-I", false, 4, kClass1},
    {"i-iv-V-i", true, 4, kClass2},
    {"Pachelbel", false, 8, kClass3},
};
const Progression kFolkItems[] = {
    {"i-VII-VI-VII", true, 4, kFolk1},
    {"i-VI-III-VII", true, 4, kFolk2},
    {"I-ii-I-V", false, 4, kFolk3},
};

}  // namespace

const ProgStyle kProgStyles[] = {
    {"POP", kPopItems, 4},
    {"ROCK", kRockItems, 3},
    {"JAZZ", kJazzItems, 4},
    {"BLUES", kBluesItems, 2},
    {"CLASSICAL", kClassItems, 3},
    {"FOLK", kFolkItems, 3},
    {"CUSTOM", nullptr, 0},
};

const int kProgStyleCount = (int)(sizeof(kProgStyles) / sizeof(kProgStyles[0]));

ProgStep gCustomSteps[4] = {
    {0, 0, Q_MAJ}, {4, 0, Q_MAJ}, {5, 0, Q_MIN}, {3, 0, Q_MAJ},
};
const int kCustomLen = 4;

int progTypeIndex(uint8_t qual)
{
    switch (qual) {
        case Q_MIN: return 1;
        case Q_DOM7: return 4;
        case Q_MAJ7: return 5;
        case Q_MIN7: return 6;
        case Q_DIM: return 2;
        default: return 0;
    }
}

void progChord(mt::Note keyTonic, bool keyMinor, const ProgStep& s, mt::Note& root, int& typeIdx)
{
    const mt::Scale sc = mt::scaleOf(keyTonic, keyMinor);
    root               = sc.notes[s.degree % 7];
    root.acc           = (int8_t)(root.acc + s.acc);
    typeIdx            = progTypeIndex(s.qual);
}

void progRoman(const ProgStep& s, bool keyMinor, char* out, int cap)
{
    const char* rom = mt::romanNumeral(s.degree % 7, keyMinor);
    char        buf[8];
    int         n = 0;
    if (s.acc < 0) buf[n++] = 'b';
    if (s.acc > 0) buf[n++] = '#';
    for (int i = 0; rom[i] && n < 6; ++i) buf[n++] = rom[i];
    buf[n] = '\0';
    snprintf(out, (size_t)cap, "%s", buf);
}
