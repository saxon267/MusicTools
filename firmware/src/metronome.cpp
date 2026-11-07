#include "metronome.h"

#include <M5Unified.h>
#include <cmath>

namespace {

constexpr uint32_t kRate    = 16000;
constexpr int      kSamples = 208;  // 13 ms

int16_t gAccent[kSamples];
int16_t gNormal[kSamples];

// A click is a transient, so it needs shaping: a short fade in so the first
// sample is not a step, and a fade out to zero before the buffer ends. Without
// them the codec clips and the click buzzes instead of ticking.
void render(int16_t* dst, float freq, float gain)
{
    const float dur    = kSamples / (float)kRate;
    const float attack = 0.0018f;
    const float tail   = 0.0025f;

    for (int i = 0; i < kSamples; ++i) {
        const float t = i / (float)kRate;

        float env = expf(-t * 95.0f);
        if (t < attack) env *= t / attack;
        const float left = dur - t;
        if (left < tail) env *= left / tail;
        if (env < 0.0f) env = 0.0f;

        const float s = sinf(6.2831853f * freq * t);
        float       v = s * env * gain;
        if (v > 22000.0f) v = 22000.0f;
        if (v < -22000.0f) v = -22000.0f;
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
    render(gAccent, 1180.0f, 8000.0f);
    render(gNormal, 790.0f, 4600.0f);
}

void metroClick(bool accented)
{
    if (!M5.Speaker.isEnabled()) return;
    M5.Speaker.playRaw(accented ? gAccent : gNormal, kSamples, kRate, false, 0, -1, false);
}
