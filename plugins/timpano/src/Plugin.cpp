// MYE Tímpano — listening points: everything we hear passes through a membrane.
// The sound as heard *from* somewhere: through an object, through a device, or
// from inside the body. Each scene = transmission filter + object resonance +
// the space on the other side + the defect (distortion / compression / noise).
#include "MyeCommon.h"
#include <complex>

namespace
{
enum Group { through, device, body };
enum Dist { none, soft, hard, crush };
enum Noise { quiet, hiss, hum, radio, walkie, phone };

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
    { "Llamada telefónica",   device,  300,  3400, true,  1500,3, 1.0f, 0, 0, 1,     1.0f, crush,0.5f, 0.7f, 0.00f, 0.00f, phone, 0.5f, -3,  0,    false },
    { "Radio AM",             device,  150,  4500, true,  1200,3, 1.0f, 0, 0, 1,     1.0f, soft, 0.4f, 0.6f, 0.00f, 0.00f, radio, 0.6f, -4, 0,    false },
    { "Walkie-talkie",        device,  400,  2800, true,  1800,5, 1.5f, 0, 0, 1,     1.0f, hard, 0.7f, 0.9f, 0.00f, 0.00f, walkie,0.7f, -2, 0,    false },
    { "Megáfono",             device,  500,  3800, true,  1500,8, 2.0f, 3000,4, 2.0f, 1.0f, hard, 0.6f, 0.5f, 0.80f, 0.35f, quiet, 0,    -2,  0,    false },
    { "TV de al lado",        device,  120,  1800, true,  500, 4, 1.0f, 2500,2, 1.5f, 1.0f, soft, 0.2f, 0.5f, 0.40f, 0.45f, hum,   0.5f, -10, 0,    false },
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
    addFloat (l, "lowcut", "Corte grave", skewed (20.0f, 2000.0f, 200.0f, 1.0f), 40.0f, "Hz");
    addFloat (l, "highcut", "Corte agudo", skewed (200.0f, 20000.0f, 2000.0f, 1.0f), 500.0f, "Hz");
    addFloat (l, "resfreq", "Resonancia", skewed (60.0f, 6000.0f, 600.0f, 1.0f), 180.0f, "Hz");
    addFloat (l, "resgain", "Realce", -12.0f, 18.0f, 5.0f, "dB");
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

float lerp (float a, float b, float t) { return a + (b - a) * t; }

// allocation-free biquad (RBJ cookbook) with two channels of state, so the
// coefficients can be moved many times per block while Transition sweeps
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {};

    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }
    void copyCoefs (const Biquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }

    float process (int c, float x)
    {
        const float y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }
    void highPass (double sr, float f, float q)
    {
        const double w = juce::MathConstants<double>::twoPi * f / sr, cs = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
        b0 = (float) ((1.0 + cs) * 0.5 / a0); b1 = (float) (-(1.0 + cs) / a0); b2 = b0;
        a1 = (float) (-2.0 * cs / a0); a2 = (float) ((1.0 - al) / a0);
    }
    void lowPass (double sr, float f, float q)
    {
        const double w = juce::MathConstants<double>::twoPi * f / sr, cs = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
        b0 = (float) ((1.0 - cs) * 0.5 / a0); b1 = (float) ((1.0 - cs) / a0); b2 = b0;
        a1 = (float) (-2.0 * cs / a0); a2 = (float) ((1.0 - al) / a0);
    }
    void peak (double sr, float f, float q, float gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = juce::MathConstants<double>::twoPi * f / sr, cs = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al / A;
        b0 = (float) ((1.0 + al * A) / a0); b1 = (float) (-2.0 * cs / a0); b2 = (float) ((1.0 - al * A) / a0);
        a1 = b1; a2 = (float) ((1.0 - al / A) / a0);
    }
    float magnitudeDb (double sr, double f) const
    {
        const double w = juce::MathConstants<double>::twoPi * f / sr;
        const std::complex<double> u1 = std::polar (1.0, -w), u2 = std::polar (1.0, -2.0 * w);
        const auto h = ((double) b0 + (double) b1 * u1 + (double) b2 * u2) / (1.0 + (double) a1 * u1 + (double) a2 * u2);
        return (float) (20.0 * std::log10 (juce::jmax (1.0e-6, std::abs (h))));
    }
};

