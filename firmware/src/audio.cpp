#include "audio.h"

#include <M5Unified.h>
#include <cmath>

namespace {

constexpr uint32_t kRate = 16000;
constexpr int      kSamples = (int)(kRate * 0.9f);  // 0.9 s of audio, ~28.8 KB

int16_t gBuf[kSamples];

float noteFreq(int midi)
{
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

}  // namespace

void audioInit()
{
    if (!M5.Speaker.isEnabled()) return;
    M5.Speaker.setVolume(110);
}

bool audioBusy()
{
    return M5.Speaker.isEnabled() && M5.Speaker.isPlaying();
}

void audioPlayChord(const mt::Chord& c)
{
    if (!M5.Speaker.isEnabled()) return;

    static const int8_t kMaj[3] = {0, 4, 7};
    static const int8_t kMin[3] = {0, 3, 7};
    static const int8_t kDim[3] = {0, 3, 6};
    static const int8_t kAug[3] = {0, 4, 8};

    const int8_t* iv = kMaj;
    switch (c.q) {
        case mt::Quality::Min: iv = kMin; break;
        case mt::Quality::Dim: iv = kDim; break;
        case mt::Quality::Aug: iv = kAug; break;
        default: break;
    }

    // root sits in the C3..B3 octave, plus the octave above for body
    const int rootMidi = 48 + mt::pitchClass(c.root);
    int       midi[4]  = {rootMidi, rootMidi + iv[1], rootMidi + iv[2], rootMidi + 12};
    float     amp[4]   = {1.0f, 0.85f, 0.75f, 0.45f};

    float freq[4];
    for (int v = 0; v < 4; ++v) freq[v] = noteFreq(midi[v]);

    const float invRate = 1.0f / (float)kRate;
    float       ph[4]   = {0, 0, 0, 0};

    for (int i = 0; i < kSamples; ++i) {
        const float t = i * invRate;

        float env;
        const float attack = 0.012f;
        const float release = 0.20f;
        const float dur     = kSamples * invRate;
        if (t < attack) {
            env = t / attack;
        } else if (t > dur - release) {
            env = (dur - t) / release;
            if (env < 0) env = 0;
        } else {
            // gentle decay to a 0.62 sustain
            float d = (t - attack) / 0.25f;
            if (d > 1.0f) d = 1.0f;
            env = 1.0f - 0.38f * d;
        }

        float s = 0;
        for (int v = 0; v < 4; ++v) {
            ph[v] += 6.2831853f * freq[v] * invRate;
            if (ph[v] > 6.2831853f) ph[v] -= 6.2831853f;
            s += amp[v] * (sinf(ph[v]) + 0.30f * sinf(2.0f * ph[v]) + 0.12f * sinf(3.0f * ph[v]));
        }

        float val = s * env * 3400.0f;
        if (val > 32000.0f) val = 32000.0f;
        if (val < -32000.0f) val = -32000.0f;
        gBuf[i] = (int16_t)val;
    }

    M5.Speaker.playRaw(gBuf, kSamples, kRate, false, 0, -1, true);
}
