#include "PluginEditor.h"
#include "Parameters.h"

using namespace juce;
namespace ids = tunec::ids;
using namespace tunecui;

//==============================================================================
// Look & feel
//==============================================================================
TunECLookAndFeel::TunECLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, col::bg);
    setColour (Slider::textBoxTextColourId, col::text);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, col::violet.withAlpha (0.5f));
    setColour (Label::textColourId, col::textDim);
    setColour (ComboBox::backgroundColourId, Colour (0xFF0D121B));
    setColour (ComboBox::outlineColourId, col::panelEdge);
    setColour (ComboBox::textColourId, col::text);
    setColour (ComboBox::arrowColourId, col::teal);
    setColour (PopupMenu::backgroundColourId, Colour (0xFF0F1520));
    setColour (PopupMenu::textColourId, col::text);
    setColour (PopupMenu::highlightedBackgroundColourId, col::violet.withAlpha (0.55f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TextEditor::backgroundColourId, Colour (0xFF0D121B));
    setColour (TextEditor::textColourId, col::text);
}

Label* TunECLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (FontOptions (12.0f));
    l->setJustificationType (Justification::centred);
    return l;
}

void TunECLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float startA, float endA, Slider& s)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float radius = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float lw = jmax (3.0f, radius * 0.13f);
    const float arcR = radius - lw * 0.5f;
    const float ang = startA + pos * (endA - startA);
    const bool enabled = s.isEnabled();

    // glow behind value arc
    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startA, endA, true);
    g.setColour (Colour (0xFF1B2333));
    g.strokePath (track, PathStrokeType (lw, PathStrokeType::curved, PathStrokeType::rounded));

    Path val;
    val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startA, ang, true);
    g.setGradientFill (ColourGradient (col::teal, c.x - radius, c.y, col::violet, c.x + radius, c.y, false));
    g.setOpacity (enabled ? 1.0f : 0.4f);
    g.strokePath (val, PathStrokeType (lw, PathStrokeType::curved, PathStrokeType::rounded));

    // body
    const float bodyR = radius - lw * 1.9f;
    g.setGradientFill (ColourGradient (Colour (0xFF2B3548), c.x, c.y - bodyR, Colour (0xFF121722), c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
    g.setColour (Colour (0xFF3A465E));
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);

    // pointer
    Path p;
    p.addRoundedRectangle (-1.5f, -bodyR + 3.0f, 3.0f, bodyR * 0.45f, 1.5f);
    g.setColour (Colours::white);
    g.fillPath (p, AffineTransform::rotation (ang).translated (c.x, c.y));
}

void TunECLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setGradientFill (ColourGradient (col::teal.withAlpha (0.95f), r.getX(), r.getY(), col::violet.withAlpha (0.9f), r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.0f);
    }
    else
    {
        g.setColour (Colour (0xFF0D121B));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (highlighted ? col::teal.withAlpha (0.7f) : col::panelEdge);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }
    g.setColour (on ? Colour (0xFF06090E) : col::text);
    g.setFont (FontOptions (12.5f, Font::bold));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), Justification::centred, 1);
}

void TunECLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (Colour (0xFF0D121B));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.hasKeyboardFocus (true) ? col::teal : col::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (col::teal);
    g.fillPath (arrow);
}

//==============================================================================
// Pitch scope
//==============================================================================
PitchScope::PitchScope (TunECAudioProcessor& p) : proc (p)
{
    inTrace.fill (NAN);
    outTrace.fill (NAN);
    tgtTrace.fill (NAN);
    startTimerHz (30);
}

String PitchScope::noteName (float midi, bool withOctave)
{
    const int n = (int) std::lround (midi);
    String s (tunec::kKeyNames[((n % 12) + 12) % 12]);
    if (withOctave) s << (n / 12 - 1);
    return s;
}

void PitchScope::timerCallback()
{
    curVoiced = proc.tmVoiced.load();
    curHasTarget = proc.tmHasTarget.load();
    activeMask = proc.tmActiveMask.load();
    head = (head + 1) % N;
    if (curVoiced)
    {
        curIn = proc.tmInput.load();
        curTarget = proc.tmTarget.load();
        inTrace[(size_t) head] = curIn;
        outTrace[(size_t) head] = proc.tmOutput.load();
        tgtTrace[(size_t) head] = curHasTarget ? curTarget : NAN;
        viewCentre += (curIn - viewCentre) * 0.06f;
    }
    else
    {
        inTrace[(size_t) head] = NAN;
        outTrace[(size_t) head] = NAN;
        tgtTrace[(size_t) head] = NAN;
    }
    repaint();
}

