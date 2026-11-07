// Circle-of-fifths music theory core. Pure C++, no Arduino dependency,
// so it can be unit-tested on a PC.
#pragma once

#include <cstdint>

namespace mt {

// A spelled note: letter C..B plus an accidental -2..+2.
struct Note {
    int8_t letter;  // 0=C 1=D 2=E 3=F 4=G 5=A 6=B
    int8_t acc;     // -2..+2
};

inline int imod(int v, int m)
{
    int r = v % m;
    return r < 0 ? r + m : r;
}

constexpr int kSlots = 12;

// Fifths position of each ring slot, C at the top, practical spelling on both
// ends of the circle (F# ... Db) instead of the theoretical G#/D#/A# majors.
extern const int8_t kSlotFifths[kSlots];

int  pitchClass(Note n);
int  letterNaturalPc(int letter);
void noteName(Note n, char* out);  // out >= 4 bytes, e.g. "F#", "Bb"

Note majorTonic(int slot);      // tonic of the major key in that ring slot
int  signature(int slot);       // signed fifths: >0 sharps, <0 flats

struct Scale {
    Note notes[7];
};
Scale scaleOf(Note tonic, bool minor);

enum class Quality : uint8_t { Maj, Min, Dim, Aug };

struct Chord {
    Note    root;
    Quality q;
};

void        diatonicChords(Note tonic, bool minor, Chord out[7]);
const char* qualitySuffix(Quality q);
void        chordName(const Chord& c, char* out);  // out >= 8 bytes

// Roman numeral without the diminished circle: "I", "ii", "vii" ...
const char* romanNumeral(int degree, bool minor);

Note relativeMinorOf(Note majorTonic);
Note relativeMajorOf(Note minorTonic);

}  // namespace mt
