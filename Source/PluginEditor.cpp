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

// layout, in design pixels (1040 x 620): header, body (pink left column + right column of rows), footer
constexpr float headerH = 44, footerH = 36, leftW = 560, cellH = 40, rowH = 36;
constexpr float slotLabelW = 40;           // the "A" / "B" column on the pink panel
constexpr float variantBW = 320;           // variant B cells, then the routing cells
constexpr float loopGroups[] { 216, 360 }; // x offsets of the looper group boundaries (state | speed | position)
constexpr float bodyTop = headerH, bodyBottom = 620 - footerH;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      family(param(p, ids::family), { "Mosaic", "Cloud", "Shard", "Tap" }, "Family"),
      variant(param(p, ids::variant), variantNames[0], "Variant"),
      familyB(param(p, ids::family2), { "B off", "Mosaic", "Cloud", "Shard", "Tap" }, "Family B"),
      variantB(param(p, ids::variant2), { "A", "B", "C", "D" }, "Variant B"),
      routeB(param(p, ids::route2), { "Serial", "Parallel" }, "Routing B"),
      hazeMode(param(p, ids::hazemode), { "Room", "Plate", "Hall", "Swell" }, "Haze mode"),
      loop(param(p, ids::loop), { "Off", "Rec", "Play", "Dub" }, "Loop"),
      loopSpeed(param(p, ids::loopspeed), { "1/2", "1x", "2x" }, "Loop speed"),
      loopPos(param(p, ids::looppos), { "Pre-FX", "Post-FX" }, "Loop position"),
      familyWatcher(param(p, ids::family), [this](float) { updateVariantLabels(); }),
      familyBWatcher(param(p, ids::family2), [this](float) { updateVariantLabels(); })
{
    setLookAndFeel(&look);
    setTitle("grainsexuell");

    for (auto* c : { &presetName, &prevPreset, &nextPreset, &savePreset })
        addAndMakeVisible(c);
    prevPreset.setTitle("Previous preset");
    nextPreset.setTitle("Next preset");
    presetName.setTitle("Preset menu");
    presetName.setTooltip("Presets: factory, yours, save, delete");
    presetName.getProperties().set("menu", true);
    savePreset.setTitle("Save preset");
    prevPreset.onClick = [this] { stepPreset(-1); };
    nextPreset.onClick = [this] { stepPreset(1); };
    presetName.onClick = [this] { showPresetMenu(); };
    savePreset.onClick = [this] { askAndSavePreset(); };

    for (auto* row : { &family, &variant, &familyB, &variantB, &routeB })
        addAndMakeVisible(row);
    addTarget(family, ids::family);
    addTarget(variant, ids::variant);
    addTarget(familyB, ids::family2);
    addTarget(variantB, ids::variant2);
    addTarget(routeB, ids::route2);
    addAndMakeVisible(activity);

    // signal order: input trim, the macros, the effects, both mixes, output trim
    const std::pair<const char*, const char*> rowList[] {
        { ids::ingain, "In" },   { ids::swarm, "Swarm" }, { ids::tail, "Tail" }, { ids::span, "Span" },
        { ids::scatter, "Scatter" }, { ids::bend, "Bend" }, { ids::dusk, "Dusk" }, { ids::haze, "Haze" },
        { ids::mix, "Mix" },     { ids::mix2, "Mix B" },  { ids::outgain, "Out" }
    };
    int number = 1;
    for (const auto& [id, text] : rowList)
    {
        auto* r = rows.add(new gui::ParamRow(param(p, id), text, number++));
        addAndMakeVisible(r);
        addTarget(*r, id);
        rowParams.push_back(indexOf(id));
    }

    addAndMakeVisible(hazeMode);
    addTarget(hazeMode, ids::hazemode);
    addToggle(autoGain, ids::autogain, "Auto Gain");
    addToggle(hold, ids::hold, "Hold");

    for (auto* row : { &loop, &loopSpeed, &loopPos })
        addAndMakeVisible(row);
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
        l->setColour(juce::Label::textColourId, gui::palette::ink);
        l->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(l);
    }
    meter.setJustificationType(juce::Justification::centredRight);
    status.setText("RIGHT-CLICK ANY CONTROL TO ASSIGN A MIDI CC", juce::dontSendNotification);

    addMouseListener(this, true);

    // fixed aspect ratio, size remembered in the plugin state (read first: setResizeLimits resizes the editor)
    const int savedWidth = processor.editorWidth.load();
    setResizable(true, true);
    setResizeLimits(designWidth * 4 / 5, designHeight * 4 / 5, designWidth * 2, designHeight * 2); // smaller: labels under 9 px
    getConstrainer()->setFixedAspectRatio(static_cast<double>(designWidth) / designHeight);
    const int w = juce::jlimit(designWidth * 4 / 5, designWidth * 2, savedWidth);
    setSize(w, w * designHeight / designWidth);

    updateVariantLabels();
    timerCallback(); // preset name, captions and status before the first tick
    startTimerHz(gui::ActivityTrace::ticksPerSecond);
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
    button.getProperties().set("switch", true); // drawn with an on/off marker, unlike radio cells
    addTarget(button, id);
}

