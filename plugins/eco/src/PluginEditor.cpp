#include "PluginEditor.h"

namespace
{
    const juce::Colour bg     { 0xff141414 };
    const juce::Colour ink    { 0xffe8e2d4 };
    const juce::Colour accent { 0xffc8873a };
}

EcoEditor::EcoEditor (EcoProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf.setColour (juce::Slider::rotarySliderFillColourId, accent);
    lnf.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2c2c2c));
    lnf.setColour (juce::Slider::thumbColourId, ink);
    lnf.setColour (juce::Slider::textBoxTextColourId, ink);
    lnf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lnf.setColour (juce::Label::textColourId, ink.withAlpha (0.7f));
    lnf.setColour (juce::ToggleButton::textColourId, ink);
    lnf.setColour (juce::ToggleButton::tickColourId, accent);
    lnf.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff222222));
    lnf.setColour (juce::ComboBox::textColourId, ink);
    lnf.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff333333));
    setLookAndFeel (&lnf);

    addKnob ("time", "TIME");
    addKnob ("spread", "OFFSET R");
    addKnob ("feedback", "FEEDBACK");
    addKnob ("drive", "DRIVE");
    addKnob ("mix", "MIX");
    addKnob ("tone", "TONE");
    addKnob ("lowcut", "LOW CUT");
    addKnob ("wow", "WOW");
    addKnob ("flutter", "FLUTTER");
    addKnob ("output", "OUTPUT");
    addKnob ("panrate", "PAN RATE");
    addKnob ("pandepth", "PAN DEPTH");

    addToggle ("sync", "Sync");
    addToggle ("pingpong", "Ping-pong");
    addToggle ("freeze", "Freeze");
    addChoice ("division", "DIVISION");
    addChoice ("pantarget", "PAN");

    setSize (720, 470);
}

void EcoEditor::addKnob (const juce::String& id, const juce::String& name)
{
    auto* k = knobs.add (new Knob());
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
}

void EcoEditor::addToggle (const juce::String& id, const juce::String& name)
{
    auto* t = toggles.add (new Toggle());
    t->button.setButtonText (name);
    addAndMakeVisible (t->button);
    t->att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, t->button);
}

void EcoEditor::addChoice (const juce::String& id, const juce::String& name)
{
    auto* c = choices.add (new Choice());
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->box.addItemList (param->choices, 1);
    c->label.setText (name, juce::dontSendNotification);
    c->label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (c->box);
    addAndMakeVisible (c->label);
    c->att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c->box);
}

void EcoEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (ink);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("ECO", 24, 14, 200, 32, juce::Justification::left);
    g.setColour (ink.withAlpha (0.45f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("M.Y.E.  —  martillo · yunque · estribo", getWidth() - 324, 22, 300, 20, juce::Justification::right);
    g.setColour (accent.withAlpha (0.6f));
    g.drawHorizontalLine (56, 24.0f, (float) getWidth() - 24.0f);
}

void EcoEditor::resized()
{
    auto area = getLocalBounds().reduced (24).withTrimmedTop (44);

    auto bar = area.removeFromBottom (36);
    for (auto* t : toggles)
        t->button.setBounds (bar.removeFromLeft (110).reduced (2));
    for (auto* c : choices)
    {
        c->label.setBounds (bar.removeFromLeft (70));
        c->box.setBounds (bar.removeFromLeft (110).reduced (2, 6));
        bar.removeFromLeft (10);
    }

    area.removeFromBottom (8);
    const int cols = 6, rows = 2;
    const int w = area.getWidth() / cols, h = area.getHeight() / rows;
    for (int i = 0; i < knobs.size(); ++i)
    {
        juce::Rectangle<int> cell (area.getX() + (i % cols) * w, area.getY() + (i / cols) * h, w, h);
        knobs[i]->label.setBounds (cell.removeFromTop (18));
        knobs[i]->slider.setBounds (cell.reduced (6));
    }
}
