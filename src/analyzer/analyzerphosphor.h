#pragma once

#include <memory>
#include <vector>

#include "analyzer/analyzer.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"
#include "waveform/phosphor/phosphorcolortrack.h"
#include "waveform/phosphor/phosphordata.h"

/// Builds the Phosphor waveform data of a track while it is decoded: the mono
/// sum (L+R)/2 as int16 at the file's sample rate (ADR-0002). Nothing is
/// stored. It only runs when AnalyzerTrack::Options::buildPhosphor is set,
/// which only deck loads do.
///
/// The data object is handed to the track in initialize(), so renderers can
/// draw what has been decoded so far.
class AnalyzerPhosphor : public Analyzer {
  public:
    explicit AnalyzerPhosphor(UserSettingsPointer pConfig);

    bool initialize(const AnalyzerTrack& track,
            mixxx::audio::SampleRate sampleRate,
            SINT frameLength) override;
    bool processSamples(const CSAMPLE* pIn, SINT count) override;
    void storeResults(TrackPointer pTrack) override;
    void cleanup() override;

  private:
    UserSettingsPointer m_pConfig;
    TrackPointer m_pTrack;
    std::shared_ptr<mixxx::phosphor::PhosphorData> m_pData;
    std::vector<int16_t> m_mono;
    std::unique_ptr<mixxx::phosphor::ColorTrackBuilder> m_pColor;
    std::vector<uint16_t> m_colorOut;
    bool m_stored;
};
