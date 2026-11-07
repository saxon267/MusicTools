#pragma once

#include <cstdint>

#include "theory.h"

struct UiState {
    int  page;        // 0 = circle of fifths, 1 = chord query, 2 = guitar
    int  slot;        // ring slot 0..11, C at the top
    bool minorView;   // circle page: show the relative minor
    int  selDegree;   // circle page: selected diatonic degree 0..6
    int  battery;     // -1 when unknown
    bool audioBusy;

    // chord page
    char      chordText[12];
    mt::Note  chordRoot;
    int       chordType;
    bool      chordValid;
    int       gtrPage;  // visible page of guitar voicings
};

void uiBegin();
bool uiHasCanvas();
void uiRender(const UiState& st);

