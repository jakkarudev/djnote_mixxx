#pragma once

#include <QString>

// Pure layout logic for the track overview (no widget, painting or GL
// dependencies). The overview passes in its state and paints the returned
// description. All positions are "along the track axis": x for horizontal
// overviews, y for vertical ones. The overview maps them to x/y.
namespace mixxx {
namespace overviewlayout {

enum class TimeRulerState {
    Inactive,
    RightClickHold,
    LeftDrag,
};

struct Input {
    // Engine samples (frames * channel count). 0 if no track is loaded.
    double trackSamples = 0.0;
    // Track sample rate in frames per second.
    double sampleRate = 0.0;
    double rateRatio = 1.0;
    // The "playposition" control value (may be slightly outside 0..1).
    double playPositionFraction = 0.0;
    // Pointer position along the track axis, in overview pixels.
    double pointerPos = 0.0;
    // Overview length along the track axis, in pixels.
    double overviewLength = 0.0;
    TimeRulerState timeRulerState = TimeRulerState::Inactive;
};

struct TimeRulerLayout {
    bool lineShown = false;
    // Pointer position along the track axis, clamped to [0, overviewLength].
    double linePos = 0.0;
    // Labels are shown iff the time ruler is active.
    bool labelsShown = false;
    // "M:SS -M:SS" (position and time remaining)
    QString positionLabelText;
    // Signed time distance to the play position.
    QString distanceLabelText;
};

TimeRulerLayout layoutTimeRuler(const Input& input);

} // namespace overviewlayout
} // namespace mixxx
