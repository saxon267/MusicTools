#pragma once

#include <cstdint>

#include "theory.h"
#include "tuner.h"

struct UiState {
    int  page;        // 0 = circle, 1 = chord query, 2 = guitar, 3 = scales
    int  slot;        // ring slot 0..11, C at the top
    bool minorView;   // circle page: show the relative minor
    int  selDegree;   // circle page: selected diatonic degree 0..6
    int  battery;     // -1 when unknown
    int  volume;      // 0..100 global output volume
    bool showVolume;  // draw the transient volume bar
    bool audioBusy;

    // chord page
    char      chordText[12];
    mt::Note  chordRoot;
    int       chordType;
    bool      chordValid;
    int       gtrPage;  // visible page of guitar voicings
    mt::Note  scaleRoot;
    int       scaleType;

    // metronome page
    int  bpm;
    int  metroPattern;
    bool metroOn;
    int  beat;  // -1 before the first click

    // tuner page
    TunerReading tuner;
};

void uiBegin();
void uiSplash();  // boot logo, blocking (call once from setup)
bool uiHasCanvas();
void uiRender(const UiState& st);





