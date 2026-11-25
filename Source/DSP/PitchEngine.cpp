#include "PitchEngine.h"

#include <algorithm>
#include <limits>

namespace tunec
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

inline double clampd (double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline double smoothstep (double x)
{
    x = clampd (x, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}
inline double midiOf (double hz) { return 69.0 + 12.0 * std::log2 (hz / 440.0); }
inline double hzOf (double midi) { return 440.0 * std::exp2 ((midi - 69.0) / 12.0); }
} // namespace

float PitchEngine::hermite (const float* b, int64_t mask, double pos)
{
    const int64_t i = (int64_t) std::floor (pos);
    const float f = (float) (pos - (double) i);
    const float ym1 = b[(i - 1) & mask], y0 = b[i & mask], y1 = b[(i + 1) & mask], y2 = b[(i + 2) & mask];
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * f + c2) * f + c1) * f + y0;
}

void PitchEngine::prepare (double sr, int nch)
{
    sampleRate = sr;
    channels = std::max (1, std::min (nch, kMaxChannels));
    decim = std::max (1, (int) std::floor (sr / 12000.0));
    srDec = sr / decim;
    maxTau = (int) std::ceil (srDec / 28.0) + 4;
    winBuf.assign ((size_t) (2 * maxTau + 8), 0.0f);
    yinCmnd.assign ((size_t) (maxTau + 4), 0.0f);
    decRing.assign ((size_t) kDecRing, 0.0f);
    for (auto& r : ring) r.assign ((size_t) kRing, 0.0f);
    hop = std::max (32, (int) (sr * 0.003));
    hopSec = (double) hop / sr;
    smCoef = 1.0 - std::exp (-1.0 / (0.002 * sr));
    bypassCoef = 1.0 - std::exp (-1.0 / (0.010 * sr));
    configureForInput();
    reset();
}

void PitchEngine::reset()
{
    for (auto& r : ring) std::fill (r.begin(), r.end(), 0.0f);
    std::fill (decRing.begin(), decRing.end(), 0.0f);
    writeIdx = 0; decPos = 0; decCnt = 0; hopCnt = 0; decAcc = 0;
    hist = { 0, 0, 0 };
    voicedState = false; unvoicedHops = 99; gapSec = 10.0; noteAge = 0.0;
    center = 60.0; period = sampleRate / 150.0; shiftState = 0.0; targetShift = 0.0;
    lastTargetNote = -1; haveTarget = false; shiftSm = 0.0; ratio = 1.0; phi = 1.0;
    for (auto& g : grains) g.active = false;
    nextGrain = 0; haveLast = false; lastC = 0;
    vibPhase = 0; vibEnv = 0; vibAgeMs = 0; lfoSm = 0; rateMul = 1; depthMul = 1;
    bypassMix = p.bypass ? 1.0 : 0.0;
    tm = {};
    dirtyAllowed = true;
}

void PitchEngine::configureForInput()
{
    switch (p.inputType)
    {
        case InputType::Soprano:        fMin = 140.0; fMax = 1400.0; break;
        case InputType::AltoTenor:      fMin = 80.0;  fMax = 900.0;  break;
        case InputType::LowMale:        fMin = 55.0;  fMax = 500.0;  break;
        case InputType::Instrument:     fMin = 70.0;  fMax = 1500.0; break;
        case InputType::BassInstrument: fMin = 30.0;  fMax = 400.0;  break;
    }
    tauMin = std::max (2, (int) std::floor (srDec / fMax));
    tauMax = std::min (maxTau, (int) std::ceil (srDec / fMin));
    latency = (int) (sampleRate / fMin) + hop;
}

void PitchEngine::setParams (const EngineParams& np)
{
    const bool inputChanged = np.inputType != p.inputType;
    const bool tableChanged = np.key != p.key || np.scale != p.scale || np.useMidiTargets != p.useMidiTargets
                              || np.midiOctavesMatch != p.midiOctavesMatch;
    p = np;
    if (inputChanged) configureForInput();
    if (tableChanged) dirtyAllowed = true;
}

