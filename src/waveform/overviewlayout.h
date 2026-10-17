#pragma once

#include <QString>
#include <functional>

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

enum class BeatDistanceUnit {
    Bars,
    Beats,
};

enum class BeatDistancePrecision {
    Whole,
    Fractional,
};

enum class HoverReadoutPosition {
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

// How the hover readout relates to the time ruler.
enum class HoverMode {
    // The hover line is the time ruler's line; the time ruler also shows the
    // hover readout.
    ExtendTimeRuler,
    // Plain hover has its own line and label; both are hidden while the time
    // ruler is active, which then has no hover readout.
    SeparateHoverLine,
};

// What happens when the pointer is over a cue (its line or its label), where
// Mixxx draws cue times that would collide with the hover readout.
enum class HoverOverCue {
    // The beat distance is appended to the cue times' distance label.
    JoinCueTimes,
    // The hover readout is not drawn.
    HideHoverReadout,
    // The cue times are not drawn; the hover readout is drawn at the cue line.
    HideCueTimes,
    // The hover readout is drawn left of the cue line.
    MoveHoverReadout,
};

// Settings of the hover readout (Preferences -> Waveforms -> Hover readout).
// Later tickets add the remaining hover settings here.
struct HoverReadoutSettings {
    bool showHoverReadout = true;
    HoverDisplay display = HoverDisplay::LineAndLabel;
    BeatDistanceUnit unit = BeatDistanceUnit::Bars;
    BeatDistancePrecision precision = BeatDistancePrecision::Whole;
    HoverReadoutPosition position = HoverReadoutPosition::Auto;
    HoverMode hoverMode = HoverMode::ExtendTimeRuler;
    HoverOverCue overCue = HoverOverCue::JoinCueTimes;
    // Snapped seek (see layoutSnappedSeek()).
    bool snapSeeks = true;
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

    // Hover readout inputs (see layoutHoverReadout()).
    // The track's beat grid, or nullptr if there is none. Not owned.
    const mixxx::Beats* pBeats = nullptr;
    HoverReadoutSettings hoverReadout;
    // Height of one label row in pixels, and the overview's extent across
    // the track axis (height for horizontal, width for vertical overviews).
    // Used for the "three label rows fit" merge rule.
    double labelHeight = 0.0;
    double overviewBreadth = 0.0;

    // The cue the pointer is over (line or label), if any, and only if the
    // overview rendered it this frame. While one is hovered, the cue is the
    // target of the hover readout and plain hover has no hover line. The
    // layout ignores it while the time ruler is active.
    bool cueHovered = false;
    // Its position in engine samples, and along the track axis in overview
    // pixels where its line is drawn (not necessarily proportional to the
    // samples: the overview insets and clamps its marks).
    double hoveredCueSamples = 0.0;
    double hoveredCuePos = 0.0;
    // Whether the skin shows cue times (ShowCueTimes).
    bool cueTimesShown = true;

    // Extent in pixels along the track axis of a hover readout label with the
    // given text, including its padding: its width on horizontal overviews,
    // its height on vertical ones. Only used to decide whether a hover
    // readout left of a cue fits. Unset counts as zero.
    std::function<double(const QString&)> labelExtent;
    // Gap between a line and a label next to it, in pixels.
    double labelGap = 0.0;
};

// A snapped seek: the hover line and the time ruler's line sit on the bar line
// (Unit = Bars) or beat (Unit = Beats) nearest the pointer, and a seek from
// the overview moves the play position by the whole number of bars or beats
// between the one the play marker is in and that one. The play position keeps
// its offset from the beat grid. Snapping needs "Snap seeks to beat distance"
// and a beat grid; it does not depend on the hover readout or its Display.
struct SnappedSeekLayout {
    // Whether snapping is in effect. If not, the hover line, the time ruler
    // and the seek follow the pointer, and the other fields describe that.
    bool snapped = false;
    // The line's position along the track axis, clamped to [0, overviewLength].
    double linePos = 0.0;
    // The jump in bars or beats (the Unit), and in beats. Zero if not snapped.
    int jumpCount = 0;
    int jumpBeats = 0;
    // Where a seek lands, in engine samples, clamped to the track.
    double landingSamples = 0.0;
};

SnappedSeekLayout layoutSnappedSeek(const Input& input);

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

// Which side of the hover line the hover readout sits on.
enum class LabelSide {
    Right, // "+4 bars"; below the line on vertical overviews
    Left,  // "bars +4"; above the line on vertical overviews
};

enum class LabelSlot {
    // Its own row at the bottom end of the line.
    OwnBottomRow,
    // Appended to the time ruler's distance label (overview too short for
    // three label rows). Always on the right side of the line.
    JoinedToTimeRulerDistanceLabel,
    // Appended to the cue times' distance label (a cue is hovered). Always on
    // the right side of the cue line.
    JoinedToCueTimeDistanceLabel,
};

struct HoverReadoutLayout {
    bool shown = false;
    // The hover readout text (the beat distance), e.g. "+4 bars" (side Right) or "bars +4"
    // (side Left).
    QString text;
    LabelSide side = LabelSide::Right;
    LabelSlot slot = LabelSlot::OwnBottomRow;
    // For slots JoinedToTimeRulerDistanceLabel and JoinedToCueTimeDistanceLabel: the
    // complete replacement text of the time ruler's respectively the cue
    // times' distance label, e.g. "-0:32  +4 bars". Empty otherwise.
    QString joinedDistanceLabelText;
    // Where the line the readout is attached to is, along the track axis in
    // overview pixels: the pointer clamped to the overview, or the hovered
    // cue's position.
    double anchorPos = 0.0;
};

// The hover readout. Shown iff "Show hover readout" is on, the track has
// a beat grid, and either the time ruler is active or (plain hover) the pointer
// is present and Display includes the label. Plain hover uses the own row slot,
// except over a cue (see HoverOverCue), where the readout is measured to the cue.
HoverReadoutLayout layoutHoverReadout(const Input& input);

// The hover line of plain hover (no time ruler). While the time ruler is
// active its own line is used instead (layoutTimeRuler()).
struct HoverLineLayout {
    bool lineShown = false;
    // Pointer position along the track axis, clamped to [0, overviewLength].
    double linePos = 0.0;
};

// Hidden while a cue is hovered: the cue's own line marks the spot.
HoverLineLayout layoutHoverLine(const Input& input);

// The cue times (position and time distance labels) Mixxx draws for the
// hovered cue.
struct CueTimesLayout {
    // True if neither label is drawn (HoverOverCue::HideCueTimes in effect).
    bool hidden = false;
    // The label texts, empty if no cue is hovered or the cue times are
    // hidden. The distance label text includes the beat distance when the
    // hover readout is joined to it.
    // "M:SS -M:SS" (position and time remaining)
    QString positionLabelText;
    // Signed time distance to the play position.
    QString distanceLabelText;
};

CueTimesLayout layoutCueTimes(const Input& input);

} // namespace overviewlayout
} // namespace mixxx
