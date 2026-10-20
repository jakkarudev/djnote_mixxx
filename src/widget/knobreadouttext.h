#pragma once

#include <QString>

#include "preferences/configobject.h"

// Knob readout text: the text of the knob adjustment readout (see
// GLOSSARY.md). Pure: no widget, control or painting dependencies, so the
// whole unit table is testable in isolation (knobreadouttexttest.cpp).
namespace mixxx {
namespace knobreadout {

// What a knob is, as far as its readout is concerned. Resolved by the caller
// from the knob's connected control. More facts (an effect parameter's units
// hint, the EQ maximum) join here as the unit table grows.
struct KnobIdentity {
    ConfigKey key;
};

// A knob's state at the moment of the readout.
struct KnobValue {
    // The control's value, e.g. the linear gain factor of a gain knob.
    double value = 0.0;
    // The knob position, 0 (fully left) .. 1 (fully right).
    double parameter = 0.0;
};

// How the text is written. Simple/Precise mode joins here.
struct Options {
    // The compact form for the knob-face style: see compactForm().
    bool compact = false;
};

// Audio-taper gain controls (deck, sampler, microphone and auxiliary pregain;
// main, headphone and booth gain) read in dB with one decimal place, "minus infinity
// dB" at 0, with a true minus sign (U+2212). Every other knob reads as its 0-100% knob
// position.
QString text(const KnobIdentity& knob,
        const KnobValue& value,
        const Options& options = Options{});

// The compact form of a readout text, for the knob-face style: the unit is
// dropped, and so are a "+" sign and the space after a leading label
// ("+12.0 dB" -> "12.0", "40%" -> "40", "L 30%" -> "L30"). Generic, so it
// applies to every unit family; text without a number is returned unchanged.
QString compactForm(const QString& fullText);

} // namespace knobreadout
} // namespace mixxx