// what the transmission is doing right now (read by the curve display)
struct CurveState
{
    std::atomic<float> hp { 40 }, lp { 500 }, p1f { 180 }, p1g { 0 }, p1q { 1 }, p2f { 1000 }, p2g { 0 }, p2q { 1 }, levelDb { 0 }, sr { 48000 };
    std::atomic<bool> steep { true };
};
} // namespace

class TimpanoProcessor : public mye::ProcessorBase
{
public:
    TimpanoProcessor() : ProcessorBase (layout())
    {
        // picking a scene moves every knob to that scene's starting point
        presetActions["scene"] = [this] (int sc)
        {
            struct P { float distance, material, space, defect, tinnitus, pulse; };
            const P presets[] {
                { 20, 40, 50, 50, 0, 0 },  { 30, 70, 50, 50, 0, 0 },  { 40, 50, 60, 50, 0, 0 },  { 40, 50, 60, 50, 0, 0 },
                { 30, 60, 30, 50, 0, 0 },  { 30, 50, 60, 50, 0, 15 }, { 10, 40, 0, 50, 0, 0 },   { 0, 40, 0, 50, 0, 0 },
                { 0, 35, 0, 60, 0, 0 },    { 0, 30, 0, 60, 0, 0 },    { 40, 40, 60, 60, 0, 0 },  { 40, 50, 50, 50, 0, 0 },
                { 20, 50, 0, 40, 0, 0 },   { 20, 60, 40, 30, 60, 30 },{ 0, 50, 0, 0, 0, 20 },    { 0, 50, 0, 0, 0, 40 } };
            sc = juce::jlimit (0, numScenes - 1, sc);
            const auto& pr = presets[sc];
            const auto& s = scenes[sc];
            setParam ("lowcut", s.hp); setParam ("highcut", s.lp);
            setParam ("resfreq", s.p1f > 0.0f ? s.p1f : 1000.0f); setParam ("resgain", s.p1g);
            setParam ("distance", pr.distance); setParam ("material", pr.material); setParam ("space", pr.space);
            setParam ("defect", pr.defect); setParam ("tinnitus", pr.tinnitus); setParam ("pulse", pr.pulse);
        };
    }
    const juce::String getName() const override { return "MYE Timpano"; }
    double getTailLengthSeconds() const override { return 3.0; }

    CurveState curve;

