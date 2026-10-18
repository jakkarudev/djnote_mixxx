#include "analyzer/analyzerphosphor.h"

#include <algorithm>
#include <cmath>

#include "analyzer/constants.h"
#include "track/track.h"
#include "waveform/phosphor/phosphordata.h"
#include "util/logger.h"
#include "waveform/phosphor/phosphorsettings.h"

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
    // The tier is read per track, so a change applies from the next load. It
    // is a floor: a long input drops to a lower tier to stay under the cap.
    const auto settings = mixxx::phosphor::loadSettings(m_pConfig);
    const auto detail = mixxx::phosphor::effectiveDetail(settings.detail, frameLength);
    const int blockSize = mixxx::phosphor::blockSizeOf(detail);
    const TrackPointer pTrack = track.getTrack();
    std::shared_ptr<mixxx::phosphor::PhosphorData> pData;
    for (;;) {
        // Already built, or being built for another deck, with the settings
        // that apply now: share it. Otherwise replace it, unless another
        // thread replaced it first (then look again).
        const auto pExisting = pTrack->getPhosphorData();
        if (pExisting && pExisting->blockSize() == blockSize &&
                pExisting->sampleRate() == sampleRate.value() &&
                pExisting->colorParams() == settings.color) {
            return false;
        }
        if (!pData) {
            pData = std::make_shared<mixxx::phosphor::PhosphorData>(
                    sampleRate.value(), frameLength, blockSize);
            // The color settings are read per track too: they apply from the
            // next load.
            pData->setColorParams(settings.color);
        }
        if (pTrack->replacePhosphorData(pExisting, pData)) {
            break;
        }
    }
    // From now on other threads can touch it.
    m_pTrack = pTrack;
    m_pData = std::move(pData);
    m_stored = false;
    m_pColor = std::make_unique<mixxx::phosphor::ColorTrackBuilder>(
            sampleRate.value(), settings.color);
    return true;
}

bool AnalyzerPhosphor::processSamples(const CSAMPLE* pIn, SINT count) {
    DEBUG_ASSERT(m_pData);
    const SINT frames = count / mixxx::kAnalysisChannels;
    m_mono.resize(static_cast<size_t>(frames));
    for (SINT i = 0; i < frames; ++i) {
        m_mono[static_cast<size_t>(i)] = monoSample(pIn[2 * i], pIn[2 * i + 1]);
    }
    m_pData->append(m_mono.data(), frames);
    // The color track trails the samples (filter delay, look-ahead).
    m_colorOut.clear();
    m_pColor->append(m_mono.data(), frames, &m_colorOut);
    m_pData->appendColor(m_colorOut.data(), static_cast<SINT>(m_colorOut.size() / 3));
    return true;
}

void AnalyzerPhosphor::storeResults(TrackPointer pTrack) {
    Q_UNUSED(pTrack);
    if (m_pData) {
        m_colorOut.clear();
        m_pColor->finish(&m_colorOut);
        m_pData->appendColor(m_colorOut.data(), static_cast<SINT>(m_colorOut.size() / 3));
        m_pData->finish();
        m_stored = true;
    }
}

void AnalyzerPhosphor::cleanup() {
    if (m_pTrack && m_pData && !m_stored) {
        // Aborted: do not leave half-filled data on the track, so that the
        // next load builds it again.
        // (Unless a newer build already replaced it.)
        m_pTrack->replacePhosphorData(m_pData, nullptr);
    }
    m_pTrack.reset();
    m_pData.reset();
    m_mono.clear();
    m_pColor.reset();
    m_colorOut.clear();
    m_stored = false;
}
