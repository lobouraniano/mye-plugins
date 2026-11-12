// MYE Tímpano — listening points: everything we hear passes through a membrane.
// The sound as heard *from* somewhere: through an object, through a device, or
// from inside the body. Each scene = transmission filter + object resonance +
// the space on the other side + the defect (distortion / compression / noise).
#include "MyeCommon.h"

namespace
{
enum Group { through, device, body };
enum Dist { none, soft, hard, crush };
enum Noise { quiet, hiss, hum, crackle };

struct Scene
{
    const char* name; Group group;
    float hp, lp; bool steep;
    float p1f, p1g, p1q, p2f, p2g, p2q;
    float mono; Dist dist; float distAmt, comp;
    float roomSize, roomAmt; Noise noise; float noiseAmt;
    float levelDb, tinnitus; bool wobble;
};

const Scene scenes[] = {
    // ---- a través de algo
    { "Puerta",               through, 40,   500,  true,  180, 5, 1.2f, 0, 0, 1,     0.3f, none, 0,    0,    0.30f, 0.35f, quiet, 0,    -6,  0,    false },
    { "Pared",                through, 30,   280,  true,  110, 4, 1.0f, 0, 0, 1,     0.5f, none, 0,    0,    0.40f, 0.30f, quiet, 0,    -10, 0,    false },
    { "Ventana cerrada",      through, 150,  1500, false, 2500,3, 2.0f, 0, 0, 1,     0.2f, none, 0,    0,    0.70f, 0.25f, quiet, 0,    -6,  0,    false },
    { "Otra habitación",      through, 60,   3500, false, 300, 2, 1.0f, 0, 0, 1,     0.4f, none, 0,    0,    0.60f, 0.60f, quiet, 0,    -8,  0,    false },
    { "Auto desde afuera",    through, 30,   380,  true,  90,  7, 1.5f, 0, 0, 1,     0.6f, none, 0,    0,    0.10f, 0.10f, quiet, 0,    -8,  0,    false },
    { "Bajo el agua",         through, 40,   420,  true,  250, 6, 1.0f, 0, 0, 1,     0.5f, none, 0,    0,    0.80f, 0.50f, quiet, 0,    -6,  0,    true  },
    // ---- por un aparato
    { "Parlante de celular",  device,  600,  7000, true,  1000,5, 1.5f, 2800,4, 2.0f, 1.0f, soft, 0.5f, 0.3f, 0.00f, 0.00f, quiet, 0,    -3,  0,    false },
    { "Llamada telefónica",   device,  300,  3400, true,  1500,3, 1.0f, 0, 0, 1,     1.0f, crush,0.5f, 0.7f, 0.00f, 0.00f, hiss,  0.3f, -3,  0,    false },
    { "Radio AM",             device,  150,  4500, true,  1200,3, 1.0f, 0, 0, 1,     1.0f, soft, 0.4f, 0.6f, 0.00f, 0.00f, crackle,0.6f,-4, 0,    false },
    { "Walkie-talkie",        device,  400,  2800, true,  1800,5, 1.5f, 0, 0, 1,     1.0f, hard, 0.7f, 0.9f, 0.00f, 0.00f, crackle,0.7f,-2, 0,    false },
    { "Megáfono",             device,  500,  3800, true,  1500,8, 2.0f, 3000,4, 2.0f, 1.0f, hard, 0.6f, 0.5f, 0.80f, 0.35f, quiet, 0,    -2,  0,    false },
    { "TV de al lado",        device,  120,  1800, true,  500, 4, 1.0f, 2500,2, 1.5f, 1.0f, soft, 0.2f, 0.5f, 0.40f, 0.45f, hum,   0.3f, -10, 0,    false },
    { "Auriculares ajenos",   device,  1500, 9000, false, 3000,6, 1.5f, 0, 0, 1,     0.7f, soft, 0.3f, 0.3f, 0.00f, 0.00f, quiet, 0,    -16, 0,    false },
    // ---- en el cuerpo
    { "Aturdido",             body,    20,   700,  true,  0,   0, 1.0f, 0, 0, 1,     0.3f, none, 0,    0,    0.30f, 0.20f, quiet, 0,    -6,  0.6f, false },
    { "Oído tapado",          body,    20,   1200, false, 200, 6, 0.8f, 0, 0, 1,     0.2f, none, 0,    0,    0.00f, 0.00f, quiet, 0,    -3,  0,    false },
    { "Adentro de la cabeza", body,    20,   1800, false, 150, 8, 0.7f, 0, 0, 1,     1.0f, none, 0,    0,    0.00f, 0.00f, quiet, 0,    0,   0,    false },
};
constexpr int numScenes = (int) (sizeof (scenes) / sizeof (scenes[0]));

juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace mye;
    Layout l;
    juce::StringArray names;
    for (auto& s : scenes) names.add (utf8 (s.name));
    addChoice(l, "scene", "Escena", names, 0);
    addFloat (l, "transition", utf8 ("Transición"), 0.0f, 100.0f, 100.0f, "%");
    addFloat (l, "distance", "Distancia", 0.0f, 100.0f, 30.0f, "%");
    addFloat (l, "material", "Material / calidad", 0.0f, 100.0f, 50.0f, "%");
    addFloat (l, "space", "Espacio", 0.0f, 100.0f, 50.0f, "%");
    addFloat (l, "defect", "Defecto", 0.0f, 100.0f, 50.0f, "%");
    addFloat (l, "tinnitus", utf8 ("Acúfeno"), 0.0f, 100.0f, 0.0f, "%");
    addFloat (l, "pulse", "Pulso", 0.0f, 100.0f, 0.0f, "%");
    addFloat (l, "pulsebpm", "Pulso bpm", 40.0f, 160.0f, 68.0f, "bpm");
    addFloat (l, "output", "Salida", -24.0f, 12.0f, 0.0f, "dB");
    return { l.begin(), l.end() };
}

