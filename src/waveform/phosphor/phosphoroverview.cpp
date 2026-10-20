#include "waveform/phosphor/phosphoroverview.h"

#include <algorithm>
#include <cmath>

#include "util/assert.h"

namespace mixxx::phosphor {

namespace {

// Research 9: |rgb| = W . (r, g, b, b^4) * |levels|^level_exponent * d^0.25.
constexpr double kWeightR = 83.5;
constexpr double kWeightG = 242.0;
constexpr double kWeightB = -37.4;
constexpr double kWeightB4 = 606.1;
constexpr double kAlphaPerGreen = 2.0;
// A density share of this stores as 255.
constexpr double kDensityFull = 0.8;

inline double clamp255(double v) {
    return std::clamp(v, 0.0, 255.0);
}

} // namespace

QImage overviewImage(const OverviewColumnsView& columns,
        const OverviewDrawSettings& settings,
        int height) {
    DEBUG_ASSERT(height > 0);
    // 62 rows, per channel B, G, R, A, as floats.
    std::vector<double> src(static_cast<size_t>(kOverviewRows) * kOverviewColumns * 4, 0.0);
    const int done = std::clamp(columns.columnsDone, 0, kOverviewColumns);
    const double halfSample = 0.5 / std::max(1, columns.samplesPerColumn);
    for (int c = 0; c < done; ++c) {
        const uint8_t* density = columns.density + static_cast<size_t>(c) * kOverviewRows;
        int top = -1;
        int bottom = -1;
        for (int r = 0; r < kOverviewRows; ++r) {
            if (density[r] != 0) {
                if (top < 0) {
                    top = r;
                }
                bottom = r;
            }
        }
        if (top < 0) {
            continue;
        }
        const uint8_t* lv8 = columns.levels + static_cast<size_t>(c) * 3;
        const double lr = lv8[0] / 255.0 * settings.gainLow;
        const double lg = lv8[1] / 255.0 * settings.gainMid;
        const double lb = lv8[2] / 255.0 * settings.gainHigh;
        const double norm = std::sqrt(lr * lr + lg * lg + lb * lb);
        double ur = 0, ug = 0, ub = 0, mag = 0;
        const bool live = norm > 0;
        if (live) {
            ur = lr / norm;
            ug = lg / norm;
            ub = lb / norm;
            mag = (kWeightR * ur + kWeightG * ug + kWeightB * ub +
                          kWeightB4 * std::pow(ub, 4)) *
                    std::pow(norm, settings.levelExponent) * settings.brightness;
        }
        for (int r = top; r <= bottom; ++r) {
            double* px = &src[(static_cast<size_t>(r) * kOverviewColumns + c) * 4];
            if (!live) {
                px[3] = 255.0; // silence: an opaque black line
                continue;
            }
            const double d = std::max(
                    kDensityFull * std::pow(density[r] / 255.0, 4), halfSample);
            const double k = mag * std::pow(d, settings.densityExponent);
            px[0] = clamp255(ub * k);
            px[1] = clamp255(ug * k);
            px[2] = clamp255(ur * k);
            px[3] = clamp255(kAlphaPerGreen * ug * k);
        }
    }

    QImage image(kOverviewColumns, height, QImage::Format_ARGB32);
    const auto roundU8 = [](double v) {
        return static_cast<uint8_t>(std::clamp(std::floor(v + 0.5), 0.0, 255.0));
    };
    std::vector<double> acc(static_cast<size_t>(kOverviewColumns) * 4);
    for (int y = 0; y < height; ++y) {
        // Area average of the source rows covered by output row y.
        const double lo = static_cast<double>(y) * kOverviewRows / height;
        const double hi = lo + static_cast<double>(kOverviewRows) / height;
        std::fill(acc.begin(), acc.end(), 0.0);
        for (int r = static_cast<int>(std::floor(lo)); r < kOverviewRows && r < hi; ++r) {
            const double w = (std::min(hi, r + 1.0) - std::max(lo, static_cast<double>(r))) *
                    height / kOverviewRows;
            if (w <= 0) {
                continue;
            }
            const double* row = &src[static_cast<size_t>(r) * kOverviewColumns * 4];
            for (size_t i = 0; i < acc.size(); ++i) {
                acc[i] += row[i] * w;
            }
        }
        uint8_t* out = image.scanLine(y);
        for (size_t i = 0; i < acc.size(); ++i) {
            out[i] = roundU8(acc[i]);
        }
    }
    return image;
}

int overviewExtentRows(const OverviewColumnsView& columns) {
    const int done = std::clamp(columns.columnsDone, 0, kOverviewColumns);
    int extent = 0;
    for (int c = 0; c < done; ++c) {
        const uint8_t* density = columns.density + static_cast<size_t>(c) * kOverviewRows;
        for (int r = 0; r < kOverviewRows; ++r) {
            if (density[r] != 0) {
                // Row r above the centre is 31 - r rows from it, below it r - 30.
                extent = std::max(extent, kOverviewRows / 2 - r);
                break;
            }
        }
        for (int r = kOverviewRows - 1; r >= 0; --r) {
            if (density[r] != 0) {
                extent = std::max(extent, r - (kOverviewRows / 2 - 1));
                break;
            }
        }
    }
    return extent;
}

PhosphorOverviewData::PhosphorOverviewData(int sampleRate, SINT totalFrames)
        : m_sampleRate(sampleRate),
          m_totalFrames(totalFrames),
          m_samplesPerColumn(static_cast<int>(
                  std::max<SINT>(1, (totalFrames + kOverviewColumns - 1) / kOverviewColumns))),
          m_levels(static_cast<size_t>(kOverviewColumns) * 3, 0),
          m_density(static_cast<size_t>(kOverviewColumns) * kOverviewRows, 0),
          m_completed(0) {
}

OverviewColumnsView PhosphorOverviewData::view() const {
    OverviewColumnsView v;
    v.levels = m_levels.data();
    v.density = m_density.data();
    v.samplesPerColumn = m_samplesPerColumn;
    v.columnsDone = completedColumns();
    return v;
}

} // namespace mixxx::phosphor
