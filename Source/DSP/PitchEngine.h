// TunEC - original pitch correction engine (no third-party code).
// Pure C++17, no JUCE dependency, so it can be unit-tested stand-alone.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace tunec
{
enum class InputType : int { Soprano = 0, AltoTenor, LowMale, Instrument, BassInstrument };
enum class VibratoShape : int { Off = 0, Sine, Triangle, Square };

constexpr int bitsOf (std::initializer_list<int> l)
{
    int m = 0;
    for (int v : l) m |= (1 << v);
    return m;
}

inline constexpr int kNumScales = 15;
inline constexpr const char* kScaleNames[kNumScales] = {
    "Chromatic", "Major", "Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian",
    "Lydian", "Mixolydian", "Locrian", "Pentatonic Major", "Pentatonic Minor", "Blues",
    "Whole Tone", "Custom (MIDI)"
};
inline constexpr int kCustomScaleIndex = kNumScales - 1;
inline constexpr std::array<int, kNumScales> kScaleMasks = {
    0xFFF,
    bitsOf ({ 0, 2, 4, 5, 7, 9, 11 }),
    bitsOf ({ 0, 2, 3, 5, 7, 8, 10 }),
    bitsOf ({ 0, 2, 3, 5, 7, 8, 11 }),
    bitsOf ({ 0, 2, 3, 5, 7, 9, 11 }),
    bitsOf ({ 0, 2, 3, 5, 7, 9, 10 }),
    bitsOf ({ 0, 1, 3, 5, 7, 8, 10 }),
    bitsOf ({ 0, 2, 4, 6, 7, 9, 11 }),
    bitsOf ({ 0, 2, 4, 5, 7, 9, 10 }),
    bitsOf ({ 0, 1, 3, 5, 6, 8, 10 }),
    bitsOf ({ 0, 2, 4, 7, 9 }),
    bitsOf ({ 0, 3, 5, 7, 10 }),
    bitsOf ({ 0, 3, 5, 6, 7, 10 }),
    bitsOf ({ 0, 2, 4, 6, 8, 10 }),
    0xFFF
};
inline constexpr const char* kKeyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

struct EngineParams
{
    // Correction
    float retuneMs = 20.0f;       // 0..400 ms (0 = instant)
    float flexTune = 0.0f;        // 0..1
    float naturalVibrato = 0.0f;  // 0..2 (1 = keep the singer's vibrato as is)
    float humanize = 0.0f;        // 0..1
    InputType inputType = InputType::AltoTenor;
    int key = 0;                  // 0..11
    int scale = 0;                // index into kScaleNames
    bool bypass = false;
    bool ignoreVibrato = true;

    // Formant / tuning
    bool formantCorrection = true;
    float throatLength = 1.0f;    // 0.5..1.5
    float transposeSemis = 0.0f;  // -24..24
    float detuneCents = 0.0f;     // -100..100

    // MIDI
    bool useMidiTargets = false;
    bool midiOctavesMatch = false;

    // Vibrato generator
    VibratoShape vibShape = VibratoShape::Off;
    float vibRate = 5.5f;         // Hz
    float vibOnsetDelayMs = 500.0f;
    float vibOnsetRateMs = 500.0f;
    float vibVariation = 0.0f;    // 0..1
    float vibPitchCents = 25.0f;  // depth
    float vibAmplitude = 0.0f;    // 0..1
    float vibFormant = 0.0f;      // 0..1
};

struct Telemetry
{
    float inputMidi = 0.0f;
    float targetMidi = 0.0f;
    float outputMidi = 0.0f;
    float level = 0.0f;
    bool voiced = false;
    bool hasTarget = false;
};

class PitchEngine
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setParams (const EngineParams& p);

    void noteOn (int note);
    void noteOff (int note);
    void allNotesOff();
    void setLearning (bool shouldLearn);
    void setCustomMask (int mask) { customMask = mask & 0xFFF; dirtyAllowed = true; }
    int getCustomMask() const { return customMask; }

    int getLatencySamples() const { return latency; }
    int getActiveMask() const { return activeMask; }
    Telemetry getTelemetry() const { return tm; }

    // In-place processing. data[ch][i]
    void process (float* const* data, int numSamples);

private:
    struct Grain
    {
        bool active = false;
        double start = 0, Ps = 1, c = 0, phi = 1;
    };

    void configureForInput();
    void analyze();
    bool detectPitch (double& f0, float& rms);
    double chooseTarget (double midi);
    bool allowedNote (int n) const { return allowed[(size_t) n]; }
    void rebuildAllowed();
    void spawnGrain (double gs);
    void tickVibrato (double& semis, double& formSemis, double& ampGain);

    static float hermite (const float* b, int64_t mask, double pos);

    EngineParams p;
    double sampleRate = 48000.0;
    int channels = 1;

    // buffers
    static constexpr int kRing = 1 << 16;
    static constexpr int kDecRing = 1 << 12;
    std::array<std::vector<float>, kMaxChannels> ring;
    std::vector<float> decRing, winBuf, yinCmnd;
    int64_t writeIdx = 0, decPos = 0;
    int decim = 4, decCnt = 0, hop = 144, hopCnt = 0;
    float decAcc = 0;
    double srDec = 12000.0;
    int maxTau = 500, tauMin = 8, tauMax = 200;
    double fMin = 80.0, fMax = 800.0;
    int latency = 0;

    // analysis state
    std::array<double, 3> hist { 0, 0, 0 };
    bool voicedState = false;
    int unvoicedHops = 99;
    double gapSec = 10.0, noteAge = 0.0, hopSec = 0.003;
    double center = 60.0, period = 200.0, shiftState = 0.0, targetShift = 0.0;
    int lastTargetNote = -1;
    bool haveTarget = false;
    double lastInputMidi = 60.0;

    // note tables
    std::array<bool, 128> allowed {};
    std::array<bool, 128> held {};
    int activeMask = 0xFFF;
    int customMask = 0;
    bool learning = false, learnFresh = false, dirtyAllowed = true;

    // synthesis state
    std::array<Grain, 6> grains;
    double nextGrain = 0, lastC = 0;
    bool haveLast = false;
    double ratio = 1.0, phi = 1.0, shiftSm = 0.0;
    double smCoef = 0.01, bypassMix = 0.0, bypassCoef = 0.01;

    // vibrato generator
    double vibPhase = 0, vibEnv = 0, vibAgeMs = 0, lfoSm = 0, rateMul = 1, depthMul = 1;
    uint32_t rng = 12345u;

    Telemetry tm;
};
} // namespace tunec
