// Metronome: time-signature patterns and the two click sounds.
#pragma once

#include <cstdint>

struct MetroPattern {
    const char* name;
    uint8_t     beats;
    uint16_t    accents;  // bit i set = beat i is accented
};

extern const MetroPattern kMetroPatterns[];
extern const int          kMetroPatternCount;

void metroInit();
void metroClick(bool accented);
