#include "waveform/phosphor/phosphordata.h"

#include <algorithm>

namespace mixxx::phosphor {

PhosphorData::PhosphorData(int sampleRate, SINT frameCapacity)
        : m_sampleRate(sampleRate),
          m_samples(static_cast<size_t>(std::max<SINT>(frameCapacity, 0)), 0),
          m_written(0),
          m_completed(0),
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

void PhosphorData::finish() {
    m_totalFrames.store(m_written, std::memory_order_release);
    m_finished.store(true, std::memory_order_release);
}

} // namespace mixxx::phosphor
