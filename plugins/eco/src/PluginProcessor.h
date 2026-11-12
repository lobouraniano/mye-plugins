#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// MYE Eco — stereo tape-ish delay with ping-pong, saturated feedback
// (can self-oscillate), filters in the loop, wow/flutter and auto-pan.
class EcoProcessor : public juce::AudioProcessor
{
public:
    EcoProcessor();
    ~EcoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MYE Eco"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    float readDelay (int ch, float delaySamples) const;
    double hostDelayMs (int divisionIndex) const;

    static constexpr float maxDelaySeconds = 4.0f;

    juce::AudioBuffer<float> delayBuffer;
    int writePos = 0;
    int bufferSize = 0;
    double sr = 44100.0;

    juce::SmoothedValue<float> timeSmooth[2];
    juce::SmoothedValue<float> mixSmooth, fbSmooth;

    // one-pole filters inside the feedback loop
    float lpState[2] {}, hpState[2] {};

    double wowPhase = 0.0, flutterPhase = 0.0, panPhase = 0.0;
    juce::Random rng;
    float driftValue = 0.0f, driftTarget = 0.0f;
    int driftCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EcoProcessor)
};
