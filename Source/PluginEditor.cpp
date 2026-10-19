#include "PluginEditor.h"

namespace
{
using namespace grain;

int indexOf(const char* id)
{
    for (size_t i = 0; i < ids::mappable.size(); ++i)
        if (juce::String(ids::mappable[i]) == id)
            return static_cast<int>(i);
    jassertfalse;
    return 0;
}

juce::RangedAudioParameter& param(PluginProcessor& p, const char* id)
{
    auto* prm = p.apvts.getParameter(id);
    jassert(prm != nullptr);
    return *prm;
}

const juce::StringArray variantNames[4] {
    { "Stack", "Low", "High", "Tumble" },      // Mosaic
    { "Mist", "Shimmer", "Murk", "Backwash" }, // Cloud
    { "Shuffle", "Crush", "Arp", "Stutter" },  // Shard
    { "Echo", "Ping-pong", "Reverse", "Cascade" } // Tap
};
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      family(param(p, ids::family), { "Mosaic", "Cloud", "Shard", "Tap" }, "Family"),
      variant(param(p, ids::variant), variantNames[0], "Variant"),
      hazeMode(param(p, ids::hazemode), { "Room", "Plate", "Hall", "Swell" }, "Haze mode"),
      loop(param(p, ids::loop), { "Off", "Rec", "Play", "Dub" }, "Loop"),
      loopSpeed(param(p, ids::loopspeed), { "1/2", "1x", "2x" }, "Loop speed"),
      loopPos(param(p, ids::looppos), { "Pre-FX", "Post-FX" }, "Loop position"),
      familyWatcher(param(p, ids::family), [this](float) { updateVariantLabels(); })
{
    setLookAndFeel(&look);
    setTitle("grainsexuell");

    for (auto* c : { &prevPreset, &nextPreset, &presetName, &savePreset })
        addAndMakeVisible(c);
    prevPreset.setTitle("Previous preset");
    nextPreset.setTitle("Next preset");
    presetName.setTitle("Preset menu");
    presetName.setTooltip("Presets: factory, yours, save, delete");
    savePreset.setTitle("Save preset");
    prevPreset.onClick = [this] { stepPreset(-1); };
    nextPreset.onClick = [this] { stepPreset(1); };
    presetName.onClick = [this] { showPresetMenu(); };
    savePreset.onClick = [this] { askAndSavePreset(); };

    addAndMakeVisible(family);
    addAndMakeVisible(variant);
    addTarget(family, ids::family);
    addTarget(variant, ids::variant);

    const std::pair<const char*, const char*> knobList[] { { ids::ingain, "In" },    { ids::swarm, "Swarm" }, { ids::tail, "Tail" },
                                                           { ids::span, "Span" },   { ids::scatter, "Scatter" },
                                                           { ids::bend, "Bend" },   { ids::dusk, "Dusk" },
                                                           { ids::haze, "Haze" },   { ids::mix, "Mix" },
                                                           { ids::outgain, "Out" } };
    for (const auto& [id, text] : knobList)
    {
        auto* k = knobs.add(new gui::Knob(param(p, id), text));
        addAndMakeVisible(k);
        addTarget(*k, id);
        knobParams.push_back(indexOf(id));
    }

    addAndMakeVisible(hazeMode);
    addTarget(hazeMode, ids::hazemode);
    addToggle(autoGain, ids::autogain, "Auto Gain");
    addToggle(hold, ids::hold, "Hold");

    addAndMakeVisible(loop);
    addAndMakeVisible(loopSpeed);
    addAndMakeVisible(loopPos);
    addTarget(loop, ids::loop);
    addTarget(loopSpeed, ids::loopspeed);
    addTarget(loopPos, ids::looppos);
    addToggle(loopReverse, ids::loopreverse, "Reverse");
    addToggle(loopQuant, ids::loopquant, "Quantize");
    addToggle(loopBurst, ids::loopburst, "Burst");

    // Undo is momentary: on while pressed (the looper reacts to the rising edge)
    addAndMakeVisible(loopUndo);
    loopUndo.setButtonText("Undo");
    loopUndo.setTitle("Loop undo");
    loopUndo.onStateChange = [this] {
        auto& prm = param(processor, ids::loopundo);
        const float want = loopUndo.isDown() ? 1.0f : 0.0f;
        if (prm.getValue() != want)
            prm.setValueNotifyingHost(want);
    };
    addTarget(loopUndo, ids::loopundo);

    for (auto* l : { &status, &meter })
    {
        l->setColour(juce::Label::textColourId, gui::palette::textDim);
        l->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(l);
    }
    meter.setJustificationType(juce::Justification::centredRight);
    status.setText("Right-click any control to assign a MIDI CC", juce::dontSendNotification);

    addMouseListener(this, true);

    // fixed aspect ratio, size remembered in the plugin state (read first: setResizeLimits resizes the editor)
    const int savedWidth = processor.editorWidth.load();
    setResizable(true, true);
    setResizeLimits(designWidth * 2 / 3, designHeight * 2 / 3, designWidth * 3, designHeight * 3);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(designWidth) / designHeight);
    const int w = juce::jlimit(designWidth * 2 / 3, designWidth * 3, savedWidth);
    setSize(w, w * designHeight / designWidth);

