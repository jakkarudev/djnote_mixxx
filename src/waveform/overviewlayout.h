#pragma once

#include <QString>

#include "track/beats.h"

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

enum class BeatLabelUnit {
    Bars,
    Beats,
};

enum class BeatLabelPrecision {
    Whole,
    Fractional,
};

enum class BeatLabelPosition {
    Auto,
    AlwaysRight,
    AlwaysLeft,
};

// What plain hover (no time ruler) shows.
enum class HoverDisplay {
    LineAndLabel,
    LineOnly,
    LabelOnly,
};

// Settings of the hover readout (Preferences -> Waveforms -> Hover readout).
// Later tickets add the remaining hover settings here.
struct HoverReadoutSettings {
    bool showHoverReadout = true;
    HoverDisplay display = HoverDisplay::LineAndLabel;
    BeatLabelUnit unit = BeatLabelUnit::Bars;
    BeatLabelPrecision precision = BeatLabelPrecision::Whole;
    BeatLabelPosition position = BeatLabelPosition::Auto;
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
    // Whether the pointer is over the overview. Plain hover (this with an
    // inactive time ruler) uses pointerPos as well.
    bool pointerPresent = false;

    // Hover readout / beat label inputs (see layoutBeatLabel()).
    // The track's beat grid, or nullptr if there is none. Not owned.
    const mixxx::Beats* pBeats = nullptr;
    HoverReadoutSettings hoverReadout;
    // Height of one label row in pixels, and the overview's extent across
    // the track axis (height for horizontal, width for vertical overviews).
    // Used for the "three label rows fit" merge rule.
    double labelHeight = 0.0;
    double overviewBreadth = 0.0;
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

// Which side of the line the beat label sits on.
enum class LabelSide {
    Right, // "+4 bars"; below the line on vertical overviews
    Left,  // "bars +4"; above the line on vertical overviews
};

enum class LabelSlot {
    // Its own row at the bottom end of the line.
    OwnBottomRow,
    // Appended to the time ruler's distance label (overview too short for
    // three label rows). Always on the right side of the line.
    JoinedToDistanceLabel,
};

struct BeatLabelLayout {
    bool shown = false;
    // The beat distance label, e.g. "+4 bars" (side Right) or "bars +4"
    // (side Left).
    QString text;
    LabelSide side = LabelSide::Right;
    LabelSlot slot = LabelSlot::OwnBottomRow;
    // For slot JoinedToDistanceLabel: the complete replacement text of the
    // time ruler's distance label, e.g. "-0:32  +4 bars". Empty otherwise.
    QString joinedDistanceLabelText;
};

// The beat distance label. Shown iff "Show hover readout" is on, the track has
// a beat grid, and either the time ruler is active or (plain hover) the pointer
// is present and Display includes the label. Plain hover always uses the own
// row slot.
BeatLabelLayout layoutBeatLabel(const Input& input);

// The hover line of plain hover (no time ruler). While the time ruler is
// active its own line is used instead (layoutTimeRuler()).
struct HoverReadoutLayout {
    bool lineShown = false;
    // Pointer position along the track axis, clamped to [0, overviewLength].
    double linePos = 0.0;
};

HoverReadoutLayout layoutHoverReadout(const Input& input);

} // namespace overviewlayout
} // namespace mixxx