    void prepareToPlay (double sampleRate, int block) override
    {
        sr = sampleRate;
        for (auto* f : { &hpA, &hpB, &lpA, &lpB, &pkA, &pkB }) f->reset();
        reverb.setSampleRate (sr);
        reverb.reset();
        wet.setSize (2, block);
        room.setSize (2, block);
        tBuf.assign ((size_t) block, 0.0f);
        transSm.reset (sr, 0.08);
        transSm.setCurrentAndTargetValue (p ("transition") / 100.0f);
        env = 0.0f; holdCount = 0.0f; holdVal[0] = holdVal[1] = 0.0f; lastRoomGain = 0.0f;
        wobPh = tinPh = tinPh2 = beatPh = 0.0;
        lastScene = -1;
        curve.sr.store ((float) sr);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals nd;
        spreadMonoInput (buffer);
        const int n = buffer.getNumSamples(), nch = juce::jmin (2, buffer.getNumChannels());
        if (nch == 0) return;
        if (wet.getNumSamples() < n) { wet.setSize (2, n, false, false, true); room.setSize (2, n, false, false, true); }
        if ((int) tBuf.size() < n) tBuf.resize ((size_t) n);

        const int si = juce::jlimit (0, numScenes - 1, (int) p ("scene"));
        const Scene& s = scenes[si];
        if (si != lastScene) { reverb.reset(); lastScene = si; }
        const float dist = p ("distance") / 100.0f, mat = p ("material") / 100.0f;
        const float space = p ("space") / 100.0f, defect = p ("defect") / 100.0f;
        transSm.setTargetValue (p ("transition") / 100.0f);

        // ---- targets at Transition = 100 %: the frequency knobs, shaped by material and distance
        float hpT = p ("lowcut"), lpT = p ("highcut"), resMul = 1.0f, distAmt = s.distAmt, levelDb = s.levelDb;
        const float p1f = p ("resfreq"), p1gT = p ("resgain");
        if (s.group == through)     { resMul = lerp (1.8f, 0.5f, mat); lpT *= lerp (1.5f, 0.5f, mat); levelDb += lerp (3.0f, -9.0f, mat); }
        else if (s.group == device) { hpT *= lerp (1.6f, 0.6f, mat); lpT *= lerp (0.55f, 1.6f, mat); distAmt *= lerp (1.8f, 0.3f, mat); }
        else                          lpT *= lerp (1.6f, 0.5f, mat);
        lpT *= std::pow (0.2f, dist);                      // air and obstacles eat the highs with distance
        levelDb -= 24.0f * dist;
        distAmt = juce::jlimit (0.0f, 1.0f, distAmt * 2.0f * defect);
        const float noiseAmt = s.noiseAmt * 2.0f * defect;
        const float roomAmt = juce::jlimit (0.0f, 1.5f, (s.roomAmt + 0.5f * dist) * 2.0f * space);

        const float nyq = (float) sr * 0.45f, top = juce::jmin (20000.0f, nyq);
        hpT = juce::jlimit (10.0f, top * 0.5f, hpT);
        lpT = juce::jlimit (60.0f, top, lpT);
        const float att = 1.0f - std::exp (-1.0f / (0.002f * (float) sr)), rel = 1.0f - std::exp (-1.0f / (0.08f * (float) sr));
        float t = 0, hpE = hpT, lpE = lpT, g1 = 0, g2 = 0, levelNowDb = 0;

        // ---- wet chain, in small chunks: Transition moves the transmission itself
        // (the door closes), instead of crossfading two fixed signals
        for (int pos = 0; pos < n;)
        {
            const int len = juce::jmin (32, n - pos);
            t = transSm.getCurrentValue();

            float lpNow = lpT;
            if (s.wobble)                                  // underwater: the surface moves
            {
                wobPh = std::fmod (wobPh + (double) len * 0.23 / sr, 1.0);
                lpNow = juce::jmin (top, lpNow * (1.0f + 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * wobPh)));
            }
            hpE = 10.0f * std::pow (hpT / 10.0f, t);       // log sweep: perceptually even
            lpE = top * std::pow (lpNow / top, t);
            g1 = p1gT * resMul * t; g2 = s.p2g * resMul * t;
            levelNowDb = levelDb * t;
            hpA.highPass (sr, hpE, 0.707f); hpB.copyCoefs (hpA);
            lpA.lowPass (sr, lpE, 0.707f);  lpB.copyCoefs (lpA);
            pkA.peak (sr, juce::jlimit (20.0f, nyq, p1f), s.p1q, g1);
            pkB.peak (sr, juce::jlimit (20.0f, nyq, s.p2f > 0 ? s.p2f : 1000.0f), s.p2q, g2);

            const float level = mye::dbToGain (levelNowDb);
            const float monoE = s.mono * t, distE = distAmt * t, compE = s.comp * t, noiseE = noiseAmt * t;
            const float ratio = 1.0f + 9.0f * compE;
            const float holdLen = 1.0f + distE * ((float) sr / 4000.0f - 1.0f);
            const float q = std::pow (2.0f, 12.0f - 6.0f * distE);

            for (int i = pos; i < pos + len; ++i)
            {
                tBuf[(size_t) i] = transSm.getNextValue();
                float x[2];
                const float l0 = buffer.getSample (0, i), r0 = nch > 1 ? buffer.getSample (1, i) : l0;
                const float mid = 0.5f * (l0 + r0);
                x[0] = lerp (l0, mid, monoE); x[1] = lerp (r0, mid, monoE);

                if (s.noise != quiet && noiseE > 0.0f)     // noise enters *before* the transmission: the device colours it
                {
                    float sigGain = 1.0f;
                    const float nz = noiseSample (s.noise, noiseE, std::abs (mid), sigGain);
                    for (int c = 0; c < nch; ++c) x[c] = x[c] * sigGain + nz;
                }

                for (int c = 0; c < nch; ++c)
                {
                    float y = hpA.process (c, x[c]);
                    if (s.steep) y = hpB.process (c, y);
                    y = lpA.process (c, y);
                    if (s.steep) y = lpB.process (c, y);
                    y = pkA.process (c, y);
                    if (s.p2g != 0.0f) y = pkB.process (c, y);
                    x[c] = y;
                }

                if (s.dist != none && distE > 0.001f)      // defect: distortion
                {
                    const float g = 10.0f * distE;
                    for (int c = 0; c < nch; ++c)
                    {
                        if (s.dist == soft) x[c] = std::tanh (x[c] * g) / std::tanh (g);
                        else if (s.dist == hard) x[c] = juce::jlimit (-1.0f, 1.0f, x[c] * (1.0f + g)) / juce::jmin (1.0f + g, 3.0f);
                    }
                    if (s.dist == crush)
                    {
                        holdCount += 1.0f;
                        if (holdCount >= holdLen) { holdCount -= holdLen; for (int c = 0; c < nch; ++c) holdVal[c] = std::round (x[c] * q) / q; }
                        for (int c = 0; c < nch; ++c) x[c] = holdVal[c];
                    }
                }

                if (compE > 0.0f)                          // defect: devices squash everything
                {
                    const float lv = juce::jmax (std::abs (x[0]), std::abs (x[nch - 1]));
                    env += (lv > env ? att : rel) * (lv - env);
                    const float over = juce::Decibels::gainToDecibels (env, -100.0f) + 20.0f;
                    const float gr = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
                    const float g = mye::dbToGain (-gr + 6.0f * compE);
                    for (int c = 0; c < nch; ++c) x[c] *= g;
                }

                for (int c = 0; c < nch; ++c) wet.setSample (c, i, x[c] * level);
            }
            pos += len;
        }

