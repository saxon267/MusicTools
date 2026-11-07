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

// ---------------------------------------------------------------------------
// Chord dictionary: used by the chord query page (forward and reverse lookup).
// ---------------------------------------------------------------------------

struct ChordType {
    const char* suffix;    // what the user types, "" for a plain major triad
    const char* label;     // short name for the screen
    uint8_t     n;         // 3 or 4 chord tones
    int8_t      semis[4];  // semitone offsets from the root: 0,4,7,10 ...
    int8_t      degrees[4];  // letter offsets from the root: 0,2,4,6 ...
};

extern const ChordType kChordTypes[];
extern const int       kChordTypeCount;

int chordTypeFromSuffix(const char* suffix);  // -1 when unknown

// Spells the chord tones the way a musician writes them (G7 -> G B D F,
// Db7 -> Db F Ab Cb).
void spellChord(Note root, int typeIdx, Note out[4]);

bool parseChord(const char* text, Note& root, int& typeIdx);
void formatChord(Note root, int typeIdx, char* out, int cap);
Note pcToNote(int pc, bool preferFlats);

struct KeyHit {
    int8_t  slot;     // 0..11 ring slot
    bool    minor;    // natural minor key
    int8_t  degree;   // 0..6, the chord's scale degree in that key
    uint8_t quality;  // 0=maj 1=min 2=dim 3=aug
};

// Every key whose diatonic chords contain this exact chord, in circle order.
int findChordInKeys(Note root, int typeIdx, KeyHit* hits, int maxHits);

// True when the chord behaves as a dominant: it resolves a fifth down.
bool dominantTarget(Note root, int typeIdx, Note& target);

const char* intervalName(int semis);  // 0->"1", 4->"3", 10->"b7"

// ---------------------------------------------------------------------------
// Scale dictionary for the scale query page.
// ---------------------------------------------------------------------------

struct ScaleType {
    const char* name;
    uint8_t     n;
    int8_t      semis[8];    // semitone offsets from the tonic
    int8_t      degrees[8];  // letter offsets, so spelling stays correct
};

extern const ScaleType kScaleTypes[];
extern const int       kScaleTypeCount;

// Spells every scale tone; out[] needs room for 8 notes.
void spellScale(Note root, int typeIdx, Note out[8]);

}  // namespace mt

