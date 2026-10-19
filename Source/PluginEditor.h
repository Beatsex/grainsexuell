#pragma once

#include "PluginProcessor.h"
#include "gui/Widgets.h"

class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override; // right-click: MIDI learn menu

    static constexpr int designWidth = 760, designHeight = 470;

private:
    struct Target
    {
        juce::Component* component;
        int param; // index in ids::mappable
    };

    void timerCallback() override;
    void addTarget(juce::Component& c, const char* id);
    void addToggle(juce::TextButton& button, const char* id, const juce::String& text);
    void showLearnMenu(int param);
    void showPresetMenu();
    void stepPreset(int direction);
    void askAndSavePreset();
    void updateVariantLabels();

    PluginProcessor& processor;
    grain::gui::GrainLookAndFeel look;
    juce::TooltipWindow tooltips { this, 600 };

    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, presetName, savePreset { "Save" };
    grain::gui::ChoiceRow family, variant;
    juce::OwnedArray<grain::gui::Knob> knobs;
    grain::gui::ChoiceRow hazeMode, loop, loopSpeed, loopPos;
    juce::TextButton autoGain, hold, loopReverse, loopQuant, loopBurst, loopUndo;
    juce::ParameterAttachment familyWatcher;
    juce::OwnedArray<juce::ButtonParameterAttachment> toggleAttachments;
    juce::Label status, meter;

    std::vector<Target> targets;
    std::vector<int> knobParams; // index in ids::mappable of each knob
};