void PitchScope::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (Colour (0xFF0B0F17));
    g.fillRoundedRectangle (area, 8.0f);

    auto graph = area.reduced (10.0f).withTrimmedRight (236.0f);
    auto info = area.reduced (10.0f).removeFromRight (222.0f);

    // grid
    const float range = 8.0f;
    const float lo = viewCentre - range, hi = viewCentre + range;
    auto yOf = [&] (float m) { return graph.getBottom() - (m - lo) / (hi - lo) * graph.getHeight(); };

    g.saveState();
    g.reduceClipRegion (graph.toNearestInt());
    for (int m = (int) std::floor (lo); m <= (int) std::ceil (hi); ++m)
    {
        const bool inScale = (activeMask >> (((m % 12) + 12) % 12)) & 1;
        const float y = yOf ((float) m);
        if (inScale)
        {
            g.setColour (col::violet.withAlpha (0.10f));
            g.fillRect (graph.getX(), y - 5.0f, graph.getWidth(), 10.0f);
            g.setColour (col::violet.withAlpha (0.45f));
        }
        else
            g.setColour (Colour (0x14FFFFFF));
        g.drawHorizontalLine ((int) y, graph.getX(), graph.getRight());
        if (inScale)
        {
            g.setColour (col::textDim);
            g.setFont (FontOptions (10.5f));
            g.drawText (noteName ((float) m), (int) graph.getX() + 4, (int) y - 12, 40, 12, Justification::left);
        }
    }

    auto drawTrace = [&] (const std::array<float, N>& t, Colour c, float thick)
    {
        Path p;
        bool pen = false;
        for (int i = 0; i < N; ++i)
        {
            const float v = t[(size_t) ((head + 1 + i) % N)];
            if (std::isnan (v)) { pen = false; continue; }
            const float x = graph.getX() + graph.getWidth() * (float) i / (float) (N - 1);
            const float y = yOf (v);
            if (! pen) { p.startNewSubPath (x, y); pen = true; }
            else p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));
    };
    drawTrace (inTrace, col::pink.withAlpha (0.85f), 1.6f);
    drawTrace (outTrace, col::teal, 2.4f);
    g.restoreState();

    // legend
    g.setFont (FontOptions (10.5f));
    g.setColour (col::pink);
    g.fillRect (graph.getRight() - 120.0f, graph.getY() + 8.0f, 10.0f, 3.0f);
    g.setColour (col::textDim);
    g.drawText ("INPUT", (int) graph.getRight() - 106, (int) graph.getY() + 3, 40, 12, Justification::left);
    g.setColour (col::teal);
    g.fillRect (graph.getRight() - 62.0f, graph.getY() + 8.0f, 10.0f, 3.0f);
    g.setColour (col::textDim);
    g.drawText ("OUTPUT", (int) graph.getRight() - 48, (int) graph.getY() + 3, 48, 12, Justification::left);

    // read-out
    auto top = info.removeFromTop (92.0f);
    g.setColour (curVoiced ? col::text : col::textDim.withAlpha (0.6f));
    g.setFont (FontOptions (58.0f, Font::bold));
    g.drawText (curVoiced ? noteName (curIn) : "--", top.removeFromLeft (130.0f).toNearestInt(), Justification::centredLeft);

    if (curVoiced)
    {
        const float ref = curHasTarget ? curTarget : std::round (curIn);
        const float cents = (curIn - ref) * 100.0f;
        g.setFont (FontOptions (15.0f, Font::bold));
        g.setColour (std::abs (cents) < 8.0f ? col::teal : (std::abs (cents) < 25.0f ? col::amber : col::pink));
        g.drawText (String (cents >= 0 ? "+" : "") + String ((int) std::lround (cents)) + " ct", top.toNearestInt().withTrimmedTop (18), Justification::topLeft);
        g.setColour (col::textDim);
        g.setFont (FontOptions (11.0f));
        g.drawText (curHasTarget ? "TARGET " + noteName (curTarget) : "NO TARGET", top.toNearestInt().withTrimmedTop (42), Justification::topLeft);
    }
    else
    {
        g.setColour (col::textDim);
        g.setFont (FontOptions (12.0f));
        g.drawText ("Listening...", top.toNearestInt().withTrimmedTop (30), Justification::topLeft);
    }

    // 12-note scale strip
    auto strip = info.removeFromBottom (58.0f);
    const float cw = strip.getWidth() / 12.0f;
    const int curPc = curVoiced ? ((int) std::lround (curTarget) % 12 + 12) % 12 : -1;
    const int inPc = curVoiced ? ((int) std::lround (curIn) % 12 + 12) % 12 : -1;
    for (int i = 0; i < 12; ++i)
    {
        auto cell = Rectangle<float> (strip.getX() + cw * (float) i, strip.getY(), cw - 2.0f, strip.getHeight()).reduced (0, 2);
        const bool on = (activeMask >> i) & 1;
        const bool black = (i == 1 || i == 3 || i == 6 || i == 8 || i == 10);
        const bool isTarget = curHasTarget && i == curPc;
        Colour fill = on ? col::violet.withAlpha (black ? 0.55f : 0.75f) : Colour (black ? 0xFF161C28 : 0xFF1D2535);
        if (isTarget) fill = col::teal;
        g.setColour (fill);
        g.fillRoundedRectangle (cell, 3.0f);
        if (i == inPc)
        {
            g.setColour (col::pink);
            g.drawRoundedRectangle (cell, 3.0f, 1.5f);
        }
        g.setColour (on || isTarget ? Colour (0xFF06090E) : col::textDim.withAlpha (0.7f));
        g.setFont (FontOptions (9.0f, Font::bold));
        g.drawText (tunec::kKeyNames[i], cell.toNearestInt(), Justification::centred);
    }
}