using Filter = juce::dsp::IIR::Filter<float>;
using Coefs = juce::dsp::IIR::Coefficients<float>;
float lerp (float a, float b, float t) { return a + (b - a) * t; }
} // namespace

class TimpanoProcessor : public mye::ProcessorBase
{
public:
    TimpanoProcessor() : ProcessorBase (layout()) {}
    const juce::String getName() const override { return "MYE Timpano"; }
    double getTailLengthSeconds() const override { return 3.0; }

    void prepareToPlay (double sampleRate, int block) override
    {
        sr = sampleRate;
        for (int c = 0; c < 2; ++c)
            for (auto* f : { &hp1[c], &hp2[c], &lp1[c], &lp2[c], &pk1[c], &pk2[c] })
            {
                f->coefficients = Coefs::makeLowPass (sr, 1000.0f);
                f->reset();
            }
        reverb.setSampleRate (sr);
        reverb.reset();
        wet.setSize (2, block);
        room.setSize (2, block);
        transSm.reset (sr, 0.08);
        env = 0.0f; holdCount = 0.0f; holdVal[0] = holdVal[1] = 0.0f;
        wobPh = tinPh = tinPh2 = beatPh = 0.0;
        lastScene = -1;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals nd;
        spreadMonoInput (buffer);
        const int n = buffer.getNumSamples(), nch = juce::jmin (2, buffer.getNumChannels());
        if (nch == 0) return;
        if (wet.getNumSamples() < n) { wet.setSize (2, n, false, false, true); room.setSize (2, n, false, false, true); }

        const int si = juce::jlimit (0, numScenes - 1, (int) p ("scene"));
        const Scene& s = scenes[si];
        if (si != lastScene) { reverb.reset(); lastScene = si; }
        const float dist = p ("distance") / 100.0f, mat = p ("material") / 100.0f;
        const float space = p ("space") / 100.0f, defect = p ("defect") / 100.0f;
        transSm.setTargetValue (p ("transition") / 100.0f);

        // ---- map the shared controls onto the scene
        float hpF = s.hp, lpF = s.lp, resMul = 1.0f, distAmt = s.distAmt, levelDb = s.levelDb;
        if (s.group == through) { lpF *= lerp (2.0f, 0.5f, mat); resMul = lerp (1.5f, 0.7f, mat); levelDb -= 6.0f * mat; }
        else if (s.group == device) { hpF *= lerp (1.5f, 0.6f, mat); lpF *= lerp (0.6f, 1.5f, mat); distAmt *= lerp (1.5f, 0.4f, mat); }
        else lpF *= lerp (1.6f, 0.5f, mat);
        lpF *= 1.0f - 0.6f * dist;                         // air absorbs highs with distance
        levelDb -= 12.0f * dist;
        distAmt = juce::jlimit (0.0f, 1.0f, distAmt * 2.0f * defect);
        const float noiseAmt = s.noiseAmt * 2.0f * defect;
        const float roomAmt = juce::jlimit (0.0f, 1.0f, (s.roomAmt + 0.4f * dist * (s.roomSize > 0.0f ? 1.0f : 0.5f)) * 2.0f * space);

        if (s.wobble)                                      // underwater: the surface moves
        {
            wobPh = std::fmod (wobPh + (double) n * 0.23 / sr, 1.0);
            lpF *= 1.0f + 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * wobPh);
        }
        const float nyq = (float) sr * 0.45f;
        hpF = juce::jlimit (10.0f, nyq, hpF);
        lpF = juce::jlimit (60.0f, nyq, lpF);

