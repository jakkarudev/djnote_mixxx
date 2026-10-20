#pragma once

#include <QColor>
#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

#include "audio/frame.h"
#include "track/beats.h"
#include "waveform/overviewlinelayout.h"

// Beat and bar lines for the scrolling waveform: which lines to draw, where and
// how strong, wide and in what colour. Pure, like the overview line layout (no widget, painting or GL
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

// Width of a line in whole pixels. Always within 1..5: out-of-range values are
// clamped on construction.
class LineWidthPx {
  public:
    static constexpr int kMin = 1;
    static constexpr int kMax = 5;

    constexpr explicit LineWidthPx(int px = kMin)
            : m_px(std::clamp(px, kMin, kMax)) {
    }
    constexpr int px() const {
        return m_px;
    }

  private:
    int m_px;
};

struct LineSettings {
    static constexpr int kDefaultBarOpacity = 100;
    static constexpr int kDefaultBeatOpacity = 80;
    static constexpr int kDefaultBarWidth = 2;
    static constexpr int kDefaultBeatWidth = 1;

    // DJNote's master switch. Off: the lines are drawn as upstream Mixxx draws
    // them (every beat alike, at upstream's beat grid opacity).
    bool enabled = true;
    OpacityPercent barOpacity = OpacityPercent(kDefaultBarOpacity);
    OpacityPercent beatOpacity = OpacityPercent(kDefaultBeatOpacity);
    LineWidthPx barWidth = LineWidthPx(kDefaultBarWidth);
    LineWidthPx beatWidth = LineWidthPx(kDefaultBeatWidth);
    // A custom colour, or none for the skin's beat colour.
    std::optional<QColor> barColor;
    std::optional<QColor> beatColor;
};

struct Line {
    double x = 0.0;
    LineKind kind = LineKind::Beat;
    // 1..100 (lines at opacity 0 are not returned).
    int opacityPercent = 0;
    // 1..5 whole pixels.
    int widthPx = 1;
    // The custom colour of the line's kind, else the skin's beat colour. The
    // opacity is not applied to it.
    QColor color;
};

// Lines for the beats in [start, end], ascending in frame order. frameToX maps
// a beat's frame position to its x on screen. upstreamOpacity is upstream's
// beat grid opacity (0..100, clamped), used for every line when the master
// switch is off, along with a width of 1 px and skinColor (the skin's beat
// colour, also used for lines without a custom colour). A null grid or an invalid or empty range yields no lines.
std::vector<Line> layoutLines(const Beats* pBeats,
        audio::FramePos start,
        audio::FramePos end,
        const std::function<double(audio::FramePos)>& frameToX,
        const LineSettings& settings,
        int upstreamOpacity,
        const QColor& skinColor);

} // namespace scrollinglinelayout
} // namespace mixxx
