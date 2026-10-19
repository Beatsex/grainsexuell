#pragma once

#include "ActivityTrace.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace grain::gui
{
// Look: a cream sheet with ink hairlines, a dusty pink panel, small uppercase labels, monospaced numbers, square
// thumbs, no rounded corners. Drawn with paths only (no images); system fonts.
namespace palette
{
inline const juce::Colour paper { 0xfff3f1ee };
inline const juce::Colour ink { 0xff111111 };
inline const juce::Colour pink { 0xfff2b8c6 };
inline const juce::Colour faint = ink.withAlpha(0.55f); // >= 3:1 on paper (UI element contrast)
} // namespace palette

inline juce::FontOptions sans(float size, bool bold = false)
{
    return juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain).withKerningFactor(0.08f);
}

inline juce::FontOptions mono(float size)
{
    return juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain);
}

class GrainLookAndFeel : public juce::LookAndFeel_V4
{
public:
    float scale = 1.0f; // the editor's width / design width

    GrainLookAndFeel()
    {
        using namespace palette;
        setColour(juce::ResizableWindow::backgroundColourId, paper);
        setColour(juce::Label::textColourId, ink);
        setColour(juce::PopupMenu::backgroundColourId, paper);
        setColour(juce::PopupMenu::textColourId, ink);
        setColour(juce::PopupMenu::headerTextColourId, ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, ink);
        setColour(juce::PopupMenu::highlightedTextColourId, paper);
        setColour(juce::TooltipWindow::backgroundColourId, ink);
        setColour(juce::TooltipWindow::textColourId, paper);
        setColour(juce::TooltipWindow::outlineColourId, ink);
        setColour(juce::AlertWindow::backgroundColourId, paper);
        setColour(juce::AlertWindow::textColourId, ink);
        setColour(juce::AlertWindow::outlineColourId, ink);
        setColour(juce::TextEditor::backgroundColourId, juce::Colours::white);
        setColour(juce::TextEditor::textColourId, ink);
        setColour(juce::TextEditor::outlineColourId, ink);
        setColour(juce::TextEditor::focusedOutlineColourId, ink);
        setColour(juce::TextButton::buttonColourId, paper);
        setColour(juce::TextButton::textColourOffId, ink);
        setColour(juce::TextButton::textColourOnId, paper);
        setColour(juce::Slider::textBoxTextColourId, ink);
    }

