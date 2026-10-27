#include "PluginEditor.h"

namespace
{
    const juce::Colour bg     { 0xff0e0a14 };
    const juce::Colour panel  { 0xff1a1424 };
    const juce::Colour ink    { 0xffefe6ff };
    const juce::Colour dimInk { 0xff8f84a3 };

    juce::Colour psyche (float t, float hueShift, float alpha = 1.0f)
    {
        return juce::Colour::fromHSV (std::fmod (0.78f + t * 0.55f + hueShift, 1.0f), 0.85f, 1.0f, alpha);
    }
}

// ------------------------------------------------------------------ look and feel
MeltLookAndFeel::MeltLookAndFeel()
{
    setColour (juce::Label::textColourId, ink);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::outlineColourId, dimInk.withAlpha (0.4f));
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::arrowColourId, ink);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff7b3fe4));
    setColour (juce::Slider::textBoxTextColourId, dimInk);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void MeltLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float a0, float a1, juce::Slider&)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const auto c = r.getCentre();
    const float thick = juce::jmax (3.0f, radius * 0.14f);
    const float angle = a0 + pos * (a1 - a0);

    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - thick, radius - thick, 0, a0, a1, true);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc drawn in short segments so its colour sweeps through the spectrum
    const int segs = 48;
    for (int i = 0; i < segs; ++i)
    {
        const float t0 = (float) i / segs, t1 = (float) (i + 1) / segs;
        if (t0 > pos) break;
        juce::Path seg;
        seg.addCentredArc (c.x, c.y, radius - thick, radius - thick, 0,
                           a0 + t0 * (a1 - a0), a0 + juce::jmin (t1, pos) * (a1 - a0), true);
        g.setColour (psyche (t0, hueShift));
        g.strokePath (seg, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    const float inner = radius - thick * 2.2f;
    g.setColour (panel);
    g.fillEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2);
    juce::Point<float> tip (c.x + std::sin (angle) * inner * 0.82f, c.y - std::cos (angle) * inner * 0.82f);
    g.setColour (ink);
    g.drawLine ({ c, tip }, juce::jmax (2.0f, radius * 0.06f));
}

void MeltLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    if (on)
    {
        juce::ColourGradient grad (psyche (0.0f, hueShift), r.getX(), r.getY(),
                                   psyche (0.6f, hueShift), r.getRight(), r.getBottom(), false);
        g.setGradientFill (grad);
    }
    else
        g.setColour (highlighted ? panel.brighter (0.15f) : panel);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (on ? juce::Colours::white : dimInk.withAlpha (0.6f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f);
    g.setColour (on ? juce::Colour (0xff120a1c) : ink);
    g.setFont (juce::FontOptions (r.getHeight() * 0.48f, juce::Font::bold));
    g.drawText (on ? "MELTING..." : "MELT", r, juce::Justification::centred);
}

// ------------------------------------------------------------------ editor
MeltEditor::MeltEditor (MeltProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    auto setup = [this] (Knob& k, const char* id, const char* name, bool big)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, big ? 22 : 16);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                      juce::MathConstants<float>::pi * 2.8f, true);
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.slider.setColour (juce::Slider::textBoxTextColourId, big ? ink : dimInk);
        k.label.setText (name, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (juce::FontOptions (big ? 20.0f : 13.0f, juce::Font::bold));
        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);
        k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    };

    setup (chaos, "chaos", "CHAOS", true);
    const char* ids[]   = { "warble", "dive", "stutter", "reverse", "crush", "filter", "smear", "mix", "output", "meltTime" };
    const char* names[] = { "Warble", "Dive", "Stutter", "Reverse", "Crush", "Filter", "Smear", "Mix", "Output", "Melt Time" };
    for (size_t i = 0; i < knobs.size(); ++i)
        setup (knobs[i], ids[i], names[i], false);
    knobs[8].slider.setTextValueSuffix (" dB");
    knobs[9].slider.setTextValueSuffix (" s");

    addAndMakeVisible (meltButton);
    meltAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "melt", meltButton);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    setSize (760, 440);
    startTimerHz (30);
}

