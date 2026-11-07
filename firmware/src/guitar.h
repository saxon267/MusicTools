// Guitar fingering search for standard tuning (E A D G B e).
//
// Instead of shipping a table of every chord, this searches the fretboard for
// playable shapes and ranks them the way a chord book would: low position,
// compact span, lots of ringing strings, root in the bass. A small table of the
// classic open/barre triads is used as the first suggestion so that the most
// common chords always show the shape people actually play.
#pragma once

#include "theory.h"

struct Fingering {
    int8_t fret[6];  // -1 muted, 0 open, 1..12 fret
};

// Fills out[] with up to maxShapes fingerings for the chord, best first.
int guitarShapes(mt::Note root, int typeIdx, Fingering* out, int maxShapes);
