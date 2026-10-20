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

// How the text is written. Simple/Precise mode and the compact (knob-face)
// form join here.
struct Options {};

// Audio-taper gain controls (deck, sampler, microphone and auxiliary pregain;
// main, headphone and booth gain) read in dB with one decimal place, "minus infinity
// dB" at 0, with a true minus sign (U+2212). Every other knob reads as its 0-100% knob
// position.
QString text(const KnobIdentity& knob,
        const KnobValue& value,
        const Options& options = Options{});

} // namespace knobreadout
} // namespace mixxx
