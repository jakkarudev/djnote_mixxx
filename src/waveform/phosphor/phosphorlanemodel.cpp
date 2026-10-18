#include "waveform/phosphor/phosphorlanemodel.h"

#include <algorithm>
#include <QtGlobal>
#include <cmath>

namespace mixxx::phosphor {

namespace {

constexpr double kMinFramesPerColumn = 1e-3;

/// The inclusive index range of the points of a column's window. Empty
/// (hi - lo < 1) when the column has no data.
struct Window {
    SINT lo;
    SINT hi;
    bool isEmpty() const {
        return hi - lo < 1;
    }
};

double windowSpan(const LaneParams& params) {
    return params.windowColumns * std::max(params.framesPerColumn, kMinFramesPerColumn) / 2;
}

// First index in [0, n) with pointTime(index) >= t (n if none).
SINT lowerBound(const PhosphorData& data, SINT n, double t) {
    SINT lo = 0;
    SINT hi = n;
    while (lo < hi) {
        const SINT mid = lo + (hi - lo) / 2;
        if (data.pointTime(mid) < t) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

// First index in [0, n) with pointTime(index) > t (n if none).
SINT upperBound(const PhosphorData& data, SINT n, double t) {
    SINT lo = 0;
    SINT hi = n;
    while (lo < hi) {
        const SINT mid = lo + (hi - lo) / 2;
        if (data.pointTime(mid) <= t) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

// Points whose time lies in [c - span, c + span], widened to always include
// the point at or before c and the next one, clipped to the track.
Window windowOf(const PhosphorData& data, const LaneParams& params, double centre) {
    const SINT n = data.pointCount();
    if (n <= 0) {
        return {0, -1};
    }
    const double span = windowSpan(params);
    SINT lo = lowerBound(data, n, centre - span);
    SINT hi = upperBound(data, n, centre + span) - 1;
    const SINT i0 = upperBound(data, n, centre) - 1;
    lo = std::max(std::min(lo, i0), SINT{0});
    hi = std::min(std::max(hi, i0 + 1), n - 1);
    return {lo, hi};
}

} // namespace

double pxPerBarForZoom(double zoom) {
    return 869.6 / zoom;
}

double visualSamplePerPixelForZoom(double zoom, double bpm) {
    // framesPerColumn() in visual samples; the sample rate cancels out.
    return framesPerColumn(441.0, pxPerBarForZoom(zoom), bpm);
}

double framesPerColumn(double sampleRate, double pxPerBar, double bpm) {
    const double usedBpm = bpm > 0 ? bpm : kFallbackBpm;
    // pixels per second = pxPerBar * bpm / 240
    return sampleRate / (pxPerBar * usedBpm / 240.0);
}

int rowOfValue(double value, int height) {
    const double centre = (height - 1) / 2.0;
    const double half = kFullScale * height / 2.0;
    const double row = std::floor(centre + value / 32768.0 * half + 0.5);
    return static_cast<int>(std::clamp(row, 0.0, static_cast<double>(height - 1)));
}

namespace {

// The two colour blocks around a column centre and the weight of the second.
struct ColourBlocks {
    SINT lo;
    SINT hi;
    double t;
};

ColourBlocks colourBlocksAt(const PhosphorData& data, double centreFrame) {
    const double u = centreFrame / data.colourBlockFrames() - 0.5;
    const SINT last = std::max<SINT>(data.totalFrames() / data.colourBlockFrames() - 1, 0);
    const double clamped = std::clamp(u, 0.0, static_cast<double>(last));
    const SINT lo = static_cast<SINT>(std::floor(clamped));
    return {lo, std::min(lo + 1, last), clamped - lo};
}

} // namespace

bool isColumnReady(const PhosphorData& data, const LaneParams& params, double centreFrame) {
    const Window w = windowOf(data, params, centreFrame);
    if (w.isEmpty()) {
        return true;
    }
    if (w.hi >= data.pointsCompleted()) {
        return false;
    }
    // The colour track trails the samples.
    return data.isFinished() ||
            colourBlocksAt(data, centreFrame).hi < data.colourBlocksCompleted();
}

LaneColumn computeColumn(const PhosphorData& data,
        const LaneParams& params,
        double centreFrame) {
    LaneColumn column;
    const int height = params.height;
    if (height < 1) {
        return column;
    }
    const Window w = windowOf(data, params, centreFrame);
    if (w.isEmpty()) {
        return column;
    }

    // Rows of the window's points.
    int rowMin = height;
    int rowMax = -1;
    bool anySignal = false;
    // Difference array: weight added from lo row, removed after hi row.
    std::vector<double> acc(static_cast<size_t>(height) + 1, 0.0);

    int prevRow = 0;
    double prevTime = 0;
    for (SINT i = w.lo; i <= w.hi; ++i) {
        const int value = data.pointValue(i);
        const double time = data.pointTime(i);
        const int row = rowOfValue(value, height);
        anySignal = anySignal || value != 0;
        rowMin = std::min(rowMin, row);
        rowMax = std::max(rowMax, row);
        if (i > w.lo) {
            // Every row the segment crosses (inclusive) gets dt / rows crossed.
            const int loRow = std::min(prevRow, row);
            const int hiRow = std::max(prevRow, row);
            const double weight = (time - prevTime) / (hiRow - loRow + 1);
            acc[static_cast<size_t>(loRow)] += weight;
            acc[static_cast<size_t>(hiRow) + 1] -= weight;
        }
        prevRow = row;
        prevTime = time;
    }

    const double windowTime = data.pointTime(w.hi) - data.pointTime(w.lo);
    const double scale = windowTime > 0 ? height / 2.0 / windowTime : 0.0;
    column.density.resize(static_cast<size_t>(height));
    double running = 0;
    for (int row = 0; row < height; ++row) {
        running += acc[static_cast<size_t>(row)];
        column.density[static_cast<size_t>(row)] = static_cast<float>(running * scale);
    }
    column.rowMin = rowMin;
    column.rowMax = rowMax;
    if (data.colourBlocksCompleted() > 0) {
        const ColourBlocks blocks = colourBlocksAt(data, centreFrame);
        for (int c = 0; c < 3; ++c) {
            const double lo = data.colourLevel(blocks.lo, c);
            const double hi = data.colourLevel(blocks.hi, c);
            column.levels[c] = static_cast<float>((lo + (hi - lo) * blocks.t) / 65535.0);
        }
    }
    column.status = anySignal ? ColumnStatus::Signal : ColumnStatus::Silence;
    return column;
}

LaneParams slipOverlayParams(const LaneParams& lane) {
    LaneParams overlay = lane;
    overlay.height = std::max(lane.height / 2, 1);
    return overlay;
}

} // namespace mixxx::phosphor
