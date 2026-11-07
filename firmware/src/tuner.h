// Chromatic tuner: captures a short window from the microphone and estimates
// the fundamental with autocorrelation.
#pragma once

#include <cstdint>

struct TunerReading {
    bool  mic;    // false when the board has no usable microphone
    bool  valid;  // a pitch was found in the last window
    float freq;   // Hz
    int   note;   // MIDI note number
    float cents;  // -50..+50
};

void tunerInit();
void tunerPoll();  // call frequently; captures and analyses asynchronously
const TunerReading& tunerLast();
