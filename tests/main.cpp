// Offline torture test: random parameter sets, several signals, mono and stereo.
#include "MyeCommon.h"
juce::AudioProcessor* createEstribo(); juce::AudioProcessor* createMartillo();
juce::AudioProcessor* createYunque();  juce::AudioProcessor* createTimpano();
juce::AudioProcessor* createLaringe();

// every knob: does it keep the value it is given, and does its own text read back as the same value?
static int knobCheck (const char* name, juce::Component& root)
{
    int bad = 0;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        if (auto* sl = dynamic_cast<juce::Slider*> (&c))
        {
            for (double frac : { 0.13, 0.5, 0.87 })
            {
                const double v = sl->proportionOfLengthToValue (frac);
                sl->setValue (v, juce::sendNotificationSync);
                const double span = sl->getMaximum() - sl->getMinimum();
                const double kept = sl->getValue(), fromText = sl->getValueFromText (sl->getTextFromValue (kept));
                if (std::abs (kept - v) > span * 0.01)
                { printf ("%s: knob does not hold value (%s): set %.3f got %.3f\n", name, sl->getTextFromValue (v).toRawUTF8(), v, kept); ++bad; }
                if (std::abs (fromText - kept) > juce::jmax (span * 0.01, std::abs (kept) * 0.06))
                { printf ("%s: text '%s' reads back as %.3f (value %.3f)\n", name, sl->getTextFromValue (kept).toRawUTF8(), fromText, kept); ++bad; }
            }
        }
        for (auto* ch : c.getChildren()) walk (*ch);
    };
    walk (root);
    return bad;
}

static int run (const char* name, juce::AudioProcessor* (*make)())
{
    int failures = 0;
    juce::Random rng (1234);
    for (int layout = 0; layout < 2; ++layout)
    for (int trial = 0; trial < 40; ++trial)
    {
        std::unique_ptr<juce::AudioProcessor> p (make());
        if (layout == 1)
        {
            juce::AudioProcessor::BusesLayout l;
            l.inputBuses.add (juce::AudioChannelSet::mono());
            l.outputBuses.add (juce::AudioChannelSet::stereo());
            if (! p->setBusesLayout (l)) { printf ("%s: mono->stereo layout rejected\n", name); ++failures; break; }
        }
        const double sr = trial % 3 == 0 ? 96000.0 : 48000.0;
        const int block = trial % 2 ? 512 : 77;
        p->prepareToPlay (sr, block);
        // trial 0 = defaults, others = random params
        if (trial > 0)
            for (auto* prm : p->getParameters()) prm->setValueNotifyingHost (rng.nextFloat());
        juce::AudioBuffer<float> b (2, block);
        juce::MidiBuffer m;
        double peak = 0, sumSq = 0; long count = 0;
        const int blocks = (int) (sr * 4.0 / block);
        double ph = 0;
        for (int k = 0; k < blocks; ++k)
        {
            for (int i = 0; i < block; ++i)
            {
                const double t = (k * block + i) / sr;
                float x = 0;
                if (t < 1.0) x = 0.5f * (float) std::sin (ph += juce::MathConstants<double>::twoPi * 220.0 / sr);
                else if (t < 1.5) x = (rng.nextFloat() * 2 - 1) * 0.7f;
                else if (t < 1.52) x = 1.0f; // loud transient
                b.setSample (0, i, x);
                b.setSample (1, i, layout == 1 ? 0.0f : x * 0.8f);
            }
            p->processBlock (b, m);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i)
                {
                    const float v = b.getSample (c, i);
                    if (! std::isfinite (v)) { printf ("%s: NaN/Inf (layout %d trial %d)\n", name, layout, trial); ++failures; k = blocks; break; }
                    peak = std::max (peak, (double) std::abs (v)); sumSq += v * v; ++count;
                }
        }
        if (peak > 16.0) { printf ("%s: peak too high %.1f (layout %d trial %d)\n", name, peak, layout, trial); ++failures; }
        if (trial == 0) printf ("%-9s %s defaults: peak %.2f  rms %.3f\n", name, layout ? "mono->st" : "stereo  ", peak, std::sqrt (sumSq / std::max (1L, count)));
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor()); // editor builds?
        if (ed == nullptr || ed->getWidth() < 300) { printf ("%s: editor problem\n", name); ++failures; }
        if (trial == 0 && layout == 0)
        {
            printf ("%-9s editor %dx%d\n", name, ed->getWidth(), ed->getHeight());
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (name) + ".png"));
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            os.flush();
            failures += knobCheck (name, *ed);
        }
    }
    return failures;
}

static void noiseReport()
{
    // Timpano: level of the medium's noise vs the signal, per noisy scene
    const int scenesToCheck[] { 7, 8, 9, 11 };
    for (int sc : scenesToCheck)
    {
        std::unique_ptr<juce::AudioProcessor> p (createTimpano());
        p->prepareToPlay (48000.0, 512);
        auto* prm = p->getParameters()[0];                      // scene
        prm->setValueNotifyingHost (sc / 15.0f);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
        double sig = 0, sil = 0; long ns = 0, nq = 0; double ph = 0;
        for (int k = 0; k < 48000 * 6 / 512; ++k)
        {
            for (int i = 0; i < 512; ++i)
            {
                const double t = (k * 512 + i) / 48000.0;
                const float x = t < 3.0 ? 0.3f * (float) std::sin (ph += 6.2831853 * 440.0 / 48000.0) * (std::fmod (t, 0.5) < 0.3 ? 1.0f : 0.0f) : 0.0f;
                b.setSample (0, i, x); b.setSample (1, i, x);
            }
            p->processBlock (b, m);
            for (int i = 0; i < 512; ++i)
            {
                const double t = (k * 512 + i) / 48000.0; const float v = b.getSample (0, i);
                if (t > 0.5 && t < 3.0) { sig += v * v; ++ns; } else if (t > 4.0) { sil += v * v; ++nq; }
            }
        }
        printf ("Timpano scene %2d: signal %.1f dB, noise alone %.1f dB\n", sc,
                juce::Decibels::gainToDecibels ((float) std::sqrt (sig / ns)), juce::Decibels::gainToDecibels ((float) std::sqrt (sil / nq)));
    }
}

