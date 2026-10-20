#include "analyzer/analyzerphosphoroverview.h"

#include <algorithm>
#include <cmath>

#include "analyzer/constants.h"
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

AnalyzerPhosphorOverview::AnalyzerPhosphorOverview(UserSettingsPointer pConfig)
        : m_pConfig(std::move(pConfig)),
          m_histogram{},
          m_colorBlockFrames(1),
          m_columnFill(0),
          m_densityColumns(0),
          m_writtenColumns(0),
          m_framesSeen(0),
          m_stored(false) {
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
    for (;;) {
        // Built or being built for another deck: share it. Otherwise replace
        // it, unless another thread replaced it first (then look again).
        const auto pExisting = pTrack->getPhosphorOverview();
        if (!needsBuild(true, pExisting, sampleRate.value(), frameLength)) {
            return false;
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
    return true;
}

void AnalyzerPhosphorOverview::finishColumn() {
    uint8_t* pDensity = m_pData->writableDensity(m_densityColumns);
    const double count = std::max(1, m_columnFill);
    for (int r = 0; r < kOverviewRows; ++r) {
        pDensity[r] = roundU8(255.0 * std::pow(m_histogram[r] / count / 0.8, 0.25));
    }
    m_histogram.fill(0);
    m_columnFill = 0;
    ++m_densityColumns;
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
        if (m_densityColumns < kOverviewColumns) {
            const int row = std::clamp(
                    static_cast<int>(std::floor(mono / 32768.0 * 31 + 31)),
                    0,
                    kOverviewRows - 1);
            ++m_histogram[row];
            ++m_columnFill;
            if (m_columnFill == n) {
                finishColumn();
            }
        }
    }
    m_framesSeen += frames;
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
    if (m_columnFill > 0 && m_densityColumns < kOverviewColumns) {
        finishColumn(); // the track was shorter than announced
    }
    writeReadyColumns(true);
    // Columns past the end of the track stay empty.
    m_pData->setCompletedColumns(kOverviewColumns);
    m_stored = true;
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
    m_histogram.fill(0);
    m_columnFill = 0;
    m_densityColumns = 0;
    m_writtenColumns = 0;
    m_framesSeen = 0;
    m_stored = false;
}
