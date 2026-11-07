#include "metronome.h"

#include <M5Unified.h>
#include <cmath>

namespace {

constexpr uint32_t kRate    = 16000;
constexpr int      kSamples = 240;  // 15 ms, short enough not to smear the beat

int16_t gAccent[kSamples];
int16_t gNormal[kSamples];

void render(int16_t* dst, float freq, float gain)
{
    for (int i = 0; i < kSamples; ++i) {
        const float t   = i / (float)kRate;
        const float env = expf(-t * 220.0f);  // sharp click, no tail
        float       s   = sinf(6.2831853f * freq * t);
        if (i > 0) {
            // a touch of noise on the attack makes it cut through
            s += 0.25f * sinf(6.2831853f * freq * 2.7f * t);
        }
        float v = s * env * gain;
        if (v > 32000.0f) v = 32000.0f;
        if (v < -32000.0f) v = -32000.0f;
        dst[i] = (int16_t)v;
    }
}

}  // namespace

const MetroPattern kMetroPatterns[] = {
    {"4/4", 4, 0x0001},
    {"3/4", 3, 0x0001},
    {"2/4", 2, 0x0001},
    {"6/8", 6, 0x0009},  // accents on beats 1 and 4
    {"5/4", 5, 0x0001},
    {"7/8", 7, 0x0015},  // 2+2+3 feel: accents on 1, 3, 5
};

const int kMetroPatternCount = (int)(sizeof(kMetroPatterns) / sizeof(kMetroPatterns[0]));

void metroInit()
{
    render(gAccent, 1568.0f, 15000.0f);
    render(gNormal, 1046.0f, 9000.0f);
}

void metroClick(bool accented)
{
    if (!M5.Speaker.isEnabled()) return;
    M5.Speaker.playRaw(accented ? gAccent : gNormal, kSamples, kRate, false, 0, -1, false);
}
