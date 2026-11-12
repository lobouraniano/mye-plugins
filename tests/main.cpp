// Offline torture test: random parameter sets, several signals, mono and stereo.
#include "MyeCommon.h"
juce::AudioProcessor* createEstribo(); juce::AudioProcessor* createMartillo();
juce::AudioProcessor* createYunque();  juce::AudioProcessor* createTimpano();

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
        }
    }
    return failures;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    int f = run ("Estribo", createEstribo) + run ("Martillo", createMartillo) + run ("Yunque", createYunque) + run ("Timpano", createTimpano);
    printf (f == 0 ? "ALL OK\n" : "FAILURES: %d\n", f);
    return f == 0 ? 0 : 1;
}
