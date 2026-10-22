#pragma once

#include "audio/frame.h"
#include "audio/signalinfo.h"
#include "library/nml/nmldocument.h"
#include "proto/keys.pb.h"
#include "track/beats.h"

class Track;

// DJNote: turns the preparation data of an NmlEntry (beat grid, key, BPM lock)
// into Track state, and owns the NML millisecond <-> frame conversion that
// the cue import and the export reuse.

namespace mixxx {
namespace nml {

/// Converts NML positions (milliseconds) to track frames and back, including
/// the MP3/AAC decoder-delay correction (the same one the Serato import uses).
class NmlPositionConverter {
  public:
    NmlPositionConverter(const audio::SignalInfo& signalInfo,
            double timingOffsetMillis,
            double trackEndMillis = 0.0)
            : m_signalInfo(signalInfo),
              m_timingOffsetMillis(timingOffsetMillis),
              m_trackEndMillis(trackEndMillis) {
    }

    /// Uses the track's sample rate/channels and guesses the decoder delay
    /// from its file.
    static NmlPositionConverter forTrack(const Track& track);

    audio::FramePos toFrame(double nmlMillis) const;
    double toMillis(audio::FramePos frame) const;

    double timingOffsetMillis() const {
        return m_timingOffsetMillis;
    }
    const audio::SignalInfo& signalInfo() const {
        return m_signalInfo;
    }
    /// Length of the track in NML milliseconds, 0 if unknown.
    double trackEndMillis() const {
        return m_trackEndMillis;
    }

  private:
    audio::SignalInfo m_signalInfo;
    double m_timingOffsetMillis;
    double m_trackEndMillis;
};

/// Grid markers (Grid cues) of the entry as a Beats object, or null if the
/// entry has none or the converter's sample rate is unknown. One marker ->
/// constant tempo; several -> beat markers with every tempo, plus a closing
/// marker at the end of the track: Mixxx saves a flexible grid only up to its
/// last marker, so without it the last section's tempo is lost on reload. Markers without
/// GRID@BPM (VERSION 19) use TEMPO@BPM. The result carries kBeatsSubVersion.
BeatsPointer beatsFromNmlEntry(const NmlEntry& entry, const NmlPositionConverter& converter);

/// MUSICAL_KEY (Traktor value + 1 = Mixxx key), else INFO@KEY text; INVALID if neither.
track::io::key::ChromaticKey keyFromNmlEntry(const NmlEntry& entry);

/// Applies grid, key and BPM lock to a track that is new to the library.
void applyNmlPreparation(Track* pTrack, const NmlEntry& entry);

} // namespace nml
} // namespace mixxx
