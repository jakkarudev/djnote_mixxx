#pragma once

#include <QByteArray>
#include <QImage>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "util/types.h"

namespace mixxx::phosphor {

/// The Phosphor overview model (research 9, ADR-0005, spec #50).
constexpr int kOverviewColumns = 1920;
constexpr int kOverviewRows = 62;
constexpr int kOverviewColumnBytes = 3 + kOverviewRows; // levels + density

/// Version of the stored columns: bump when the row map, the column count, the
/// encoding or the color track constants change.
constexpr const char* kOverviewVersion = "PhosphorOverview-1.0";
constexpr const char* kOverviewDescription =
        "Phosphor overview: 1920 columns of 3 band levels and 62 density rows";

/// How the stored columns are drawn. Plain values at the calibrated defaults;
/// the settings group loads these from the config.
struct OverviewDrawSettings {
    double brightness = 1.0;
    double levelExponent = 0.7;
    double densityExponent = 0.25;
    double gainLow = 1.0;
    double gainMid = 1.0;
    double gainHigh = 1.0;
};

/// A read-only view on stored columns: kOverviewColumns x 3 level bytes
/// (low, mid, high; round(255 * level)) and kOverviewColumns x kOverviewRows
/// density bytes (round(255 * (d / 0.8)^0.25), zero outside the extent).
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
        const OverviewDrawSettings& settings,
        int height = kOverviewRows);

/// The outermost non-empty row of the built columns, as a distance from the
/// centre line in rows (0..31): 31 is a track that reaches 0 dBFS. Normalize
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