//==============================================================================
// Editor
//==============================================================================
static constexpr int kW = 1000, kH = 664;

TunECAudioProcessorEditor::TunECAudioProcessorEditor (TunECAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), scope (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (scope);

    // Tuning panel
    addCombo (ids::inputType, "INPUT TYPE");
    addCombo (ids::key, "KEY");
    addCombo (ids::scale, "SCALE");
    addToggle (ids::ignoreVibrato, "Ignore Vibrato");
    addKnob (ids::retuneSpeed, "Retune Speed");
    addKnob (ids::flexTune, "Flex Tune");
    addKnob (ids::naturalVibrato, "Natural Vibrato");
    addKnob (ids::humanize, "Humanize");

    // Formant / pitch
    addToggle (ids::formantCorrection, "Formant Correction");
    addKnob (ids::throatLength, "Throat Length");
    addKnob (ids::transpose, "Transpose");
    addKnob (ids::detune, "Detune");

    // MIDI
    addToggle (ids::targetViaMidi, "Target Notes via MIDI");
    addToggle (ids::learnScale, "Learn Scale from MIDI");
    addCombo (ids::midiOctaves, "MIDI OCTAVES");

    // Vibrato
    addCombo (ids::vibShape, "VIBRATO SHAPE");
    addKnob (ids::vibRate, "Rate");
    addKnob (ids::vibOnsetDelay, "Onset Delay");
    addKnob (ids::vibOnsetRate, "Onset Rate");
    addKnob (ids::vibVariation, "Variation");
    addKnob (ids::vibPitch, "Pitch");
    addKnob (ids::vibAmplitude, "Amplitude");
    addKnob (ids::vibFormant, "Formant");

    // header bypass
    addToggle (ids::masterBypass, "Bypass");

    setSize (kW, kH);
}

TunECAudioProcessorEditor::~TunECAudioProcessorEditor() { setLookAndFeel (nullptr); }

TunECAudioProcessorEditor::Knob& TunECAudioProcessorEditor::addKnob (const char* id, const String& title)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (Slider::TextBoxBelow, false, 84, 16);
    k->slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    k->slider.setMouseDragSensitivity (180);
    k->slider.setScrollWheelEnabled (false);
    k->label.setText (title, dontSendNotification);
    k->label.setJustificationType (Justification::centred);
    k->label.setFont (FontOptions (11.5f, Font::bold));
    k->label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    auto& ref = *k;
    knobById[id] = &ref;
    knobs.push_back (std::move (k));
    return ref;
}

TunECAudioProcessorEditor::Combo& TunECAudioProcessorEditor::addCombo (const char* id, const String& title)
{
    auto c = std::make_unique<Combo>();
    if (auto* param = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->box.addItemList (param->choices, 1);
    c->label.setText (title, dontSendNotification);
    c->label.setFont (FontOptions (10.5f, Font::bold));
    c->label.setJustificationType (Justification::centredLeft);
    c->label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (c->box);
    addAndMakeVisible (c->label);
    c->attach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c->box);
    auto& ref = *c;
    comboById[id] = &ref;
    combos.push_back (std::move (c));
    return ref;
}

TunECAudioProcessorEditor::Toggle& TunECAudioProcessorEditor::addToggle (const char* id, const String& title)
{
    auto t = std::make_unique<Toggle>();
    t->button.setButtonText (title);
    t->button.setClickingTogglesState (true);
    addAndMakeVisible (t->button);
    t->attach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, t->button);
    auto& ref = *t;
    toggleById[id] = &ref;
    toggles.push_back (std::move (t));
    return ref;
}

