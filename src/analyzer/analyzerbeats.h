#pragma once

#include <QHash>
#include <QList>
#include <memory>

#include "analyzer/analyzer.h"
#include "analyzer/beatgridrefiner.h"
#include "analyzer/plugins/analyzerplugin.h"
#include "preferences/beatdetectionsettings.h"
#include "preferences/usersettings.h"

class AnalyzerBeats : public Analyzer {
  public:
    explicit AnalyzerBeats(
            UserSettingsPointer pConfig,
            bool enforceBpmDetection = false);
    ~AnalyzerBeats() override = default;

    static QList<mixxx::AnalyzerPluginInfo> availablePlugins();
    static mixxx::AnalyzerPluginInfo defaultPlugin();

    bool initialize(const AnalyzerTrack& track,
            mixxx::audio::SampleRate sampleRate,
            SINT frameLength) override;
    bool processSamples(const CSAMPLE* pIn, SINT count) override;
    void storeResults(TrackPointer tio) override;
    void cleanup() override;

  private:
    bool shouldAnalyze(TrackPointer pTrack) const;
    static QHash<QString, QString> getExtraVersionInfo(const QString& pluginId,
            bool bPreferencesFastAnalysis,
            bool bPreferencesFixedTempo);

    BeatDetectionSettings m_bpmSettings;
    std::unique_ptr<mixxx::AnalyzerBeatsPlugin> m_pPlugin;
    /// Places constant-tempo grids precisely; only used with the fixed tempo
    /// assumption.
    std::unique_ptr<mixxx::BeatGridRefiner> m_pRefiner;
    const bool m_enforceBpmDetection;
    QString m_pluginId;
    bool m_bPreferencesReanalyzeOldBpm;
    bool m_bPreferencesReanalyzeImported;
    bool m_bPreferencesFixedTempo;
    bool m_bPreferencesFastAnalysis;

    mixxx::audio::SampleRate m_sampleRate;
    SINT m_maxFramesToProcess;
    SINT m_currentFrame;
};
