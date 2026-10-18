#pragma once

namespace mixxx::waveformgains {

/// Everything that decides the drawn gains of a scrolling waveform.
struct Inputs {
    /// The deck's total gain (gain knob and ReplayGain), without the x2
    /// compensation.
    float totalGain = 1.0f;
    float lowKnob = 1.0f;
    float midKnob = 1.0f;
    float highKnob = 1.0f;
    bool lowKill = false;
    bool midKill = false;
    bool highKill = false;
    /// Whether the deck's EQ is enabled.
    bool eqEnabled = true;
    float visualAll = 1.0f;
    float visualLow = 1.0f;
    float visualMid = 1.0f;
    float visualHigh = 1.0f;
    bool followsGain = true;
    bool followsEq = true;
    /// Whether the renderer applies the x2 compensation to the total gain.
    bool applyCompensation = false;
};

struct Gains {
    float all = 1.0f;
    float low = 1.0f;
    float mid = 1.0f;
    float high = 1.0f;
};

/// Follows gain off: the total gain counts as 1.0 (the compensation still
/// applies). Follows EQ off, or the deck's EQ disabled: the knobs count as 1.0
/// and the kills are ignored. The band visual gains always apply.
Gains calculateGains(const Inputs& in);

} // namespace mixxx::waveformgains