static void transitionReport()
{
    printf ("Timpano 'Puerta', transition sweep (white noise in at -15.2 dB rms):\n");
    for (float tr : { 0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        std::unique_ptr<juce::AudioProcessor> p (createTimpano());
        for (auto* prm : p->getParameters())
            if (auto* w = dynamic_cast<juce::AudioProcessorParameterWithID*> (prm))
                if (w->paramID == "transition") prm->setValueNotifyingHost (tr);
        p->prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; juce::Random r (7);
        double all = 0, hi = 0, lpState = 0; long cnt = 0;
        for (int k = 0; k < 48000 * 3 / 512; ++k)
        {
            for (int i = 0; i < 512; ++i) { const float x = (r.nextFloat() * 2 - 1) * 0.3f; b.setSample (0, i, x); b.setSample (1, i, x); }
            p->processBlock (b, m);
            if (k > 100) for (int i = 0; i < 512; ++i)
            {
                const double v = b.getSample (0, i); lpState += 0.123 * (v - lpState);      // ~1 kHz split
                const double h = v - lpState; all += v * v; hi += h * h; ++cnt;
            }
        }
        printf ("  %3.0f %%: total %6.1f dB   above 1 kHz %6.1f dB\n", tr * 100, 10 * std::log10 (all / cnt + 1e-12), 10 * std::log10 (hi / cnt + 1e-12));
    }
}

static void setPlain (juce::AudioProcessor& p, const char* id, float v)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->paramID == id) r->setValueNotifyingHost (r->convertTo0to1 (v));
}

// Laringe: does a voice really come out at the pitch that was asked for?
static int pitchReport()
{
    int bad = 0;
    for (double f0 : { 110.0, 200.0, 330.0 })
    for (float semis : { -12.0f, 4.0f, 7.0f, 12.0f })
    {
        std::unique_ptr<juce::AudioProcessor> p (createLaringe());
        for (const char* id : { "v2on", "v3on", "v4on" }) setPlain (*p, id, 0.0f);
        setPlain (*p, "v1pitch", semis); setPlain (*p, "v1fine", 0.0f); setPlain (*p, "anim", 0.0f); setPlain (*p, "dry", -60.0f);
        const double sr = 48000.0;
        p->prepareToPlay (sr, 512);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
        std::vector<float> out; double ph = 0;
        for (int k = 0; k < 48000 * 2 / 512; ++k)
        {
            for (int i = 0; i < 512; ++i)
            {
                ph += f0 / sr; float x = 0;
                for (int h = 1; h <= 8; ++h) x += (float) std::sin (6.283185307 * ph * h) / (float) h;   // voice-like: harmonics
                b.setSample (0, i, 0.25f * x); b.setSample (1, i, 0.25f * x);
            }
            p->processBlock (b, m);
            if (k > 120) for (int i = 0; i < 512; ++i) out.push_back (b.getSample (0, i));
        }
        // fundamental of the output by autocorrelation
        const int N = 16384; double best = 0; std::vector<double> ac (2000, 0.0);
        for (int tau = 40; tau < 1999; ++tau) { double a = 0; for (int i = 0; i < N; ++i) a += out[(size_t) i] * out[(size_t) (i + tau)]; ac[(size_t) tau] = a; best = std::max (best, a); }
        double got = 0;
        for (int tau = 41; tau < 1998; ++tau)
            if (ac[(size_t) tau] > 0.85 * best && ac[(size_t) tau] >= ac[(size_t) tau - 1] && ac[(size_t) tau] >= ac[(size_t) tau + 1])
            {
                const double a = ac[(size_t) tau - 1], bb = ac[(size_t) tau], c = ac[(size_t) tau + 1];
                got = sr / (tau + 0.5 * (a - c) / (a - 2 * bb + c)); break;
            }
        const double want = f0 * std::pow (2.0, semis / 12.0), cents = 1200.0 * std::log2 (got / want);
        double rms = 0; for (int i = 0; i < N; ++i) rms += out[(size_t) i] * out[(size_t) i];
        printf ("Laringe %5.0f Hz %+3.0f st: want %6.1f Hz, got %6.1f Hz (%+.0f cents), level %.1f dB\n", f0, semis, want, got, cents, 10 * std::log10 (rms / N + 1e-12));
        if (std::abs (cents) > 25.0) ++bad;
    }
    return bad;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    noiseReport();
    transitionReport();
    int f = run ("Estribo", createEstribo) + run ("Martillo", createMartillo) + run ("Yunque", createYunque) + run ("Timpano", createTimpano) + run ("Laringe", createLaringe);
    f += pitchReport();
    printf (f == 0 ? "ALL OK\n" : "FAILURES: %d\n", f);
    return f == 0 ? 0 : 1;
}
