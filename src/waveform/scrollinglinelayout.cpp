#include "waveform/scrollinglinelayout.h"

#include <cmath>

namespace mixxx {
namespace scrollinglinelayout {

namespace {

int positiveMod(int a, int n) {
    return ((a % n) + n) % n;
}

constexpr int kStepsPerBeat = 4;

// Adds the step lines between the beats at 'from' and 'to' that are in
// [start, end], if they are far enough apart on screen.
void addSteps(std::vector<Line>* pLines,
        audio::FramePos from,
        audio::FramePos to,
        audio::FramePos start,
        audio::FramePos end,
        const std::function<double(audio::FramePos)>& frameToX,
        const LineSettings& settings,
        const QColor& skinColor) {
    const double beatPx = std::abs(frameToX(to) - frameToX(from));
    if (beatPx / kStepsPerBeat < settings.stepMinSpacing.px()) {
        return;
    }
    const double beatFrames = to - from;
    for (int step = 1; step < kStepsPerBeat; ++step) {
        const audio::FramePos position = from + beatFrames * step / kStepsPerBeat;
        if (position < start || position > end) {
            continue;
        }
        Line line;
        line.x = frameToX(position);
        line.kind = LineKind::Step;
        line.opacityPercent = settings.stepOpacity.percent();
        line.widthPx = settings.stepWidth.px();
        line.color = settings.stepColor.value_or(skinColor);
        pLines->push_back(line);
    }
}

} // namespace

std::vector<Line> layoutLines(const Beats* pBeats,
        audio::FramePos start,
        audio::FramePos end,
        const std::function<double(audio::FramePos)>& frameToX,
        const LineSettings& settings,
        int upstreamOpacity,
        const QColor& skinColor) {
    std::vector<Line> lines;
    if (!pBeats || !frameToX || !start.isValid() || !end.isValid() || end < start) {
        return lines;
    }

    const int upstreamPercent = OpacityPercent(upstreamOpacity).percent();
    const auto first = pBeats->cfirstmarker();
    const bool showSteps = settings.enabled && settings.stepsEnabled &&
            settings.stepOpacity.percent() > 0;
    auto it = pBeats->iteratorFrom(start);
    if (showSteps && it != pBeats->cbegin() && it != pBeats->cend()) {
        // Steps between the last beat before the range and the first in it.
        auto previous = it;
        --previous;
        addSteps(&lines, *previous, *it, start, end, frameToX, settings, skinColor);
    }
    for (; it != pBeats->cend() && *it <= end; ++it) {
        Line line;
        line.x = frameToX(*it);
        if (!settings.enabled) {
            line.kind = LineKind::Beat;
            line.opacityPercent = upstreamPercent;
            line.color = skinColor;
        } else if (positiveMod(it - first, kBeatsPerBar) == 0) {
            line.kind = LineKind::Bar;
            line.opacityPercent = settings.barOpacity.percent();
            line.widthPx = settings.barWidth.px();
            line.color = settings.barColor.value_or(skinColor);
        } else {
            line.kind = LineKind::Beat;
            line.opacityPercent = settings.beatOpacity.percent();
            line.widthPx = settings.beatWidth.px();
            line.color = settings.beatColor.value_or(skinColor);
        }
        if (line.opacityPercent > 0) {
            lines.push_back(line);
        }
        if (showSteps) {
            auto next = it;
            ++next;
            if (next != pBeats->cend()) {
                addSteps(&lines, *it, *next, start, end, frameToX, settings, skinColor);
            }
        }
    }
    return lines;
}

} // namespace scrollinglinelayout
} // namespace mixxx
