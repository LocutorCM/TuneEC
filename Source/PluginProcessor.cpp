#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

using namespace juce;
namespace ids = tunec::ids;

namespace
{
std::unique_ptr<AudioParameterFloat> makeFloat (const char* id, const char* name, float lo, float hi, float def,
                                                float skew, const char* unit, int decimals)
{
    const String u (unit);
    return std::make_unique<AudioParameterFloat> (
        ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.0f, skew), def,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([u, decimals] (float v, int) { return String (v, decimals) + (u.isEmpty() ? "" : " " + u); })
            .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); }));
}
std::unique_ptr<AudioParameterBool> makeBool (const char* id, const char* name, bool def)
{
    return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def);
}
std::unique_ptr<AudioParameterChoice> makeChoice (const char* id, const char* name, const StringArray& c, int def)
{
    return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def);
}
} // namespace

AudioProcessorValueTreeState::ParameterLayout TunECAudioProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;

    StringArray inputs { "Soprano", "Alto/Tenor", "Low Male", "Instrument", "Bass Instrument" };
    StringArray keys, scales;
    for (auto* k : tunec::kKeyNames) keys.add (k);
    for (auto* s : tunec::kScaleNames) scales.add (s);

    l.add (makeFloat (ids::retuneSpeed, "Retune Speed", 0.f, 400.f, 20.f, 0.45f, "ms", 0));
    l.add (makeFloat (ids::flexTune, "Flex Tune", 0.f, 100.f, 0.f, 1.f, "%", 0));
    l.add (makeFloat (ids::naturalVibrato, "Natural Vibrato", 0.f, 200.f, 0.f, 1.f, "%", 0));
    l.add (makeFloat (ids::humanize, "Humanize", 0.f, 100.f, 0.f, 1.f, "%", 0));
    l.add (makeChoice (ids::inputType, "Input Type", inputs, 1));
    l.add (makeChoice (ids::key, "Key", keys, 0));
    l.add (makeChoice (ids::scale, "Scale", scales, 0));
    l.add (makeBool (ids::masterBypass, "Master Bypass", false));
    l.add (makeBool (ids::ignoreVibrato, "Ignore Vibrato", true));

    l.add (makeBool (ids::formantCorrection, "Formant Correction", true));
    l.add (makeFloat (ids::throatLength, "Throat Length", 50.f, 150.f, 100.f, 1.f, "%", 0));
    l.add (makeFloat (ids::transpose, "Transpose", -24.f, 24.f, 0.f, 1.f, "st", 2));
    l.add (makeFloat (ids::detune, "Detune", -100.f, 100.f, 0.f, 1.f, "cents", 1));

    l.add (makeBool (ids::targetViaMidi, "Target Notes via MIDI", false));
    l.add (makeBool (ids::learnScale, "Learn Scale from MIDI", false));
    l.add (makeChoice (ids::midiOctaves, "MIDI Octaves", { "All", "Match" }, 0));

    l.add (makeChoice (ids::vibShape, "Vibrato Shape", { "Off", "Sine", "Triangle", "Square" }, 0));
    l.add (makeFloat (ids::vibRate, "Vibrato Rate", 0.1f, 10.f, 5.5f, 0.6f, "Hz", 2));
    l.add (makeFloat (ids::vibOnsetDelay, "Vibrato Onset Delay", 0.f, 2000.f, 500.f, 0.6f, "ms", 0));
    l.add (makeFloat (ids::vibOnsetRate, "Vibrato Onset Rate", 0.f, 2000.f, 500.f, 0.6f, "ms", 0));
    l.add (makeFloat (ids::vibVariation, "Vibrato Variation", 0.f, 100.f, 0.f, 1.f, "%", 0));
    l.add (makeFloat (ids::vibPitch, "Vibrato Pitch", 0.f, 100.f, 25.f, 1.f, "cents", 0));
    l.add (makeFloat (ids::vibAmplitude, "Vibrato Amplitude", 0.f, 100.f, 0.f, 1.f, "%", 0));
    l.add (makeFloat (ids::vibFormant, "Vibrato Formant", 0.f, 100.f, 0.f, 1.f, "%", 0));
    return l;
}

