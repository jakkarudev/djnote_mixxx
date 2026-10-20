#pragma once

#include <QByteArray>
#include <QImage>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "util/types.h"
#include "waveform/phosphor/phosphorsettings.h"

namespace mixxx::phosphor {

/// The Phosphor overview model (research 9, ADR-0005, spec #50).
constexpr int kOverviewColumns = 1920;
constexpr int kOverviewRows = 62;
/// The row of a sample of 0, and so the center line of the row map.
constexpr int kOverviewCenterRow = kOverviewRows / 2;
/// A share of the window of this size stores as 255 (density byte =
/// round(255 * (share / kDensityFull)^kDensityStorePower)).
constexpr double kDensityFull = 0.8;
constexpr double kDensityStorePower = 0.25;
/// round half up, clipped to a byte.
inline uint8_t roundU8(double v) {
    return static_cast<uint8_t>(std::clamp(std::floor(v + 0.5), 0.0, 255.0));
}

/// The row of an int16 mono sample: floor(v / 32768 * 31 + 31), 0..61,
/// positive downward.
inline int overviewRowOf(int16_t sample) {
    constexpr double kInt16Scale = 32768.0;
    return std::clamp(static_cast<int>(std::floor(
                              sample / kInt16Scale * kOverviewCenterRow + kOverviewCenterRow)),
            0,
            kOverviewRows - 1);
}

/// Version of the stored columns: bump when the row map, the column count, the
/// encoding, the column window or the color track constants change (1.2:
/// extent and density from a two-column window, ticket #58).
constexpr const char* kOverviewVersion = "PhosphorOverview-1.2";
constexpr const char* kOverviewDescription =
        "Phosphor overview: 1920 columns of 3 band levels and 62 density rows";

/// A read-only view on stored columns: kOverviewColumns x 3 level bytes
/// (low, mid, high; round(255 * level)) and kOverviewColumns x kOverviewRows
/// density bytes (round(255 * (d / kDensityFull)^kDensityStorePower), zero outside the extent).
struct OverviewColumnsView {
    const uint8_t* levels = nullptr;
    const uint8_t* density = nullptr;
    int samplesPerColumn = 1;
    /// Columns built so far; the rest are drawn empty.
    int columnsDone = 0;
};

/// The overview source image: kOverviewColumns wide, `height` rows covering
/// -1.0 (top) to +1.0, Format_ARGB32 (straight alpha). Pure.
QImage overviewImage(const OverviewColumnsView& columns,
        const PhosphorOverviewSettings& settings,
        int height = kOverviewRows);

/// What the widget draws from overviewImage(): the same colour, opaque (alpha
/// 255) for every pixel with any alpha or colour, transparent elsewhere,
/// Format_ARGB32_Premultiplied. The stripe's alpha (2 x green) is a model
/// output for the fit; drawing it would darken the colour a second time. Pure.
QImage overviewDisplayImage(const QImage& overview);

/// The outermost non-empty row of the built columns, as a distance from the
/// center line in rows (0..31): 31 is a track that reaches 0 dBFS. Normalize
/// crops to this once the data is complete.
int overviewExtentRows(const OverviewColumnsView& columns);

/// Stored columns of one track. One writer (the analyzer), many readers: a
/// column below completedColumns() is final.
class PhosphorOverviewData {
  public:
    PhosphorOverviewData(int sampleRate, SINT totalFrames);

    int sampleRate() const {
        return m_sampleRate;
    }
    SINT totalFrames() const {
        return m_totalFrames;
    }
    /// ceil(frames / kOverviewColumns)
    int samplesPerColumn() const {
        return m_samplesPerColumn;
    }

    int completedColumns() const {
        return m_completed.load(std::memory_order_acquire);
    }
    bool isComplete() const {
        return completedColumns() >= kOverviewColumns;
    }

    OverviewColumnsView view() const;

    /// The stored form: a small header (sample rate, total frames) followed by
    /// all levels, then all densities. Zlib compression is the store's job.
    QByteArray toByteArray() const;
    /// Complete data from toByteArray(); null if the bytes do not fit.
    static std::shared_ptr<PhosphorOverviewData> fromByteArray(const QByteArray& bytes);

    // Writer side.
    uint8_t* writableLevels(int column) {
        return &m_levels[static_cast<size_t>(column) * 3];
    }
    uint8_t* writableDensity(int column) {
        return &m_density[static_cast<size_t>(column) * kOverviewRows];
    }
    void setCompletedColumns(int columns) {
        m_completed.store(columns, std::memory_order_release);
    }

  private:
    const int m_sampleRate;
    const SINT m_totalFrames;
    const int m_samplesPerColumn;
    std::vector<uint8_t> m_levels;
    std::vector<uint8_t> m_density;
    std::atomic<int> m_completed;
};

using PhosphorOverviewPointer = std::shared_ptr<const PhosphorOverviewData>;

} // namespace mixxx::phosphor