void PitchEngine::noteOn (int n)
{
    if (n < 0 || n > 127) return;
    held[(size_t) n] = true;
    if (learning)
    {
        if (learnFresh) { customMask = 0; learnFresh = false; }
        customMask |= (1 << (n % 12));
    }
    dirtyAllowed = true;
}

void PitchEngine::noteOff (int n)
{
    if (n < 0 || n > 127) return;
    held[(size_t) n] = false;
    dirtyAllowed = true;
}

void PitchEngine::allNotesOff()
{
    held.fill (false);
    dirtyAllowed = true;
}

void PitchEngine::setLearning (bool shouldLearn)
{
    if (shouldLearn && ! learning) learnFresh = true;
    if (shouldLearn != learning) dirtyAllowed = true;
    learning = shouldLearn;
}

void PitchEngine::rebuildAllowed()
{
    dirtyAllowed = false;
    allowed.fill (false);
    if (p.useMidiTargets)
    {
        std::array<bool, 12> pc {};
        for (int n = 0; n < 128; ++n)
            if (held[(size_t) n]) pc[(size_t) (n % 12)] = true;
        int mask = 0;
        for (int i = 0; i < 12; ++i) if (pc[(size_t) i]) mask |= (1 << i);
        activeMask = mask;
        for (int n = 0; n < 128; ++n)
            allowed[(size_t) n] = p.midiOctavesMatch ? held[(size_t) n] : pc[(size_t) (n % 12)];
        return;
    }

    int mask;
    if (learning || p.scale == kCustomScaleIndex)
        mask = customMask != 0 ? customMask : 0xFFF;
    else
    {
        const int rel = kScaleMasks[(size_t) std::max (0, std::min (p.scale, kNumScales - 1))];
        mask = 0;
        for (int pcI = 0; pcI < 12; ++pcI)
            if ((rel >> (((pcI - p.key) % 12 + 12) % 12)) & 1) mask |= (1 << pcI);
    }
    activeMask = mask;
    for (int n = 0; n < 128; ++n) allowed[(size_t) n] = (mask >> (n % 12)) & 1;
}

double PitchEngine::chooseTarget (double m)
{
    const double detune = p.detuneCents / 100.0;
    const double mm = m - detune;
    const int c = (int) std::lround (mm);
    int best = -1;
    double bestD = 1e9;
    for (int off = 0; off <= 64; ++off)
    {
        for (int s = 0; s < 2; ++s)
        {
            const int n = s ? c + off : c - off;
            if (n < 0 || n > 127 || ! allowedNote (n)) continue;
            const double d = std::fabs (mm - n);
            if (d < bestD) { bestD = d; best = n; }
        }
        if (best >= 0 && off > bestD + 1) break;
    }
    if (best < 0) return std::numeric_limits<double>::quiet_NaN();
    if (lastTargetNote >= 0 && allowedNote (lastTargetNote) && std::fabs (mm - lastTargetNote) <= bestD + 0.15)
        best = lastTargetNote;
    lastTargetNote = best;
    return best + detune;
}

