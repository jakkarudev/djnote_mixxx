#pragma once

#include <cstdint>
#include <vector>

#include "waveform/phosphor/phosphordata.h"

namespace mixxx::phosphor {

/// The Phosphor lane model: from a track's Phosphor data and a view, the
/// outline and the per-row density of each column. Pure CPU logic, no Qt, no
/// GL, no Track (ADR-0003). The renderer uploads the result.

/// A bar is 4 beats at the track's BPM; 120 if the track has none.
constexpr double kFallbackBpm = 120.0;

/// Zoom, like Traktor's: the deck's zoom control value z gives 869.6 / z
/// pixels per bar (z = 4 is 217.4). Zoomed out at z = 10, as in Mixxx; zoomed
/// in until a column is under one audio frame (about one frame at 120 BPM).
constexpr double kPhosphorMinZoom = 0.005;
constexpr double kPhosphorMaxZoom = 10.0;
/// The mouse wheel step of this type (the other types use 1.05).
constexpr double kPhosphorWheelStep = 1.15;

double pxPerBarForZoom(double zoom);

/// The view scale as the shared overlays (beat grid, marks) read it: visual
/// samples (441 per second) per logical pixel. It depends on the zoom and the
/// track's BPM only, never on the rate ratio.
double visualSamplePerPixelForZoom(double zoom, double bpm);

/// Audio frames per screen column for a zoom set in pixels per bar.
double framesPerColumn(double sampleRate, double pxPerBar, double bpm);

/// Everything besides the data and the column's centre that decides what a
/// column looks like. Two columns of the same data, params and centre are
/// identical, whatever was computed before.
struct LaneParams {
    /// Audio frames per column. Lower bound 1e-3, to stay finite.
    double framesPerColumn = 100.0;
    /// Lane height in physical pixels (rows).
    int height = 1;
    /// Shape window, in columns.
    double windowColumns = 2.0;
    /// The All visual gain: scales the height of the outline (1.0 = the
    /// calibrated height). Colour is not affected.
    double heightGain = 1.0;

    bool operator==(const LaneParams& other) const = default;
};

/// The params of the slip-mode overlay lane: the same view, half as many
/// rows, so the full signed shape is drawn at scale 0.5 in the top half of
/// the lane.
LaneParams slipOverlayParams(const LaneParams& lane);

/// The calibrated full scale: 0.8 of half the lane.
constexpr double kFullScale = 0.8;

enum class ColumnStatus : uint8_t {
    Outside = 0, // no data there (outside of the track, or not decoded yet)
    Signal = 1,
    Silence = 2, // digital silence: the renderer draws a black centre row
};

struct LaneColumn {
    ColumnStatus status = ColumnStatus::Outside;
    /// Outline rows, 0 is the top, positive values are drawn downward.
    int rowMin = 0;
    int rowMax = 0;
    /// Per row: hits / samples in the window * height / 2. Empty when the
    /// column is outside.
    std::vector<float> density;
    /// The band levels (0..1) of the colour track at the column centre,
    /// interpolated between blocks: R = Low, G = Mid, B = High. Zero when
    /// the column is outside.
    float levels[3] = {0.f, 0.f, 0.f};
};

/// The row of a value (int16 units). Positive values go down.
int rowOfValue(double value, int height, double heightGain = 1.0);

/// Whether the data decoded so far is enough to compute the column centred on
/// the given frame. False for columns whose window reaches data that was not
/// decoded yet; those must not be drawn or cached.
bool isColumnReady(const PhosphorData& data, const LaneParams& params, double centreFrame);

/// The column centred on a frame of the track.
LaneColumn computeColumn(const PhosphorData& data,
        const LaneParams& params,
        double centreFrame);

} // namespace mixxx::phosphor
