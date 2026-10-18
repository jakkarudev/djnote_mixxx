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

    /// The colour track (ADR-0002): three uint16 smoothed band levels (R, G, B =
    /// Low, Mid, High; level * 65535) per block of colourBlockFrames() frames,
    /// 1 ms. It fills in behind the sample buffer.
    SINT colourBlockFrames() const {
        return m_colourBlockFrames;
    }
    /// Blocks that were written so far (a prefix of the track).
    SINT colourBlocksCompleted() const {
        return m_colourCompleted.load(std::memory_order_acquire);
    }
    int colourLevel(SINT block, int channel) const {
        return m_colour[static_cast<size_t>(block) * 3 + static_cast<size_t>(channel)];
    }

    /// The exponent the levels were built with (level = peak^exponent); the
    /// shader needs it to turn levels back into a brightness. Set before the
    /// data is shared.
    double colourMixExponent() const {
        return m_colourMixExponent;
    }
    void setColourMixExponent(double exponent) {
        m_colourMixExponent = exponent;
    }

    /// Writer side. Appends blocks of (R, G, B) levels; extra blocks beyond
    /// the capacity are dropped.
    void appendColour(const uint16_t* pLevels, SINT blockCount);

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
    double m_colourMixExponent = 0.35;
    const SINT m_colourBlockFrames;
    std::vector<uint16_t> m_colour;
    SINT m_colourWritten; // writer thread only
    std::atomic<SINT> m_colourCompleted;
    std::atomic<SINT> m_totalFrames;
    std::atomic<bool> m_finished;
};

using PhosphorDataPointer = std::shared_ptr<const PhosphorData>;

} // namespace mixxx::phosphor
