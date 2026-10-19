#pragma once

#include "FactoryPresets.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <string_view>

namespace grain::presets
{
// Applies factory presets and saves / loads user presets (small XML files in one folder). Message thread only:
// it does file I/O and notifies the host about parameter changes, so never call it from processBlock.
//
// A user preset file only ever sets the known sound parameters; unknown ids are ignored, values are clamped, NaN is
// skipped, files over 64 KB or with the wrong root are refused. Names are sanitised so a name can never leave the folder.
class PresetManager
{
public:
    static constexpr int maxFileBytes = 64 * 1024;
    static constexpr int maxNameLength = 64;

    PresetManager(juce::AudioProcessorValueTreeState& state, juce::File userFolder);

    static juce::File defaultUserFolder(); // <app data>/grainsexuell/Presets; not created until the first save

    // Returns the trimmed name, or an empty string if it is not allowed as a file name.
    static juce::String sanitiseName(const juce::String& name);

    // Cheap single-pass check before handing text to the recursive XML parser: no DOCTYPE / ENTITY declarations
    // (entity expansion) and nesting no deeper than maxXmlDepth (stack overflow).
    static constexpr int maxXmlDepth = 32;
    static bool isSafeXml(std::string_view text);

    bool applyFactory(int index);
    bool applyUser(const juce::String& name);
    bool saveUser(const juce::String& name);
    bool deleteUser(const juce::String& name);
    juce::StringArray userPresetNames() const; // sorted

    juce::String currentName() const;
    void setCurrentName(const juce::String& name);
    int currentFactoryIndex() const; // -1 when the current preset is not a factory one

private:
    juce::File fileFor(const juce::String& cleanName) const;
    bool applyXml(const juce::XmlElement& xml);
    void setSound(const char* id, float plain);
    void resetSlotB();

    juce::AudioProcessorValueTreeState& apvts;
    juce::File folder;
    mutable juce::CriticalSection nameLock;
    juce::String name;
    int factoryIndex = -1;
};
} // namespace grain::presets
