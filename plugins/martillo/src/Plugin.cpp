// MYE Martillo — the hammer strikes; the compressor tames the strike.
// Classic optical leveling amplifier behaviour (inspired by the 1960s opto
// levelers): fixed-threshold program-dependent compression with a two-stage,
// memory-dependent release, HF emphasis in the detector and a tube output stage.
#include "MyeCommon.h"

namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace mye;
    Layout l;
    addFloat (l, "peak", utf8 ("Reducción"), 0.0f, 100.0f, 40.0f);
    addFloat (l, "gain", "Ganancia", 0.0f, 40.0f, 6.0f, "dB");
    addChoice(l, "mode", "Modo", juce::StringArray { "Compress", "Limit" }, 0);
    addFloat (l, "emphasis", utf8 ("Énfasis HF"), 0.0f, 100.0f, 0.0f, "%");
    addFloat (l, "tube", utf8 ("Válvula"), 0.0f, 100.0f, 30.0f, "%");
    addFloat (l, "mix", "Mezcla", 0.0f, 100.0f, 100.0f, "%");
    return { l.begin(), l.end() };
}
} // namespace

class MartilloProcessor : public mye::ProcessorBase
{
public:
    MartilloProcessor() : ProcessorBase (layout()) {}
    const juce::String getName() const override { return "MYE Martillo"; }
    double getTailLengthSeconds() const override { return 0.0; }

    std::atomic<float> meterGr { 0.0f }, meterOut { -60.0f };

    void prepareToPlay (double sampleRate, int) override
    {
        sr = sampleRate;
        env = 0.0f; fast = slow = memory = 0.0f;
        hpState[0] = hpState[1] = 0.0f;
        gainSm.reset (sr, 0.05);
        mixSm.reset (sr, 0.05);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals nd;
        spreadMonoInput (buffer);
        const int n = buffer.getNumSamples(), nch = juce::jmin (2, buffer.getNumChannels());
        if (nch == 0) return;

        const bool limit = p ("mode") > 0.5f;
        const float ratio = limit ? 12.0f : 3.0f, knee = limit ? 4.0f : 12.0f;
        const float scGain = mye::dbToGain (-10.0f + 0.45f * p ("peak"));
        const float emph = p ("emphasis") / 100.0f * 3.0f;
        const float tube = p ("tube") / 100.0f;
        gainSm.setTargetValue (mye::dbToGain (p ("gain")));
        mixSm.setTargetValue (p ("mix") / 100.0f);

        const float hpC = mye::onePole (2000.0f, sr);
        const float envAtt = coeff (0.001f), envRel = coeff (0.03f);
        const float att = coeff (0.010f), fastRel = coeff (0.06f), memC = coeff (4.0f);
        const float k = 1.0f + 3.0f * tube, bias = 0.15f * tube;
        const float tubeNorm = k * (1.0f - std::pow (std::tanh (k * bias), 2.0f));
        constexpr float threshold = -12.0f;
        float maxGr = 0.0f, peakOut = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            // ---- detector (stereo-linked), with HF emphasis
            float sc = 0.0f;
            for (int c = 0; c < nch; ++c)
            {
                const float x = buffer.getSample (c, i);
                hpState[c] += hpC * (x - hpState[c]);
                const float hf = x - hpState[c];
                sc = juce::jmax (sc, std::abs ((x + emph * hf) * scGain));
            }
            const float pw = sc * sc;
            env += (pw > env ? envAtt : envRel) * (pw - env);
            const float levelDb = 10.0f * std::log10 (env + 1.0e-12f);

            // ---- static curve with soft knee
            const float over = levelDb - threshold;
            float target = 0.0f;
            if (over > knee * 0.5f) target = over * (1.0f - 1.0f / ratio);
            else if (over > -knee * 0.5f)
            {
                const float t = over + knee * 0.5f;
                target = (1.0f - 1.0f / ratio) * t * t / (2.0f * knee);
            }

            // ---- optical cell: 10 ms attack, two-stage release with memory
            if (target > fast) { fast += att * (target - fast); slow += att * (target - slow); }
            else
            {
                const float slowTau = 0.4f + 4.6f * memory;
                fast += fastRel * (target - fast);
                slow += coeff (slowTau) * (target - slow);
            }
            const float gr = juce::jmax (0.0f, 0.5f * fast + 0.5f * slow);
            memory += memC * (juce::jlimit (0.0f, 1.0f, gr / 8.0f) - memory);
            maxGr = juce::jmax (maxGr, gr);

            const float g = mye::dbToGain (-gr) * gainSm.getNextValue();
            const float mix = mixSm.getNextValue();
            for (int c = 0; c < nch; ++c)
            {
                const float dry = buffer.getSample (c, i);
                float y = dry * g;
                if (tube > 0.0f) y = (std::tanh (k * (y + bias)) - std::tanh (k * bias)) / tubeNorm;
                const float o = dry * (1.0f - mix) + y * mix;
                buffer.setSample (c, i, o);
                peakOut = juce::jmax (peakOut, std::abs (o));
            }
        }
        meterGr.store (maxGr);
        meterOut.store (juce::Decibels::gainToDecibels (peakOut, -60.0f));
    }

    juce::AudioProcessorEditor* createEditor() override;

