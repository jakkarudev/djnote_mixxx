#pragma once

#include <QSqlDatabase>
#include <array>
#include <memory>
#include <vector>

#include "analyzer/analyzer.h"
#include "library/dao/analysisdao.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"
#include "waveform/phosphor/phosphorcolortrack.h"
#include "waveform/phosphor/phosphoroverview.h"

/// Builds the Phosphor overview columns of a track (ADR-0005, spec #50) from
/// the samples the analyzer thread already streams: per column the peak hold
/// of the 1 ms color track (fixed calibrated constants) and the row density
/// of the mono sum (L+R)/2.
///
/// It runs only while Phosphor is the selected overview type and the track
/// holds no current data. The data object is handed to the track in
/// initialize(); its completion is published as columns become final.
class AnalyzerPhosphorOverview : public Analyzer {
  public:
    /// `dbConnection` is the analysis store; without one (default) nothing is
    /// loaded or saved and the data lives on the track only.
    explicit AnalyzerPhosphorOverview(UserSettingsPointer pConfig,
            const QSqlDatabase& dbConnection = QSqlDatabase());

    /// True if the overview type setting is Phosphor.
    static bool isSelected(const UserSettingsPointer& pConfig);

    /// The single place that decides whether a track needs its overview built:
    /// Phosphor is selected, and the data `existing` on the track is missing
    /// or not current. In-progress data counts as current, so a second
    /// request for the same track does not start a second build.
    static bool needsBuild(bool selected,
            const mixxx::phosphor::PhosphorOverviewPointer& existing,
            int sampleRate,
            SINT frameLength);

    bool initialize(const AnalyzerTrack& track,
            mixxx::audio::SampleRate sampleRate,
            SINT frameLength) override;
    bool processSamples(const CSAMPLE* pIn, SINT count) override;
    void storeResults(TrackPointer pTrack) override;
    void cleanup() override;

  private:
    /// Stored data of the track, if current; deletes stale rows. Null if none.
    mixxx::phosphor::PhosphorOverviewPointer loadStored(
            TrackId trackId, int sampleRate, SINT frameLength);
    void finishColumn();
    void writeReadyColumns(bool finished);

    UserSettingsPointer m_pConfig;
    AnalysisDao m_analysisDao;
    TrackPointer m_pTrack;
    std::shared_ptr<mixxx::phosphor::PhosphorOverviewData> m_pData;
    std::unique_ptr<mixxx::phosphor::ColorTrackBuilder> m_pColor;
    std::vector<int16_t> m_mono;
    std::vector<uint16_t> m_color; // 3 per 1 ms block, all so far
    std::vector<uint16_t> m_colorOut;
    std::array<int, mixxx::phosphor::kOverviewRows> m_histogram;
    int m_colorBlockFrames;
    int m_columnFill;     // samples in the column being counted
    int m_densityColumns; // columns whose density is final
    int m_writtenColumns; // columns whose levels are final too
    std::array<double, 3> m_held{}; // last column's held levels (colour tails)
    SINT m_framesSeen;
    bool m_stored;
};
