#pragma once

#include "preferences/usersettings.h"

// The key matching settings (see GLOSSARY.md, "Key matching"), shown on the
// DJNote preferences page and stored under [DJNote] in `KeyMatching...` keys.
namespace mixxx {
namespace keymatch {

// Which tiers are highlighted.
enum class Highlight {
    Off,
    StrictOnly,
    StrictAndFuzzy,
};

// Add a field here, its key in keymatchsettings.cpp and its control on
// DlgPrefDJNote; the key matcher reloads the whole struct when the page changes
// something, so a new setting applies live without more plumbing.
struct Settings {
    Highlight highlight = Highlight::StrictAndFuzzy;
};

// Missing keys fall back to the defaults above.
Settings loadSettings(const UserSettingsPointer& pConfig);
void saveSettings(const UserSettingsPointer& pConfig, const Settings& settings);

} // namespace keymatch
} // namespace mixxx