        // ---- the space on the other side (or the room the device sits in)
        const float roomGain = roomAmt * 0.6f * t;
        if (roomGain > 0.001f || lastRoomGain > 0.001f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = s.roomSize > 0.0f ? s.roomSize : 0.35f; rp.damping = 0.6f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 0.8f;
            reverb.setParameters (rp);
            for (int c = 0; c < nch; ++c) room.copyFrom (c, 0, wet, c, 0, n);
            if (nch > 1) reverb.processStereo (room.getWritePointer (0), room.getWritePointer (1), n);
            else reverb.processMono (room.getWritePointer (0), n);
            for (int c = 0; c < nch; ++c) wet.addFromWithRamp (c, 0, room.getReadPointer (c), n, lastRoomGain, roomGain);
        }
        lastRoomGain = roomGain;

        // ---- body sounds + output. Below 5 % the dry signal fades back in, so 0 % is a true bypass
        const float tin = p ("tinnitus") / 100.0f, pulse = p ("pulse") / 100.0f, outGain = mye::dbToGain (p ("output"));
        const double beatSec = 60.0 / p ("pulsebpm"), beatInc = 1.0 / (beatSec * sr);
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        for (int i = 0; i < n; ++i)
        {
            const float ts = tBuf[(size_t) i], m = juce::jmin (1.0f, ts * 20.0f);
            float add = 0.0f;
            if (tin > 0.0f)
            {
                tinPh = std::fmod (tinPh + 6200.0 / sr, 1.0); tinPh2 = std::fmod (tinPh2 + 6207.0 / sr, 1.0);
                add += tin * 0.03f * (float) (std::sin (twoPi * tinPh) + 0.6 * std::sin (twoPi * tinPh2));
            }
            if (pulse > 0.0f)
            {
                beatPh = std::fmod (beatPh + beatInc, 1.0);
                add += pulse * heartbeat (beatPh, beatSec);
            }
            for (int c = 0; c < nch; ++c)
                buffer.setSample (c, i, (buffer.getSample (c, i) * (1.0f - m) + (wet.getSample (c, i) + add * ts) * m) * outGain);
        }

