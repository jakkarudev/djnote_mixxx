#include "analyzer/analyzerphosphoroverview.h"

#include <algorithm>
#include <cmath>

#include "analyzer/constants.h"
#include "preferences/waveformsettings.h"
#include "track/track.h"
#include "util/logger.h"
#include "widget/woverview.h"

namespace {

mixxx::Logger kLogger("AnalyzerPhosphorOverview");

using namespace mixxx::phosphor;

const ConfigKey kOverviewTypeKey(
        QStringLiteral("[Waveform]"), QStringLiteral("WaveformOverviewType"));

/// floor((L + R) / 2 + 0.5) on int16 values, clipped (ties round up).
inline int16_t monoSample(CSAMPLE left, CSAMPLE right) {
    const double mono = std::floor((static_cast<double>(left) + right) * 0.5 * 32768.0 + 0.5);
    return static_cast<int16_t>(std::clamp(mono, -32768.0, 32767.0));
}

inline uint8_t roundU8(double v) {
    return static_cast<uint8_t>(std::clamp(std::floor(v + 0.5), 0.0, 255.0));
}

} // namespace

AnalyzerPhosphorOverview::AnalyzerPhosphorOverview(
        UserSettingsPointer pConfig, const QSqlDatabase& dbConnection)
        : m_pConfig(pConfig),
          m_analysisDao(pConfig),
          m_colorBlockFrames(1),
          m_densityColumns(0),
          m_writtenColumns(0),
          m_framesSeen(0),
          m_stored(false) {
    m_analysisDao.initialize(dbConnection);
}

// static
bool AnalyzerPhosphorOverview::isSelected(const UserSettingsPointer& pConfig) {
    return pConfig->getValue<WOverview::Type>(kOverviewTypeKey, WOverview::Type::RGB) ==
            WOverview::Type::Phosphor;
}

// static
bool AnalyzerPhosphorOverview::needsBuild(bool selected,
        const PhosphorOverviewPointer& existing,
        int sampleRate,
        SINT frameLength) {
    if (!selected) {
        return false;
    }
    // Same sample rate and length: same columns. A changed length (re-encoded
    // file) changes the column size and the data is rebuilt.
    return !existing || existing->sampleRate() != sampleRate ||
            existing->totalFrames() != frameLength;
}

PhosphorOverviewPointer AnalyzerPhosphorOverview::loadStored(
        TrackId trackId, int sampleRate, SINT frameLength) {
    if (!trackId.isValid() || !WaveformSettings(m_pConfig).waveformCachingEnabled()) {
        return nullptr;
    }
    PhosphorOverviewPointer pLoaded;
    const auto rows = m_analysisDao.getAnalysesForTrackByType(
            trackId, AnalysisDao::TYPE_PHOSPHOR_OVERVIEW);
    for (const auto& row : rows) {
        std::shared_ptr<PhosphorOverviewData> pData;
        if (row.version == kOverviewVersion) {
            pData = PhosphorOverviewData::fromByteArray(row.data);
        }
        if (!pLoaded && pData && !needsBuild(true, pData, sampleRate, frameLength)) {
            pLoaded = pData;
        } else {
            // Older version, other length, unreadable, or a duplicate: rebuild.
            m_analysisDao.deleteAnalysis(row.analysisId);
        }
    }
    return pLoaded;
}

bool AnalyzerPhosphorOverview::initialize(const AnalyzerTrack& track,
        mixxx::audio::SampleRate sampleRate,
        SINT frameLength) {
    cleanup();
    if (!isSelected(m_pConfig)) {
        return false;
    }
    if (!sampleRate.isValid() || frameLength <= 0) {
        kLogger.warning() << "Cannot build the Phosphor overview: invalid signal info";
        return false;
    }
    const TrackPointer pTrack = track.getTrack();
    std::shared_ptr<PhosphorOverviewData> pData;
    bool triedStore = false;
    for (;;) {
        // Built or being built for another deck: share it. Otherwise replace
        // it, unless another thread replaced it first (then look again).
        const auto pExisting = pTrack->getPhosphorOverview();
        if (!needsBuild(true, pExisting, sampleRate.value(), frameLength)) {
            return false;
        }
        if (!pExisting && !triedStore) {
            // First look in the analysis store: current data is simply shown.
            triedStore = true;
            if (const auto pStored = loadStored(pTrack->getId(), sampleRate.value(), frameLength)) {
                if (pTrack->replacePhosphorOverview(pExisting, pStored)) {
                    return false;
                }
                continue; // lost the race, look again
            }
        }
        if (!pData) {
            pData = std::make_shared<PhosphorOverviewData>(sampleRate.value(), frameLength);
        }
        if (pTrack->replacePhosphorOverview(pExisting, pData)) {
            break;
        }
    }
    m_pTrack = pTrack;
    m_pData = std::move(pData);
    m_stored = false;
    // Fixed calibrated constants, never the scrolling waveform's settings.
    m_pColor = std::make_unique<ColorTrackBuilder>(sampleRate.value());
    m_colorBlockFrames = colorBlockFrames(sampleRate.value());
    m_own.assign(kOverviewColumns, Histogram{});
    m_first.assign(kOverviewColumns, Histogram{});
    m_last.assign(kOverviewColumns, Histogram{});
    m_ownNonzero.assign(kOverviewColumns, false);
    return true;
}

