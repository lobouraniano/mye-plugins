// MYE Estribo — the stirrup carries the horse; the gallop repeats.
// Stereo delay with character modes (Clean / Tape / Analog 1-4 / LoFi), saturated
// feedback that can self-oscillate, modulation, auto-pan, width and phase.
#include "MyeCommon.h"

namespace
{
const juce::StringArray divisions { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                    "1/4T", "1/4", "1/4D", "1/2", "1/1" };
const double divisionBeats[] { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0 };

enum Mode { clean, tape, analog1, analog2, analog3, analog4, lofi };

struct ModeSpec
{
    float drive, asym, bandwidth, noise, hum, wow, flutter, hardness;
};
// base settings per mode at character = 1 (scaled by the character knob)
const ModeSpec modeSpecs[] {
    { 0.0f, 0.0f, 20000.0f, 0.0f,    0.0f,   0.0f, 0.0f, 0.0f }, // Limpio
    { 6.0f, 0.1f, 9000.0f,  0.0006f, 0.0f,   1.0f, 0.6f, 0.0f }, // Cinta
    { 3.0f, 0.0f, 8000.0f,  0.0004f, 0.0f,   0.0f, 0.1f, 0.0f }, // Analog 1 suave
    { 6.0f, 0.3f, 6000.0f,  0.0008f, 0.0005f,0.0f, 0.15f,0.0f }, // Analog 2 cálido
    { 10.0f,0.2f, 4500.0f,  0.0015f, 0.0008f,0.1f, 0.2f, 0.3f }, // Analog 3 sucio
    { 14.0f,0.4f, 3000.0f,  0.0025f, 0.0015f,0.2f, 0.3f, 1.0f }, // Analog 4 roto
    { 2.0f, 0.0f, 4000.0f,  0.0f,    0.0f,   0.0f, 0.0f, 0.0f }, // LoFi
};

juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    mye::Layout l;
    using namespace mye;
    addFloat (l, "time", "Tiempo", skewed (1.0f, 3000.0f, 300.0f, 0.1f), 350.0f, "ms");
    addBool  (l, "sync", "Sync", false);
    addChoice(l, "division", utf8 ("División"), divisions, 6);
    addFloat (l, "offset", "Offset R", -50.0f, 50.0f, 0.0f, "%");
    addBool  (l, "pingpong", "Ping-pong", false);
    addFloat (l, "feedback", "Feedback", 0.0f, 120.0f, 45.0f, "%");
    addFloat (l, "tone", "Tono", skewed (300.0f, 20000.0f, 3000.0f, 1.0f), 8000.0f, "Hz");
    addFloat (l, "lowcut", "Low cut", skewed (20.0f, 2000.0f, 200.0f, 1.0f), 80.0f, "Hz");
    addChoice(l, "mode", "Modo", juce::StringArray { "Limpio", "Cinta", "Analog 1", "Analog 2", "Analog 3", "Analog 4", "LoFi" }, 1);
    addFloat (l, "character", utf8 ("Carácter"), 0.0f, 100.0f, 40.0f, "%");
    addFloat (l, "modrate", "Mod rate", skewed (0.05f, 8.0f, 1.0f), 0.8f, "Hz");
    addFloat (l, "moddepth", "Mod depth", 0.0f, 100.0f, 0.0f, "%");
    addFloat (l, "panrate", "Pan rate", skewed (0.02f, 12.0f, 1.0f), 0.5f, "Hz");
    addFloat (l, "pandepth", "Pan depth", 0.0f, 100.0f, 0.0f, "%");
    addChoice(l, "pantarget", "Pan", juce::StringArray { "Pan: ecos", "Pan: todo" }, 0);
    addFloat (l, "mix", "Mix", 0.0f, 100.0f, 35.0f, "%");
    addFloat (l, "width", "Ancho", 0.0f, 200.0f, 100.0f, "%");
    addBool  (l, "phase", "Fase R", false);
    addBool  (l, "freeze", "Freeze", false);
    addFloat (l, "output", "Salida", -24.0f, 12.0f, 0.0f, "dB");
    return { l.begin(), l.end() };
}
} // namespace

class EstriboProcessor : public mye::ProcessorBase
{
public:
    EstriboProcessor() : ProcessorBase (layout()) {}
    const juce::String getName() const override { return "MYE Estribo"; }
    double getTailLengthSeconds() const override { return 8.0; }

