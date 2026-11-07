#pragma once

#include <cstdint>

struct UiState {
    int  slot;        // ring slot 0..11, C at the top
    bool minorView;   // show the relative minor instead of the major key
    int  selDegree;   // selected diatonic degree 0..6
    int  battery;     // -1 when unknown
    bool audioBusy;
};

void uiBegin();
bool uiHasCanvas();
void uiRender(const UiState& st);
