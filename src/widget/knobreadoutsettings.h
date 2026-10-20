#pragma once

#include "preferences/usersettings.h"

// The knob adjustment readout's settings (see GLOSSARY.md), shown on the
// DJNote preferences page and stored under [DJNote] in DJNote's own keys.
namespace mixxx {
namespace knobreadout {

// Add a field here, its key in knobreadoutsettings.cpp and its control on
// DlgPrefDJNote; the overlay reads the whole struct on every readout, so a new
// setting applies live without more plumbing.
// Where the readout is drawn.
enum class Style {
    // Just under the knob, in full form.
    UnderKnob,
    // Centred on the knob face, in compact form, shrunk to fit; falls back to
    // under the knob when even the compact text doesn't fit.
    KnobFace,
};

struct Settings {
    // Show knob adjustment readout.
    bool enabled = true;
    Style style = Style::UnderKnob;
};

// Missing keys fall back to the defaults above.
Settings loadSettings(const UserSettingsPointer& pConfig);
void saveSettings(const UserSettingsPointer& pConfig, const Settings& settings);

} // namespace knobreadout
} // namespace mixxx