private:
    float coeff (float seconds) const { return 1.0f - std::exp (-1.0f / (seconds * (float) sr)); }

    double sr = 44100.0;
    float env = 0, fast = 0, slow = 0, memory = 0, hpState[2] {};
    juce::SmoothedValue<float> gainSm, mixSm;
};

// ---------------------------------------------------------------- VU-style meter
class GrMeter : public juce::Component
{
public:
    explicit GrMeter (MartilloProcessor& pr) : proc (pr) {}
    void paint (juce::Graphics& g) override
    {
        const auto acc = juce::Colour (0xffc8873a), ink = juce::Colour (0xffe8e2d4);
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff1c1c1c));
        g.fillRoundedRectangle (r, 6.0f);
        r.reduce (12, 8);

        // smoothed like a VU needle
        grShown += 0.35f * (proc.meterGr.load() - grShown);
        outShown += 0.35f * (proc.meterOut.load() - outShown);

        auto drawBar = [&] (juce::Rectangle<float> row, const juce::String& name, float norm, bool fromRight, const juce::String& val)
        {
            g.setColour (ink.withAlpha (0.6f));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (name, row.removeFromLeft (110), juce::Justification::centredLeft);
            auto valArea = row.removeFromRight (70);
            g.drawText (val, valArea, juce::Justification::centredRight);
            g.setColour (juce::Colour (0xff2c2c2c));
            g.fillRoundedRectangle (row.reduced (0, 5), 3.0f);
            const float w = row.getWidth() * juce::jlimit (0.0f, 1.0f, norm);
            g.setColour (acc);
            auto bar = row.reduced (0, 5);
            g.fillRoundedRectangle (fromRight ? bar.removeFromRight (w) : bar.removeFromLeft (w), 3.0f);
        };
        auto top = r.removeFromTop (r.getHeight() / 2);
        drawBar (top, mye::utf8 ("REDUCCIÓN"), grShown / 20.0f, true, juce::String (-grShown, 1) + " dB");
        drawBar (r, "SALIDA", (outShown + 60.0f) / 60.0f, false, juce::String (outShown, 1) + " dB");
    }

private:
    MartilloProcessor& proc;
    float grShown = 0.0f, outShown = -60.0f;
};

class MartilloEditor : public mye::SectionEditor
{
public:
    explicit MartilloEditor (MartilloProcessor& p)
        : SectionEditor (p, "MARTILLO", mye::utf8 ("M.Y.E. · compresor óptico · el martillo golpea"),
          {
              { "COMPRESOR", { { "peak", mye::utf8 ("Reducción"), Kind::knob }, { "gain", "Ganancia", Kind::knob },
                               { "mode", "", Kind::choice } } },
              { mye::utf8 ("DETECCIÓN"), { { "emphasis", mye::utf8 ("Énfasis HF"), Kind::knob } } },
              { "COLOR", { { "tube", mye::utf8 ("Válvula"), Kind::knob }, { "mix", "Mezcla", Kind::knob } } },
          }),
          meter (p)
    {
        setExtraComponent (&meter, 70);
    }

private:
    GrMeter meter;
};

juce::AudioProcessorEditor* MartilloProcessor::createEditor() { return new MartilloEditor (*this); }

juce::AudioProcessor* createMartillo() { return new MartilloProcessor(); }
#ifndef MYE_TEST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MartilloProcessor(); }
#endif
