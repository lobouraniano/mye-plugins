#pragma once
// Shared base processor + section-based editor for all M.Y.E. plugins.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

namespace mye
{
// ---------------------------------------------------------------- processor base
class ProcessorBase : public juce::AudioProcessor
{
public:
    ProcessorBase (juce::AudioProcessorValueTreeState::ParameterLayout layout, bool allowMonoToStereo = true)
        : AudioProcessor (BusesProperties()
                              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "STATE", std::move (layout)),
          monoToStereo (allowMonoToStereo) {}

    juce::AudioProcessorValueTreeState apvts;

    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    // set a parameter (plain value) as a host-visible gesture; message thread only
    void setParam (const char* id, float plain)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
            prm->endChangeGesture();
        }
    }

    // presets: called when the USER picks an item in a selector (not on automation / session load)
    std::map<juce::String, std::function<void (int)>> presetActions;

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
        const auto mono = juce::AudioChannelSet::mono(), st = juce::AudioChannelSet::stereo();
        return (in == mono && out == mono) || (in == st && out == st) || (monoToStereo && in == mono && out == st);
    }

    // call at the top of processBlock: duplicates a mono input to both outputs
    void spreadMonoInput (juce::AudioBuffer<float>& b)
    {
        if (getTotalNumInputChannels() == 1 && b.getNumChannels() > 1)
            b.copyFrom (1, 0, b, 0, 0, b.getNumSamples());
    }

    void releaseResources() override {}
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& d) override
    {
        if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
    }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

private:
    bool monoToStereo;
};

// ---------------------------------------------------------------- parameter helpers
using Layout = std::vector<std::unique_ptr<juce::RangedAudioParameter>>;

inline juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi, step);
    r.setSkewForCentre (centre);
    return r;
}
inline void addFloat (Layout& l, const char* id, const juce::String& name, juce::NormalisableRange<float> r, float def,
                      const juce::String& unit = {})
{
    const float span = r.end - r.start;
    const int dec = span >= 100.0f ? 0 : (span >= 10.0f ? 1 : 2);
    auto toText = [dec, unit] (float v, int)
    {
        juce::String t = (unit == "Hz" && v >= 1000.0f) ? juce::String (v / 1000.0f, 1) + " k" : juce::String (v, dec) + " ";
        return (t + unit).trim();
    };
    l.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                     juce::AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (toText)));
}
inline void addFloat (Layout& l, const char* id, const juce::String& name, float lo, float hi, float def,
                      const juce::String& unit = {})
{
    addFloat (l, id, name, juce::NormalisableRange<float> (lo, hi, 0.0f), def, unit);
}
inline void addBool (Layout& l, const char* id, const juce::String& name, bool def)
{
    l.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
}
inline void addChoice (Layout& l, const char* id, const juce::String& name, const juce::StringArray& c, int def)
{
    l.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, c, def));
}

inline float dbToGain (float db) { return juce::Decibels::decibelsToGain (db); }
inline float onePole (float hz, double sr) { return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr); }

// ---------------------------------------------------------------- editor
struct Palette
{
    juce::Colour bg { 0xff141414 }, panel { 0xff1c1c1c }, ink { 0xffe8e2d4 }, accent { 0xffc8873a };
};

class SectionEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    enum class Kind { knob, toggle, choice };
    struct Item { juce::String id, label; Kind kind; };
    struct Section { juce::String title; std::vector<Item> items; int minSlots = 0; };

    SectionEditor (ProcessorBase& p, juce::String titleText, juce::String subtitleText,
                   std::vector<Section> secs, Palette pal = {})
        : AudioProcessorEditor (&p), proc (p), title (std::move (titleText)),
          subtitle (std::move (subtitleText)), sections (std::move (secs)), colours (pal)
    {
        lnf.setColour (juce::Slider::rotarySliderFillColourId, colours.accent);
        lnf.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2e2e2e));
        lnf.setColour (juce::Slider::thumbColourId, colours.ink);
        lnf.setColour (juce::Slider::textBoxTextColourId, colours.ink);
        lnf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        lnf.setColour (juce::Label::textColourId, colours.ink.withAlpha (0.65f));
        lnf.setColour (juce::ToggleButton::textColourId, colours.ink);
        lnf.setColour (juce::ToggleButton::tickColourId, colours.accent);
        lnf.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff242424));
        lnf.setColour (juce::ComboBox::textColourId, colours.ink);
        lnf.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff353535));
        lnf.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff202020));
        lnf.setColour (juce::PopupMenu::textColourId, colours.ink);
        lnf.setColour (juce::PopupMenu::highlightedBackgroundColourId, colours.accent.withAlpha (0.5f));
        setLookAndFeel (&lnf);

        for (auto& s : sections)
            for (auto& it : s.items)
                build (it);

        // width: one column per knob-ish slot, sections side by side
        int slots = 0;
        for (auto& s : sections) slots += juce::jmax (1, slotsFor (s));
        setSize (juce::jmax (520, slots * slotW + 48 + ((int) sections.size() - 1) * gap), 330 + extraHeight);
        startTimerHz (20);
    }

    ~SectionEditor() override { setLookAndFeel (nullptr); }

    // optional extra area under the header (meters etc.)
    void setExtraComponent (juce::Component* c, int height)
    {
        extra = c; extraHeight = height;
        addAndMakeVisible (c);
        setSize (getWidth(), 330 + extraHeight);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours.bg);
        g.setColour (colours.ink);
        g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
        g.drawText (title, 24, 14, 300, 32, juce::Justification::left);
        const int tw = juce::GlyphArrangement::getStringWidthInt (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)), title);
        g.setColour (colours.ink.withAlpha (0.4f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("v" + versionString, 24 + tw + 10, 27, 80, 16, juce::Justification::left);
        g.drawText (subtitle, getWidth() - 424, 22, 400, 20, juce::Justification::right);
        g.setColour (colours.accent.withAlpha (0.6f));
        g.drawHorizontalLine (56, 24.0f, (float) getWidth() - 24.0f);

        for (auto& r : sectionRects)
        {
            g.setColour (colours.panel);
            g.fillRoundedRectangle (r.second.toFloat(), 6.0f);
            g.setColour (colours.accent.withAlpha (0.85f));
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (r.first, r.second.getX() + 10, r.second.getY() + 6, r.second.getWidth() - 20, 16,
                        juce::Justification::left);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (24).withTrimmedTop (44);
        if (extra != nullptr)
            extra->setBounds (area.removeFromTop (extraHeight).reduced (0, 6));
        sectionRects.clear();

        int x = area.getX();
        for (auto& s : sections)
        {
            const int w = juce::jmax (1, slotsFor (s)) * slotW;
            juce::Rectangle<int> r (x, area.getY(), w, area.getHeight());
            sectionRects.push_back ({ s.title, r });
            auto inner = r.reduced (6).withTrimmedTop (20);

            // knobs on top row(s), toggles/choices in a bottom strip
            int nKnobs = 0, nOther = 0;
            for (auto& it : s.items) (it.kind == Kind::knob ? nKnobs : nOther)++;
            const int ocols = juce::jmax (1, slotsFor (s));
            auto bottom = inner.removeFromBottom (nOther > 0 ? 30 * ((nOther + ocols - 1) / ocols) : 0);
            const int cols = juce::jmax (1, juce::jmin (ocols, nKnobs));
            const int rows = juce::jmax (1, (nKnobs + cols - 1) / cols);
            const int cw = inner.getWidth() / cols, ch = inner.getHeight() / rows;
            int k = 0, o = 0;
            const int ow = nOther == 1 ? bottom.getWidth() : bottom.getWidth() / ocols;
            for (auto& it : s.items)
            {
                auto* c = controls[it.id];
                if (it.kind == Kind::knob)
                {
                    juce::Rectangle<int> cell (inner.getX() + (k % cols) * cw, inner.getY() + (k / cols) * ch, cw, ch);
                    c->label.setBounds (cell.removeFromTop (16));
                    c->slider.setBounds (cell.reduced (4, 0));
                    ++k;
                }
                else
                {
                    juce::Rectangle<int> cell (bottom.getX() + (o % ocols) * ow, bottom.getY() + (o / ocols) * 30, ow, 30);
                    if (it.kind == Kind::toggle) c->toggle.setBounds (cell.reduced (2));
                    else c->combo.setBounds (cell.reduced (3, 4));
                    ++o;
                }
            }
            x += w + gap;
        }
    }

    juce::String versionString { JucePlugin_VersionString };

protected:
    ProcessorBase& proc;

private:
    struct UserCombo : juce::ComboBox
    {
        void showPopup() override { pickedAt = juce::Time::getMillisecondCounter(); ComboBox::showPopup(); }
        bool userIsPicking() const { return pickedAt != 0 && juce::Time::getMillisecondCounter() - pickedAt < 120000; }
        juce::uint32 pickedAt = 0;
    };

    struct Ctl
    {
        juce::Slider slider; juce::Label label; juce::ToggleButton toggle; UserCombo combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
    };

    static juce::String upper (juce::String t)
    {
        t = t.toUpperCase();
        const char* pairs[][2] { { "\xc3\xa1", "\xc3\x81" }, { "\xc3\xa9", "\xc3\x89" }, { "\xc3\xad", "\xc3\x8d" },
                                 { "\xc3\xb3", "\xc3\x93" }, { "\xc3\xba", "\xc3\x9a" }, { "\xc3\xb1", "\xc3\x91" } };
        for (auto& pr : pairs) t = t.replace (juce::String::fromUTF8 (pr[0]), juce::String::fromUTF8 (pr[1]));
        return t;
    }

    static int slotsFor (const Section& s)
    {
        if (s.minSlots > 0) return juce::jmax (s.minSlots, slotsForItems (s));
        return slotsForItems (s);
    }

    static int slotsForItems (const Section& s)
    {
        int knobs = 0, other = 0;
        for (auto& it : s.items) (it.kind == Kind::knob ? knobs : other)++;
        if (knobs == 0) return juce::jmax (1, juce::jmin (2, other));
        return knobs <= 2 ? knobs : (knobs + 1) / 2; // two rows when >2 knobs
    }

    void build (const Item& it)
    {
        auto c = std::make_unique<Ctl>();
        if (it.kind == Kind::knob)
        {
            c->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            c->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 16);
            c->label.setText (upper (it.label), juce::dontSendNotification);
            c->label.setJustificationType (juce::Justification::centred);
            c->label.setFont (juce::FontOptions (11.0f));
            addAndMakeVisible (c->slider);
            addAndMakeVisible (c->label);
            c->sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, it.id, c->slider);
        }
        else if (it.kind == Kind::toggle)
        {
            c->toggle.setButtonText (it.label);
            addAndMakeVisible (c->toggle);
            c->ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, it.id, c->toggle);
        }
        else
        {
            if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (it.id)))
                c->combo.addItemList (pc->choices, 1);
            c->combo.setTooltip (it.label);
            addAndMakeVisible (c->combo);
            c->ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, it.id, c->combo);
            auto* combo = &c->combo;
            const auto id = it.id;
            c->combo.onChange = [this, combo, id]
            {
                if (! combo->userIsPicking()) return;      // ignore automation / state restore
                combo->pickedAt = 0;
                auto a = proc.presetActions.find (id);
                if (a != proc.presetActions.end())
                {
                    const int idx = combo->getSelectedItemIndex();
                    auto fn = a->second;
                    // after the attachment has written the selector value
                    juce::MessageManager::callAsync ([fn, idx] { fn (idx); });
                }
            };
        }
        controls[it.id] = c.get();
        owned.push_back (std::move (c));
    }

    void timerCallback() override { if (extra != nullptr) extra->repaint(); }

    juce::LookAndFeel_V4 lnf; // must outlive the controls below
    juce::String title, subtitle;
    std::vector<Section> sections;
    Palette colours;
    std::vector<std::unique_ptr<Ctl>> owned;
    std::map<juce::String, Ctl*> controls;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sectionRects;
    juce::Component* extra = nullptr;
    int extraHeight = 0;
    static constexpr int slotW = 92, gap = 10;
};

inline juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }
} // namespace mye