    void prepareToPlay (double sampleRate, int) override
    {
        sr = sampleRate;
        bufSize = (int) (maxSeconds * sr) + 16;
        buf.setSize (2, bufSize);
        buf.clear();
        writePos = 0;
        for (auto& t : timeSm) { t.reset (sr, 0.25); t.setCurrentAndTargetValue (350.0f); }
        mixSm.reset (sr, 0.03);
        fbSm.reset (sr, 0.05);
        for (int c = 0; c < 2; ++c) lp[c] = hp[c] = bw[c] = holdVal[c] = 0.0f;
        holdCount = 0.0f;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals nd;
        spreadMonoInput (buffer);
        const int n = buffer.getNumSamples();
        const int nch = juce::jmin (2, buffer.getNumChannels());
        if (nch == 0 || bufSize == 0) return;

        const bool stereo = nch == 2;
        const bool sync = p ("sync") > 0.5f, pingpong = p ("pingpong") > 0.5f && stereo;
        const bool freeze = p ("freeze") > 0.5f, panAll = p ("pantarget") > 0.5f, phaseR = p ("phase") > 0.5f;
        const int mode = (int) p ("mode");
        const float ch = p ("character") / 100.0f;
        const auto& ms = modeSpecs[juce::jlimit (0, 6, mode)];

        double baseMs = sync ? hostMs ((int) p ("division")) : (double) p ("time");
        const double maxMs = maxSeconds * 1000.0 - 80.0;
        baseMs = juce::jlimit (1.0, maxMs, baseMs);
        timeSm[0].setTargetValue ((float) baseMs);
        timeSm[1].setTargetValue ((float) juce::jlimit (1.0, maxMs, baseMs * (1.0 + p ("offset") / 100.0)));
        fbSm.setTargetValue (freeze ? 1.0f : p ("feedback") / 100.0f);
        mixSm.setTargetValue (p ("mix") / 100.0f);

        const float lpC = mye::onePole (p ("tone"), sr), hpC = mye::onePole (p ("lowcut"), sr);
        // character: bandwidth limit moves from 20 kHz toward the mode's value
        const float bwHz = juce::jmin ((float) sr * 0.45f, 20000.0f + (ms.bandwidth - 20000.0f) * (mode == clean ? 0.0f : 0.4f + 0.6f * ch));
        const float bwC = mye::onePole (bwHz, sr);
        const float drive = mye::dbToGain (ms.drive * ch);
        const float asym = ms.asym * ch, hard = ms.hardness;
        const float noiseAmt = ms.noise * ch, humAmt = ms.hum * ch;
        const float wow = ms.wow * ch, flutter = ms.flutter * ch;
        const float modDepthMs = p ("moddepth") / 100.0f * 4.0f;
        const double modInc = p ("modrate") / sr, panInc = p ("panrate") / sr;
        const float panDepth = p ("pandepth") / 100.0f;
        const float width = p ("width") / 100.0f;
        const float out = mye::dbToGain (p ("output"));
        // LoFi: sample-rate & bit reduction scale with character
        const float holdLen = mode == lofi ? 1.0f + ch * ((float) sr / 6000.0f - 1.0f) : 1.0f;
        const float bits = mode == lofi ? 12.0f - 7.0f * ch : 0.0f;
        const float q = bits > 0.0f ? std::pow (2.0f, bits - 1.0f) : 0.0f;
        const float inGain = freeze ? 0.0f : 1.0f;

        float* io[2] { buffer.getWritePointer (0), buffer.getWritePointer (stereo ? 1 : 0) };
        float* dl[2] { buf.getWritePointer (0), buf.getWritePointer (1) };
        constexpr double twoPi = juce::MathConstants<double>::twoPi;

        for (int i = 0; i < n; ++i)
        {
            if (--driftCount <= 0) { driftTarget = rng.nextFloat() * 2.0f - 1.0f; driftCount = (int) (sr * 0.3); }
            drift += (driftTarget - drift) * 0.00005f;
            wowPh = std::fmod (wowPh + 0.55 / sr, 1.0);
            flPh = std::fmod (flPh + 6.8 / sr, 1.0);
            modPh = std::fmod (modPh + modInc, 1.0);
            panPh = std::fmod (panPh + panInc, 1.0);
            humPh = std::fmod (humPh + 50.0 / sr, 1.0);

            const float tapeMs = wow * (5.0f * (float) std::sin (twoPi * wowPh) + 3.0f * drift)
                               + flutter * 0.5f * (float) std::sin (twoPi * flPh);
            const float modS = (float) std::sin (twoPi * modPh);
            const float fb = fbSm.getNextValue(), mix = mixSm.getNextValue();

            float dry[2], wet[2];
            for (int c = 0; c < nch; ++c)
            {
                dry[c] = io[c][i];
                const float m = modDepthMs * (c == 0 ? modS : -modS); // opposite phase L/R = stereo chorus
                const float t = juce::jmax (0.5f, timeSm[c].getNextValue() + tapeMs + m);
                wet[c] = read (c, t * 0.001f * (float) sr);
            }

            float loop[2];
            for (int c = 0; c < nch; ++c)
            {
                float x = wet[c];
                if (! freeze)
                {
                    lp[c] += lpC * (x - lp[c]);  x = lp[c];
                    hp[c] += hpC * (x - hp[c]);  x -= hp[c];
                    bw[c] += bwC * (x - bw[c]);  x = bw[c];
                }
                loop[c] = saturate (x, drive, asym, hard) * fb;
            }

            if (pingpong)
            {
                dl[0][writePos] = 0.5f * (dry[0] + dry[1]) * inGain + loop[1];
                dl[1][writePos] = loop[0];
            }
            else
                for (int c = 0; c < nch; ++c) dl[c][writePos] = dry[c] * inGain + loop[c];
            if (++writePos >= bufSize) writePos = 0;

            // LoFi applied on the audible repeats
            if (mode == lofi)
            {
                holdCount += 1.0f;
                if (holdCount >= holdLen) { holdCount -= holdLen; for (int c = 0; c < nch; ++c) holdVal[c] = std::round (wet[c] * q) / q; }
                for (int c = 0; c < nch; ++c) wet[c] = holdVal[c];
            }
            // noise floor + hum of the "circuit"
            if (noiseAmt > 0.0f || humAmt > 0.0f)
            {
                const float hum = humAmt * (float) (std::sin (twoPi * humPh) + 0.4 * std::sin (2.0 * twoPi * humPh));
                for (int c = 0; c < nch; ++c) wet[c] += noiseAmt * (rng.nextFloat() * 2.0f - 1.0f) + hum;
            }

            if (stereo)
            {
                // width (M/S) and phase on the repeats
                const float mid = 0.5f * (wet[0] + wet[1]), side = 0.5f * (wet[0] - wet[1]) * width;
                float wL = mid + side, wR = mid - side;
                if (phaseR) wR = -wR;
                float gL = 1.0f, gR = 1.0f;
                if (panDepth > 0.0f)
                {
                    const float a = juce::MathConstants<float>::pi * 0.25f * (1.0f + panDepth * (float) std::sin (twoPi * panPh));
                    gL = std::cos (a) * juce::MathConstants<float>::sqrt2;
                    gR = std::sin (a) * juce::MathConstants<float>::sqrt2;
                }
                float dL = dry[0], dR = dry[1];
                wL *= gL; wR *= gR;
                if (panAll) { dL *= gL; dR *= gR; }
                io[0][i] = (dL * (1.0f - mix) + wL * mix) * out;
                io[1][i] = (dR * (1.0f - mix) + wR * mix) * out;
            }
            else
                io[0][i] = (dry[0] * (1.0f - mix) + wet[0] * mix) * out;
        }
    }

