#include "waveform/scrollinglinelayout.h"

#include <cmath>
#include <limits>
#include <utility>

#include "util/assert.h"

namespace mixxx {
namespace scrollinglinelayout {

namespace {

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

// The on-screen distance from the beat at 'it' to its neighbour: the next
// beat, or the previous one for the last beat of the grid.
double beatSpacingPxAt(const Beats& beats,
        Beats::ConstIterator it,
        const std::function<double(audio::FramePos)>& frameToX) {
    auto neighbour = it;
    ++neighbour;
    if (neighbour == beats.cend()) {
        neighbour = it;
        if (neighbour == beats.cbegin()) {
            return std::numeric_limits<double>::infinity();
        }
        --neighbour;
    }
    return std::abs(frameToX(*neighbour) - frameToX(*it));
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
        } else if (overviewlayout::beatIndexInCycle(*pBeats, it, kBeatsPerBar) == 0) {
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
        // Too dense to read when zoomed far out: only the bar lines stay. The
        // steps of the beat's interval are still up to addSteps().
        const bool tooDense = settings.enabled && line.kind == LineKind::Beat &&
                beatSpacingPxAt(*pBeats, it, frameToX) < settings.beatMinSpacing.px();
        if (line.opacityPercent > 0 && !tooDense) {
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

std::vector<LineBatch> lineBatches(const std::vector<Line>& lines) {
    std::vector<LineBatch> batches;
    for (const LineKind kind : {LineKind::Step, LineKind::Bar, LineKind::Beat}) {
        LineBatch batch;
        batch.kind = kind;
        const Line* pFirst = nullptr;
        for (const Line& line : lines) {
            if (line.kind != kind) {
                continue;
            }
            if (!pFirst) {
                pFirst = &line;
                batch.color = line.color;
                batch.color.setAlphaF(line.opacityPercent / 100.0f);
            }
            // One batch, one look: see the header.
            DEBUG_ASSERT(line.color == pFirst->color &&
                    line.widthPx == pFirst->widthPx &&
                    line.opacityPercent == pFirst->opacityPercent);
            const double centre = line.x + 0.5;
            const double halfWidth = line.widthPx / 2.0;
            batch.spans.push_back(LineSpan{centre - halfWidth, centre + halfWidth});
        }
        if (!batch.spans.empty()) {
            batches.push_back(std::move(batch));
        }
    }
    return batches;
}

} // namespace scrollinglinelayout
} // namespace mixxx
