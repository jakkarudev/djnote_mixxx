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
/// At block size 1 (Full detail) every sample is a point. At a block size N > 1
/// (the detail tiers) each block of N samples gives two points: the block's
/// minimum and maximum in the order they first occurred, at 1/4 and 3/4 of the
/// block's length (a shorter last block uses its own length).
class PhosphorData {
  public:
    /// frameCapacity is the expected length. It can be inaccurate, so writes
    /// are bounded by it.
    PhosphorData(int sampleRate, SINT frameCapacity, int blockSize = 1);

    /// Samples per block: 1 keeps every sample.
    int blockSize() const {
        return m_blockSize;
    }

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
        return pointsFor(totalFrames());
    }

    /// The time of a point in frames.
    double pointTime(SINT index) const;

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
    SINT pointsFor(SINT frames) const {
        return m_blockSize == 1 ? frames : (frames + m_blockSize - 1) / m_blockSize * 2;
    }
    void flushBlock();

    const int m_sampleRate;
    const int m_blockSize;
    const SINT m_frameCapacity;
    std::vector<int16_t> m_samples;
    // Writer thread only.
    SINT m_written; // points written
    SINT m_framesWritten;
    int m_blockFill;
    int m_blockMin;
    int m_blockMax;
    int m_blockMinAt;
    int m_blockMaxAt;
    std::atomic<SINT> m_completed;
    std::atomic<SINT> m_totalFrames;
    std::atomic<bool> m_finished;
};

using PhosphorDataPointer = std::shared_ptr<const PhosphorData>;

} // namespace mixxx::phosphor