    juce::AudioProcessorEditor* createEditor() override
    {
        using K = mye::SectionEditor::Kind;
        return new mye::SectionEditor (*this, "ESTRIBO", mye::utf8 ("M.Y.E. · delay · el estribo transmite"),
        {
            { "TIEMPO",       { { "time", "Tiempo", K::knob }, { "offset", "Offset R", K::knob },
                                { "sync", "Sync", K::toggle }, { "division", "", K::choice }, { "pingpong", "Ping-pong", K::toggle } } },
            { "REPETICIONES", { { "feedback", "Feedback", K::knob }, { "tone", "Tono", K::knob }, { "lowcut", "Low cut", K::knob },
                                { "freeze", "Freeze", K::toggle } } },
            { mye::utf8 ("CARÁCTER"), { { "character", mye::utf8 ("Carácter"), K::knob }, { "mode", "", K::choice } } },
            { "MOVIMIENTO",   { { "modrate", "Mod rate", K::knob }, { "moddepth", "Mod depth", K::knob },
                                { "panrate", "Pan rate", K::knob }, { "pandepth", "Pan depth", K::knob }, { "pantarget", "", K::choice } } },
            { "SALIDA",       { { "mix", "Mix", K::knob }, { "width", "Ancho", K::knob }, { "output", "Salida", K::knob },
                                { "phase", "Fase R", K::toggle } } },
        });
    }

private:
    static constexpr double maxSeconds = 4.0;

    double hostMs (int div) const
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = *b;
        return 60000.0 / bpm * divisionBeats[juce::jlimit (0, 11, div)];
    }

    static float saturate (float x, float drive, float asym, float hard)
    {
        // bounded => feedback above 100% self-oscillates without blowing up
        float y = x * drive + asym * x * x * drive;
        y = hard > 0.0f ? (1.0f - hard) * std::tanh (y) + hard * juce::jlimit (-0.7f, 0.7f, y) / 0.7f : std::tanh (y);
        return y / drive; // unity small-signal gain
    }

    float read (int c, float d) const
    {
        const float* b = buf.getReadPointer (c);
        float rp = (float) writePos - d;
        while (rp < 0.0f) rp += (float) bufSize;
        const int i1 = (int) rp % bufSize;
        const float f = rp - std::floor (rp);
        const int i0 = (i1 - 1 + bufSize) % bufSize, i2 = (i1 + 1) % bufSize, i3 = (i1 + 2) % bufSize;
        const float y0 = b[i0], y1 = b[i1], y2 = b[i2], y3 = b[i3];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    double sr = 44100.0;
    juce::AudioBuffer<float> buf;
    int bufSize = 0, writePos = 0;
    juce::SmoothedValue<float> timeSm[2], mixSm, fbSm;
    float lp[2] {}, hp[2] {}, bw[2] {}, holdVal[2] {}, holdCount = 0.0f;
    double wowPh = 0, flPh = 0, modPh = 0, panPh = 0, humPh = 0;
    juce::Random rng;
    float drift = 0, driftTarget = 0;
    int driftCount = 0;
};

juce::AudioProcessor* createEstribo() { return new EstriboProcessor(); }
#ifndef MYE_TEST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EstriboProcessor(); }
#endif
