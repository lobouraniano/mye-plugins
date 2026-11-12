// MYE Yunque — on the anvil metal is forged: heat and shape.
// Saturation with four materials (Tube, Tape, Transformer, Forge = metallic folder),
// a pre/de-emphasis "shape" control deciding where the heat lands, 4x oversampling
// and automatic level matching.
#include "MyeCommon.h"

namespace
{
enum Kind { valve, tapeK, transformer, forge };

juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace mye;
    Layout l;
    addFloat (l, "heat", "Calor", 0.0f, 36.0f, 12.0f, "dB");
    addChoice(l, "kind", "Material", juce::StringArray { utf8 ("Válvula"), "Cinta", "Transformador", "Fragua" }, 0);
    addFloat (l, "shape", "Forma", -100.0f, 100.0f, 0.0f, "%");
    addFloat (l, "asym", utf8 ("Asimetría"), 0.0f, 100.0f, 25.0f, "%");
    addFloat (l, "low", "Grave", -12.0f, 12.0f, 0.0f, "dB");
    addFloat (l, "high", "Brillo", -12.0f, 12.0f, 0.0f, "dB");
    addFloat (l, "mix", "Mezcla", 0.0f, 100.0f, 100.0f, "%");
    addFloat (l, "output", "Salida", -24.0f, 12.0f, 0.0f, "dB");
    addBool  (l, "autogain", "Auto nivel", true);
    return { l.begin(), l.end() };
}

using Filter = juce::dsp::IIR::Filter<float>;
using Coefs = juce::dsp::IIR::Coefficients<float>;
} // namespace

class YunqueProcessor : public mye::ProcessorBase
{
public:
    YunqueProcessor() : ProcessorBase (layout()) {}
    const juce::String getName() const override { return "MYE Yunque"; }
    double getTailLengthSeconds() const override { return 0.0; }