        auto hpC = Coefs::makeHighPass (sr, hpF, 0.707f);
        auto lpC = Coefs::makeLowPass (sr, lpF, 0.707f);
        auto p1 = Coefs::makePeakFilter (sr, juce::jlimit (20.0f, nyq, s.p1f > 0 ? s.p1f : 1000.0f), s.p1q, mye::dbToGain (s.p1g * resMul));
        auto p2 = Coefs::makePeakFilter (sr, juce::jlimit (20.0f, nyq, s.p2f > 0 ? s.p2f : 1000.0f), s.p2q, mye::dbToGain (s.p2g * resMul));
        for (int c = 0; c < 2; ++c)
        {
            *hp1[c].coefficients = *hpC; *hp2[c].coefficients = *hpC;
            *lp1[c].coefficients = *lpC; *lp2[c].coefficients = *lpC;
            *pk1[c].coefficients = *p1;  *pk2[c].coefficients = *p2;
        }

        // ---- wet chain
        for (int c = 0; c < nch; ++c) wet.copyFrom (c, 0, buffer, c, 0, n);
        const float level = mye::dbToGain (levelDb);
        const float ratio = 1.0f + 9.0f * s.comp;
        const float att = 1.0f - std::exp (-1.0f / (0.002f * (float) sr)), rel = 1.0f - std::exp (-1.0f / (0.08f * (float) sr));
        const float holdLen = s.dist == crush ? 1.0f + distAmt * ((float) sr / 4000.0f - 1.0f) : 1.0f;
        const float q = std::pow (2.0f, 12.0f - 6.0f * distAmt);

        for (int i = 0; i < n; ++i)
        {
            float x[2];
            const float l0 = wet.getSample (0, i), r0 = nch > 1 ? wet.getSample (1, i) : l0;
            const float mid = 0.5f * (l0 + r0);
            x[0] = lerp (l0, mid, s.mono); x[1] = lerp (r0, mid, s.mono);

            for (int c = 0; c < nch; ++c)
            {
                float y = hp1[c].processSample (x[c]);
                if (s.steep) y = hp2[c].processSample (y);
                y = lp1[c].processSample (y);
                if (s.steep) y = lp2[c].processSample (y);
                if (s.p1g != 0.0f) y = pk1[c].processSample (y);
                if (s.p2g != 0.0f) y = pk2[c].processSample (y);
                x[c] = y;
            }

            // defect: distortion
            if (s.dist != none && distAmt > 0.0f)
            {
                const float g = 1.0f + 8.0f * distAmt;
                for (int c = 0; c < nch; ++c)
                {
                    if (s.dist == soft) x[c] = std::tanh (x[c] * g) / std::tanh (g) ;
                    else if (s.dist == hard) x[c] = juce::jlimit (-1.0f, 1.0f, x[c] * g) / juce::jmin (g, 3.0f);
                }
                if (s.dist == crush)
                {
                    holdCount += 1.0f;
                    if (holdCount >= holdLen) { holdCount -= holdLen; for (int c = 0; c < nch; ++c) holdVal[c] = std::round (x[c] * q) / q; }
                    for (int c = 0; c < nch; ++c) x[c] = holdVal[c];
                }
            }

            // defect: compression (devices squash everything)
            if (s.comp > 0.0f)
            {
                const float lv = juce::jmax (std::abs (x[0]), std::abs (x[nch - 1]));
                env += (lv > env ? att : rel) * (lv - env);
                const float envDb = juce::Decibels::gainToDecibels (env, -100.0f);
                const float over = envDb + 20.0f;
                const float gr = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
                const float g = mye::dbToGain (-gr + 6.0f * s.comp);
                for (int c = 0; c < nch; ++c) x[c] *= g;
            }

            for (int c = 0; c < nch; ++c) wet.setSample (c, i, x[c] * level);
        }

        // space on the other side
        if (roomAmt > 0.001f && s.roomSize > 0.0f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = s.roomSize; rp.damping = 0.6f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 0.8f;
            reverb.setParameters (rp);
            for (int c = 0; c < nch; ++c) room.copyFrom (c, 0, wet, c, 0, n);
            if (nch > 1) reverb.processStereo (room.getWritePointer (0), room.getWritePointer (1), n);
            else reverb.processMono (room.getWritePointer (0), n);
            for (int c = 0; c < nch; ++c) wet.addFrom (c, 0, room, c, 0, n, roomAmt * 0.6f);
        }

