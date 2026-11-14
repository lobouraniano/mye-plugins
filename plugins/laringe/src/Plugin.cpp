// MYE Laringe — where the pitch of a voice is born.
// Four-voice harmonizer for monophonic sources. Each voice has its own pitch,
// formant, delay, pan and level. Pitch-synchronous overlap-add: the detected
// period sets the grain length, grain spacing sets the new pitch (formants stay
// put), and the read speed inside each grain moves the formants independently.
#include "MyeCommon.h"

namespace
{
constexpr int numVoices = 4, maxGrains = 24;
constexpr float minHz = 65.0f, maxHz = 800.0f;
enum { vOn, vPitch, vFine, vFormant, vDelay, vPan, vLevel, vNumParams };
const char* const suffix[] { "on", "pitch", "fine", "formant", "delay", "pan", "level" };

juce::String vid (int v, int what) { return "v" + juce::String (v + 1) + suffix[what]; }

struct VoiceSet { float on, pitch, fine, formant, delay, pan, level; };
struct Preset { const char* name; VoiceSet v[numVoices]; float dry; };
const Preset presets[] {
    { "Doblaje",          { { 1, 0, -8, 0, 12, -70, -3 },  { 1, 0, 8, 0, 18, 70, -3 },    { 0, -12, 0, 0, 0, 0, -6 },    { 0, 7, 0, 0, 25, 30, -9 } },   0 },
    { "Ensanche estéreo", { { 1, 0, -12, 0, 8, -100, -4 }, { 1, 0, 12, 0, 14, 100, -4 },  { 1, 0, -5, 0, 25, -50, -8 },  { 1, 0, 5, 0, 31, 50, -8 } },   0 },
    { "Octava abajo",     { { 1, -12, 0, 0, 0, 0, -3 },    { 0, 0, 8, 0, 18, 70, -3 },    { 0, -12, 0, 0, 0, 0, -6 },    { 0, 7, 0, 0, 25, 30, -9 } },   0 },
    { "Tercera y quinta", { { 1, 4, 0, 0, 10, -50, -5 },   { 1, 7, 0, 0, 15, 50, -6 },    { 0, -12, 0, 0, 0, 0, -6 },    { 0, 12, 0, 0, 25, 30, -9 } },  0 },
    { "Coro",             { { 1, 0, -10, 0, 15, -80, -4 }, { 1, 0, 10, 0, 22, 80, -4 },   { 1, -12, 0, 0, 30, -30, -8 }, { 1, 12, 0, 0, 26, 30, -10 } }, 0 },
    { "Criatura",         { { 1, -7, 0, -5, 0, -20, -2 },  { 1, -12, 0, -8, 9, 20, -3 },  { 1, -19, 0, -3, 20, 0, -8 },  { 0, 7, 0, 0, 25, 30, -9 } },   -12 },
};
constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));

juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace mye;
    Layout l;
    juce::StringArray names;
    for (auto& pr : presets) names.add (utf8 (pr.name));
    addChoice (l, "preset", "Preset", names, 0);
    for (int v = 0; v < numVoices; ++v)
    {
        const auto& d = presets[0].v[v];
        const juce::String n = "Voz " + juce::String (v + 1) + " ";
        addBool  (l, vid (v, vOn).toRawUTF8(), n + "activa", d.on > 0.5f);
        addFloat (l, vid (v, vPitch).toRawUTF8(), n + "tono", juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), d.pitch, "st");
        addFloat (l, vid (v, vFine).toRawUTF8(), n + "fino", juce::NormalisableRange<float> (-50.0f, 50.0f, 1.0f), d.fine, "ct");
        addFloat (l, vid (v, vFormant).toRawUTF8(), n + "formante", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), d.formant, "st");
        addFloat (l, vid (v, vDelay).toRawUTF8(), n + "retardo", skewed (0.0f, 500.0f, 60.0f, 1.0f), d.delay, "ms");
        addFloat (l, vid (v, vPan).toRawUTF8(), n + "paneo", juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), d.pan);
        addFloat (l, vid (v, vLevel).toRawUTF8(), n + "nivel", juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f), d.level, "dB");
    }
    addFloat (l, "anim", utf8 ("Animación"), 0.0f, 100.0f, 30.0f, "%");
    addFloat (l, "dry", "Original", juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), 0.0f, "dB");
    addFloat (l, "wet", "Voces", juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f), -3.0f, "dB");
    addFloat (l, "output", "Salida", juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f, "dB");
    return { l.begin(), l.end() };
}
} // namespace