        curve.hp = hpE; curve.lp = lpE; curve.p1f = p1f; curve.p1g = g1; curve.p1q = s.p1q;
        curve.p2f = s.p2f > 0 ? s.p2f : 1000.0f; curve.p2g = g2; curve.p2q = s.p2q;
        curve.levelDb = levelNowDb; curve.steep = s.steep;
    }

    juce::AudioProcessorEditor* createEditor() override;

private:
    // ------------------------------------------------------------------ noise of the medium
    float white() { return rng.nextFloat() * 2.0f - 1.0f; }

    float pink()  // Paul Kellet's economy pink filter
    {
        const float w = white();
        pk[0] = 0.99765f * pk[0] + w * 0.0990460f;
        pk[1] = 0.96300f * pk[1] + w * 0.2965164f;
        pk[2] = 0.57000f * pk[2] + w * 1.0526913f;
        return (pk[0] + pk[1] + pk[2] + w * 0.1848f) * 0.12f;
    }

    // slow random walk 0..1 (signal fading, drifting whistle)
    float wander (float& v, float& target, int& count, float seconds)
    {
        if (--count <= 0) { target = rng.nextFloat(); count = (int) (sr * seconds * (0.5f + rng.nextFloat())); }
        v += (target - v) * (1.0f / (float) (sr * seconds * 0.5));
        return v;
    }

    float noiseSample (Noise type, float amt, float inLevel, float& sigGain)
    {
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        switch (type)
        {
            case hiss: return amt * 0.01f * pink();

            case hum:  // mains hum: 50 Hz + harmonics, rectifier buzz on the even ones, slow breathing
            {
                humPh = std::fmod (humPh + 50.0 / sr, 1.0);
                const float breathe = 0.85f + 0.15f * wander (fadeV, fadeT, fadeC, 3.0f);
                float h = 0.0f;
                for (int k = 1; k <= 14; ++k)
                    h += (float) std::sin (twoPi * humPh * k) * (k % 2 == 0 ? 0.9f : 0.45f) / std::pow ((float) k, 1.1f);
                return amt * breathe * (0.012f * h + 0.002f * pink());
            }

            case radio:  // AM: fading, static bursts, pops, heterodyne whistle
            {
                const float fade = wander (fadeV, fadeT, fadeC, 1.5f);
                sigGain = 1.0f - 0.55f * amt * fade * fade;
                float o = amt * (0.006f + 0.014f * fade) * pink();
                // static: bursts of crackle (5-60 ms), a few per second
                if (burstLeft <= 0 && rng.nextFloat() < (0.6f + 2.5f * amt) / (float) sr)
                {
                    burstLeft = (int) (sr * (0.005 + 0.055 * rng.nextFloat()));
                    burstAmp = amt * (0.05f + 0.25f * rng.nextFloat());
                }
                if (burstLeft > 0)
                {
                    --burstLeft;
                    if (rng.nextFloat() < 0.25f) o += burstAmp * white();
                    burstAmp *= 0.9993f;
                }
                if (rng.nextFloat() < 0.4f * amt / (float) sr) popEnv = amt * 0.35f;    // single pops
                o += popEnv * white(); popEnv *= 0.97f;
                // heterodyne whistle drifting slowly between ~0.9 and 2.4 kHz
                const float wf = 900.0f + 1500.0f * wander (whV, whT, whC, 6.0f);
                whPh = std::fmod (whPh + wf / sr, 1.0);
                o += amt * 0.0025f * (float) std::sin (twoPi * whPh);
                return o;
            }

            case walkie:  // squelch "kssht" when the signal opens and closes + band hiss
            {
                inEnv += (inLevel > inEnv ? 0.01f : 0.0005f) * (inLevel - inEnv);
                const bool open = inEnv > 0.02f;
                if (open != squelchOpen)
                {
                    squelchOpen = open;
                    squelchLeft = (int) (sr * (open ? 0.06 : 0.16));
                    squelchAmp = amt * (open ? 0.12f : 0.22f);
                }
                float o = amt * 0.004f * white() * (squelchOpen ? 1.0f : 0.25f);
                if (squelchLeft > 0) { --squelchLeft; o += squelchAmp * white(); squelchAmp *= 0.9997f; }
                if (rng.nextFloat() < 1.5f * amt / (float) sr) popEnv = amt * 0.15f;
                o += popEnv * white(); popEnv *= 0.96f;
                return o;
            }

            case phone:  // line hiss + digital dropouts
            {
                if (dropLeft <= 0 && rng.nextFloat() < 0.25f * amt / (float) sr)
                    dropLeft = (int) (sr * (0.03 + 0.09 * rng.nextFloat()));
                if (dropLeft > 0) { --dropLeft; sigGain = 0.05f; }
                return amt * 0.03f * pink();
            }

            default: return 0.0f;
        }
    }

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
    Biquad hpA, hpB, lpA, lpB, pkA, pkB;
    juce::Reverb reverb;
    juce::AudioBuffer<float> wet, room;
    std::vector<float> tBuf;
    float lastRoomGain = 0;
    juce::SmoothedValue<float> transSm;
    juce::Random rng;
    float env = 0, holdCount = 0, holdVal[2] {};
    float pk[3] {}, fadeV = 0, fadeT = 0, whV = 0.5f, whT = 0.5f, burstAmp = 0, popEnv = 0, inEnv = 0, squelchAmp = 0;
    int fadeC = 0, whC = 0, burstLeft = 0, squelchLeft = 0, dropLeft = 0;
    bool squelchOpen = false;
    double whPh = 0;
    double wobPh = 0, tinPh = 0, tinPh2 = 0, beatPh = 0, humPh = 0;
    int lastScene = -1;
};

