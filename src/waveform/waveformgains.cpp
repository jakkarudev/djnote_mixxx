#include "waveform/waveformgains.h"

namespace mixxx::waveformgains {

Gains calculateGains(const Inputs& in) {
    const float total = in.followsGain ? in.totalGain : 1.0f;
    const float compensation = in.applyCompensation ? 2.0f : 1.0f;
    const bool eqActive = in.followsEq && in.eqEnabled;

    const auto band = [eqActive](float knob, bool kill, float visual) {
        if (eqActive) {
            return kill ? 0.0f : knob * visual;
        }
        return visual;
    };

    Gains g;
    g.all = total * compensation * in.visualAll;
    g.low = band(in.lowKnob, in.lowKill, in.visualLow);
    g.mid = band(in.midKnob, in.midKill, in.visualMid);
    g.high = band(in.highKnob, in.highKill, in.visualHigh);
    return g;
}

} // namespace mixxx::waveformgains