    void prepareToPlay (double sampleRate, int block) override
    {
        sr = sampleRate;
        os = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing ((size_t) block);
        setLatencySamples ((int) std::round (os->getLatencyInSamples()));
        for (auto* f : { preLo, preHi, postLo, postHi, toneLo, toneHi })
            for (int c = 0; c < 2; ++c) { f[c].coefficients = Coefs::makeLowShelf (sr, 800.0, 0.7f, 1.0f); f[c].reset(); }
        dryBuf.setSize (2, block);
        lowState[0] = lowState[1] = 0.0f;
        autoGain.reset (sr, 0.3);
        autoGain.setCurrentAndTargetValue (1.0f);
        inRms = outRms = 1.0e-6f;
        // delay line so the dry path lines up with the oversampler latency
        dryDelay.prepare ({ sr, (juce::uint32) block, 2 });
        dryDelay.setMaximumDelayInSamples (64);
        dryDelay.setDelay ((float) os->getLatencyInSamples());
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals nd;
        spreadMonoInput (buffer);
        const int n = buffer.getNumSamples(), nch = juce::jmin (2, buffer.getNumChannels());
        if (nch == 0 || os == nullptr) return;
        if (dryBuf.getNumSamples() < n) dryBuf.setSize (2, n, false, false, true);

        const int kind = (int) p ("kind");
        const float drive = mye::dbToGain (p ("heat"));
        const float shapeDb = p ("shape") / 100.0f * 9.0f;
        const float bias = p ("asym") / 100.0f * 0.6f;
        const float mix = p ("mix") / 100.0f;
        const float out = mye::dbToGain (p ("output"));
        const bool autoOn = p ("autogain") > 0.5f;

        // shape: tilt before the saturator, inverse tilt after (so only the *distortion* changes colour)
        auto lsPre = Coefs::makeLowShelf (sr, 800.0, 0.7f, mye::dbToGain (-shapeDb));
        auto hsPre = Coefs::makeHighShelf (sr, 800.0, 0.7f, mye::dbToGain (shapeDb));
        auto lsPost = Coefs::makeLowShelf (sr, 800.0, 0.7f, mye::dbToGain (shapeDb));
        auto hsPost = Coefs::makeHighShelf (sr, 800.0, 0.7f, mye::dbToGain (-shapeDb));
        auto lowC = Coefs::makeLowShelf (sr, 120.0, 0.7f, mye::dbToGain (p ("low")));
        auto highC = Coefs::makeHighShelf (sr, 6000.0, 0.7f, mye::dbToGain (p ("high")));
        for (int c = 0; c < 2; ++c)
        {
            *preLo[c].coefficients = *lsPre;  *preHi[c].coefficients = *hsPre;
            *postLo[c].coefficients = *lsPost; *postHi[c].coefficients = *hsPost;
            *toneLo[c].coefficients = *lowC;  *toneHi[c].coefficients = *highC;
        }

        // dry copy (latency-aligned)
        for (int c = 0; c < nch; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);
        {
            juce::dsp::AudioBlock<float> db (dryBuf.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (db);
            dryDelay.process (ctx);
        }

        float inSum = 0.0f;
        for (int c = 0; c < nch; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                inSum += d[i] * d[i];
                d[i] = preHi[c].processSample (preLo[c].processSample (d[i]));
            }
        }

        // ---- saturate at 4x
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
        auto up = os->processSamplesUp (block);
        const float lowC2 = mye::onePole (150.0f, sr * 4.0);
        const float biasOut = shaper (0.0f, kind, bias, 0);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                float x = d[i] * drive;
                if (kind == transformer)
                {
                    // the low band saturates harder, like an iron core
                    lowState[c] += lowC2 * (x - lowState[c]);
                    const float lo = lowState[c];
                    x = (x - lo) + std::tanh (lo * 1.8f + bias) / 1.8f - std::tanh (bias) / 1.8f;
                    d[i] = std::tanh (x * 0.8f) / 0.8f;
                }
                else
                    d[i] = shaper (x, kind, bias, 0) - biasOut;
            }
        }
        os->processSamplesDown (block);

        float outSum = 0.0f;
        for (int c = 0; c < nch; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                float y = postHi[c].processSample (postLo[c].processSample (d[i]));
                y = toneHi[c].processSample (toneLo[c].processSample (y));
                outSum += y * y;
                d[i] = y;
            }
        }

        // ---- level matching (slow, so it follows level, not the waveform)
        const float a = 1.0f - std::exp (-(float) n / (0.4f * (float) sr));
        inRms += a * (inSum / (float) (n * nch) - inRms);
        outRms += a * (outSum / (float) (n * nch) - outRms);
        autoGain.setTargetValue (autoOn ? juce::jlimit (0.05f, 4.0f, std::sqrt ((inRms + 1e-9f) / (outRms + 1e-9f))) : 1.0f);

        for (int i = 0; i < n; ++i)
        {
            const float g = autoGain.getNextValue();
            for (int c = 0; c < nch; ++c)
            {
                const float wet = buffer.getSample (c, i) * g;
                buffer.setSample (c, i, (dryBuf.getSample (c, i) * (1.0f - mix) + wet * mix) * out);
            }
        }
    }

    juce::AudioProcessorEditor* createEditor() override
    {
        using K = mye::SectionEditor::Kind;
        return new mye::SectionEditor (*this, "YUNQUE", mye::utf8 ("M.Y.E. · saturación · el yunque forja"),
        {
            { "FRAGUA", { { "heat", "Calor", K::knob }, { "asym", mye::utf8 ("Asimetría"), K::knob }, { "kind", "", K::choice } } },
            { "FORMA",  { { "shape", "Forma", K::knob } } },
            { "TONO",   { { "low", "Grave", K::knob }, { "high", "Brillo", K::knob } } },
            { "SALIDA", { { "mix", "Mezcla", K::knob }, { "output", "Salida", K::knob }, { "autogain", "Auto nivel", K::toggle } } },
        });
    }

private:
    static float shaper (float x, int kind, float bias, int)
    {
        switch (kind)
        {
            case valve: return std::tanh (x + bias);                                     // smooth, even harmonics via bias
            case tapeK: { const float y = x + bias * 0.5f; return y / (1.0f + std::abs (y)) * 1.3f; } // softer knee, compresses
            case forge: { const float y = x + bias; return std::sin (juce::MathConstants<float>::halfPi * juce::jlimit (-12.0f, 12.0f, y)); } // folds: metallic
            default:    return std::tanh (x);
        }
    }

    double sr = 44100.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    Filter preLo[2], preHi[2], postLo[2], postHi[2], toneLo[2], toneHi[2];
    juce::AudioBuffer<float> dryBuf;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dryDelay { 64 };
    float lowState[2] {};
    juce::SmoothedValue<float> autoGain;
    float inRms = 1e-6f, outRms = 1e-6f;
};

juce::AudioProcessor* createYunque() { return new YunqueProcessor(); }
#ifndef MYE_TEST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new YunqueProcessor(); }
#endif