// ---------------------------------------------------------------- what passes, what doesn't
class ResponseCurve : public juce::Component
{
public:
    explicit ResponseCurve (const CurveState& c) : cs (c) {}

    void paint (juce::Graphics& g) override
    {
        const auto acc = juce::Colour (0xffc8873a), ink = juce::Colour (0xffe8e2d4);
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff1c1c1c));
        g.fillRoundedRectangle (r, 6.0f);
        auto plot = r.reduced (12.0f, 8.0f);
        auto labels = plot.removeFromBottom (13.0f);

        const double sr = cs.sr.load();
        Biquad hp, lp, k1, k2;
        hp.highPass (sr, cs.hp.load(), 0.707f);
        lp.lowPass (sr, cs.lp.load(), 0.707f);
        k1.peak (sr, cs.p1f.load(), cs.p1q.load(), cs.p1g.load());
        k2.peak (sr, cs.p2f.load(), cs.p2q.load(), cs.p2g.load());
        const float order = cs.steep.load() ? 2.0f : 1.0f, level = cs.levelDb.load();

        constexpr float topDb = 18.0f, botDb = -60.0f;
        auto xFor = [&] (double f) { return plot.getX() + plot.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0)); };
        auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (botDb, topDb, db), topDb, botDb, plot.getY(), plot.getBottom()); };

        g.setFont (juce::FontOptions (10.0f));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            const bool major = f == 100.0 || f == 1000.0 || f == 10000.0;
            g.setColour (ink.withAlpha (major ? 0.14f : 0.06f));
            g.drawVerticalLine ((int) xFor (f), plot.getY(), plot.getBottom());
            if (major)
            {
                g.setColour (ink.withAlpha (0.5f));
                g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + " kHz" : juce::String ((int) f) + " Hz",
                            juce::Rectangle<float> (xFor (f) - 30.0f, labels.getY(), 60.0f, labels.getHeight()), juce::Justification::centred);
            }
        }
        for (float db : { 0.0f, -20.0f, -40.0f })
        {
            g.setColour (ink.withAlpha (db == 0.0f ? 0.3f : 0.08f));
            g.drawHorizontalLine ((int) yFor (db), plot.getX(), plot.getRight());
            g.setColour (ink.withAlpha (0.45f));
            g.drawText (juce::String ((int) db) + " dB", juce::Rectangle<float> (plot.getX() + 2.0f, yFor (db) - 12.0f, 50.0f, 11.0f), juce::Justification::left);
        }

        juce::Path line;
        for (float px = 0.0f; px <= plot.getWidth(); px += 2.0f)
        {
            const double f = juce::jmin (sr * 0.49, 20.0 * std::pow (1000.0, (double) (px / plot.getWidth())));
            const float db = order * (hp.magnitudeDb (sr, f) + lp.magnitudeDb (sr, f)) + k1.magnitudeDb (sr, f) + k2.magnitudeDb (sr, f) + level;
            if (px == 0.0f) line.startNewSubPath (plot.getX(), yFor (db)); else line.lineTo (plot.getX() + px, yFor (db));
        }
        juce::Path fill (line);
        fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
        g.setColour (acc.withAlpha (0.16f));
        g.fillPath (fill);
        g.setColour (acc);
        g.strokePath (line, juce::PathStrokeType (2.0f));
    }

