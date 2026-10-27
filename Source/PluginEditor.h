#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class MeltLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MeltLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    float hueShift = 0.0f;
};

class MeltEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MeltEditor (MeltProcessor&);
    ~MeltEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    MeltProcessor& proc;
    MeltLookAndFeel lnf;
    Knob chaos;
    std::array<Knob, 10> knobs;
    juce::ToggleButton meltButton { "MELT" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> meltAtt;
    juce::ComboBox presetBox;

    float shownChaos = 0.0f, time = 0.0f;
    juce::Point<float> swirlCentre;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeltEditor)
};
