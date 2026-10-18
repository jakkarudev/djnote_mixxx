#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "util/types.h"
#include "waveform/phosphor/phosphorsettings.h"

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

    /// The color track (ADR-0002): three uint16 smoothed band levels (R, G, B =
    /// Low, Mid, High; level * 65535) per block of colorBlockFrames() frames,
    /// 1 ms. It fills in behind the sample buffer.
    SINT colorBlockFrames() const {
        return m_colorBlockFrames;
    }
    /// Blocks that were written so far (a prefix of the track).
    SINT colorBlocksCompleted() const {
        return m_colorCompleted.load(std::memory_order_acquire);
    }
    int colorLevel(SINT block, int channel) const {
        return m_color[static_cast<size_t>(block) * 3 + static_cast<size_t>(channel)];
    }

    /// The settings the color track is built with. Set before the data is
    /// shared. A track whose data was built with other settings is rebuilt.
    const ColorTrackParams& colorParams() const {
        return m_colorParams;
    }
    void setColorParams(const ColorTrackParams& params) {
        m_colorParams = params;
    }
    /// The exponent the levels were built with (level = peak^exponent); the
    /// shader needs it to turn levels back into a brightness.
    double colorMixExponent() const {
        return m_colorParams.mixExponent;
    }

    /// Writer side. Appends blocks of (R, G, B) levels; extra blocks beyond
    /// the capacity are dropped.
    void appendColor(const uint16_t* pLevels, SINT blockCount);

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
    ColorTrackParams m_colorParams;
    const SINT m_colorBlockFrames;
    std::vector<uint16_t> m_color;
    SINT m_colorWritten; // writer thread only
    std::atomic<SINT> m_colorCompleted;
    std::atomic<SINT> m_totalFrames;
    std::atomic<bool> m_finished;
};

using PhosphorDataPointer = std::shared_ptr<const PhosphorData>;

} // namespace mixxx::phosphor