// YIN pitch detector on the decimated signal.
bool PitchEngine::detectPitch (double& f0, float& rms)
{
    const int W = tauMax;
    const int N = 2 * W;
    if (decPos < N) { rms = 0; return false; }

    double mean = 0;
    for (int k = 0; k < N; ++k)
    {
        const float v = decRing[(size_t) ((decPos - N + k) & (kDecRing - 1))];
        winBuf[(size_t) k] = v;
        mean += v;
    }
    mean /= N;
    double e = 0;
    for (int k = 0; k < N; ++k)
    {
        winBuf[(size_t) k] -= (float) mean;
        e += (double) winBuf[(size_t) k] * winBuf[(size_t) k];
    }
    rms = (float) std::sqrt (e / N);
    if (rms < 0.0015f) return false;

    double running = 0;
    yinCmnd[0] = 1.0f;
    for (int tau = 1; tau <= W; ++tau)
    {
        double s = 0;
        const float* a = winBuf.data();
        const float* b = winBuf.data() + tau;
        for (int j = 0; j < W; ++j)
        {
            const float d = a[j] - b[j];
            s += (double) d * d;
        }
        running += s;
        yinCmnd[(size_t) tau] = (float) (running > 0 ? s * tau / running : 1.0);
    }

    constexpr float threshold = 0.15f;
    int found = -1;
    for (int tau = tauMin; tau < W; ++tau)
    {
        if (yinCmnd[(size_t) tau] < threshold)
        {
            while (tau + 1 < W && yinCmnd[(size_t) tau + 1] < yinCmnd[(size_t) tau]) ++tau;
            found = tau;
            break;
        }
    }
    if (found < 2 || found >= W - 1) return false;

    const double a = yinCmnd[(size_t) found - 1], b = yinCmnd[(size_t) found], c = yinCmnd[(size_t) found + 1];
    const double den = a - 2.0 * b + c;
    const double shift = std::fabs (den) > 1e-9 ? 0.5 * (a - c) / den : 0.0;
    const double tauI = found + clampd (shift, -1.0, 1.0);
    f0 = srDec / tauI;
    return f0 >= fMin * 0.9 && f0 <= fMax * 1.1;
}

void PitchEngine::analyze()
{
    if (dirtyAllowed) rebuildAllowed();

    double f0 = 0;
    float rms = 0;
    const bool detected = detectPitch (f0, rms);
    tm.level = rms;

    if (detected)
    {
        double m = midiOf (f0);
        if (! voicedState)
        {
            hist = { m, m, m };
        }
        else
        {
            hist[2] = hist[1]; hist[1] = hist[0]; hist[0] = m;
        }
        if (! voicedState) hist = { m, m, m };
        const double a = hist[0], b = hist[1], c = hist[2];
        const double mf = std::max (std::min (a, b), std::min (std::max (a, b), c)); // median of 3
        period = clampd (sampleRate / hzOf (mf), sampleRate / 2000.0, sampleRate / 25.0);

        const bool newNote = ! voicedState && gapSec > 0.05;
        if (newNote)
        {
            center = mf; noteAge = 0.0; vibAgeMs = 0.0; lastTargetNote = -1;
        }
        unvoicedHops = 0;
        gapSec = 0.0;
        voicedState = true;
        lastInputMidi = mf;

        // slow "note centre" tracker: separates vibrato from the note itself
        if (std::fabs (mf - center) > 0.9) center = mf;
        else center += (mf - center) * (1.0 - std::exp (-hopSec / 0.12));
        const double vib = clampd (mf - center, -1.0, 1.0);

        const double selectionPitch = p.ignoreVibrato ? center : mf;
        const int prevTarget = lastTargetNote;
        const double tgt = chooseTarget (selectionPitch);

        if (std::isnan (tgt))
        {
            haveTarget = false;
            targetShift = 0.0;
            tm.hasTarget = false;
        }
        else
        {
            haveTarget = true;
            if (lastTargetNote != prevTarget && prevTarget >= 0) { noteAge = 0.0; vibAgeMs = 0.0; }
            const double dCenter = center - tgt;

            // Flex-Tune: leave small deviations alone, pull larger ones fully.
            const double zone = p.flexTune * 0.5; // semitones
            double g = 1.0;
            if (zone > 1e-4)
            {
                const double aDev = std::fabs (dCenter);
                g = smoothstep ((aDev - zone * 0.5) / (zone * 0.5));
            }
            const double desired = tgt + dCenter * (1.0 - g) + vib * p.naturalVibrato;
            targetShift = desired - mf;
            tm.targetMidi = (float) tgt;
            tm.hasTarget = true;
        }
        noteAge += hopSec;

        // retune speed + humanize (slower on held notes)
        double tauMs = p.retuneMs + p.humanize * std::min (noteAge, 2.0) * 150.0;
        const double alpha = tauMs < 0.5 ? 1.0 : 1.0 - std::exp (-hopSec * 1000.0 / tauMs);
        shiftState += (targetShift - shiftState) * alpha;

        tm.inputMidi = (float) mf;
        tm.outputMidi = (float) (mf + shiftState + p.transposeSemis);
        tm.voiced = true;
    }
    else
    {
        ++unvoicedHops;
        if (unvoicedHops >= 4)
        {
            voicedState = false;
            gapSec += hopSec;
            if (gapSec > 0.06) shiftState += (0.0 - shiftState) * (1.0 - std::exp (-hopSec / 0.06));
            tm.voiced = false;
            tm.hasTarget = false;
        }
    }
}