void AnalyzerPhosphorOverview::writeReadyDensity(bool finished) {
    const int n = m_pData->samplesPerColumn();
    const int over = n / 2; // the window extends half a column on each side
    while (m_densityColumns < kOverviewColumns) {
        const int k = m_densityColumns;
        const SINT a = static_cast<SINT>(k) * n;
        if (finished ? a >= m_framesSeen : a + n + over > m_framesSeen) {
            break;
        }
        Histogram window = m_own[k];
        if (m_ownNonzero[k]) {
            // A digitally silent column stays a one-row line instead.
            for (int r = 0; r < kOverviewRows; ++r) {
                window[r] += (k > 0 ? m_last[k - 1][r] : 0) +
                        (k + 1 < kOverviewColumns ? m_first[k + 1][r] : 0);
            }
        }
        int count = 0;
        for (int r = 0; r < kOverviewRows; ++r) {
            count += window[r];
        }
        uint8_t* pDensity = m_pData->writableDensity(k);
        for (int r = 0; r < kOverviewRows; ++r) {
            pDensity[r] = roundU8(255.0 * std::pow(static_cast<double>(window[r]) / std::max(1, count) / 0.8, 0.25));
        }
        ++m_densityColumns;
    }
}

void AnalyzerPhosphorOverview::writeReadyColumns(bool finished) {
    const int n = m_pData->samplesPerColumn();
    const int blocks = static_cast<int>(m_color.size() / 3);
    while (m_writtenColumns < m_densityColumns) {
        const SINT a = static_cast<SINT>(m_writtenColumns) * n;
        const SINT b = std::min<SINT>(a + n, m_framesSeen);
        const int l0 = static_cast<int>(a / m_colorBlockFrames);
        const int l1 = std::max(l0 + 1, static_cast<int>(b / m_colorBlockFrames));
        if (!finished && blocks < l1) {
            break; // the color track still trails the samples
        }
        uint16_t peak[3] = {0, 0, 0};
        for (int j = l0; j < std::min(l1, blocks); ++j) {
            for (int c = 0; c < 3; ++c) {
                peak[c] = std::max(peak[c], m_color[static_cast<size_t>(j) * 3 + c]);
            }
        }
        uint8_t* pLevels = m_pData->writableLevels(m_writtenColumns);
        for (int c = 0; c < 3; ++c) {
            pLevels[c] = roundU8(peak[c] * 255.0 / 65535.0);
        }
        ++m_writtenColumns;
    }
    m_pData->setCompletedColumns(m_writtenColumns);
}

bool AnalyzerPhosphorOverview::processSamples(const CSAMPLE* pIn, SINT count) {
    DEBUG_ASSERT(m_pData);
    const SINT frames = count / mixxx::kAnalysisChannels;
    const int n = m_pData->samplesPerColumn();
    m_mono.resize(static_cast<size_t>(frames));
    for (SINT i = 0; i < frames; ++i) {
        const int16_t mono = monoSample(pIn[2 * i], pIn[2 * i + 1]);
        m_mono[static_cast<size_t>(i)] = mono;
        const SINT frame = m_framesSeen + i;
        const SINT column = frame / n;
        if (column < kOverviewColumns) {
            const int row = std::clamp(
                    static_cast<int>(std::floor(mono / 32768.0 * 31 + 31)),
                    0,
                    kOverviewRows - 1);
            const SINT start = column * n;
            ++m_own[column][row];
            if (mono != 0) {
                m_ownNonzero[column] = true;
            }
            if (frame - start < n / 2) {
                ++m_first[column][row];
            }
            if (frame - start >= n - n / 2) {
                ++m_last[column][row];
            }
        }
    }
    m_framesSeen += frames;
    writeReadyDensity(false);
    // The color track trails the samples (filter delay, look-ahead).
    m_colorOut.clear();
    m_pColor->append(m_mono.data(), frames, &m_colorOut);
    m_color.insert(m_color.end(), m_colorOut.begin(), m_colorOut.end());
    writeReadyColumns(false);
    return true;
}

void AnalyzerPhosphorOverview::storeResults(TrackPointer pTrack) {
    Q_UNUSED(pTrack);
    if (!m_pData) {
        return;
    }
    m_colorOut.clear();
    m_pColor->finish(&m_colorOut);
    m_color.insert(m_color.end(), m_colorOut.begin(), m_colorOut.end());
    writeReadyDensity(true);
    writeReadyColumns(true);
    // Columns past the end of the track stay empty.
    m_pData->setCompletedColumns(kOverviewColumns);
    m_stored = true;
    const TrackId trackId = m_pTrack ? m_pTrack->getId() : TrackId();
    if (trackId.isValid() && WaveformSettings(m_pConfig).waveformCachingEnabled()) {
        AnalysisDao::AnalysisInfo info;
        info.trackId = trackId;
        info.type = AnalysisDao::TYPE_PHOSPHOR_OVERVIEW;
        info.description = kOverviewDescription;
        info.version = kOverviewVersion;
        info.data = m_pData->toByteArray();
        if (!m_analysisDao.saveAnalysis(&info)) {
            kLogger.warning() << "Failed to save the Phosphor overview of track" << trackId;
        }
    }
}

void AnalyzerPhosphorOverview::cleanup() {
    if (m_pTrack && m_pData && !m_stored) {
        // Aborted: do not leave half-built data on the track, so that the
        // next load builds it again (unless a newer build replaced it).
        m_pTrack->replacePhosphorOverview(m_pData, nullptr);
    }
    m_pTrack.reset();
    m_pData.reset();
    m_pColor.reset();
    m_mono.clear();
    m_color.clear();
    m_colorOut.clear();
    m_own.clear();
    m_first.clear();
    m_last.clear();
    m_ownNonzero.clear();
    m_densityColumns = 0;
    m_writtenColumns = 0;
    m_framesSeen = 0;
    m_stored = false;
}
