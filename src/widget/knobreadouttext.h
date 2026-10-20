#pragma once

#include <QString>
#include <optional>

#include "effects/backends/effectmanifestparameter.h"
#include "preferences/configobject.h"

// Knob readout text: the text of the knob adjustment readout (see
// GLOSSARY.md). Pure: no widget, control or painting dependencies, so the
// whole unit table is testable in isolation (knobreadouttexttest.cpp).
namespace mixxx {
namespace knobreadout {

// What a knob is, as far as its readout is concerned. Resolved by the caller
// from the knob's connected control.
struct KnobIdentity {
    ConfigKey key;
    // The units hint of the effect parameter's manifest; set for effect
    // parameter knobs whose effect is loaded, empty for every other knob.
    std::optional<EffectManifestParameter::UnitsHint> unitsHint = std::nullopt;
};

// A knob's state at the moment of the readout.
struct KnobValue {
    // The control's value, e.g. the linear gain factor of a gain knob.
    double value = 0.0;
    // The knob position, 0 (fully left) .. 1 (fully right).
    double parameter = 0.0;
};

// Simple: deck EQ knobs read as a signed percent of the knob's travel from
// the neutral centre. Precise: they read in dB. Gains read in dB either way.
enum class Mode {
    Simple,
    Precise,
};

// How the text is written.
struct Options {
    Mode mode = Mode::Simple;
    // The compact form for the knob-face style: see compactForm().
    bool compact = false;
};

// The unit table (true minus sign U+2212 throughout):
// - Audio-taper gains (deck, sampler, microphone and auxiliary pregain; main,
//   headphone and booth gain): dB, one decimal place, "minus infinity dB" at 0,
//   in both modes.
// - Deck EQ knobs (EQ-rack effect parameters): Simple, a signed percent of the
//   knob's travel from the centre (-100% .. +100%); Precise, dB from the gain
//   factor.
// - Main balance: "L n%" / "C" / "R n%". Headphone mix: "Main n% / Cue m%".
// - Effect parameter knobs with a units hint: the value with its unit.
// - Every other knob (quick effect super knob, meta knobs, "Unknown" units):
//   its 0-100% knob position.
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
