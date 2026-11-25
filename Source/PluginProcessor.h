#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

#include "DSP/PitchEngine.h"

class TunECAudioProcessor : public juce::AudioProcessor
{
public:
    TunECAudioProcessor();
    ~TunECAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TunEC"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Telemetry for the GUI (written on the audio thread, read on the message thread)
    std::atomic<float> tmInput { 0.f }, tmTarget { 0.f }, tmOutput { 0.f }, tmLevel { 0.f };
    std::atomic<bool> tmVoiced { false }, tmHasTarget { false };
    std::atomic<int> tmActiveMask { 0xFFF };

private:
    tunec::EngineParams readParams() const;

    tunec::PitchEngine engine;
    std::atomic<int> pendingCustomMask { -1 };
    int lastLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunECAudioProcessor)
};