class LaringeProcessor : public mye::ProcessorBase
{
public:
    LaringeProcessor() : ProcessorBase (layout())
    {
        for (int v = 0; v < numVoices; ++v)
            for (int k = 0; k < vNumParams; ++k)
                vp[v][k] = apvts.getRawParameterValue (vid (v, k));

        presetActions["preset"] = [this] (int i)
        {
            const auto& pr = presets[juce::jlimit (0, numPresets - 1, i)];
            for (int v = 0; v < numVoices; ++v)
            {
                const float vals[] { pr.v[v].on, pr.v[v].pitch, pr.v[v].fine, pr.v[v].formant, pr.v[v].delay, pr.v[v].pan, pr.v[v].level };
                for (int k = 0; k < vNumParams; ++k) setParam (vid (v, k).toRawUTF8(), vals[k]);
            }
            setParam ("dry", pr.dry);
        };
    }
    const juce::String getName() const override { return "MYE Laringe"; }
    double getTailLengthSeconds() const override { return 0.6; }

    void prepareToPlay (double sampleRate, int) override
    {
        sr = sampleRate;
        latency = (int) std::ceil (1.5 * sr / minHz) + 8;
        setLatencySamples (latency);
        const int size = juce::nextPowerOfTwo ((int) (sr * 0.75) + latency + 8192);
        mask = size - 1;
        mono.assign ((size_t) size, 0.0f); dryL.assign ((size_t) size, 0.0f); dryR.assign ((size_t) size, 0.0f);
        wp = 0;
        decim = juce::jmax (1, juce::roundToInt (sr / 12000.0));
        decAcc = 0.0f; decCount = 0; decPos = 0; hopCount = 0;
        std::fill (std::begin (decBuf), std::end (decBuf), 0.0f);
        period = sr / 150.0;
        for (auto& vc : voices) vc = Voice();
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals nd;
        spreadMonoInput (buffer);
        const int n = buffer.getNumSamples(), nch = juce::jmin (2, buffer.getNumChannels());
        if (nch == 0 || mono.empty()) return;

        // ---- per-block voice settings
        const float anim = p ("anim") / 100.0f;
        const float dryDb = p ("dry"), dryG = dryDb <= -59.5f ? 0.0f : mye::dbToGain (dryDb);
        const float wetG = mye::dbToGain (p ("wet")), outG = mye::dbToGain (p ("output"));
        const float driftCoef = 1.0f - std::exp (-(float) n / (0.5f * (float) sr));
        struct Set { bool on; double ratio; float formant, delay, gL, gR; } set[numVoices];
        for (int v = 0; v < numVoices; ++v)
        {
            auto& vc = voices[v];
            vc.driftCount -= n;                              // slow random detune, different on every voice
            if (vc.driftCount <= 0) { vc.driftTarget = rng.nextFloat() * 2.0f - 1.0f; vc.driftCount = (int) (sr * (0.4 + 0.9 * rng.nextFloat())); }
            vc.drift += driftCoef * (vc.driftTarget - vc.drift);

            const float cents = vp[v][vFine]->load() + vc.drift * anim * 14.0f;
            const float a = juce::MathConstants<float>::pi * 0.25f * (1.0f + vp[v][vPan]->load() / 100.0f);
            const float lv = mye::dbToGain (vp[v][vLevel]->load());
            set[v] = { vp[v][vOn]->load() > 0.5f,
                       std::pow (2.0, (vp[v][vPitch]->load() + cents / 100.0f) / 12.0),
                       std::pow (2.0f, vp[v][vFormant]->load() / 12.0f),
                       vp[v][vDelay]->load() * 0.001f * (float) sr,
                       lv * std::cos (a), lv * std::sin (a) };
        }

        float* io[2] { buffer.getWritePointer (0), buffer.getWritePointer (nch > 1 ? 1 : 0) };
        for (int i = 0; i < n; ++i)
        {
            const float l = io[0][i], r = io[1][i];
            const int idx = (int) (wp & mask);
            dryL[(size_t) idx] = l; dryR[(size_t) idx] = r; mono[(size_t) idx] = 0.5f * (l + r);

            // pitch detector runs on a decimated copy (~12 kHz), every ~5 ms
            decAcc += mono[(size_t) idx];
            if (++decCount >= decim)
            {
                decBuf[decPos++ & (decSize - 1)] = decAcc / (float) decim;
                decAcc = 0.0f; decCount = 0;
                if (++hopCount >= 64) { hopCount = 0; detectPitch(); }
            }

            float wL = 0.0f, wR = 0.0f;
            for (int v = 0; v < numVoices; ++v)
            {
                auto& vc = voices[v];
                if (set[v].on)
                {
                    vc.countdown -= 1.0;
                    if (vc.countdown <= 0.0)
                    {
                        spawn (vc, set[v].ratio, set[v].formant, set[v].delay, set[v].gL, set[v].gR);
                        vc.countdown += juce::jmax (8.0, period / set[v].ratio);
                    }
                }
                else vc.lastCentre = -1.0;

                for (auto& g : vc.grains)
                {
                    if (! g.active) continue;
                    const double pos = g.centre + (double) g.j * g.step;
                    const auto i0 = (juce::int64) std::floor (pos);
                    const float fr = (float) (pos - (double) i0);
                    const float s0 = mono[(size_t) (i0 & mask)], s1 = mono[(size_t) ((i0 + 1) & mask)];
                    const float w = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * g.j / g.half));
                    const float s = (s0 + fr * (s1 - s0)) * w;
                    wL += s * g.gL; wR += s * g.gR;
                    g.j += 1.0f;
                    if (g.j >= g.half) g.active = false;
                }
            }

