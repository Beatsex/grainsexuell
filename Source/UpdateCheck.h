#pragma once

#include <juce_core/juce_core.h>

#ifndef GRAIN_VERSION
#define GRAIN_VERSION "0.0.0" // set from project(VERSION) in CMakeLists.txt
#endif

namespace grain::update
{
// the editor asks GitHub once per session for the latest release; offline, private repo or no release: nothing shows
inline constexpr const char* latestApi = "https://api.github.com/repos/Beatsex/grainsexuell/releases/latest";
inline constexpr const char* releasesPage = "https://github.com/Beatsex/grainsexuell/releases";

// "v0.3.0" against "0.2.0": dotted numbers, a leading v and anything after a '-' (e.g. "-beta") ignored
inline bool isNewer(const juce::String& tag, const juce::String& current)
{
    const auto parts = [](const juce::String& v) {
        juce::StringArray p;
        p.addTokens(v.trim().trimCharactersAtStart("vV").upToFirstOccurrenceOf("-", false, false), ".", {});
        while (p.size() < 3)
            p.add("0");
        return p;
    };
    const auto a = parts(tag), b = parts(current);
    if (! a[0].containsOnly("0123456789") || a[0].isEmpty())
        return false;
    for (int i = 0; i < 3; ++i)
        if (a[i].getIntValue() != b[i].getIntValue())
            return a[i].getIntValue() > b[i].getIntValue();
    return false;
}
} // namespace grain::update
