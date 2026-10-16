#include "waveform/overviewlayout.h"

#include <QCoreApplication>
#include <algorithm>
#include <cmath>
#include <iterator>

#include "engine/engine.h"
#include "util/duration.h"

namespace mixxx {
namespace overviewlayout {

namespace {

// Provides tr() for the beat label unit words (translation context
// "OverviewBeatLabel").
class OverviewBeatLabel {
    Q_DECLARE_TR_FUNCTIONS(OverviewBeatLabel)
  public:
    // Number is already signed, e.g. "+4" or "0".
    static QString rightOfLine(const QString& number, BeatLabelUnit unit, bool singular) {
        if (unit == BeatLabelUnit::Bars) {
            return singular ? tr("%1 bar").arg(number) : tr("%1 bars").arg(number);
        }
        return singular ? tr("%1 beat").arg(number) : tr("%1 beats").arg(number);
    }
    static QString leftOfLine(const QString& number, BeatLabelUnit unit, bool singular) {
        if (unit == BeatLabelUnit::Bars) {
            return singular ? tr("bar %1").arg(number) : tr("bars %1").arg(number);
        }
        return singular ? tr("beat %1").arg(number) : tr("beats %1").arg(number);
    }
};

constexpr double kBeatsPerBar = 4.0;
// The label switches sides when the pointer passes this fraction of the
// overview length.
constexpr double kAutoSideSwitchFraction = 0.75;

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
    const double value = settings.unit == BeatLabelUnit::Bars
            ? beatDistance / kBeatsPerBar
            : beatDistance;
    const bool fractional = settings.precision == BeatLabelPrecision::Fractional;
    const int decimals = !fractional ? 0 : (settings.unit == BeatLabelUnit::Bars ? 2 : 1);
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
    const bool singular = !fractional && std::fabs(scaled) == 1.0;
    return side == LabelSide::Right
            ? OverviewBeatLabel::rightOfLine(number, settings.unit, singular)
            : OverviewBeatLabel::leftOfLine(number, settings.unit, singular);
}

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

BeatLabelLayout layoutBeatLabel(const Input& input) {
    BeatLabelLayout layout;
    if (input.timeRulerState == TimeRulerState::Inactive ||
            input.pBeats == nullptr ||
            input.trackSamples <= 0.0 ||
            input.overviewLength <= 0.0) {
        return layout;
    }

    const double pointerFraction =
            std::clamp(input.pointerPos, 0.0, input.overviewLength) /
            input.overviewLength;
    const auto pointerPosition = audio::FramePos::fromEngineSamplePos(
            pointerFraction * input.trackSamples);
    const auto playPosition = audio::FramePos::fromEngineSamplePos(
            input.playPositionFraction * input.trackSamples);
    const double beatDistance = fractionalBeatIndex(*input.pBeats, pointerPosition) -
            fractionalBeatIndex(*input.pBeats, playPosition);

    layout.shown = true;
    const bool joined = input.overviewBreadth < 3.0 * input.labelHeight;
    if (joined) {
        layout.slot = LabelSlot::JoinedToDistanceLabel;
        layout.side = LabelSide::Right;
    } else {
        switch (input.hoverReadout.position) {
        case BeatLabelPosition::AlwaysRight:
            layout.side = LabelSide::Right;
            break;
        case BeatLabelPosition::AlwaysLeft:
            layout.side = LabelSide::Left;
            break;
        case BeatLabelPosition::Auto:
            layout.side = pointerFraction > kAutoSideSwitchFraction
                    ? LabelSide::Left
                    : LabelSide::Right;
            break;
        }
    }
    layout.text = formatBeatDistance(beatDistance, input.hoverReadout, layout.side);
    if (joined) {
        layout.joinedDistanceLabelText =
                layoutTimeRuler(input).distanceLabelText + "  " + layout.text;
    }
    return layout;
}

} // namespace overviewlayout
} // namespace mixxx
