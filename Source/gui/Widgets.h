#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

namespace grain::gui
{
// Own palette and shapes; drawn with paths and gradients only (no images, nothing borrowed from other products).
namespace palette
{
inline const juce::Colour background { 0xff14161a };
inline const juce::Colour panel { 0xff1d2127 };
inline const juce::Colour track { 0xff303741 };
inline const juce::Colour accent { 0xffe8a64a };
inline const juce::Colour accentDim { 0xff7d5a2c };
inline const juce::Colour text { 0xffdde1e7 };
inline const juce::Colour textDim { 0xff7e8896 };
inline const juce::Colour learn { 0xff5cc8d6 };
} // namespace palette

class GrainLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GrainLookAndFeel()
    {
        using namespace palette;
        setColour(juce::ResizableWindow::backgroundColourId, background);
        setColour(juce::Label::textColourId, text);
        setColour(juce::TextButton::buttonColourId, panel);
        setColour(juce::TextButton::buttonOnColourId, accent);
        setColour(juce::TextButton::textColourOffId, text);
        setColour(juce::TextButton::textColourOnId, background);
        setColour(juce::ComboBox::backgroundColourId, panel);
        setColour(juce::ComboBox::textColourId, text);
        setColour(juce::ComboBox::outlineColourId, track);
        setColour(juce::ComboBox::arrowColourId, accent);
        setColour(juce::PopupMenu::backgroundColourId, panel);
        setColour(juce::PopupMenu::textColourId, text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, accentDim);
        setColour(juce::TooltipWindow::backgroundColourId, panel);
        setColour(juce::TooltipWindow::textColourId, text);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle,
                          juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w),
                                                   static_cast<float>(h)).reduced(6.0f);
        const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float thickness = std::max(3.0f, radius * 0.16f);
        const float arcRadius = radius - thickness * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        juce::Path trackPath;
        trackPath.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour(palette::track);
        g.strokePath(trackPath, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valuePath;
        valuePath.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour(slider.isEnabled() ? palette::accent : palette::accentDim);
        g.strokePath(valuePath, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // body with a soft top light, then the pointer
        const float bodyRadius = radius - thickness - 4.0f;
        g.setGradientFill(juce::ColourGradient(palette::panel.brighter(0.25f), centre.x, centre.y - bodyRadius,
                                               palette::panel.darker(0.4f), centre.x, centre.y + bodyRadius, false));
        g.fillEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

        juce::Path pointer;
        pointer.addRoundedRectangle(-1.5f, -bodyRadius, 3.0f, bodyRadius * 0.55f, 1.5f);
        g.setColour(palette::text);
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        if (slider.hasKeyboardFocus(false))
        {
            g.setColour(palette::accent.withAlpha(0.6f));
            g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);
        }
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down) override
    {
        const auto area = b.getLocalBounds().toFloat().reduced(1.0f);
        const bool on = b.getToggleState();
        auto fill = on ? palette::accent : palette::panel;
        if (! on && highlighted)
            fill = fill.brighter(0.15f);
        if (down) // feedback on press, not on release
            fill = on ? fill.darker(0.15f) : fill.brighter(0.3f);
        g.setColour(fill);
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(on ? palette::accent : palette::track);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);
        if (b.hasKeyboardFocus(false))
        {
            g.setColour(palette::learn);
            g.drawRoundedRectangle(area.reduced(2.0f), 4.0f, 1.0f);
        }
    }

    juce::Font getTextButtonFont(juce::TextButton&, int height) override
    {
        return juce::FontOptions(static_cast<float>(height) * 0.42f);
    }
};

// One parameter shown as a row of radio buttons (family, variant, loop state, ...).
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
            b->setConnectedEdges((i > 0 ? juce::Button::ConnectedOnLeft : 0)
                                 | (i < labels.size() - 1 ? juce::Button::ConnectedOnRight : 0));
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

// Rotary knob with a name label and a value read-out; double-click resets to the default.
class Knob : public juce::Component
{
public:
    Knob(juce::RangedAudioParameter& param, const juce::String& labelText)
        : name(labelText), attachment(param, slider)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        slider.setColour(juce::Slider::textBoxTextColourId, palette::textDim);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setRotaryParameters(juce::degreesToRadians(225.0f), juce::degreesToRadians(495.0f), true);
        slider.setDoubleClickReturnValue(true, param.convertFrom0to1(param.getDefaultValue()));
        slider.setTitle(labelText);
        slider.setTooltip(labelText + ": drag, double-click to reset, right-click for MIDI learn");
        // read-outs: Hz / kHz for the filter, percent for the 0..1 macros (the attachment's own text has 7 decimals)
        if (param.getLabel() == "dB")
        {
            slider.textFromValueFunction = [](double v) { return (v > 0.0 ? "+" : "") + juce::String(v, 1) + " dB"; };
            slider.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue(); };
        }
        else if (param.getLabel() == "Hz")
        {
            slider.textFromValueFunction = [](double v) {
                return v >= 1000.0 ? juce::String(v / 1000.0, 1) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz";
            };
            slider.valueFromTextFunction = [](const juce::String& t) {
                return t.getDoubleValue() * (t.containsIgnoreCase("k") ? 1000.0 : 1.0);
            };
        }
        else
        {
            slider.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; };
            slider.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue() / 100.0; };
        }
        slider.updateText();
        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(slider);
        addAndMakeVisible(label);
    }

    void setCaption(const juce::String& extra)
    {
        label.setText(extra.isEmpty() ? name : name + "  " + extra, juce::dontSendNotification);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds(area.removeFromTop(20));
        slider.setBounds(area);
    }

    juce::Slider slider;

private:
    juce::String name;
    juce::Label label;
    juce::SliderParameterAttachment attachment;
};
} // namespace grain::gui