TunECAudioProcessor::TunECAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TunEC", createLayout())
{
}

bool TunECAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

tunec::EngineParams TunECAudioProcessor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    tunec::EngineParams p;
    p.retuneMs = v (ids::retuneSpeed);
    p.flexTune = v (ids::flexTune) * 0.01f;
    p.naturalVibrato = v (ids::naturalVibrato) * 0.01f;
    p.humanize = v (ids::humanize) * 0.01f;
    p.inputType = (tunec::InputType) (int) v (ids::inputType);
    p.key = (int) v (ids::key);
    p.scale = (int) v (ids::scale);
    p.bypass = v (ids::masterBypass) > 0.5f;
    p.ignoreVibrato = v (ids::ignoreVibrato) > 0.5f;
    p.formantCorrection = v (ids::formantCorrection) > 0.5f;
    p.throatLength = v (ids::throatLength) * 0.01f;
    p.transposeSemis = v (ids::transpose);
    p.detuneCents = v (ids::detune);
    p.useMidiTargets = v (ids::targetViaMidi) > 0.5f;
    p.midiOctavesMatch = (int) v (ids::midiOctaves) == 1;
    p.vibShape = (tunec::VibratoShape) (int) v (ids::vibShape);
    p.vibRate = v (ids::vibRate);
    p.vibOnsetDelayMs = v (ids::vibOnsetDelay);
    p.vibOnsetRateMs = v (ids::vibOnsetRate);
    p.vibVariation = v (ids::vibVariation) * 0.01f;
    p.vibPitchCents = v (ids::vibPitch);
    p.vibAmplitude = v (ids::vibAmplitude) * 0.01f;
    p.vibFormant = v (ids::vibFormant) * 0.01f;
    return p;
}

void TunECAudioProcessor::prepareToPlay (double sr, int)
{
    engine.setParams (readParams());
    engine.prepare (sr, jmax (1, getTotalNumInputChannels()));
    engine.setParams (readParams());
    lastLatency = engine.getLatencySamples();
    setLatencySamples (lastLatency);
}

void TunECAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numCh = jmin (buffer.getNumChannels(), (int) tunec::PitchEngine::kMaxChannels);
    const int ns = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, ns);

    const int pending = pendingCustomMask.exchange (-1);
    if (pending >= 0) engine.setCustomMask (pending);

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn()) engine.noteOn (msg.getNoteNumber());
        else if (msg.isNoteOff()) engine.noteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allNotesOff();
    }

    engine.setParams (readParams());
    engine.setLearning (apvts.getRawParameterValue (ids::learnScale)->load() > 0.5f);

    if (numCh > 0)
        engine.process (buffer.getArrayOfWritePointers(), ns);

    if (engine.getLatencySamples() != lastLatency)
    {
        lastLatency = engine.getLatencySamples();
        setLatencySamples (lastLatency);
    }

    const auto tm = engine.getTelemetry();
    tmInput.store (tm.inputMidi);
    tmTarget.store (tm.targetMidi);
    tmOutput.store (tm.outputMidi);
    tmLevel.store (tm.level);
    tmVoiced.store (tm.voiced);
    tmHasTarget.store (tm.hasTarget);
    tmActiveMask.store (engine.getActiveMask());
}

AudioProcessorEditor* TunECAudioProcessor::createEditor() { return new TunECAudioProcessorEditor (*this); }

void TunECAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("customMask", engine.getCustomMask(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TunECAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = ValueTree::fromXml (*xml);
            pendingCustomMask.store ((int) tree.getProperty ("customMask", 0));
            apvts.replaceState (tree);
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TunECAudioProcessor(); }
