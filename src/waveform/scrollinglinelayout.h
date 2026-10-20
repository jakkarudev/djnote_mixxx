#pragma once

#include <functional>
#include <vector>

#include "audio/frame.h"
#include "track/beats.h"
#include "waveform/overviewlinelayout.h"

// Beat and bar lines for the scrolling waveform: which lines to draw, where and
// how strong. Pure, like the overview line layout (no widget, painting or GL
// dependencies): the renderer passes in the visible range and a frame-to-x
// mapping, and draws the returned lines.
namespace mixxx {
namespace scrollinglinelayout {

using overviewlayout::kBeatsPerBar;
using overviewlayout::OpacityPercent;

enum class LineKind {
    // The beat line on a downbeat (every kBeatsPerBar-th beat from the beat
    // grid's first beat, ADR 0001).
    Bar,
    // Any other beat line.
    Beat,
};

struct LineSettings {
    static constexpr int kDefaultBarOpacity = 100;
    static constexpr int kDefaultBeatOpacity = 80;

    // DJNote's master switch. Off: the lines are drawn as upstream Mixxx draws
    // them (every beat alike, at upstream's beat grid opacity).
    bool enabled = true;
    OpacityPercent barOpacity = OpacityPercent(kDefaultBarOpacity);
    OpacityPercent beatOpacity = OpacityPercent(kDefaultBeatOpacity);
};

struct Line {
    double x = 0.0;
    LineKind kind = LineKind::Beat;
    // 1..100 (lines at opacity 0 are not returned).
    int opacityPercent = 0;
};

// Lines for the beats in [start, end], ascending in frame order. frameToX maps
// a beat's frame position to its x on screen. upstreamOpacity is upstream's
// beat grid opacity (0..100, clamped), used for every line when the master
// switch is off. A null grid or an invalid or empty range yields no lines.
std::vector<Line> layoutLines(const Beats* pBeats,
        audio::FramePos start,
        audio::FramePos end,
        const std::function<double(audio::FramePos)>& frameToX,
        const LineSettings& settings,
        int upstreamOpacity);

} // namespace scrollinglinelayout
} // namespace mixxx