            const int di = (int) ((wp - latency) & mask);   // dry is delayed so it lines up with the voices
            const float oL = (dryL[(size_t) di] * dryG + wL * wetG) * outG, oR = (dryR[(size_t) di] * dryG + wR * wetG) * outG;
            if (nch > 1) { io[0][i] = oL; io[1][i] = oR; }
            else io[0][i] = 0.5f * (oL + oR);
            ++wp;
        }
    }

    juce::AudioProcessorEditor* createEditor() override
    {
        using K = mye::SectionEditor::Kind;
        std::vector<mye::SectionEditor::Section> secs;
        for (int v = 0; v < numVoices; ++v)
            secs.push_back ({ "VOZ " + juce::String (v + 1),
                              { { vid (v, vPitch), "Tono", K::knob }, { vid (v, vFine), "Fino", K::knob },
                                { vid (v, vFormant), "Formante", K::knob }, { vid (v, vDelay), "Retardo", K::knob },
                                { vid (v, vPan), "Paneo", K::knob }, { vid (v, vLevel), "Nivel", K::knob },
                                { vid (v, vOn), "Activa", K::toggle } }, 0, 2 });
        secs.push_back ({ "GENERAL", { { "anim", mye::utf8 ("Animación"), K::knob }, { "dry", "Original", K::knob },
                                       { "wet", "Voces", K::knob }, { "output", "Salida", K::knob },
                                       { "preset", "", K::choice } }, 0, 2 });
        auto* ed = new mye::SectionEditor (*this, "LARINGE", mye::utf8 ("M.Y.E. · harmonizador · donde nace el tono"), std::move (secs));
        ed->setBaseHeight (470);
        return ed;
    }

