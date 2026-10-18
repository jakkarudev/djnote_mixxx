#include "waveform/phosphor/phosphordata.h"

#include <algorithm>

namespace mixxx::phosphor {

PhosphorData::PhosphorData(int sampleRate, SINT frameCapacity, int blockSize)
        : m_sampleRate(sampleRate),
          m_blockSize(std::max(blockSize, 1)),
          m_frameCapacity(std::max<SINT>(frameCapacity, 0)),
          m_samples(static_cast<size_t>(pointsFor(m_frameCapacity)), 0),
          m_written(0),
          m_framesWritten(0),
          m_blockFill(0),
          m_blockMin(0),
          m_blockMax(0),
          m_blockMinAt(0),
          m_blockMaxAt(0),
          m_completed(0),
          m_totalFrames(m_frameCapacity),
          m_finished(false) {
}

double PhosphorData::pointTime(SINT index) const {
    if (m_blockSize == 1) {
        return static_cast<double>(index);
    }
    const SINT block = index / 2;
    const double start = static_cast<double>(block) * m_blockSize;
    const double length = std::min<double>(m_blockSize, totalFrames() - start);
    return start + length * ((index & 1) ? 0.75 : 0.25);
}

void PhosphorData::flushBlock() {
    // The first value is whichever of min and max occurred first.
    const bool minFirst = m_blockMinAt <= m_blockMaxAt;
    m_samples[static_cast<size_t>(m_written++)] =
            static_cast<int16_t>(minFirst ? m_blockMin : m_blockMax);
    m_samples[static_cast<size_t>(m_written++)] =
            static_cast<int16_t>(minFirst ? m_blockMax : m_blockMin);
    m_blockFill = 0;
}

void PhosphorData::append(const int16_t* pSamples, SINT count) {
    const SINT n = std::min(count, m_frameCapacity - m_framesWritten);
    if (n <= 0) {
        return;
    }
    if (m_blockSize == 1) {
        std::copy(pSamples, pSamples + n, m_samples.begin() + m_written);
        m_written += n;
        m_framesWritten += n;
        m_completed.store(m_written, std::memory_order_release);
        return;
    }
    for (SINT i = 0; i < n; ++i) {
        const int v = pSamples[i];
        if (m_blockFill == 0) {
            m_blockMin = m_blockMax = v;
            m_blockMinAt = m_blockMaxAt = 0;
        } else {
            if (v < m_blockMin) {
                m_blockMin = v;
                m_blockMinAt = m_blockFill;
            }
            if (v > m_blockMax) {
                m_blockMax = v;
                m_blockMaxAt = m_blockFill;
            }
        }
        if (++m_blockFill == m_blockSize) {
            flushBlock();
        }
    }
    m_framesWritten += n;
    m_completed.store(m_written, std::memory_order_release);
}

void PhosphorData::finish() {
    if (m_blockSize > 1 && m_blockFill > 0) {
        flushBlock();
        m_completed.store(m_written, std::memory_order_release);
    }
    m_totalFrames.store(m_framesWritten, std::memory_order_release);
    m_finished.store(true, std::memory_order_release);
}

} // namespace mixxx::phosphor
