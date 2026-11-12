#pragma once
#include "PluginProcessor.h"

class EcoEditor : public juce::AudioProcessorEditor
{
public:
    explicit EcoEditor (EcoProcessor&);
    ~EcoEditor() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    };
    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };

    void addKnob (const juce::String& id, const juce::String& name);
    void addToggle (const juce::String& id, const juce::String& name);
    void addChoice (const juce::String& id, const juce::String& name);

    EcoProcessor& proc;
    juce::LookAndFeel_V4 lnf; // declared before the components so it outlives them
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<Toggle> toggles;
    juce::OwnedArray<Choice> choices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EcoEditor)
};