MeltEditor::~MeltEditor()
{
    setLookAndFeel (nullptr);
}

void MeltEditor::timerCallback()
{
   #ifdef MELT_PREVIEW
    const float target = 0.8f;
   #else
    const float target = proc.getCurrentChaos();
   #endif
    shownChaos += (target - shownChaos) * 0.25f;
    time += 1.0f / 30.0f;
    lnf.hueShift = std::fmod (time * (0.02f + 0.25f * shownChaos), 1.0f);
    repaint();
}

void MeltEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // swirling rings behind the Chaos knob: calm circles at 0, a melting vortex at full chaos
    const float c = shownChaos;
    const int rings = 14;
    for (int i = rings; i > 0; --i)
    {
        const float baseR = 40.0f + i * 17.0f;
        juce::Path ring;
        const int pts = 96;
        for (int k = 0; k <= pts; ++k)
        {
            const float a = juce::MathConstants<float>::twoPi * k / pts;
            const float wob = c * (6.0f + i * 1.6f)
                * (std::sin (a * (3 + i % 4) + time * (0.6f + i * 0.13f))
                   + 0.5f * std::sin (a * 7.0f - time * 1.7f + i));
            const float twist = c * 0.35f * std::sin (time * 0.4f + i * 0.3f);
            const float rr = baseR + wob;
            const juce::Point<float> pt (swirlCentre.x + std::cos (a + twist) * rr,
                                         swirlCentre.y + std::sin (a + twist) * rr * (1.0f + c * 0.15f));
            if (k == 0) ring.startNewSubPath (pt); else ring.lineTo (pt);
        }
        g.setColour (psyche ((float) i / rings, lnf.hueShift, 0.06f + 0.22f * c));
        g.strokePath (ring, juce::PathStrokeType (1.5f + 2.0f * c));
    }

    // title
    g.setColour (ink);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("MELT", 20, 12, 200, 34, juce::Justification::centredLeft);
    g.setColour (dimInk);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("psychedelic breakdown", 112, 22, 200, 18, juce::Justification::centredLeft);

    // knob panel
    auto panelArea = juce::Rectangle<float> (330, 84, 410, 310);
    g.setColour (panel.withAlpha (0.85f));
    g.fillRoundedRectangle (panelArea, 14.0f);

    // live chaos readout
    g.setColour (psyche (c, lnf.hueShift));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("now: " + juce::String (juce::roundToInt (c * 100)) + "%",
                (int) swirlCentre.x - 60, getHeight() - 28, 120, 18, juce::Justification::centred);
}

void MeltEditor::resized()
{
    presetBox.setBounds (getWidth() - 220, 16, 200, 28);

    auto left = juce::Rectangle<int> (20, 60, 290, 360);
    swirlCentre = { (float) left.getCentreX(), (float) left.getY() + 140.0f };
    chaos.label.setBounds (left.getX(), left.getY(), left.getWidth(), 24);
    chaos.slider.setBounds (left.getCentreX() - 115, left.getY() + 24, 230, 250);
    meltButton.setBounds (left.getCentreX() - 80, left.getY() + 290, 160, 40);

    auto right = juce::Rectangle<int> (340, 100, 390, 280);
    const int cols = 5, rowsN = 2;
    const int cw = right.getWidth() / cols, ch = right.getHeight() / rowsN;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int col = (int) i % cols, row = (int) i / cols;
        auto cell = juce::Rectangle<int> (right.getX() + col * cw, right.getY() + row * ch, cw, ch).reduced (4);
        knobs[i].label.setBounds (cell.removeFromTop (20));
        knobs[i].slider.setBounds (cell.removeFromTop (juce::jmin (cell.getHeight(), cw + 20)));
    }
}

juce::AudioProcessorEditor* MeltProcessor::createEditor() { return new MeltEditor (*this); }
