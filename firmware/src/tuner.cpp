#include "tuner.h"

#include <M5Unified.h>
#include <cmath>

namespace {

constexpr int      kWindow = 2048;  // 128 ms at 16 kHz: ten periods of a low E
constexpr uint32_t kRate   = 16000;
constexpr int      kMinLag = 12;    // ~1330 Hz
constexpr int      kMaxLag = 320;   // ~50 Hz

int16_t      gSamples[kWindow];
float        gCorr[kMaxLag + 2];
float        gPrefix[kWindow + 1];
bool         gCaptured  = false;
float        gSmoothFreq = 0.0f;
uint32_t     gLastGoodMs = 0;

TunerReading gReading = {false, false, 0.0f, 0, 0.0f, 0};

// Normalised autocorrelation: dividing by the energy of both windows removes
// the bias towards short lags, so a quiet but periodic signal still scores
// close to 1 and the threshold means something.
float detectPitch(const int16_t* x, int n, int rate, float* strength)
{
    int32_t sum = 0;
    for (int i = 0; i < n; ++i) sum += x[i];
    const int mean = (int)(sum / n);

    gPrefix[0] = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float v  = (float)(x[i] - mean);
        gPrefix[i + 1] = gPrefix[i] + v * v;
    }
    const float energy = gPrefix[n];
    const float rms    = sqrtf(energy / (float)n);

    int lvl = (int)(log10f(rms + 1.0f) * 42.0f);
    if (lvl > 100) lvl = 100;
    if (lvl < 0) lvl = 0;
    gReading.level = lvl;

    if (energy < (float)n * 4.0f) {  // nearly silent, rms below 2
        *strength = 0.0f;
        return 0.0f;
    }

    float best = 0.0f;
    for (int lag = kMinLag; lag <= kMaxLag; ++lag) {
        float acc = 0.0f;
        for (int i = 0; i + lag < n; ++i) {
            acc += (float)(x[i] - mean) * (float)(x[i + lag] - mean);
        }
        const float e1  = gPrefix[n - lag];
        const float e2  = energy - gPrefix[lag];
        const float den = sqrtf(e1 * e2);
        const float r   = den > 1e-6f ? acc / den : 0.0f;
        gCorr[lag]      = r;
        if (r > best) best = r;
    }
    *strength = best;
    if (best < 0.45f) return 0.0f;

    // first strong peak, so a louder overtone does not win an octave up
    const float threshold = best * 0.88f;
    int         peakLag   = 0;
    for (int lag = kMinLag + 1; lag < kMaxLag; ++lag) {
        if (gCorr[lag] < threshold) continue;
        if (gCorr[lag] < gCorr[lag - 1] || gCorr[lag] < gCorr[lag + 1]) continue;
        peakLag = lag;
        break;
    }
    if (peakLag == 0) return 0.0f;

    const float y0  = gCorr[peakLag - 1];
    const float y1  = gCorr[peakLag];
    const float y2  = gCorr[peakLag + 1];
    const float den = y0 - 2.0f * y1 + y2;
    float       shift = 0.0f;
    if (fabsf(den) > 1e-6f) shift = 0.5f * (y0 - y2) / den;
    if (shift > 0.5f) shift = 0.5f;
    if (shift < -0.5f) shift = -0.5f;

    const float lagF = peakLag + shift;
    if (lagF <= 0.0f) return 0.0f;
    return rate / lagF;
}

void analyse()
{
    float          strength = 0.0f;
    const float    f        = detectPitch(gSamples, kWindow, (int)kRate, &strength);
    const uint32_t now      = millis();

    if (f > 40.0f && f < 1400.0f) {
        // smooth so the cents needle does not jitter
        gSmoothFreq = (gSmoothFreq > 0.0f) ? (gSmoothFreq * 0.45f + f * 0.55f) : f;
        gLastGoodMs = now;

        const float midi = 69.0f + 12.0f * log2f(gSmoothFreq / 440.0f);
        const int   note = (int)lroundf(midi);
        gReading.valid   = true;
        gReading.freq    = gSmoothFreq;
        gReading.note    = note;
        gReading.cents   = (midi - note) * 100.0f;
        return;
    }

    // Detection drops out for a window all the time. Blanking the display on
    // every miss is what made the note flash, so hold it for a moment.
    if (gReading.valid && (now - gLastGoodMs) > 700) {
        gReading.valid = false;
        gReading.freq  = 0.0f;
        gSmoothFreq    = 0.0f;
    }
}

}  // namespace

void tunerInit()
{
    gReading.mic = M5.Mic.isEnabled();
    if (!gReading.mic) return;

    auto cfg = M5.Mic.config();
    cfg.magnification      = 200;
    cfg.noise_filter_level = 0;
    M5.Mic.config(cfg);
    M5.Mic.begin();
}

void tunerPoll()
{
    if (!M5.Mic.isEnabled()) {
        gReading.mic = false;
        return;
    }
    if (M5.Mic.isRecording()) return;

    if (gCaptured) {
        analyse();
        gCaptured = false;
    }
    if (M5.Mic.record(gSamples, kWindow, kRate)) {
        gCaptured = true;
    }
}

const TunerReading& tunerLast()
{
    return gReading;
}
