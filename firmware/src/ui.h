#pragma once

#include <cstdint>

#include "progression.h"
#include "theory.h"

struct UiState {
    int  page;        // 0 circle, 1 chord, 2 guitar, 3 scales, 4 tempo, 5 progression
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

    // progression page
    int             progView;  // 0 style list, 1 progression list, 2 player
    int             progStyle;
    int             progSel;
    int             progPos;
    bool            progPlaying;
    bool            progEdit;
    int             progSlot;
    bool            progMinor;
    const ProgStep* progSteps;
    int             progLen;
    const char*     progName;

    // modulation page
    int  modTo;     // target ring slot
    bool modMinor;  // target is the relative minor of that slot
};

void uiBegin();
void uiSplash();  // boot logo, blocking (call once from setup)
bool uiHasCanvas();
void uiRender(const UiState& st);