private:
    struct Grain { bool active = false; double centre = 0, step = 1; float j = 0, half = 1, gL = 0, gR = 0; };
    struct Voice
    {
        Grain grains[maxGrains];
        double countdown = 0.0, lastCentre = -1.0;
        float drift = 0.0f, driftTarget = 0.0f;
        int driftCount = 0;
    };

    // one grain = two periods of the source under a Hann window, centred a whole
    // number of periods after the previous one so the waveform stays in phase
    void spawn (Voice& vc, double ratio, float formant, float delay, float gL, float gR)
    {
        const double target = (double) wp - latency - delay;
        double c = target;
        if (vc.lastCentre >= 0.0 && std::abs (target - vc.lastCentre) < 8.0 * period)
            c = vc.lastCentre + std::round ((target - vc.lastCentre) / period) * period;
        vc.lastCentre = c;

        for (auto& g : vc.grains)
        {
            if (g.active) continue;
            g.active = true;
            g.centre = c;
            g.step = formant;                                 // reading faster inside the grain raises the formants
            g.half = (float) (period / formant);
            g.j = -g.half;
            const float norm = juce::jlimit (0.3f, 1.0f, std::sqrt ((float) (formant / ratio)));   // keeps loudness even across shifts
            g.gL = gL * norm; g.gR = gR * norm;
            return;
        }
    }

    // McLeod-style normalised autocorrelation on the last ~43 ms
    void detectPitch()
    {
        constexpr int W = 512;
        float x[W]; float mean = 0.0f;
        for (int i = 0; i < W; ++i) { x[i] = decBuf[(decPos - W + i) & (decSize - 1)]; mean += x[i]; }
        mean /= (float) W;
        float energy = 0.0f;
        for (auto& s : x) { s -= mean; energy += s * s; }
        if (energy / (float) W < 1.0e-7f) return;             // silence: keep the last period

        const double srd = sr / decim;
        const int tauMin = juce::jmax (2, (int) (srd / maxHz)), tauMax = juce::jmin (W / 2, (int) (srd / minHz) + 1);
        float nsdf[W / 2 + 2] {};
        for (int tau = tauMin - 1; tau <= tauMax + 1; ++tau)
        {
            float acf = 0.0f, m = 0.0f;
            for (int i = 0; i < W - tau; ++i) { acf += x[i] * x[i + tau]; m += x[i] * x[i] + x[i + tau] * x[i + tau]; }
            nsdf[tau] = m > 0.0f ? 2.0f * acf / m : 0.0f;
        }
        int start = tauMin;                                   // skip the lobe around lag 0
        while (start < tauMax && nsdf[start] > 0.0f) ++start;
        float best = 0.0f;
        for (int tau = start; tau <= tauMax; ++tau)
            if (nsdf[tau] > nsdf[tau - 1] && nsdf[tau] >= nsdf[tau + 1]) best = juce::jmax (best, nsdf[tau]);
        if (best < 0.5f) return;                              // unvoiced / noise: keep the last period
        for (int tau = start; tau <= tauMax; ++tau)
            if (nsdf[tau] > nsdf[tau - 1] && nsdf[tau] >= nsdf[tau + 1] && nsdf[tau] >= 0.9f * best)
            {
                const float a = nsdf[tau - 1], b = nsdf[tau], c = nsdf[tau + 1], den = a - 2.0f * b + c;
                const double tf = tau + (std::abs (den) > 1.0e-9f ? 0.5 * (a - c) / den : 0.0);
                const double np = juce::jlimit (sr / maxHz, sr / minHz, tf * decim);
                period = std::abs (np - period) / period > 0.15 ? np : period + 0.4 * (np - period);
                return;
            }
    }

    static constexpr int decSize = 1024;
    double sr = 44100.0, period = 300.0;
    int latency = 0, mask = 0, decim = 4, decCount = 0, decPos = 0, hopCount = 0;
    juce::int64 wp = 0;
    float decAcc = 0.0f, decBuf[decSize] {};
    std::vector<float> mono, dryL, dryR;
    Voice voices[numVoices];
    std::atomic<float>* vp[numVoices][vNumParams] {};
    juce::Random rng;
};

juce::AudioProcessor* createLaringe() { return new LaringeProcessor(); }
#ifndef MYE_TEST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LaringeProcessor(); }
#endif