    // a grid cell: transparent, ink when selected or pressed, a hairline on its right edge
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto area = b.getLocalBounds().toFloat();
        if (! b.isEnabled() && b.getToggleState()) // disabled selection: an outline, so it does not read as an error stripe
        {
            g.setColour(palette::ink.withAlpha(0.35f));
            g.drawRect(area.reduced(4.0f * scale), 1.0f);
        }
        else if (b.getToggleState() || down) // feedback on press, not on release
            g.fillAll(palette::ink.withAlpha(down && ! b.getToggleState() ? 0.85f : 1.0f));
        else if (over && b.isEnabled())
            g.fillAll(palette::ink.withAlpha(0.08f));
        g.setColour(palette::ink);
        g.fillRect(area.getRight() - 1.0f, 0.0f, 1.0f, area.getHeight());
        if (b.hasKeyboardFocus(false))
        {
            g.setColour(b.getToggleState() ? palette::pink : palette::ink);
            g.drawRect(area.reduced(3.0f), 1.0f);
        }
    }

    // Buttons flagged with the property "switch" (on/off toggles) get a square marker, filled when on, so they read
    // differently from radio cells; "menu" buttons get a small triangle.
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        const bool inverted = (b.getToggleState() && b.isEnabled()) || down;
        const auto colour = inverted ? palette::paper : palette::ink.withMultipliedAlpha(b.isEnabled() ? 1.0f : 0.35f);
        g.setColour(colour);
        const auto font = sans(12.0f * scale);
        g.setFont(font);
        const auto text = b.getButtonText().toUpperCase();
        const bool isSwitch = b.getProperties().getWithDefault("switch", false);
        const bool isMenu = b.getProperties().getWithDefault("menu", false);
        if (! isSwitch && ! isMenu)
        {
            g.drawFittedText(text, b.getLocalBounds().reduced(juce::roundToInt(4 * scale), 0), juce::Justification::centred, 1, 0.8f);
            return;
        }

        const float mark = 8.0f * scale, gap = 8.0f * scale;
        const float textW = juce::GlyphArrangement::getStringWidth(juce::Font(font), text);
        const float total = textW + gap + mark;
        const auto c = b.getLocalBounds().toFloat().getCentre();
        const float left = c.x - total * 0.5f;
        if (isSwitch) // marker before the label
        {
            const auto box = juce::Rectangle<float>(left, c.y - mark * 0.5f, mark, mark);
            if (b.getToggleState())
                g.fillRect(box);
            else
                g.drawRect(box, 1.0f);
            g.drawText(text, juce::Rectangle<float>(left + mark + gap, 0.0f, textW + 2.0f, static_cast<float>(b.getHeight())),
                       juce::Justification::centredLeft, false);
        }
        else // triangle after the label
        {
            g.drawText(text, juce::Rectangle<float>(left, 0.0f, textW + 2.0f, static_cast<float>(b.getHeight())),
                       juce::Justification::centredLeft, false);
            juce::Path tri;
            const float tx = left + textW + gap;
            tri.addTriangle(tx, c.y - mark * 0.3f, tx + mark, c.y - mark * 0.3f, tx + mark * 0.5f, c.y + mark * 0.35f);
            g.fillPath(tri);
        }
    }

    int getSliderThumbRadius(juce::Slider&) override { return juce::roundToInt(6.0f * scale); }

    // a hairline track, a 3 px fill up to the value and a square thumb
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                          juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const float cy = static_cast<float>(y) + static_cast<float>(h) * 0.5f;
        const float left = static_cast<float>(x), right = static_cast<float>(x + w);
        const float thumb = 12.0f * scale;
        g.setColour(palette::faint);
        g.fillRect(left, cy - 0.5f, right - left, 1.0f);
        g.setColour(palette::ink.withMultipliedAlpha(s.isEnabled() ? 1.0f : 0.35f));
        g.fillRect(left, cy - 1.5f * scale, std::max(0.0f, pos - left), 3.0f * scale);
        const auto square = juce::Rectangle<float>(thumb, thumb).withCentre({ pos, cy });
        g.fillRect(square);
        if (s.hasKeyboardFocus(false))
        {
            g.setColour(palette::ink);
            g.drawRect(square.expanded(3.0f * scale), 1.0f);
        }
    }
};

// One parameter shown as a row of radio cells (family, variant, loop state, ...).
class ChoiceRow : public juce::Component
{
public:
    ChoiceRow(juce::RangedAudioParameter& param, const juce::StringArray& labels, const juce::String& accessibleName)
        : attachment(param, [this](float v) { show(juce::roundToInt(v)); })
    {
        setTitle(accessibleName);
        setLabels(labels);
        attachment.sendInitialUpdate();
    }

    void setLabels(const juce::StringArray& labels)
    {
        buttons.clear();
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add(new juce::TextButton(labels[i]));
            b->setClickingTogglesState(false);
            b->setTitle(getTitle() + " " + labels[i]);
            b->onClick = [this, i] { attachment.setValueAsCompleteGesture(static_cast<float>(i)); };
            addAndMakeVisible(b);
        }
        show(selected);
        resized();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        const int n = buttons.size();
        for (int i = 0; i < n; ++i)
            buttons[i]->setBounds(area.removeFromLeft(area.getWidth() / (n - i)));
    }

private:
    void show(int index)
    {
        selected = index;
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState(i == index, juce::dontSendNotification);
    }

    juce::OwnedArray<juce::TextButton> buttons;
    int selected = 0;
    juce::ParameterAttachment attachment;
};

