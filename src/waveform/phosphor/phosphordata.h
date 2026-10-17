#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "util/types.h"

namespace mixxx::phosphor {

/// The Phosphor waveform's load-time data of one track: the mono sum
/// (L+R)/2 as int16 at the file's own sample rate (ADR-0002).
///
/// The buffer is preallocated, never resized, and filled from the start of
/// the track by a single writer (the analyzer thread). Readers on other
/// threads may use every point below pointsCompleted().
///
/// Readers access the data through "points" (a value at a time in frames).
/// Today every sample is a point (Full detail); detail tiers will keep
/// min/max pairs per block behind the same point accessors.
class PhosphorData {
  public:
    /// frameCapacity is the expected length. It can be inaccurate, so writes
    /// are bounded by it.
    PhosphorData(int sampleRate, SINT frameCapacity);

    int sampleRate() const {
        return m_sampleRate;
    }

    /// The number of frames of the track. Starts as the capacity and is
    /// trimmed to the real length by finish().
    SINT totalFrames() const {
        return m_totalFrames.load(std::memory_order_acquire);
    }

    /// Points that were written so far (a prefix of the track).
    SINT pointsCompleted() const {
        return m_completed.load(std::memory_order_acquire);
    }

    /// The number of points of the whole track.
    SINT pointCount() const {
        return totalFrames();
    }

    /// The time of a point in frames.
    double pointTime(SINT index) const {
        return static_cast<double>(index);
    }

    /// The value of a point, in int16 units.
    int pointValue(SINT index) const {
        return m_samples[static_cast<size_t>(index)];
    }

    /// True once the whole track was decoded.
    bool isFinished() const {
        return m_finished.load(std::memory_order_acquire);
    }

    /// Writer side. Appends mono samples; extra samples beyond the capacity
    /// are dropped.
    void append(const int16_t* pSamples, SINT count);

    /// Writer side. Marks the data complete and trims the track length to
    /// the frames that were really decoded.
    void finish();

  private:
    const int m_sampleRate;
    std::vector<int16_t> m_samples;
    SINT m_written; // writer thread only
    std::atomic<SINT> m_completed;
    std::atomic<SINT> m_totalFrames;
    std::atomic<bool> m_finished;
};

using PhosphorDataPointer = std::shared_ptr<const PhosphorData>;

} // namespace mixxx::phosphor
