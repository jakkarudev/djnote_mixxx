#include "waveform/overviewlayout.h"

#include <algorithm>
#include <cmath>

#include "engine/engine.h"
#include "util/duration.h"

namespace mixxx {
namespace overviewlayout {

namespace {

double samplesToSeconds(const Input& input, double samples) {
    const double trackTime = samples / (input.sampleRate * kEngineChannelCount);
    return trackTime / input.rateRatio;
}

} // namespace

TimeRulerLayout layoutTimeRuler(const Input& input) {
    TimeRulerLayout layout;
    if (input.timeRulerState == TimeRulerState::Inactive) {
        return layout;
    }

    layout.labelsShown = true;
    layout.lineShown = input.timeRulerState == TimeRulerState::RightClickHold;
    layout.linePos = std::clamp(input.pointerPos, 0.0, input.overviewLength);

    const double fraction = layout.linePos / input.overviewLength;
    const double timePosition = samplesToSeconds(input, fraction * input.trackSamples);
    const double timePositionTillEnd =
            samplesToSeconds(input, (1 - fraction) * input.trackSamples);
    const double timeDistance = samplesToSeconds(input,
            (fraction - input.playPositionFraction) * input.trackSamples);

    layout.positionLabelText = Duration::formatTime(timePosition) + " -" +
            Duration::formatTime(timePositionTillEnd);

    layout.distanceLabelText = Duration::formatTime(std::fabs(timeDistance));
    // Cast to int to avoid confusingly switching from -0:00 to 0:00 as
    // the playhead passes the point
    if (static_cast<int>(timeDistance) < 0) {
        layout.distanceLabelText = "-" + layout.distanceLabelText;
    }
    return layout;
}

} // namespace overviewlayout
} // namespace mixxx
