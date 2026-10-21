#pragma once

#include <QColor>
#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

#include "audio/frame.h"
#include "track/beats.h"
#include "waveform/overviewlinelayout.h"

// Beat, bar and step lines for the scrolling waveform: which lines to draw, where and
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
    // A line at a quarter, half or three quarters of the way between two
    // beats.
    Step,
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

// The minimum on-screen distance between adjacent step lines for them to be
// drawn, in pixels. Always within 4..30: out-of-range values are clamped on
// construction.
class StepSpacingPx {
  public:
    static constexpr int kMin = 4;
    static constexpr int kMax = 30;
    static constexpr int kDefault = 8;

    constexpr explicit StepSpacingPx(int px = kDefault)
            : m_px(std::clamp(px, kMin, kMax)) {
    }
    constexpr int px() const {
        return m_px;
    }

  private:
    int m_px;
};

// The minimum on-screen distance between adjacent beats for the beat lines
// (not the bar lines) to be drawn, in pixels. Always within 1..30: out-of-range
// values are clamped on construction. 1 effectively never hides them.
class BeatSpacingPx {
  public:
    static constexpr int kMin = 1;
    static constexpr int kMax = 30;
    static constexpr int kDefault = 12;

    constexpr explicit BeatSpacingPx(int px = kDefault)
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
    static constexpr int kDefaultStepOpacity = 45;
    static constexpr int kDefaultStepWidth = 1;

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
    // Beat lines closer than this on screen are not drawn (bar lines always
    // are), so zoomed far out they do not make a wash.
    BeatSpacingPx beatMinSpacing;

    // Step lines: three per beat, at a quarter, half and three quarters of
    // the frame gap between two beats of the grid. Between two beats they are
    // drawn only when adjacent steps are at least stepMinSpacing apart on
    // screen (no fade below it).
    bool stepsEnabled = false;
    StepSpacingPx stepMinSpacing;
    OpacityPercent stepOpacity = OpacityPercent(kDefaultStepOpacity);
    LineWidthPx stepWidth = LineWidthPx(kDefaultStepWidth);
    std::optional<QColor> stepColor;
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

// Lines for the beats (and steps) in [start, end], ascending in frame order. frameToX maps
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

// A line's horizontal extent on screen, left < right.
struct LineSpan {
    double left = 0.0;
    double right = 0.0;
};

// The lines of one kind, drawn in one go with one colour.
struct LineBatch {
    LineKind kind = LineKind::Beat;
    // The kind's colour with its opacity applied as alpha.
    QColor color;
    std::vector<LineSpan> spans;
};

// Groups lines into one batch per kind, in drawing order: steps, then bars,
// then beats (so steps are drawn underneath); kinds without lines get no
// batch. A line at x is centred on the middle of upstream's 1 px line
// (x .. x + 1) and grows equally both ways with its width.
//
// Relies on every line of a kind sharing colour, width and opacity, as
// layoutLines() returns them (a batch takes the colour of its first line).
std::vector<LineBatch> lineBatches(const std::vector<Line>& lines);

} // namespace scrollinglinelayout
} // namespace mixxx
