#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

#include "waveform/phosphor/phosphordata.h"
#include "waveform/phosphor/phosphorlanemodel.h"

namespace mixxx::phosphor {

/// Columns of the lane model, cached by their position along the track.
///
/// Column k is centered on frame (k + 0.5) * framesPerColumn, so a column
/// keeps its index while the view scrolls. Only columns that are asked for
/// are computed. What a column looks like never depends on what was cached
/// before: the cache is only a memo of computeColumn(), invalidated as a whole
/// by a params or data change, never by scroll direction.
///
/// Not thread-safe: use one instance from one thread at a time. A recompute
/// after a zoom or size change is done on a second instance on a worker thread
/// and swapped in by the owner when ready.
class PhosphorLaneCache {
  public:
    PhosphorLaneCache(PhosphorDataPointer pData, const LaneParams& params);

    const LaneParams& params() const {
        return m_params;
    }

    /// Changes the params. Drops all columns when they differ.
    void setParams(const LaneParams& params);

    /// The column index of the column that contains a frame.
    int64_t columnAtFrame(double frame) const;
    double centerOfColumn(int64_t k) const;

    /// Drops the columns outside [first, last], with some margin kept for
    /// scrolling.
    void retain(int64_t first, int64_t last);

    /// The column, computed on first use. Null while the data it needs is not
    /// decoded yet (nothing is cached then). The pointer stays valid until the
    /// next call that changes the cache (column, retain, setParams).
    const LaneColumn* column(int64_t k);

    bool contains(int64_t k) const {
        return m_columns.find(k) != m_columns.end();
    }

    const PhosphorDataPointer& data() const {
        return m_pData;
    }

    size_t cachedColumns() const {
        return m_columns.size();
    }

  private:
    void invalidateIfDataChanged();

    PhosphorDataPointer m_pData;
    LaneParams m_params;
    SINT m_totalFramesStamp;
    std::unordered_map<int64_t, LaneColumn> m_columns;
};

/// A lane cache being built on a worker thread, to be swapped in by the owner
/// (the GUI thread) when ready. Never blocks the owner: cancel() just forgets
/// the build, and its result is dropped when the worker finishes.
class PendingCacheBuild {
  public:
    using Job = std::function<std::unique_ptr<PhosphorLaneCache>()>;

    PendingCacheBuild() = default;
    ~PendingCacheBuild() {
        cancel();
    }

    /// Starts the job on a new thread; a build that was running is cancelled.
    void start(Job job);
    bool isRunning() const {
        return m_pState != nullptr;
    }
    /// The finished cache (once), or null if still running or not started.
    std::unique_ptr<PhosphorLaneCache> takeIfReady();
    void cancel() {
        m_pState.reset();
    }

  private:
    struct State {
        std::atomic<bool> ready{false};
        std::unique_ptr<PhosphorLaneCache> result;
    };
    // Shared with the worker, which outlives a cancelled build.
    std::shared_ptr<State> m_pState;
};

} // namespace mixxx::phosphor