void TunECAudioProcessorEditor::placeKnob (Knob& k, Rectangle<int> r)
{
    k.label.setBounds (r.removeFromTop (16));
    k.slider.setBounds (r);
}

void TunECAudioProcessorEditor::placeCombo (Combo& c, Rectangle<int> r)
{
    c.label.setBounds (r.removeFromTop (14));
    c.box.setBounds (r);
}

void TunECAudioProcessorEditor::drawPanel (Graphics& g, Rectangle<int> r, const String& title)
{
    auto f = r.toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (f, 10.0f);
    g.setColour (col::panelEdge);
    g.drawRoundedRectangle (f.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (col::teal);
    g.fillRoundedRectangle (f.getX() + 14.0f, f.getY() + 13.0f, 3.0f, 12.0f, 1.5f);
    g.setColour (col::text);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.drawText (title, r.getX() + 24, r.getY() + 10, 240, 18, Justification::centredLeft);
}

void TunECAudioProcessorEditor::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (col::bgTop, 0, 0, col::bg, 0, (float) kH, false));
    g.fillAll();

    // header
    g.setColour (col::text);
    g.setFont (FontOptions (30.0f, Font::bold));
    g.drawText ("Tun", 20, 12, 60, 40, Justification::centredLeft);
    g.setColour (col::teal);
    g.drawText ("EC", 20 + 55, 12, 60, 40, Justification::centredLeft);
    g.setColour (col::textDim);
    g.setFont (FontOptions (11.5f));
    g.drawText ("REAL-TIME PITCH CORRECTION", 128, 22, 260, 20, Justification::centredLeft);
    g.setColour (col::panelEdge);
    g.drawHorizontalLine (60, 16.0f, (float) kW - 16.0f);

    drawPanel (g, pTuning, "TUNING");
    drawPanel (g, pFormant, "FORMANT & PITCH");
    drawPanel (g, pMidi, "MIDI");
    drawPanel (g, pVibrato, "VIBRATO");
}

void TunECAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 0);
    toggleById[ids::masterBypass]->button.setBounds (area.getRight() - 110, 17, 110, 30);

    scope.setBounds (16, 70, kW - 32, 196);

    pTuning  = { 16, 276, 560, 180 };
    pFormant = { 588, 276, 396, 180 };
    pMidi    = { 16, 468, 296, 188 };
    pVibrato = { 324, 468, 660, 188 };

    // TUNING
    {
        auto r = pTuning.reduced (14, 0);
        r.removeFromTop (30);
        auto row = r.removeFromTop (40);
        const int cw = 132, gap = 8;
        placeCombo (*comboById[ids::inputType], row.removeFromLeft (cw)); row.removeFromLeft (gap);
        placeCombo (*comboById[ids::key], row.removeFromLeft (70)); row.removeFromLeft (gap);
        placeCombo (*comboById[ids::scale], row.removeFromLeft (150)); row.removeFromLeft (gap);
        toggleById[ids::ignoreVibrato]->button.setBounds (row.removeFromBottom (24));
        r.removeFromTop (6);
        const int kw = r.getWidth() / 4;
        for (auto id : { ids::retuneSpeed, ids::flexTune, ids::naturalVibrato, ids::humanize })
            placeKnob (*knobById[id], r.removeFromLeft (kw));
    }
    // FORMANT
    {
        auto r = pFormant.reduced (14, 0);
        r.removeFromTop (30);
        toggleById[ids::formantCorrection]->button.setBounds (r.removeFromTop (26).withWidth (170));
        r.removeFromTop (14);
        const int kw = r.getWidth() / 3;
        for (auto id : { ids::throatLength, ids::transpose, ids::detune })
            placeKnob (*knobById[id], r.removeFromLeft (kw));
    }
    // MIDI
    {
        auto r = pMidi.reduced (14, 0);
        r.removeFromTop (34);
        toggleById[ids::targetViaMidi]->button.setBounds (r.removeFromTop (30)); r.removeFromTop (10);
        toggleById[ids::learnScale]->button.setBounds (r.removeFromTop (30)); r.removeFromTop (14);
        placeCombo (*comboById[ids::midiOctaves], r.removeFromTop (44).withWidth (150));
    }
    // VIBRATO
    {
        auto r = pVibrato.reduced (14, 0);
        r.removeFromTop (30);
        placeCombo (*comboById[ids::vibShape], r.removeFromTop (40).withWidth (170));
        r.removeFromTop (6);
        const int kw = r.getWidth() / 7;
        for (auto id : { ids::vibRate, ids::vibOnsetDelay, ids::vibOnsetRate, ids::vibVariation,
                         ids::vibPitch, ids::vibAmplitude, ids::vibFormant })
            placeKnob (*knobById[id], r.removeFromLeft (kw));
    }
}
