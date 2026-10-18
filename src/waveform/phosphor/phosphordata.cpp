#include "waveform/phosphor/phosphordata.h"

#include <algorithm>
#include <cmath>

namespace mixxx::phosphor {

PhosphorData::PhosphorData(int sampleRate, SINT frameCapacity)
        : m_sampleRate(sampleRate),
          m_samples(static_cast<size_t>(std::max<SINT>(frameCapacity, 0)), 0),
          m_written(0),
          m_completed(0),
          m_colourBlockFrames(std::max(1, static_cast<int>(std::lround(sampleRate / 1000.0)))),
          m_colour(static_cast<size_t>(static_cast<SINT>(m_samples.size()) /
                           m_colourBlockFrames) *
                          3,
                  0),
          m_colourWritten(0),
          m_colourCompleted(0),
          m_totalFrames(static_cast<SINT>(m_samples.size())),
          m_finished(false) {
}

void PhosphorData::append(const int16_t* pSamples, SINT count) {
    const SINT room = static_cast<SINT>(m_samples.size()) - m_written;
    const SINT n = std::min(count, room);
    if (n <= 0) {
        return;
    }
    std::copy(pSamples, pSamples + n, m_samples.begin() + m_written);
    m_written += n;
    m_completed.store(m_written, std::memory_order_release);
}

void PhosphorData::appendColour(const uint16_t* pLevels, SINT blockCount) {
    const SINT room = static_cast<SINT>(m_colour.size() / 3) - m_colourWritten;
    const SINT n = std::min(blockCount, room);
    if (n <= 0) {
        return;
    }
    std::copy(pLevels, pLevels + n * 3, m_colour.begin() + m_colourWritten * 3);
    m_colourWritten += n;
    m_colourCompleted.store(m_colourWritten, std::memory_order_release);
}

void PhosphorData::finish() {
    // The colour track covers whole blocks only. Blocks that were not written
    // (data built without a colour track) stay at level 0.
    m_colourCompleted.store(m_written / m_colourBlockFrames, std::memory_order_release);
    m_totalFrames.store(m_written, std::memory_order_release);
    m_finished.store(true, std::memory_order_release);
}

} // namespace mixxx::phosphor
