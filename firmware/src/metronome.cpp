#include "metronome.h"

#include <M5Unified.h>
#include <cmath>

namespace {

// Generated at the speaker's own sample rate: resampling a short transient is
// what made the old click sound like a broken buzz.
constexpr int kMaxRate    = 48000;
constexpr int kMaxSamples = (int)(kMaxRate * 0.04f);

int16_t  gAccent[kMaxSamples];
int16_t  gNormal[kMaxSamples];
uint32_t gRate = 48000;
int      gLen  = 0;

void render(int16_t* dst, float freq, float gain)
{
    const float dur    = gLen / (float)gRate;
    const float attack = 0.002f;
    const float tail   = 0.004f;

    for (int i = 0; i < gLen; ++i) {
        const float t = i / (float)gRate;

        float env = expf(-t * 130.0f);
        if (t < attack) env *= t / attack;
        const float left = dur - t;
        if (left < tail) env *= left / tail;
        if (env < 0.0f) env = 0.0f;

        const float s = sinf(6.2831853f * freq * t);
        float       v = s * env * gain;
        if (v > 20000.0f) v = 20000.0f;
        if (v < -20000.0f) v = -20000.0f;
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
    uint32_t rate = M5.Speaker.config().sample_rate;
    if (rate < 8000 || rate > (uint32_t)kMaxRate) rate = 48000;
    gRate = rate;
    gLen  = (int)(rate * 0.028f);  // 28 ms: a short "beep", not a click

    render(gAccent, 2000.0f, 15000.0f);
    render(gNormal, 1350.0f, 9500.0f);
}

void metroClick(bool accented)
{
    if (!M5.Speaker.isEnabled()) return;
    M5.Speaker.playRaw(accented ? gAccent : gNormal, gLen, gRate, false, 0, -1, false);
}
