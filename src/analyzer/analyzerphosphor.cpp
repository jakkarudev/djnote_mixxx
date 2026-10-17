#include "analyzer/analyzerphosphor.h"

#include <algorithm>
#include <cmath>

#include "analyzer/constants.h"
#include "track/track.h"
#include "util/logger.h"

namespace {

mixxx::Logger kLogger("AnalyzerPhosphor");

/// floor((L + R) / 2 + 0.5) on int16 values, clipped (ties round up).
inline int16_t monoSample(CSAMPLE left, CSAMPLE right) {
    const double mono = std::floor((static_cast<double>(left) + right) * 0.5 * 32768.0 + 0.5);
    return static_cast<int16_t>(std::clamp(mono, -32768.0, 32767.0));
}

} // namespace

AnalyzerPhosphor::AnalyzerPhosphor(UserSettingsPointer pConfig)
        : m_pConfig(std::move(pConfig)),
          m_stored(false) {
}

bool AnalyzerPhosphor::initialize(const AnalyzerTrack& track,
        mixxx::audio::SampleRate sampleRate,
        SINT frameLength) {
    cleanup();
    if (!track.getOptions().buildPhosphor) {
        return false;
    }
    if (!sampleRate.isValid() || frameLength <= 0) {
        kLogger.warning() << "Cannot build Phosphor data: invalid signal info";
        return false;
    }
    // Already built, or being built for another deck: share it.
    if (track.getTrack()->getPhosphorData()) {
        return false;
    }
    m_pTrack = track.getTrack();
    m_pData = std::make_shared<mixxx::phosphor::PhosphorData>(sampleRate.value(), frameLength);
    m_stored = false;
    // From now on other threads can touch it.
    m_pTrack->setPhosphorData(m_pData);
    return true;
}

bool AnalyzerPhosphor::processSamples(const CSAMPLE* pIn, SINT count) {
    DEBUG_ASSERT(m_pData);
    const SINT frames = count / 2;
    m_mono.resize(static_cast<size_t>(frames));
    for (SINT i = 0; i < frames; ++i) {
        m_mono[static_cast<size_t>(i)] = monoSample(pIn[2 * i], pIn[2 * i + 1]);
    }
    m_pData->append(m_mono.data(), frames);
    return true;
}

void AnalyzerPhosphor::storeResults(TrackPointer pTrack) {
    Q_UNUSED(pTrack);
    if (m_pData) {
        m_pData->finish();
        m_stored = true;
    }
}

void AnalyzerPhosphor::cleanup() {
    if (m_pTrack && m_pData && !m_stored) {
        // Aborted: do not leave half-filled data on the track, so that the
        // next load builds it again.
        m_pTrack->setPhosphorData(nullptr);
    }
    m_pTrack.reset();
    m_pData.reset();
    m_mono.clear();
    m_stored = false;
}