void PitchEngine::tickVibrato (double& semis, double& formSemis, double& ampGain)
{
    if (p.vibShape == VibratoShape::Off && vibEnv < 1e-4 && std::fabs (lfoSm) < 1e-4)
    {
        semis = 0; formSemis = 0; ampGain = 1.0;
        return;
    }
    const double dtMs = 1000.0 / sampleRate;
    if (voicedState) vibAgeMs += dtMs;

    double envTarget = 0.0;
    if (p.vibShape != VibratoShape::Off && voicedState)
        envTarget = smoothstep ((vibAgeMs - p.vibOnsetDelayMs) / std::max (1.0f, p.vibOnsetRateMs));
    vibEnv += (envTarget - vibEnv) * (1.0 - std::exp (-1.0 / (0.01 * sampleRate)));

    vibPhase += (double) p.vibRate * rateMul / sampleRate;
    if (vibPhase >= 1.0)
    {
        vibPhase -= std::floor (vibPhase);
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        const double r1 = ((rng & 0xFFFF) / 32768.0) - 1.0;
        const double r2 = (((rng >> 16) & 0xFFFF) / 32768.0) - 1.0;
        rateMul = 1.0 + p.vibVariation * 0.3 * r1;
        depthMul = 1.0 + p.vibVariation * 0.5 * r2;
    }

    double lfo = 0.0;
    switch (p.vibShape)
    {
        case VibratoShape::Sine:     lfo = std::sin (2.0 * kPi * vibPhase); break;
        case VibratoShape::Triangle: lfo = vibPhase < 0.25 ? 4.0 * vibPhase : (vibPhase < 0.75 ? 2.0 - 4.0 * vibPhase : 4.0 * vibPhase - 4.0); break;
        case VibratoShape::Square:   lfo = vibPhase < 0.5 ? 1.0 : -1.0; break;
        case VibratoShape::Off:      lfo = 0.0; break;
    }
    lfoSm += (lfo - lfoSm) * (1.0 - std::exp (-1.0 / (0.004 * sampleRate)));

    const double v = lfoSm * vibEnv;
    semis = v * depthMul * ((double) p.vibPitchCents / 100.0);
    formSemis = v * (double) p.vibFormant;
    ampGain = 1.0 + 0.4 * (double) p.vibAmplitude * v;
}