        // noise of the medium + body sounds
        const float tin = juce::jlimit (0.0f, 1.0f, p ("tinnitus") / 100.0f + s.tinnitus * transSm.getTargetValue());
        const float pulse = p ("pulse") / 100.0f;
        const double beatInc = p ("pulsebpm") / 60.0 / sr;
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        for (int i = 0; i < n; ++i)
        {
            float add = 0.0f;
            if (noiseAmt > 0.0f)
            {
                if (s.noise == hiss) add += noiseAmt * 0.004f * (rng.nextFloat() * 2.0f - 1.0f);
                else if (s.noise == hum) { humPh = std::fmod (humPh + 50.0 / sr, 1.0); add += noiseAmt * 0.006f * (float) (std::sin (twoPi * humPh) + 0.5 * std::sin (3 * twoPi * humPh)); }
                else if (s.noise == crackle)
                {
                    add += noiseAmt * 0.005f * (rng.nextFloat() * 2.0f - 1.0f);
                    if (rng.nextFloat() < 0.0004f * noiseAmt) crackleEnv = 0.25f * noiseAmt;
                    add += crackleEnv * (rng.nextFloat() * 2.0f - 1.0f);
                    crackleEnv *= 0.995f;
                }
            }
            if (tin > 0.0f)
            {
                tinPh = std::fmod (tinPh + 6200.0 / sr, 1.0); tinPh2 = std::fmod (tinPh2 + 6207.0 / sr, 1.0);
                add += tin * 0.03f * (float) (std::sin (twoPi * tinPh) + 0.6 * std::sin (twoPi * tinPh2));
            }
            if (pulse > 0.0f)
            {
                beatPh = std::fmod (beatPh + beatInc, 1.0);
                add += pulse * heartbeat (beatPh, 60.0 / p ("pulsebpm"));
            }
            for (int c = 0; c < nch; ++c) wet.setSample (c, i, wet.getSample (c, i) + add);
        }

        // ---- transition: normal listening -> listening point (equal power)
        for (int i = 0; i < n; ++i)
        {
            const float t = transSm.getNextValue();
            const float gd = std::cos (t * juce::MathConstants<float>::halfPi), gw = std::sin (t * juce::MathConstants<float>::halfPi);
            for (int c = 0; c < nch; ++c)
                buffer.setSample (c, i, (buffer.getSample (c, i) * gd + wet.getSample (c, i) * gw) * mye::dbToGain (p ("output")));
        }
    }

    juce::AudioProcessorEditor* createEditor() override
    {
        using K = mye::SectionEditor::Kind;
        return new mye::SectionEditor (*this, mye::utf8 ("TÍMPANO"), mye::utf8 ("M.Y.E. · puntos de escucha"),
        {
            { "ESCENA", { { "transition", mye::utf8 ("Transición"), K::knob }, { "scene", "", K::choice } }, 2 },
            { "LUGAR",  { { "distance", "Distancia", K::knob }, { "material", "Material / calidad", K::knob },
                          { "space", "Espacio", K::knob }, { "defect", "Defecto", K::knob } } },
            { "CUERPO", { { "tinnitus", mye::utf8 ("Acúfeno"), K::knob }, { "pulse", "Pulso", K::knob }, { "pulsebpm", "Pulso bpm", K::knob } } },
            { "SALIDA", { { "output", "Salida", K::knob } } },
        });
    }

private:
    // "lub-dub": two low decaying thumps per beat
    static float heartbeat (double ph, double beatSec)
    {
        auto thump = [] (double t, double f, double decay) -> float
        {
            if (t < 0.0) return 0.0f;
            return (float) (std::sin (juce::MathConstants<double>::twoPi * f * t) * std::exp (-t / decay));
        };
        const double t = ph * beatSec;
        return 0.5f * thump (t, 55.0, 0.06) + 0.35f * thump (t - 0.28, 45.0, 0.07);
    }

    double sr = 44100.0;
    Filter hp1[2], hp2[2], lp1[2], lp2[2], pk1[2], pk2[2];
    juce::Reverb reverb;
    juce::AudioBuffer<float> wet, room;
    juce::SmoothedValue<float> transSm;
    juce::Random rng;
    float env = 0, holdCount = 0, holdVal[2] {}, crackleEnv = 0;
    double wobPh = 0, tinPh = 0, tinPh2 = 0, beatPh = 0, humPh = 0;
    int lastScene = -1;
};

juce::AudioProcessor* createTimpano() { return new TimpanoProcessor(); }
#ifndef MYE_TEST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TimpanoProcessor(); }
#endif