void PluginEditor::updateVariantLabels()
{
    const int f = juce::jlimit(0, 3, juce::roundToInt(param(processor, ids::family).convertFrom0to1(
                                         param(processor, ids::family).getValue())));
    variant.setLabels(variantNames[f]);

    const int fb = juce::roundToInt(param(processor, ids::family2).convertFrom0to1(param(processor, ids::family2).getValue())) - 1;
    variantB.setLabels(fb >= 0 && fb < 4 ? variantNames[fb] : juce::StringArray { "A", "B", "C", "D" });
    variantB.setEnabled(fb >= 0);
    routeB.setEnabled(fb >= 0);
}

void PluginEditor::paint(juce::Graphics& g)
{
    using namespace gui;
    const float s = static_cast<float>(getWidth()) / designWidth;
    const auto r = [s](float x, float y, float w, float h) { return juce::Rectangle<float>(x * s, y * s, w * s, h * s); };

    g.fillAll(palette::paper);
    g.setColour(palette::pink);
    g.fillRect(r(0, bodyTop, leftW, bodyBottom - bodyTop));

    // hairlines: header and footer rules, the column divider, the rules under the selector rows
    g.setColour(palette::ink);
    g.fillRect(r(0, headerH - 1, designWidth, 1));
    g.fillRect(r(0, bodyBottom, designWidth, 1));
    g.fillRect(r(leftW - 1, 0, 1, bodyBottom));
    for (int i = 1; i <= 4; ++i)
        g.fillRect(r(i == 2 ? 0 : slotLabelW, bodyTop + cellH * static_cast<float>(i) - 1, leftW - (i == 2 ? 0 : slotLabelW), 1));
    g.fillRect(r(slotLabelW - 1, bodyTop, 1, cellH * 4));
    g.setFont(sans(14.0f * s, true));
    g.drawText("A", r(0, bodyTop, slotLabelW, cellH * 2), juce::Justification::centred);
    g.drawText("B", r(0, bodyTop + cellH * 2, slotLabelW, cellH * 2), juce::Justification::centred);
    for (float y : { 440.0f, 476.0f, 512.0f, 548.0f })
        g.fillRect(r(leftW, y + rowH - 1, designWidth - leftW, 1));
    g.drawRect(getLocalBounds(), 1);

    // header text: name left, maker right
    g.setFont(sans(12.0f * s, true));
    g.drawText("GRAINSEXUELL", r(28, 0, 240, headerH), juce::Justification::centredLeft);
    g.setFont(sans(12.0f * s));
    g.drawText("GRANULAR LOOP FX  /  BEATSEXUELL", r(leftW - 28 - 300, 0, 300, headerH), juce::Justification::centredRight);
}

void PluginEditor::paintOverChildren(juce::Graphics& g)
{
    // group dividers: a paper gap between ink rules, so two selected (black) cells of different groups never merge
    const float s = static_cast<float>(getWidth()) / designWidth;
    const auto divider = [&g, s](float x, float y, float h) {
        g.setColour(gui::palette::paper);
        g.fillRect(juce::Rectangle<float>((x - 2) * s, y * s, 4 * s, h * s));
        g.setColour(gui::palette::ink);
        g.fillRect(juce::Rectangle<float>((x - 2) * s - 1, y * s, 1, h * s));
        g.fillRect(juce::Rectangle<float>((x + 2) * s, y * s, 1, h * s));
    };
    divider(slotLabelW + variantBW, bodyTop + cellH * 3, cellH - 1);
    for (float x : loopGroups)
        divider(leftW + x, 512, rowH - 1);
}

