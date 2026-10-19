#include "PresetManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace grain::presets
{
namespace
{
constexpr int formatVersion = 1;

bool isSoundId(const juce::String& id)
{
    for (auto* known : soundIds)
        if (id == known)
            return true;
    for (auto* known : slotBIds)
        if (id == known)
            return true;
    return false;
}
} // namespace

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& state, juce::File userFolder)
    : apvts(state), folder(std::move(userFolder))
{
}

juce::File PresetManager::defaultUserFolder()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("grainsexuell")
        .getChildFile("Presets");
}

juce::String PresetManager::sanitiseName(const juce::String& raw)
{
    const auto clean = raw.trim();
    if (clean.isEmpty() || clean.length() > maxNameLength)
        return {};
    if (clean.startsWithChar('.') || clean.endsWithChar('.'))
        return {};
    for (auto c : clean)
        if (c < 32 || juce::String("\\/:*?\"<>|").containsChar(c))
            return {};

    // Windows device names are not usable as file names, with or without an extension
    const auto stem = clean.upToFirstOccurrenceOf(".", false, false).trim().toUpperCase();
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" || stem == "CONIN$" || stem == "CONOUT$")
        return {};
    if ((stem.startsWith("COM") || stem.startsWith("LPT")) && stem.length() == 4)
    {
        const auto digit = stem[3]; // 1..9, and the superscript digits Windows also treats as device numbers
        if ((digit >= '1' && digit <= '9') || digit == 0x00B9 || digit == 0x00B2 || digit == 0x00B3)
            return {};
    }
    return clean;
}

bool PresetManager::isSafeXml(std::string_view text)
{
    const auto startsWithNoCase = [&text](size_t at, std::string_view word) {
        if (at + word.size() > text.size())
            return false;
        for (size_t i = 0; i < word.size(); ++i)
            if (std::toupper(static_cast<unsigned char>(text[at + i])) != word[i])
                return false;
        return true;
    };

    int depth = 0;
    bool inTag = false, opening = false, inQuote = false;
    char quote = 0, last = 0;
    for (size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (! inTag)
        {
            if (c != '<')
                continue;
            if (startsWithNoCase(i + 2, "DOCTYPE") && text[i + 1] == '!')
                return false;
            if (startsWithNoCase(i + 2, "ENTITY") && text[i + 1] == '!')
                return false;
            const char next = i + 1 < text.size() ? text[i + 1] : '\0';
            if (next == '/')
                depth = std::max(0, depth - 1);
            opening = next != '/' && next != '!' && next != '?';
            inTag = true;
            inQuote = false;
            last = '<';
        }
        else if (inQuote)
        {
            inQuote = c != quote;
        }
        else if (c == '"' || c == '\'')
        {
            inQuote = true;
            quote = c;
        }
        else if (c == '>')
        {
            inTag = false;
            if (opening && last != '/' && ++depth > maxXmlDepth)
                return false;
        }
        else
            last = c;
    }
    return true;
}

juce::File PresetManager::fileFor(const juce::String& cleanName) const
{
    return folder.getChildFile(cleanName + ".xml");
}

void PresetManager::setSound(const char* id, float plain)
{
    auto* param = apvts.getParameter(id);
    if (param == nullptr || ! std::isfinite(plain))
        return;
    const bool isChoice = dynamic_cast<juce::AudioParameterChoice*>(param) != nullptr;
    // (no change gesture: gestures bracket a user dragging a control, and nest badly with a real drag)
    param->setValueNotifyingHost(param->convertTo0to1(isChoice ? std::round(plain) : plain)); // clamps to the range
}

void PresetManager::resetSlotB()
{
    for (auto* id : slotBIds)
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->getDefaultValue());
}

bool PresetManager::applyFactory(int index)
{
    if (index < 0 || index >= numFactory)
        return false;
    const auto& preset = factory()[static_cast<size_t>(index)];
    for (size_t i = 0; i < soundIds.size(); ++i)
        setSound(soundIds[i], preset.values[i]);
    resetSlotB();

    const juce::ScopedLock lock(nameLock);
    name = preset.name;
    factoryIndex = index;
    return true;
}

bool PresetManager::applyXml(const juce::XmlElement& xml)
{
    if (! xml.hasTagName("GrainPreset") || xml.getIntAttribute("version", 0) < 1)
        return false;
    resetSlotB(); // a file from before slot B leaves B off; a newer file sets it below
    for (auto* e : xml.getChildWithTagNameIterator("P"))
    {
        const auto id = e->getStringAttribute("id");
        if (isSoundId(id))
            setSound(id.toRawUTF8(), static_cast<float>(e->getDoubleAttribute("value", std::nan(""))));
    }
    return true;
}

bool PresetManager::applyUser(const juce::String& rawName)
{
    const auto clean = sanitiseName(rawName);
    if (clean.isEmpty())
        return false;
    const auto file = fileFor(clean);
    if (file.getParentDirectory() != folder || ! file.existsAsFile() || file.getSize() > maxFileBytes)
        return false;

    const auto text = file.loadFileAsString();
    if (text.getNumBytesAsUTF8() > static_cast<size_t>(maxFileBytes) || ! isSafeXml(text.toRawUTF8()))
        return false;
    const auto xml = juce::XmlDocument::parse(text);
    if (xml == nullptr || ! applyXml(*xml))
        return false;

    const juce::ScopedLock lock(nameLock);
    name = clean;
    factoryIndex = -1;
    return true;
}

bool PresetManager::saveUser(const juce::String& rawName)
{
    const auto clean = sanitiseName(rawName);
    if (clean.isEmpty())
        return false;
    const auto file = fileFor(clean);
    if (file.getParentDirectory() != folder || ! folder.createDirectory().wasOk())
        return false;

    juce::XmlElement xml("GrainPreset");
    xml.setAttribute("version", formatVersion);
    xml.setAttribute("name", clean);
    std::vector<const char*> ids(soundIds.begin(), soundIds.end());
    ids.insert(ids.end(), slotBIds.begin(), slotBIds.end());
    for (auto* id : ids)
        if (auto* param = apvts.getParameter(id))
        {
            auto* e = xml.createNewChildElement("P");
            e->setAttribute("id", id);
            e->setAttribute("value", static_cast<double>(param->convertFrom0to1(param->getValue())));
        }
    if (! xml.writeTo(file))
        return false;

    const juce::ScopedLock lock(nameLock);
    name = clean;
    factoryIndex = -1;
    return true;
}

bool PresetManager::deleteUser(const juce::String& rawName)
{
    const auto clean = sanitiseName(rawName);
    if (clean.isEmpty())
        return false;
    const auto file = fileFor(clean);
    return file.getParentDirectory() == folder && file.existsAsFile() && file.deleteFile();
}

juce::StringArray PresetManager::userPresetNames() const
{
    juce::StringArray names;
    for (const auto& f : folder.findChildFiles(juce::File::findFiles, false, "*.xml"))
        names.add(f.getFileNameWithoutExtension());
    names.sort(true);
    return names;
}

juce::String PresetManager::currentName() const
{
    const juce::ScopedLock lock(nameLock);
    return name;
}

void PresetManager::setCurrentName(const juce::String& newName)
{
    const juce::ScopedLock lock(nameLock);
    name = newName.substring(0, maxNameLength);
    factoryIndex = -1;
    for (int i = 0; i < numFactory; ++i)
        if (name == factory()[static_cast<size_t>(i)].name)
            factoryIndex = i;
}

int PresetManager::currentFactoryIndex() const
{
    const juce::ScopedLock lock(nameLock);
    return factoryIndex;
}
} // namespace grain::presets
