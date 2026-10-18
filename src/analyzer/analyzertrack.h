#pragma once

#include <optional>

#include "track/track_decl.h"

/// A scheduled not-null track with additional options for analysis.
class AnalyzerTrack {
  public:
    struct Options {
        /// If set, overrides whether the analysis should assume constant BPM.
        std::optional<bool> useFixedTempo;

        /// If set, the Phosphor waveform data is built for this track. Set for
        /// deck loads only, not for samplers, preview decks or batch analysis.
        bool buildPhosphor = false;
    };

    explicit AnalyzerTrack(TrackPointer track, Options options = Options());

    /// Fetches the (not-null) track to be analyzed.
    const TrackPointer& getTrack() const;

    /// Fetches the additional options.
    const Options& getOptions() const;

  private:
    /// The (not-null) track to be analyzed.
    TrackPointer m_track;
    /// The additional options.
    Options m_options;
};