void PitchEngine::spawnGrain (double gs)
{
    const double P = period;
    double Ps = std::max (8.0, P / ratio);

    double c0 = gs + Ps - latency;
    double c = c0;
    if (haveLast)
    {
        double m = std::round ((c0 - lastC) / P);
        if (m < 0) m = 0;
        c = lastC + m * P;

        // waveform-similarity refinement so the P-spaced lattice does not drift from real pitch pulses
        if (m >= 1.0)
        {
            const int64_t mask = kRing - 1;
            const float* buf = ring[0].data();
            const int64_t refPos = (int64_t) std::llround (lastC);
            const int64_t candPos = (int64_t) std::llround (c);
            const int half = std::max (4, (int) (P * 0.5));
            const int span = std::max (2, (int) (P * 0.25));
            double bestScore = -1e30;
            int bestD = 0;
            for (int d = -span; d <= span; ++d)
            {
                double num = 0, den = 1e-9;
                for (int k = -half; k <= half; k += 2)
                {
                    const float a = buf[(refPos + k) & mask];
                    const float b = buf[(candPos + d + k) & mask];
                    num += (double) a * b;
                    den += (double) b * b;
                }
                const double score = num / std::sqrt (den);
                if (score > bestScore) { bestScore = score; bestD = d; }
            }
            c += bestD;
        }
    }

    const double cmax = gs + std::min (Ps * phi, 2.0 * Ps - Ps * phi) - 1.0;
    if (c > cmax) c -= std::ceil ((c - cmax) / P) * P;
    const double cmin = gs - (double) kRing + Ps * phi + 64.0;
    if (c < cmin) c = cmin;

    lastC = c;
    haveLast = true;

    for (auto& g : grains)
    {
        if (! g.active)
        {
            g.active = true; g.start = gs; g.Ps = Ps; g.c = c; g.phi = phi;
            break;
        }
    }
    nextGrain = gs + Ps;
}

void PitchEngine::process (float* const* data, int numSamples)
{
    const int64_t mask = kRing - 1;
    const int64_t decMask = kDecRing - 1;

    for (int i = 0; i < numSamples; ++i)
    {
        const int64_t n = writeIdx;
        float mono = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float v = data[ch][i];
            ring[(size_t) ch][(size_t) (n & mask)] = v;
            mono += v;
        }
        mono /= (float) channels;

        decAcc += mono;
        if (++decCnt >= decim)
        {
            decRing[(size_t) (decPos & decMask)] = decAcc / (float) decim;
            ++decPos; decAcc = 0; decCnt = 0;
        }
        if (++hopCnt >= hop) { hopCnt = 0; analyze(); }

        // smooth shift + vibrato
        shiftSm += ((shiftState + (double) p.transposeSemis) - shiftSm) * smCoef;
        double vibSemis = 0, vibForm = 0, ampGain = 1.0;
        tickVibrato (vibSemis, vibForm, ampGain);
        ratio = clampd (std::exp2 ((shiftSm + vibSemis) / 12.0), 0.35, 2.8);
        if (p.formantCorrection)
            phi = clampd (std::exp2 (vibForm / 12.0) / (double) p.throatLength, 0.5, 2.0);
        else
            phi = clampd (ratio, 0.5, 2.0);

        // grain scheduling
        int guard = 0;
        if ((double) n - nextGrain > 4096.0) nextGrain = (double) n;
        while ((double) n >= nextGrain && guard++ < 4) spawnGrain (nextGrain);

        float wet[kMaxChannels] = { 0, 0 };
        const double nd = (double) n;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const double x = (nd - g.start) / (2.0 * g.Ps);
            if (x >= 1.0) { g.active = false; continue; }
            if (x < 0.0) continue;
            const float w = 0.5f - 0.5f * (float) std::cos (2.0 * kPi * x);
            const double pos = g.c + (nd - g.start - g.Ps) * g.phi;
            for (int ch = 0; ch < channels; ++ch)
                wet[ch] += w * hermite (ring[(size_t) ch].data(), mask, pos);
        }

        bypassMix += ((p.bypass ? 1.0 : 0.0) - bypassMix) * bypassCoef;
        const float bm = (float) bypassMix;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float dry = ring[(size_t) ch][(size_t) ((n - latency) & mask)];
            data[ch][i] = wet[ch] * (float) ampGain * (1.0f - bm) + dry * bm;
        }
        ++writeIdx;
    }
}
} // namespace tunec
