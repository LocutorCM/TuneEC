#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <map>
#include <memory>

#include "PluginProcessor.h"

namespace tunecui
{
namespace col
{
    const juce::Colour bg        { 0xFF0A0D13 };
    const juce::Colour bgTop     { 0xFF121826 };
    const juce::Colour panel     { 0xFF121722 };
    const juce::Colour panelEdge { 0xFF202A3B };
    const juce::Colour text      { 0xFFE8ECF4 };
    const juce::Colour textDim   { 0xFF8591A6 };
    const juce::Colour teal      { 0xFF2DE2C4 };
    const juce::Colour violet    { 0xFF8B7CFF };
    const juce::Colour amber     { 0xFFFFB454 };
    const juce::Colour pink      { 0xFFFF5C9A };
} // namespace col

class TunECLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TunECLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool isDown, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (13.0f); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// Scrolling pitch monitor: detected pitch, corrected pitch, scale grid, big note read-out
class PitchScope : public juce::Component, private juce::Timer
{
public:
    explicit PitchScope (TunECAudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    static juce::String noteName (float midi, bool withOctave = true);

    TunECAudioProcessor& proc;
    static constexpr int N = 260;
    std::array<float, N> inTrace {}, outTrace {}, tgtTrace {};
    int head = 0;
    float viewCentre = 60.f;
    float curIn = 0.f, curTarget = 0.f;
    bool curVoiced = false, curHasTarget = false;
    int activeMask = 0xFFF;
};
} // namespace tunecui

class TunECAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit TunECAudioProcessorEditor (TunECAudioProcessor&);
    ~TunECAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    struct Combo
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attach;
    };
    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attach;
    };

    Knob& addKnob (const char* paramId, const juce::String& title);
    Combo& addCombo (const char* paramId, const juce::String& title);
    Toggle& addToggle (const char* paramId, const juce::String& title);
    void placeKnob (Knob&, juce::Rectangle<int>);
    void placeCombo (Combo&, juce::Rectangle<int>);

    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    TunECAudioProcessor& proc;
    tunecui::TunECLookAndFeel laf;
    tunecui::PitchScope scope;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Combo>> combos;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::map<juce::String, Knob*> knobById;
    std::map<juce::String, Combo*> comboById;
    std::map<juce::String, Toggle*> toggleById;

    juce::Rectangle<int> pTuning, pFormant, pMidi, pVibrato;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunECAudioProcessorEditor)
};
