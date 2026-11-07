#include "audio.h"

#include <M5Unified.h>
#include <cmath>

namespace {

constexpr uint32_t kRate   = 16000;
constexpr int      kSamples = (int)(kRate * 0.9f);  // 0.9 s of audio, ~28.8 KB
constexpr int      kMaxVoices = 5;

int16_t gBuf[kSamples];

float noteFreq(int midi)
{
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

}  // namespace

void audioInit()
{
    if (!M5.Speaker.isEnabled()) return;
    M5.Speaker.setVolume(255);  // the 1 W speaker needs everything it can get
}

bool audioBusy()
{
    return M5.Speaker.isEnabled() && M5.Speaker.isPlaying();
}

void audioPlayChord(mt::Note root, int typeIdx)
{
    if (!M5.Speaker.isEnabled()) return;
    if (typeIdx < 0 || typeIdx >= mt::kChordTypeCount) return;

    const mt::ChordType& t = mt::kChordTypes[typeIdx];

    // Chord tones sit in the C3..B3 octave, with the root doubled an octave up
    // so the chord still reads as "this chord" on a 1 W speaker.
    const int rootMidi = 48 + mt::pitchClass(root);
    float     freq[kMaxVoices];
    float     amp[kMaxVoices];
    int       nv = 0;

    for (int i = 0; i < t.n && nv < kMaxVoices - 1; ++i) {
        freq[nv] = noteFreq(rootMidi + t.semis[i]);
        amp[nv]  = (i == 0) ? 1.0f : 0.72f;
        ++nv;
    }
    freq[nv] = noteFreq(rootMidi + 12);
    amp[nv]  = 0.35f;
    ++nv;

    // normalise the mix so the chord peaks near full scale; the old fixed gain
    // left it at about half of what the speaker can take
    float ampSum = 0.0f;
    for (int v = 0; v < nv; ++v) ampSum += amp[v];
    const float gain = 30000.0f / (ampSum * 1.42f);  // 1.42 = peak of the harmonic mix

    const float invRate = 1.0f / (float)kRate;
    const float dur     = kSamples * invRate;
    float       ph[kMaxVoices];
    for (int v = 0; v < nv; ++v) ph[v] = 0.0f;

    for (int i = 0; i < kSamples; ++i) {
        const float tsec = i * invRate;

        float env;
        const float attack  = 0.012f;
        const float release = 0.20f;
        if (tsec < attack) {
            env = tsec / attack;
        } else if (tsec > dur - release) {
            env = (dur - tsec) / release;
            if (env < 0) env = 0;
        } else {
            float d = (tsec - attack) / 0.25f;
            if (d > 1.0f) d = 1.0f;
            env = 1.0f - 0.38f * d;
        }

        float s = 0;
        for (int v = 0; v < nv; ++v) {
            ph[v] += 6.2831853f * freq[v] * invRate;
            if (ph[v] > 6.2831853f) ph[v] -= 6.2831853f;
            s += amp[v] * (sinf(ph[v]) + 0.30f * sinf(2.0f * ph[v]) + 0.12f * sinf(3.0f * ph[v]));
        }

        float val = s * env * gain;
        if (val > 32000.0f) val = 32000.0f;
        if (val < -32000.0f) val = -32000.0f;
        gBuf[i] = (int16_t)val;
    }

    M5.Speaker.playRaw(gBuf, kSamples, kRate, false, 0, -1, true);
}

void audioStop()
{
    if (M5.Speaker.isEnabled()) M5.Speaker.stop();
}

