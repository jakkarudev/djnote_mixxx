#include "waveform/overviewlayout.h"

#include <QCoreApplication>
#include <algorithm>
#include <cmath>
#include <iterator>

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
    // the playhead passes the point
    if (static_cast<int>(timeDistance) < 0) {
        text = "-" + text;
    }
    return text;
}

// Whether the pointer targets the hovered cue instead of itself. The time
// ruler disables cue hovering, but be safe.
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

// Whether the Over a cue setting applies: a cue is hovered, the skin shows
// cue times, and there is a hover readout label that could collide with them.
bool overCueInEffect(const Input& input) {
    return targetsCue(input) && input.cueTimesShown && readoutLabelPossible(input);
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

    layout.distanceLabelText = formatTimeDistance(timeDistance);
    return layout;
}

HoverReadoutLayout layoutHoverReadout(const Input& input) {
    HoverReadoutLayout layout;
    if (!readoutLabelPossible(input)) {
        return layout;
    }
    const bool rulerActive = input.timeRulerState != TimeRulerState::Inactive;
    const bool cueTarget = targetsCue(input);
    const bool overCueEffective = overCueInEffect(input);
    const OverCue overCue = input.hoverReadout.overCue;
    if (overCueEffective && overCue == OverCue::HideHoverReadout) {
        return layout;
    }

    // The hovered cue, not the pointer, is the target while a cue is hovered.
    const double targetPos = cueTarget
            ? input.hoveredCuePos
            : std::clamp(input.pointerPos, 0.0, input.overviewLength);
    const double targetSamples = cueTarget
            ? input.hoveredCueSamples
            : targetPos / input.overviewLength * input.trackSamples;
    const double targetFraction = targetPos / input.overviewLength;
    const auto targetPosition = audio::FramePos::fromEngineSamplePos(targetSamples);
    const auto playPosition = audio::FramePos::fromEngineSamplePos(
            input.playPositionFraction * input.trackSamples);
    const double beatDistance = fractionalBeatIndex(*input.pBeats, targetPosition) -
            fractionalBeatIndex(*input.pBeats, playPosition);

    layout.anchorPos = targetPos;
    const bool joined = rulerActive && input.overviewBreadth < 3.0 * input.labelHeight;
    const bool joinedToCue = overCueEffective && overCue == OverCue::JoinCueTimes;
    if (joined || joinedToCue) {
        layout.slot = joined ? LabelSlot::JoinedToDistanceLabel
                             : LabelSlot::JoinedToCueTimeDistanceLabel;
        layout.side = LabelSide::Right;
    } else if (overCueEffective && overCue == OverCue::MoveHoverReadout) {
        layout.side = LabelSide::Left;
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
    if (overCueEffective && overCue == OverCue::MoveHoverReadout) {
        // There is no room to move it before the start edge.
        const double width = input.labelWidth ? input.labelWidth(layout.text) : 0.0;
        if (layout.anchorPos - input.labelGap - width < -kFitTolerance) {
            return HoverReadoutLayout();
        }
    }
    if (joined) {
        layout.joinedDistanceLabelText =
                layoutTimeRuler(input).distanceLabelText + "  " + layout.text;
    } else if (joinedToCue) {
        const double cueDistanceSamples = input.hoveredCueSamples -
                input.playPositionFraction * input.trackSamples;
        layout.joinedDistanceLabelText =
                formatTimeDistance(samplesToSeconds(input, cueDistanceSamples)) +
                "  " + layout.text;
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
    layout.linePos = std::clamp(input.pointerPos, 0.0, input.overviewLength);
    return layout;
}

CueTimesLayout layoutCueTimes(const Input& input) {
    CueTimesLayout layout;
    layout.hidden = overCueInEffect(input) &&
            input.hoverReadout.overCue == OverCue::HideCueTimes;
    return layout;
}

} // namespace overviewlayout
} // namespace mixxx
