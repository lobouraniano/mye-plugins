#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::StringArray divisionNames { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
                                            "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1" };
    // length in quarter notes
    const double divisionBeats[] { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5,
                                   0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0 };

    float onePoleCoeff (float cutoffHz, double sampleRate)
    {
        return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / (float) sampleRate);
    }
}

EcoProcessor::EcoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ECO", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout EcoProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto msRange = NormalisableRange<float> (1.0f, 3000.0f, 0.1f);
    msRange.setSkewForCentre (300.0f);
    auto hz = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi, 1.0f);
        r.setSkewForCentre (centre);
        return r;
    };

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "time", 1 }, "Time (ms)", msRange, 350.0f));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "sync", 1 }, "Sync to tempo", false));
    p.push_back (std::make_unique<AudioParameterChoice>(ParameterID { "division", 1 }, "Division", divisionNames, 6));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "spread", 1 }, "Stereo offset (%)",
                                                        NormalisableRange<float> (-50.0f, 50.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "feedback", 1 }, "Feedback",
                                                        NormalisableRange<float> (0.0f, 1.2f, 0.001f), 0.45f));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "pingpong", 1 }, "Ping-pong", false));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "freeze", 1 }, "Freeze", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 }, "Tone (high cut)", hz (300.0f, 20000.0f, 3000.0f), 7000.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lowcut", 1 }, "Low cut", hz (20.0f, 2000.0f, 200.0f), 80.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive (dB)",
                                                        NormalisableRange<float> (0.0f, 24.0f, 0.1f), 3.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "wow", 1 }, "Wow",
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.15f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "flutter", 1 }, "Flutter",
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.05f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "panrate", 1 }, "Pan rate (Hz)",
                                                        [] { NormalisableRange<float> r (0.02f, 12.0f, 0.001f); r.setSkewForCentre (1.0f); return r; }(), 0.5f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "pandepth", 1 }, "Pan depth",
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    p.push_back (std::make_unique<AudioParameterChoice>(ParameterID { "pantarget", 1 }, "Pan target",
                                                        StringArray { "Echoes only", "Everything" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output (dB)",
                                                        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
    return { p.begin(), p.end() };
}

bool EcoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void EcoProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    bufferSize = (int) (maxDelaySeconds * sampleRate) + 8;
    delayBuffer.setSize (2, bufferSize);
    delayBuffer.clear();
    writePos = 0;

    for (auto& t : timeSmooth)
        t.reset (sampleRate, 0.25); // slow glide => tape-like pitch bend when changing time
    mixSmooth.reset (sampleRate, 0.03);
    fbSmooth.reset (sampleRate, 0.05);
    timeSmooth[0].setCurrentAndTargetValue (350.0f);
    timeSmooth[1].setCurrentAndTargetValue (350.0f);

    for (int ch = 0; ch < 2; ++ch)
        lpState[ch] = hpState[ch] = 0.0f;
}

double EcoProcessor::hostDelayMs (int divisionIndex) const
{
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;
    return 60000.0 / bpm * divisionBeats[juce::jlimit (0, 11, divisionIndex)];
}

// 4-point Hermite interpolation
float EcoProcessor::readDelay (int ch, float delaySamples) const
{
    const float* buf = delayBuffer.getReadPointer (ch);
    float readPos = (float) writePos - delaySamples;
    while (readPos < 0.0f) readPos += (float) bufferSize;

    const int i1 = (int) readPos;
    const float f = readPos - (float) i1;
    const int i0 = (i1 - 1 + bufferSize) % bufferSize;
    const int i2 = (i1 + 1) % bufferSize;
    const int i3 = (i1 + 2) % bufferSize;

    const float y0 = buf[i0], y1 = buf[i1 % bufferSize], y2 = buf[i2], y3 = buf[i3];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

void EcoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh == 0 || bufferSize == 0) return;

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const bool sync     = get ("sync") > 0.5f;
    const bool pingpong = get ("pingpong") > 0.5f && numCh == 2;
    const bool freeze   = get ("freeze") > 0.5f;
    const bool panAll   = get ("pantarget") > 0.5f;

    double baseMs = sync ? hostDelayMs ((int) get ("division")) : (double) get ("time");
    baseMs = juce::jlimit (1.0, (double) maxDelaySeconds * 1000.0 - 60.0, baseMs);
    const float spread = get ("spread") / 100.0f;
    const float msL = (float) baseMs;
    const float msR = (float) juce::jlimit (1.0, (double) maxDelaySeconds * 1000.0 - 60.0, baseMs * (1.0 + spread));
    timeSmooth[0].setTargetValue (msL);
    timeSmooth[1].setTargetValue (numCh == 2 ? msR : msL);

    fbSmooth.setTargetValue (freeze ? 1.0f : get ("feedback"));
    mixSmooth.setTargetValue (get ("mix"));

    const float lpC = onePoleCoeff (get ("tone"), sr);
    const float hpC = onePoleCoeff (get ("lowcut"), sr);
    const float driveGain = juce::Decibels::decibelsToGain (get ("drive"));
    const float wow = get ("wow"), flutter = get ("flutter");
    const float panRate = get ("panrate"), panDepth = get ("pandepth");
    const float outGain = juce::Decibels::decibelsToGain (get ("output"));

    const double wowInc = 0.6 / sr, flutterInc = 7.3 / sr, panInc = panRate / sr;
    const float inputGain = freeze ? 0.0f : 1.0f;

    float* chans[2] { buffer.getWritePointer (0), buffer.getWritePointer (numCh > 1 ? 1 : 0) };

    for (int n = 0; n < numSamples; ++n)
    {
        // slow random drift adds an organic, unstable tape feel
        if (--driftCounter <= 0)
        {
            driftTarget = rng.nextFloat() * 2.0f - 1.0f;
            driftCounter = (int) (sr * 0.3);
        }
        driftValue += (driftTarget - driftValue) * 0.00005f;

        wowPhase += wowInc;         if (wowPhase >= 1.0) wowPhase -= 1.0;
        flutterPhase += flutterInc; if (flutterPhase >= 1.0) flutterPhase -= 1.0;
        panPhase += panInc;         if (panPhase >= 1.0) panPhase -= 1.0;

        const float modMs = wow * 6.0f * (float) std::sin (juce::MathConstants<double>::twoPi * wowPhase)
                          + wow * 3.0f * driftValue
                          + flutter * 0.6f * (float) std::sin (juce::MathConstants<double>::twoPi * flutterPhase);

        const float fb = fbSmooth.getNextValue();
        const float mix = mixSmooth.getNextValue();

        float dry[2], wet[2];
        for (int ch = 0; ch < numCh; ++ch)
        {
            dry[ch] = chans[ch][n];
            const float tMs = juce::jmax (0.5f, timeSmooth[ch].getNextValue() + modMs);
            wet[ch] = readDelay (ch, tMs * 0.001f * (float) sr);
        }

        // feedback path: filters + saturation (bounded => safe self-oscillation above 1.0)
        float loop[2];
        for (int ch = 0; ch < numCh; ++ch)
        {
            float x = wet[ch];
            if (! freeze)
            {
                lpState[ch] += lpC * (x - lpState[ch]);
                x = lpState[ch];
                hpState[ch] += hpC * (x - hpState[ch]);
                x = x - hpState[ch];
            }
            loop[ch] = std::tanh (driveGain * x) / driveGain * fb;
        }

        float* write[2] { delayBuffer.getWritePointer (0), delayBuffer.getWritePointer (1) };
        if (pingpong)
        {
            const float inMono = 0.5f * (dry[0] + dry[1]) * inputGain;
            write[0][writePos] = inMono + loop[1];
            write[1][writePos] = loop[0];
        }
        else
        {
            for (int ch = 0; ch < numCh; ++ch)
                write[ch][writePos] = dry[ch] * inputGain + loop[ch];
        }
        if (++writePos >= bufferSize) writePos = 0;

        // auto-pan (equal power)
        float gL = 1.0f, gR = 1.0f;
        if (numCh == 2 && panDepth > 0.0f)
        {
            const float lfo = (float) std::sin (juce::MathConstants<double>::twoPi * panPhase);
            const float angle = juce::MathConstants<float>::pi * 0.25f * (1.0f + panDepth * lfo);
            gL = std::cos (angle) * juce::MathConstants<float>::sqrt2;
            gR = std::sin (angle) * juce::MathConstants<float>::sqrt2;
        }

        if (numCh == 2)
        {
            float wL = wet[0], wR = wet[1], dL = dry[0], dR = dry[1];
            wL *= gL; wR *= gR;
            if (panAll) { dL *= gL; dR *= gR; }
            chans[0][n] = (dL * (1.0f - mix) + wL * mix) * outGain;
            chans[1][n] = (dR * (1.0f - mix) + wR * mix) * outGain;
        }
        else
        {
            chans[0][n] = (dry[0] * (1.0f - mix) + wet[0] * mix) * outGain;
        }
    }
}

juce::AudioProcessorEditor* EcoProcessor::createEditor() { return new EcoEditor (*this); }

void EcoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void EcoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EcoProcessor(); }
