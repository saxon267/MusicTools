#include "audio.h"

#include <M5Unified.h>
#include <cmath>

namespace {

// One buffer shared by both sounds: a chord needs 14400 samples at 16 kHz
// (0.9 s) and a scale needs at most 8 * 0.22 s at 8 kHz, which fits inside.
constexpr int      kBufSamples = 14400;
constexpr uint32_t kChordRate  = 16000;
constexpr uint32_t kScaleRate  = 8000;
constexpr int      kMaxVoices  = 5;

int16_t gBuf[kBufSamples];

float noteFreq(int midi)
{
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

}  // namespace

void audioInit()
{
    if (!M5.Speaker.isEnabled()) return;
    M5.Speaker.setVolume(255);  // overwritten with the saved level in setup()
}

void audioSetVolume(int pct)
{
    if (!M5.Speaker.isEnabled()) return;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    M5.Speaker.setVolume((uint8_t)((pct * 255 + 50) / 100));
}

bool audioBusy()
{
    return M5.Speaker.isEnabled() && M5.Speaker.isPlaying();
}

void audioStop()
{
    if (M5.Speaker.isEnabled()) M5.Speaker.stop();
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

    // normalise the mix so the chord peaks near full scale; a fixed gain left
    // it at about half of what the speaker can take
    float ampSum = 0.0f;
    for (int v = 0; v < nv; ++v) ampSum += amp[v];
    const float gain = 30000.0f / (ampSum * 1.42f);  // 1.42 = peak of the harmonic mix

    const float invRate = 1.0f / (float)kChordRate;
    const float dur     = kBufSamples * invRate;
    float       ph[kMaxVoices];
    for (int v = 0; v < nv; ++v) ph[v] = 0.0f;

    for (int i = 0; i < kBufSamples; ++i) {
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

    // repeat = 1 (play once); 0 would loop the buffer forever
    M5.Speaker.playRaw(gBuf, kBufSamples, kChordRate, false, 1, -1, true);
}

void audioPlayScale(mt::Note root, int typeIdx)
{
    if (!M5.Speaker.isEnabled()) return;
    if (typeIdx < 0 || typeIdx >= mt::kScaleTypeCount) return;

    const mt::ScaleType& t = mt::kScaleTypes[typeIdx];

    // one octave from C4, each note 0.22 s
    const int baseMidi = 60 + mt::pitchClass(root);
    const int perNote  = (int)(kScaleRate * 0.22f);
    int       total    = perNote * (int)t.n;
    if (total > kBufSamples) total = kBufSamples;
    if (total <= 0) return;

    const float invRate = 1.0f / (float)kScaleRate;
    int         idx     = 0;

    for (int n = 0; n < t.n && idx < total; ++n) {
        const float f   = noteFreq(baseMidi + t.semis[n]);
        const int   len = perNote;
        float       ph  = 0.0f;

        for (int i = 0; i < len && idx < total; ++i, ++idx) {
            const float tt  = i * invRate;
            const float rel = (len - i) * invRate;

            float env = tt < 0.010f ? tt / 0.010f : 1.0f;
            if (rel < 0.05f) env *= rel / 0.05f;
            if (env < 0) env = 0;

            ph += 6.2831853f * f * invRate;
            if (ph > 6.2831853f) ph -= 6.2831853f;

            const float s   = sinf(ph) + 0.25f * sinf(2.0f * ph);
            float       val = s * env * 9000.0f;
            if (val > 32000.0f) val = 32000.0f;
            if (val < -32000.0f) val = -32000.0f;
            gBuf[idx] = (int16_t)val;
        }
    }

    M5.Speaker.playRaw(gBuf, total, kScaleRate, false, 1, -1, true);
}