// One continuous parameter as a numbered row: 01  NAME  ----#------  value. Double-click resets the slider.
class ParamRow : public juce::Component
{
public:
    ParamRow(juce::RangedAudioParameter& param, const juce::String& labelText, int number)
        : name(labelText), index(juce::String(number).paddedLeft('0', 2)), attachment(param, slider)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setDoubleClickReturnValue(true, param.convertFrom0to1(param.getDefaultValue()));
        slider.setTitle(labelText);
        slider.setTooltip(labelText + ": drag, double-click to reset, right-click for MIDI learn");

        // read-outs: dB for the trims, Hz / kHz for the filter, percent for the 0..1 macros
        if (param.getLabel() == "dB")
            slider.textFromValueFunction = [](double v) { return (v > 0.0 ? "+" : "") + juce::String(v, 1) + " dB"; };
        else if (param.getLabel() == "Hz")
            slider.textFromValueFunction = [](double v) {
                return v >= 1000.0 ? juce::String(v / 1000.0, 1) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz";
            };
        else
            slider.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible(slider);
    }

    void setCaption(const juce::String& extra)
    {
        if (extra != caption)
        {
            caption = extra;
            repaint();
        }
    }

    void setScale(float s)
    {
        scale = s;
        resized();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour(palette::ink);
        g.fillRect(area.removeFromBottom(1.0f));
        area.removeFromLeft(16.0f * scale);
        area.removeFromRight(16.0f * scale);

        g.setFont(mono(12.0f * scale));
        g.drawText(index, area.removeFromLeft(30.0f * scale), juce::Justification::centredLeft);
        g.setFont(sans(12.0f * scale, true));
        g.drawText((caption.isEmpty() ? name : name + "  " + caption).toUpperCase(), area.removeFromLeft(118.0f * scale),
                   juce::Justification::centredLeft);
        g.setFont(mono(13.0f * scale));
        g.drawText(slider.getTextFromValue(slider.getValue()), area.removeFromRight(84.0f * scale),
                   juce::Justification::centredRight);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(juce::roundToInt((16.0f + 30.0f + 118.0f) * scale));
        area.removeFromRight(juce::roundToInt((16.0f + 84.0f + 10.0f) * scale));
        slider.setBounds(area);
    }

    juce::Slider slider;

private:
    juce::String name, index, caption;
    float scale = 1.0f;
    juce::SliderParameterAttachment attachment;
};

// The pink readout: engine events per second as a huge number, and the last ~4 s as a row of bars.
class ActivityPanel : public juce::Component
{
public:
    ActivityPanel()
    {
        setInterceptsMouseClicks(false, false);
        setTitle("Grain activity");
    }

    void push(unsigned total)
    {
        trace.push(total);
        setDescription(juce::String(trace.perSecond()) + " events per second");
        repaint();
    }

    void setScale(float s) { scale = s; }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced(28.0f * scale, 0.0f);
        area.removeFromTop(20.0f * scale);
        g.setColour(palette::ink);
        g.setFont(sans(12.0f * scale));
        const auto top = area.removeFromTop(16.0f * scale);
        g.drawText("GRAIN ACTIVITY", top, juce::Justification::centredLeft);
        g.drawText("EVENTS / S", top, juce::Justification::centredRight);

        auto barsArea = area.removeFromBottom(110.0f * scale);
        auto numberArea = area.reduced(0.0f, 6.0f * scale);
        g.setFont(juce::FontOptions(numberArea.getHeight() * 0.95f).withKerningFactor(-0.05f));
        g.drawText(juce::String(trace.perSecond()), numberArea.translated(-8.0f * scale, 0.0f),
                   juce::Justification::bottomLeft, false);

        const float step = barsArea.getWidth() / static_cast<float>(ActivityTrace::numBars);
        for (int i = 0; i < ActivityTrace::numBars; ++i)
        {
            const float h = std::max(3.0f * scale, trace.bar(i) * barsArea.getHeight());
            g.fillRect(barsArea.getX() + step * static_cast<float>(i), barsArea.getBottom() - h, 2.0f * scale, h);
        }
    }

private:
    ActivityTrace trace;
    float scale = 1.0f;
};
} // namespace grain::gui
