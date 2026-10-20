#include "waveform/scrollinglinelayout.h"

namespace mixxx {
namespace scrollinglinelayout {

namespace {

int positiveMod(int a, int n) {
    return ((a % n) + n) % n;
}

} // namespace

std::vector<Line> layoutLines(const Beats* pBeats,
        audio::FramePos start,
        audio::FramePos end,
        const std::function<double(audio::FramePos)>& frameToX,
        const LineSettings& settings,
        int upstreamOpacity) {
    std::vector<Line> lines;
    if (!pBeats || !frameToX || !start.isValid() || !end.isValid() || end < start) {
        return lines;
    }

    const int upstreamPercent = OpacityPercent(upstreamOpacity).percent();
    const auto first = pBeats->cfirstmarker();
    for (auto it = pBeats->iteratorFrom(start); it != pBeats->cend() && *it <= end; ++it) {
        Line line;
        line.x = frameToX(*it);
        if (!settings.enabled) {
            line.kind = LineKind::Beat;
            line.opacityPercent = upstreamPercent;
        } else if (positiveMod(it - first, kBeatsPerBar) == 0) {
            line.kind = LineKind::Bar;
            line.opacityPercent = settings.barOpacity.percent();
        } else {
            line.kind = LineKind::Beat;
            line.opacityPercent = settings.beatOpacity.percent();
        }
        if (line.opacityPercent > 0) {
            lines.push_back(line);
        }
    }
    return lines;
}

} // namespace scrollinglinelayout
} // namespace mixxx
