#include "waveform/overviewlayout.h"

#include <QCoreApplication>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>

#include "engine/engine.h"
#include "util/duration.h"
#include "waveform/overviewlinelayout.h"

namespace mixxx {
namespace overviewlayout {

namespace {

// Provides tr() for the hover readout unit words (translation context
// "OverviewHoverReadout").
//
// Every message is a numerus message: `count` selects the plural form of
// translations that have several. The source text is the English form, so
// without a translation the English wording is right too. English needs
// distinct singular and plural source texts because Mixxx loads no English
// translation, which would be needed to turn a "%n bar(s)" source into
// "bar" or "bars" (the fallback only substitutes %n).
class OverviewHoverReadout {
    Q_DECLARE_TR_FUNCTIONS(OverviewHoverReadout)
  public:
    // number is already signed, e.g. "+4" or "0".
    static QString rightOfLine(const QString& number, BeatDistanceUnit unit, int count) {
        const bool singular = count == 1;
        if (unit == BeatDistanceUnit::Bars) {
            return singular ? tr("%1 bar", "", count).arg(number)
                            : tr("%1 bars", "", count).arg(number);
        }
        return singular ? tr("%1 beat", "", count).arg(number)
                        : tr("%1 beats", "", count).arg(number);
    }
    static QString leftOfLine(const QString& number, BeatDistanceUnit unit, int count) {
        const bool singular = count == 1;
        if (unit == BeatDistanceUnit::Bars) {
            return singular ? tr("bar %1", "", count).arg(number)
                            : tr("bars %1", "", count).arg(number);
        }
        return singular ? tr("beat %1", "", count).arg(number)
                        : tr("beats %1", "", count).arg(number);
    }
};

// The label switches sides when the pointer passes this fraction of the
// overview length.
constexpr double kAutoSideSwitchFraction = 0.75;

// Slack for floating point error when checking whether a label fits.
constexpr double kFitTolerance = 1e-6;

// Fractional index of a track position on the beat grid: 0 at the grid's first
// marker, +1 at every following beat, linear inside a beat interval.
double fractionalBeatIndex(const Beats& beats, audio::FramePos position) {
    const auto first = beats.cfirstmarker();
    const auto next = beats.iteratorFrom(position); // first beat >= position
    if (*next == position) {
        return next - first;
    }
    const auto prev = std::prev(next);
    return (prev - first) + (position - *prev) / (*next - *prev);
}

QString formatBeatDistance(double beatDistance,
        const HoverReadoutSettings& settings,
        LabelSide side) {
    const double value = settings.unit == BeatDistanceUnit::Bars
            ? beatDistance / kBeatsPerBar
            : beatDistance;
    const bool fractional = settings.precision == BeatDistancePrecision::Fractional;
    const int decimals = !fractional ? 0 : (settings.unit == BeatDistanceUnit::Bars ? 2 : 1);
    const double scale = std::pow(10.0, decimals);
    // Round first, so that values that display as zero get no sign.
    const double scaled = std::round(value * scale);
    const double absValue = std::fabs(scaled) / scale;

    QString number = QString::number(absValue, 'f', decimals);
    if (scaled > 0) {
        number.prepend('+');
    } else if (scaled < 0) {
        number.prepend('-');
    }
    // The count selects the plural form. Fractional values (e.g. "1.25
    // bars") are never singular, so they use 2, a plural in every language.
    const int count = fractional ? 2 : static_cast<int>(std::fabs(scaled));
    return side == LabelSide::Right
            ? OverviewHoverReadout::rightOfLine(number, settings.unit, count)
            : OverviewHoverReadout::leftOfLine(number, settings.unit, count);
}

double samplesToSeconds(const Input& input, double samples) {
    const double trackTime = samples / (input.sampleRate * kEngineChannelCount);
    return trackTime / input.rateRatio;
}

// "M:SS" of a signed time distance, with a minus sign if negative.
QString formatTimeDistance(double timeDistance) {
    QString text = Duration::formatTime(std::fabs(timeDistance));
    // Cast to int to avoid confusingly switching from -0:00 to 0:00 as
    // the play marker passes the point
    if (static_cast<int>(timeDistance) < 0) {
        text = "-" + text;
    }
    return text;
}

// Signed time distance from the play position to the hovered cue.
QString cueTimeDistanceText(const Input& input) {
    const double cueDistanceSamples = input.hoveredCueSamples -
            input.playPositionFraction * input.trackSamples;
    return formatTimeDistance(samplesToSeconds(input, cueDistanceSamples));
}

// "M:SS -M:SS" (position and time remaining)
QString formatPositionLabel(double timePosition, double timePositionTillEnd) {
    return Duration::formatTime(timePosition) + " -" +
            Duration::formatTime(timePositionTillEnd);
}

// Whether the pointer targets the hovered cue instead of itself. The time
// ruler disables cue hovering.
bool targetsCue(const Input& input) {
    return input.cueHovered && input.timeRulerState == TimeRulerState::Inactive;
}

// Whether a hover readout label would be shown if no cue were hovered.
bool readoutLabelPossible(const Input& input) {
    const bool rulerActive = input.timeRulerState != TimeRulerState::Inactive;
    const HoverReadoutSettings& settings = input.hoverReadout;
    const bool plainHoverLabel = !rulerActive && input.pointerPresent &&
            settings.display != HoverDisplay::LineOnly;
    const bool rulerLabel = rulerActive &&
            settings.hoverMode == HoverMode::ExtendTimeRuler;
    return settings.showHoverReadout &&
            (rulerLabel || plainHoverLabel) &&
            input.pBeats != nullptr &&
            input.trackSamples > 0.0 &&
            input.overviewLength > 0.0;
}

// What the Over a cue choice does, when it is in effect: a cue is hovered, the
// skin shows cue times, and there is a hover readout label that could collide
// with them. Everything stays at its default when not in effect.
struct OverCueDecision {
    bool readoutHidden = false;
    bool cueTimesHidden = false;
    // Set to override the slot and side of the hover readout.
    std::optional<LabelSlot> slot;
    std::optional<LabelSide> side;
    // The hover readout is not drawn if it does not fit before the start edge.
    bool mustFitBeforeStartEdge = false;
};

OverCueDecision decideOverCue(const Input& input) {
    OverCueDecision decision;
    if (!targetsCue(input) || !input.cueTimesShown || !readoutLabelPossible(input)) {
        return decision;
    }
    switch (input.hoverReadout.overCue) {
    case HoverOverCue::JoinCueTimes:
        decision.slot = LabelSlot::JoinedToCueTimeDistanceLabel;
        decision.side = LabelSide::Right;
        break;
    case HoverOverCue::HideHoverReadout:
        decision.readoutHidden = true;
        break;
    case HoverOverCue::HideCueTimes:
        decision.cueTimesHidden = true;
        break;
    case HoverOverCue::MoveHoverReadout:
        decision.side = LabelSide::Left;
        decision.mustFitBeforeStartEdge = true;
        break;
    }
    return decision;
}

} // namespace

SnappedSeekLayout layoutSnappedSeek(const Input& input) {
    SnappedSeekLayout layout;
    layout.linePos = std::clamp(input.pointerPos, 0.0, input.overviewLength);
    layout.landingSamples = input.overviewLength > 0.0
            ? layout.linePos / input.overviewLength * input.trackSamples
            : 0.0;
    if (!input.hoverReadout.snapSeeks ||
            input.pBeats == nullptr ||
            input.trackSamples <= 0.0 ||
            input.overviewLength <= 0.0) {
        return layout;
    }
    const Beats& beats = *input.pBeats;
    const int unitBeats = input.hoverReadout.unit == BeatDistanceUnit::Bars ? kBeatsPerBar : 1;

    const auto pointerPosition = audio::FramePos::fromEngineSamplePos(layout.landingSamples);
    const auto playPosition = audio::FramePos::fromEngineSamplePos(
            input.playPositionFraction * input.trackSamples);

    // The bar (or beat) line nearest the pointer, and the bar (or beat) the
    // play marker is in, both as a count of units from the grid's first beat.
    // Counts are negative before it: the grid's bars continue backwards.
    const auto targetUnit = static_cast<int>(
            std::floor(fractionalBeatIndex(beats, pointerPosition) / unitBeats + 0.5));
    const auto playUnit = static_cast<int>(
            std::floor(fractionalBeatIndex(beats, playPosition) / unitBeats));

    layout.snapped = true;
    layout.jumpCount = targetUnit - playUnit;
    layout.jumpBeats = layout.jumpCount * unitBeats;

    const audio::FramePos linePosition = *(beats.cfirstmarker() + targetUnit * unitBeats);
    layout.linePos = std::clamp(
            linePosition.toEngineSamplePos() / input.trackSamples * input.overviewLength,
            0.0,
            input.overviewLength);

    // The same computation the engine makes when it processes the seek.
    const audio::FramePos landing = beats.findNBeatsFromPosition(playPosition, layout.jumpBeats);
    layout.landingSamples = std::clamp(landing.toEngineSamplePos(), 0.0, input.trackSamples);
    return layout;
}

TimeRulerLayout layoutTimeRuler(const Input& input) {
    TimeRulerLayout layout;
    if (input.timeRulerState == TimeRulerState::Inactive) {
        return layout;
    }

    // The labels describe the spot a seek would land on.
    const SnappedSeekLayout snap = layoutSnappedSeek(input);
    layout.labelsShown = true;
    layout.lineShown = input.timeRulerState == TimeRulerState::RightClickHold;
    layout.linePos = snap.linePos;

    const double timePosition = samplesToSeconds(input, snap.landingSamples);
    const double timePositionTillEnd =
            samplesToSeconds(input, input.trackSamples - snap.landingSamples);
    const double timeDistance = samplesToSeconds(input,
            snap.landingSamples - input.playPositionFraction * input.trackSamples);

    layout.positionLabelText = formatPositionLabel(timePosition, timePositionTillEnd);

    layout.distanceLabelText = formatTimeDistance(timeDistance);
    return layout;
}

HoverReadoutLayout layoutHoverReadout(const Input& input) {
    HoverReadoutLayout layout;
    if (!readoutLabelPossible(input)) {
        return layout;
    }
    const OverCueDecision overCue = decideOverCue(input);
    if (overCue.readoutHidden) {
        return layout;
    }
    const bool rulerActive = input.timeRulerState != TimeRulerState::Inactive;
    const bool cueTarget = targetsCue(input);

    // The hovered cue, not the pointer, is the target while a cue is hovered.
    // The label is attached to the target's line, which is snapped.
    const SnappedSeekLayout snap = layoutSnappedSeek(input);
    const double pointerPos = std::clamp(input.pointerPos, 0.0, input.overviewLength);
    const double targetPos = cueTarget ? input.hoveredCuePos : snap.linePos;
    const double targetSamples = cueTarget
            ? input.hoveredCueSamples
            : pointerPos / input.overviewLength * input.trackSamples;
    const double targetFraction = targetPos / input.overviewLength;
    const auto targetPosition = audio::FramePos::fromEngineSamplePos(targetSamples);
    const auto playPosition = audio::FramePos::fromEngineSamplePos(
            input.playPositionFraction * input.trackSamples);
    // The pointer's true beat distance. With whole precision the label shows
    // the jump instead, so that label, line and landing agree.
    double beatDistance = fractionalBeatIndex(*input.pBeats, targetPosition) -
            fractionalBeatIndex(*input.pBeats, playPosition);
    if (!cueTarget && snap.snapped &&
            input.hoverReadout.precision == BeatDistancePrecision::Whole) {
        beatDistance = snap.jumpBeats;
    }

    layout.anchorPos = targetPos;
    const bool joinedToTimeRuler = rulerActive && input.overviewBreadth < 3.0 * input.labelHeight;
    if (joinedToTimeRuler) {
        layout.slot = LabelSlot::JoinedToTimeRulerDistanceLabel;
        layout.side = LabelSide::Right;
    } else if (overCue.side) {
        layout.slot = overCue.slot.value_or(LabelSlot::OwnBottomRow);
        layout.side = *overCue.side;
    } else {
        switch (input.hoverReadout.position) {
        case HoverReadoutPosition::AlwaysRight:
            layout.side = LabelSide::Right;
            break;
        case HoverReadoutPosition::AlwaysLeft:
            layout.side = LabelSide::Left;
            break;
        case HoverReadoutPosition::Auto:
            layout.side = targetFraction > kAutoSideSwitchFraction
                    ? LabelSide::Left
                    : LabelSide::Right;
            break;
        }
    }
    layout.text = formatBeatDistance(beatDistance, input.hoverReadout, layout.side);
    if (overCue.mustFitBeforeStartEdge) {
        const double extent = input.labelExtent ? input.labelExtent(layout.text) : 0.0;
        if (layout.anchorPos - input.labelGap - extent < -kFitTolerance) {
            return HoverReadoutLayout();
        }
    }
    if (joinedToTimeRuler) {
        layout.joinedDistanceLabelText =
                layoutTimeRuler(input).distanceLabelText + "  " + layout.text;
    } else if (layout.slot == LabelSlot::JoinedToCueTimeDistanceLabel) {
        layout.joinedDistanceLabelText = cueTimeDistanceText(input) + "  " + layout.text;
    }
    layout.shown = true;
    return layout;
}

HoverLineLayout layoutHoverLine(const Input& input) {
    HoverLineLayout layout;
    if (input.timeRulerState != TimeRulerState::Inactive ||
            !input.pointerPresent ||
            targetsCue(input) ||
            !input.hoverReadout.showHoverReadout ||
            input.hoverReadout.display == HoverDisplay::LabelOnly) {
        return layout;
    }
    layout.lineShown = true;
    layout.linePos = layoutSnappedSeek(input).linePos;
    return layout;
}

CueTimesLayout layoutCueTimes(const Input& input) {
    CueTimesLayout layout;
    if (!input.cueHovered) {
        return layout;
    }
    if (decideOverCue(input).cueTimesHidden) {
        layout.hidden = true;
        return layout;
    }
    layout.positionLabelText = formatPositionLabel(
            samplesToSeconds(input, input.hoveredCueSamples),
            samplesToSeconds(input, input.trackSamples - input.hoveredCueSamples));
    const HoverReadoutLayout readout = layoutHoverReadout(input);
    layout.distanceLabelText = readout.shown &&
                    readout.slot == LabelSlot::JoinedToCueTimeDistanceLabel
            ? readout.joinedDistanceLabelText
            : cueTimeDistanceText(input);
    return layout;
}

} // namespace overviewlayout
} // namespace mixxx