void PluginEditor::resized()
{
    const float s = static_cast<float>(getWidth()) / designWidth;
    look.scale = s;
    auto place = [s](juce::Component& c, float x, float y, float w, float h) {
        c.setBounds(juce::Rectangle<float>(x * s, y * s, w * s, h * s).toNearestInt());
    };

    // header: preset name, previous, next, save
    place(presetName, leftW, 0, 312, headerH - 1);
    place(prevPreset, leftW + 312, 0, 44, headerH - 1);
    place(nextPreset, leftW + 356, 0, 44, headerH - 1);
    place(savePreset, leftW + 400, 0, 80, headerH - 1);

    // left column on pink: slot A, slot B, the activity readout
    const float cellsW = leftW - 1 - slotLabelW;
    place(family, slotLabelW, bodyTop, cellsW, cellH - 1);
    place(variant, slotLabelW, bodyTop + cellH, cellsW, cellH - 1);
    place(familyB, slotLabelW, bodyTop + cellH * 2, cellsW, cellH - 1);
    place(variantB, slotLabelW, bodyTop + cellH * 3, variantBW, cellH - 1);
    place(routeB, slotLabelW + variantBW, bodyTop + cellH * 3, cellsW - variantBW, cellH - 1);
    place(activity, 0, bodyTop + cellH * 4, leftW - 1, bodyBottom - bodyTop - cellH * 4);
    activity.setScale(s);

    // right column: 11 parameter rows, then the switch rows
    for (int i = 0; i < rows.size(); ++i)
    {
        place(*rows[i], leftW, bodyTop + rowH * static_cast<float>(i), designWidth - leftW, rowH);
        rows[i]->setScale(s);
    }
    place(hazeMode, leftW, 440, 480, rowH - 1);
    place(autoGain, leftW, 476, 240, rowH - 1);
    place(hold, leftW + 240, 476, 240, rowH - 1);
    place(loop, leftW, 512, loopGroups[0], rowH - 1);
    place(loopSpeed, leftW + loopGroups[0], 512, loopGroups[1] - loopGroups[0], rowH - 1);
    place(loopPos, leftW + loopGroups[1], 512, 480 - loopGroups[1], rowH - 1);
    place(loopReverse, leftW, 548, 120, rowH - 1);
    place(loopQuant, leftW + 120, 548, 120, rowH - 1);
    place(loopBurst, leftW + 240, 548, 120, rowH - 1);
    place(loopUndo, leftW + 360, 548, 120, rowH - 1);

    // footer
    for (auto* l : { &status, &meter })
        l->setFont(gui::mono(12.0f * s));
    place(status, 28, bodyBottom + 1, 640, footerH - 1);
    place(meter, 680, bodyBottom + 1, designWidth - 680 - 28, footerH - 1);

    if (getWidth() >= designWidth * 4 / 5) // not while the window is being torn down
        processor.editorWidth = getWidth();
}

void PluginEditor::timerCallback()
{
    activity.push(processor.activityTotal());

    auto& map = processor.midiMap;
    const int learning = map.learning();

    // captions: "CC n" on mapped rows, "learning" while waiting
    for (int i = 0; i < rows.size(); ++i)
    {
        const int id = rowParams[static_cast<size_t>(i)];
        const int cc = map.ccFor(id);
        rows[i]->setCaption(learning == id ? "(move a CC)" : (cc >= 0 ? "CC " + juce::String(cc) : juce::String()));
    }
    if (learning >= 0)
        status.setText(juce::String("LEARNING ") + juce::String(ids::mappable[static_cast<size_t>(learning)]).toUpperCase()
                           + ": MOVE A CONTROL ON YOUR CONTROLLER",
                       juce::dontSendNotification);
    else if (status.getText().startsWith("LEARNING"))
        status.setText("ASSIGNED. RIGHT-CLICK ANY CONTROL TO ASSIGN A MIDI CC", juce::dontSendNotification);

    const auto name = processor.presets.currentName();
    presetName.setButtonText(name.isEmpty() ? "Init" : name);

    const bool ag = param(processor, ids::autogain).getValue() > 0.5f;
    meter.setText(ag ? juce::String::formatted("AUTO GAIN %+.1f DB", processor.autoGainDb()) : juce::String(),
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
    // one list: Init, the factory presets, then the user presets
    juce::StringArray names { presets::PresetManager::initName };
    for (int i = 0; i < presets::numFactory; ++i)
        names.add(presets::factory()[static_cast<size_t>(i)].name);
    const int numBuiltIn = names.size();
    names.addArray(processor.presets.userPresetNames());

    int at = names.indexOf(processor.presets.currentName());
    at = at < 0 ? (direction > 0 ? 0 : names.size() - 1) : (at + direction + names.size()) % names.size();
    if (at == 0)
        processor.presets.applyInit();
    else if (at < numBuiltIn)
        processor.presets.applyFactory(at - 1);
    else
        processor.presets.applyUser(names[at]);
}

void PluginEditor::showPresetMenu()
{
    juce::PopupMenu menu, factoryMenu, userMenu, deleteMenu;
    menu.addItem(2, "Init (all sound controls to default)");
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
                           else if (result == 2)
                               processor.presets.applyInit();
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
                                        safe->status.setText("COULD NOT SAVE: THAT NAME IS NOT ALLOWED",
                                                             juce::dontSendNotification);
                                }
                            }),
                            true);
}
