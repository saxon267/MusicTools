#include "tuner.h"

#include <M5Unified.h>
#include <cmath>

namespace {

constexpr int      kWindow  = 1024;   // 64 ms at 16 kHz, enough for a low E
constexpr uint32_t kRate    = 16000;
constexpr int      kMinLag  = 12;     // ~1330 Hz
constexpr int      kMaxLag  = 300;    // ~53 Hz

int16_t      gSamples[kWindow];
float        gCorr[kMaxLag + 2];
bool         gCaptured = false;
TunerReading gReading  = {false, false, 0.0f, 0, 0.0f, 0};

float detectPitch(const int16_t* x, int n, int rate)
{
    int32_t sum = 0;
    for (int i = 0; i < n; ++i) sum += x[i];
    const int mean = (int)(sum / n);

    int32_t energy = 0;
    for (int i = 0; i < n; ++i) {
        const int v = x[i] - mean;
        energy += v * v;
    }

    // level meter: log-ish so quiet signals still move the bar
    const float rms = sqrtf((float)energy / (float)n);
    int         lvl = (int)(log10f(rms + 1.0f) * 42.0f);
    if (lvl > 100) lvl = 100;
    if (lvl < 0) lvl = 0;
    gReading.level = lvl;

    if (energy < (int32_t)n * 24) return 0.0f;  // basically silence

    float best = 0.0f;
    for (int lag = kMinLag; lag <= kMaxLag; ++lag) {
        int32_t acc = 0;
        for (int i = 0; i + lag < n; ++i) {
            acc += (int32_t)(x[i] - mean) * (int32_t)(x[i + lag] - mean);
        }
        const float r = (float)acc / (float)(n - lag);
        gCorr[lag]    = r;
        if (r > best) best = r;
    }
    if (best <= 0.0f) return 0.0f;

    // take the first peak that is close to the strongest one, so a strong
    // second harmonic does not drag us an octave up
    const float threshold = best * 0.82f;
    int         peakLag   = 0;
    for (int lag = kMinLag + 1; lag < kMaxLag; ++lag) {
        if (gCorr[lag] < threshold) continue;
        if (gCorr[lag] < gCorr[lag - 1] || gCorr[lag] < gCorr[lag + 1]) continue;
        peakLag = lag;
        break;
    }
    if (peakLag == 0) return 0.0f;

    // parabolic interpolation around the peak for a stable cent reading
    const float y0 = gCorr[peakLag - 1];
    const float y1 = gCorr[peakLag];
    const float y2 = gCorr[peakLag + 1];
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
    const float f = detectPitch(gSamples, kWindow, (int)kRate);
    if (f < 40.0f || f > 1400.0f) {
        gReading.valid = false;
        return;
    }
    const float midi  = 69.0f + 12.0f * log2f(f / 440.0f);
    const int   note  = (int)lroundf(midi);
    gReading.valid    = true;
    gReading.freq     = f;
    gReading.note     = note;
    gReading.cents    = (midi - note) * 100.0f;
}

}  // namespace

void tunerInit()
{
    gReading.mic = M5.Mic.isEnabled();
    if (!gReading.mic) return;

    // the default magnification (16) leaves a quiet instrument almost silent
    auto cfg = M5.Mic.config();
    cfg.magnification     = 128;
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

