#pragma once

#include <optional>
#include <vector>

#include "audio/frame.h"
#include "audio/types.h"
#include "track/bpm.h"
#include "util/types.h"

namespace mixxx {

/// Places a constant-tempo beat grid precisely on the kicks, with its first
/// beat on a downbeat.
///
/// The beat tracker plugins find the tempo, but their beats are only accurate
/// to about 12 ms, can sit on the off-beat of a rolling bassline, and can lock
/// onto 2/3 of the tempo. This class keeps 1 ms energy envelopes of the whole
/// track, a low band (below 150 Hz) and a high band (above 1 kHz), and folds
/// them at candidate beat lengths: the tempo is where the folded onsets are
/// most peaked, the beat is where the biggest attack of the high band is
/// followed by low-band energy (a kick), and the downbeat is where the
/// arrangement changes.
///
/// The constants are calibrated against hand-checked Traktor grids, see
/// docs/adr/0004-beat-grid-refined-on-the-kick.md.
class BeatGridRefiner {
  public:
    struct Grid {
        Bpm bpm;
        /// The first downbeat at or after the start of the track: the grid's
        /// anchor, from which bars are counted.
        audio::FramePos firstBeat;
    };

    explicit BeatGridRefiner(audio::SampleRate sampleRate);

    void processStereoSamples(const CSAMPLE* pIn, SINT sampleCount);

    /// Refines the plugin's tempo estimate `bpm` into a placed grid. Returns
    /// nullopt when the track has no beat pronounced enough to place a grid
    /// on, in which case the plugin's grid should be kept.
    std::optional<Grid> refine(Bpm bpm) const;

  private:
    std::vector<float> onsetStrength() const;
    double findBeatLength(const std::vector<float>& onsets, double beatLengthHops) const;
    double lowSharpness(double beatLengthHops) const;
    double findKickHops(double beatLengthHops) const;
    double findDownbeatHops(double beatLengthHops, double kickHops) const;

    audio::SampleRate m_sampleRate;
    SINT m_hopFrames;

    // Low band: two cascaded biquads (4th order Butterworth low-pass).
    double m_lowCoefficients[2][5];
    double m_lowState[2][2];
    // High band: one biquad high-pass.
    double m_highCoefficients[5];
    double m_highState[2];

    double m_lowHopEnergy;
    double m_highHopEnergy;
    SINT m_hopPosition;
    /// Mean energy of each 1 ms hop.
    std::vector<float> m_lowEnvelope;
    std::vector<float> m_highEnvelope;
};

} // namespace mixxx
