#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset { const char* name; float chaos, warble, dive, stutter, reverse, crush, filter, smear, mix, meltSec; };
    const Preset presets[] = {
        //  name               chaos warb  dive  stut  rev   crush filt  smear mix   meltSec
        { "Default Melt",      0.0f, 0.7f, 0.6f, 0.6f, 0.5f, 0.4f, 0.5f, 0.6f, 1.0f,  8.0f },
        { "Acid Wash",         0.0f, 1.0f, 0.3f, 0.2f, 0.4f, 0.0f, 0.7f, 0.9f, 1.0f, 12.0f },
        { "Tape Death",        0.0f, 0.8f, 1.0f, 0.4f, 0.1f, 0.2f, 0.6f, 0.4f, 1.0f,  6.0f },
        { "Glitch Storm",      0.0f, 0.3f, 0.3f, 1.0f, 0.8f, 0.7f, 0.3f, 0.3f, 1.0f,  4.0f },
        { "Reverse Dream",     0.0f, 0.6f, 0.2f, 0.3f, 1.0f, 0.0f, 0.4f, 0.8f, 1.0f, 10.0f },
        { "Bad Trip",          0.0f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.9f, 1.0f, 1.0f, 16.0f },
        { "Subtle Haze",       0.35f,0.5f, 0.1f, 0.0f, 0.2f, 0.0f, 0.3f, 0.5f, 0.6f,  8.0f },
    };
    constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));

    float getParam (juce::AudioProcessorValueTreeState& s, const char* id)
    {
        return s.getRawParameterValue (id)->load();
    }
}

MeltProcessor::MeltProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MELT", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MeltProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using ID = juce::ParameterID;
    auto pct = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; });

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<P> (ID { "chaos",   1 }, "Chaos",   juce::NormalisableRange<float> (0, 1), 0.0f, pct));
    p.push_back (std::make_unique<P> (ID { "warble",  1 }, "Warble",  juce::NormalisableRange<float> (0, 1), 0.7f, pct));
    p.push_back (std::make_unique<P> (ID { "dive",    1 }, "Dive",    juce::NormalisableRange<float> (0, 1), 0.6f, pct));
    p.push_back (std::make_unique<P> (ID { "stutter", 1 }, "Stutter", juce::NormalisableRange<float> (0, 1), 0.6f, pct));
    p.push_back (std::make_unique<P> (ID { "reverse", 1 }, "Reverse", juce::NormalisableRange<float> (0, 1), 0.5f, pct));
    p.push_back (std::make_unique<P> (ID { "crush",   1 }, "Crush",   juce::NormalisableRange<float> (0, 1), 0.4f, pct));
    p.push_back (std::make_unique<P> (ID { "filter",  1 }, "Filter",  juce::NormalisableRange<float> (0, 1), 0.5f, pct));
    p.push_back (std::make_unique<P> (ID { "smear",   1 }, "Smear",   juce::NormalisableRange<float> (0, 1), 0.6f, pct));
    p.push_back (std::make_unique<P> (ID { "mix",     1 }, "Mix",     juce::NormalisableRange<float> (0, 1), 1.0f, pct));
    p.push_back (std::make_unique<P> (ID { "output",  1 }, "Output",  juce::NormalisableRange<float> (-24, 12, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<juce::AudioParameterBool> (ID { "melt", 1 }, "Melt", false));
    p.push_back (std::make_unique<P> (ID { "meltTime", 1 }, "Melt Time",
        juce::NormalisableRange<float> (0.5f, 30.0f, 0.1f, 0.5f), 8.0f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));
    return { p.begin(), p.end() };
}

bool MeltProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void MeltProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void MeltProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                engine.setBpm (*bpm);

    melt::Params p;
    p.chaos   = getParam (apvts, "chaos");
    p.warble  = getParam (apvts, "warble");
    p.dive    = getParam (apvts, "dive");
    p.stutter = getParam (apvts, "stutter");
    p.reverse = getParam (apvts, "reverse");
    p.crush   = getParam (apvts, "crush");
    p.filter  = getParam (apvts, "filter");
    p.smear   = getParam (apvts, "smear");
    p.mix     = getParam (apvts, "mix");
    p.outDb   = getParam (apvts, "output");
    p.meltOn  = getParam (apvts, "melt") > 0.5f;
    p.meltSec = getParam (apvts, "meltTime");

    const int nCh = juce::jmin (2, buffer.getNumChannels());
    if (nCh == 0) return;
    engine.process (buffer.getArrayOfWritePointers(), nCh, buffer.getNumSamples(), p);
}

int MeltProcessor::getNumPrograms() { return numPresets; }

const juce::String MeltProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, numPresets) ? presets[i].name : "";
}

void MeltProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, numPresets)) return;
    currentProgram = i;
    const auto& pr = presets[i];
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set ("chaos", pr.chaos);     set ("warble", pr.warble);   set ("dive", pr.dive);
    set ("stutter", pr.stutter); set ("reverse", pr.reverse); set ("crush", pr.crush);
    set ("filter", pr.filter);   set ("smear", pr.smear);     set ("mix", pr.mix);
    set ("meltTime", pr.meltSec);
}

void MeltProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void MeltProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MeltProcessor(); }