private:
    const CurveState& cs;
};

class TimpanoEditor : public mye::SectionEditor
{
public:
    explicit TimpanoEditor (TimpanoProcessor& p)
        : SectionEditor (p, mye::utf8 ("TÍMPANO"), mye::utf8 ("M.Y.E. · puntos de escucha"),
          {
              { "ESCENA", { { "transition", mye::utf8 ("Transición"), Kind::knob }, { "scene", "", Kind::choice } }, 2 },
              { "FRECUENCIAS", { { "lowcut", "Corte grave", Kind::knob }, { "highcut", "Corte agudo", Kind::knob },
                                 { "resfreq", "Resonancia", Kind::knob }, { "resgain", "Realce", Kind::knob } } },
              { "LUGAR",  { { "distance", "Distancia", Kind::knob }, { "material", "Material / calidad", Kind::knob },
                            { "space", "Espacio", Kind::knob }, { "defect", "Defecto", Kind::knob } } },
              { "CUERPO", { { "tinnitus", mye::utf8 ("Acúfeno"), Kind::knob }, { "pulse", "Pulso", Kind::knob }, { "pulsebpm", "Pulso bpm", Kind::knob } } },
              { "SALIDA", { { "output", "Salida", Kind::knob } } },
          }),
          curve (p.curve)
    {
        setExtraComponent (&curve, 180);
    }

private:
    ResponseCurve curve;
};

juce::AudioProcessorEditor* TimpanoProcessor::createEditor() { return new TimpanoEditor (*this); }

juce::AudioProcessor* createTimpano() { return new TimpanoProcessor(); }
#ifndef MYE_TEST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TimpanoProcessor(); }
#endif