    updateVariantLabels();
    startTimerHz(15);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void PluginEditor::addTarget(juce::Component& c, const char* id)
{
    targets.push_back({ &c, indexOf(id) });
}

void PluginEditor::addToggle(juce::TextButton& button, const char* id, const juce::String& text)
{
    addAndMakeVisible(button);
    button.setButtonText(text);
    button.setTitle(text);
    toggleAttachments.add(new juce::ButtonParameterAttachment(param(processor, id), button));
    button.setClickingTogglesState(true);
    addTarget(button, id);
}

void PluginEditor::updateVariantLabels()
{
    const int f = juce::jlimit(0, 3, juce::roundToInt(param(processor, ids::family).convertFrom0to1(
                                         param(processor, ids::family).getValue())));
    variant.setLabels(variantNames[f]);
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(gui::palette::background);
    const float s = static_cast<float>(getWidth()) / designWidth;
    g.setColour(gui::palette::panel);
    g.fillRoundedRectangle(12.0f * s, 132.0f * s, (designWidth - 24.0f) * s, 198.0f * s, 8.0f * s);
    g.fillRoundedRectangle(12.0f * s, 336.0f * s, (designWidth - 24.0f) * s, 92.0f * s, 8.0f * s);
    g.setColour(gui::palette::textDim);
    g.setFont(juce::FontOptions(11.0f * s));
    g.drawText("LOOPER", juce::Rectangle<float>(24.0f * s, 340.0f * s, 80.0f * s, 14.0f * s), juce::Justification::left);
}

void PluginEditor::resized()
{
    const float s = static_cast<float>(getWidth()) / designWidth;
    auto place = [s](juce::Component& c, float x, float y, float w, float h) {
        c.setBounds(juce::Rectangle<float>(x * s, y * s, w * s, h * s).toNearestInt());
    };

    place(prevPreset, 12, 10, 32, 30);
    place(presetName, 48, 10, 300, 30);
    place(nextPreset, 352, 10, 32, 30);
    place(savePreset, 392, 10, 64, 30);
    place(meter, 560, 10, 188, 30);

    place(family, 12, 52, 420, 34);
    place(variant, 12, 94, 420, 34);
    place(hazeMode, 452, 52, 296, 34);
    place(autoGain, 452, 94, 144, 34);
    place(hold, 604, 94, 144, 34);

    const float knobWidth = (designWidth - 24.0f) / static_cast<float>(knobs.size());
    for (int i = 0; i < knobs.size(); ++i)
        place(*knobs[i], 12.0f + knobWidth * static_cast<float>(i), 136, knobWidth, 190);

    place(loop, 24, 358, 220, 34);
    place(loopSpeed, 256, 358, 150, 34);
    place(loopPos, 418, 358, 140, 34);
    place(loopReverse, 24, 394, 110, 30);
    place(loopQuant, 142, 394, 110, 30);
    place(loopBurst, 260, 394, 110, 30);
    place(loopUndo, 378, 394, 110, 30);
    place(status, 12, 436, 736, 24);

    if (getWidth() >= designWidth * 2 / 3) // not while the window is being torn down
        processor.editorWidth = getWidth();
}

void PluginEditor::timerCallback()
{
    auto& map = processor.midiMap;
    const int learning = map.learning();

    // captions: "CC n" on mapped knobs, "learning" while waiting
    for (int i = 0; i < knobs.size(); ++i)
    {
        const int cc = map.ccFor(knobParams[static_cast<size_t>(i)]);
        knobs[i]->setCaption(learning == knobParams[static_cast<size_t>(i)] ? "(move a CC)" : (cc >= 0 ? "CC " + juce::String(cc) : juce::String()));
    }
    if (learning >= 0)
        status.setText(juce::String("Learning ") + ids::mappable[static_cast<size_t>(learning)]
                           + ": move a knob or fader on your controller",
                       juce::dontSendNotification);
    else if (status.getText().startsWith("Learning"))
        status.setText("Assigned. Right-click any control to assign a MIDI CC", juce::dontSendNotification);

    const auto name = processor.presets.currentName();
    presetName.setButtonText(name.isEmpty() ? "Init" : name);

    const bool ag = param(processor, ids::autogain).getValue() > 0.5f;
    meter.setText(ag ? juce::String::formatted("Auto Gain %+.1f dB", processor.autoGainDb()) : juce::String(),
                  juce::dontSendNotification);
}

void PluginEditor::mouseDown(const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        return;
    for (auto* c = e.eventComponent; c != nullptr && c != this; c = c->getParentComponent())
        for (const auto& t : targets)
            if (t.component == c)
            {
                showLearnMenu(t.param);
                return;
            }
}

void PluginEditor::showLearnMenu(int paramIndex)
{
    auto& map = processor.midiMap;
    const int cc = map.ccFor(paramIndex);
    juce::PopupMenu menu;
    menu.addSectionHeader(juce::String(ids::mappable[static_cast<size_t>(paramIndex)]));
    menu.addItem(1, "MIDI learn: move a controller after clicking");
    menu.addItem(2, cc >= 0 ? "Clear MIDI (CC " + juce::String(cc) + ")" : juce::String("Clear MIDI"), cc >= 0);
    menu.addItem(3, "Cancel learning", map.learning() >= 0);
    menu.showMenuAsync(juce::PopupMenu::Options(), [this, paramIndex](int result) {
        if (result == 1)
            processor.midiMap.startLearn(paramIndex);
        else if (result == 2)
            processor.midiMap.clear(paramIndex);
        else if (result == 3)
            processor.midiMap.cancelLearn();
    });
}

void PluginEditor::stepPreset(int direction)
{
    // one list: the factory presets, then the user presets
    juce::StringArray names;
    for (int i = 0; i < presets::numFactory; ++i)
        names.add(presets::factory()[static_cast<size_t>(i)].name);
    const int numFactory = names.size();
    names.addArray(processor.presets.userPresetNames());

    int at = names.indexOf(processor.presets.currentName());
    at = at < 0 ? (direction > 0 ? 0 : names.size() - 1) : (at + direction + names.size()) % names.size();
    if (at < numFactory)
        processor.presets.applyFactory(at);
    else
        processor.presets.applyUser(names[at]);
}

void PluginEditor::showPresetMenu()
{
    juce::PopupMenu menu, factoryMenu, userMenu, deleteMenu;
    const char* families[] { "Mosaic", "Cloud", "Shard", "Tap" };
    for (int f = 0; f < 4; ++f)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < 4; ++i)
            sub.addItem(100 + f * 4 + i, presets::factory()[static_cast<size_t>(f * 4 + i)].name);
        factoryMenu.addSubMenu(families[f], sub);
    }
    menu.addSubMenu("Factory", factoryMenu);

    const auto users = processor.presets.userPresetNames();
    for (int i = 0; i < users.size(); ++i)
    {
        userMenu.addItem(1000 + i, users[i]);
        deleteMenu.addItem(2000 + i, users[i]);
    }
    menu.addSubMenu("User", userMenu, users.size() > 0);
    menu.addSeparator();
    menu.addItem(1, "Save as...");
    menu.addSubMenu("Delete", deleteMenu, users.size() > 0);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetName),
                       [this, users](int result) {
                           if (result >= 100 && result < 116)
                               processor.presets.applyFactory(result - 100);
                           else if (result >= 1000 && result < 1000 + users.size())
                               processor.presets.applyUser(users[result - 1000]);
                           else if (result >= 2000 && result < 2000 + users.size())
                               processor.presets.deleteUser(users[result - 2000]);
                           else if (result == 1)
                               askAndSavePreset();
                       });
}

void PluginEditor::askAndSavePreset()
{
    auto* window = new juce::AlertWindow("Save preset", "Name for this preset:", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor("name", processor.presets.currentName());
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PluginEditor> safe(this);
    window->enterModalState(true,
                            juce::ModalCallbackFunction::create([safe, window](int result) {
                                if (result == 1 && safe != nullptr)
                                {
                                    const auto name = window->getTextEditorContents("name");
                                    if (! safe->processor.presets.saveUser(name))
                                        safe->status.setText("Could not save: that name is not allowed",
                                                             juce::dontSendNotification);
                                }
                            }),
                            true);
}
