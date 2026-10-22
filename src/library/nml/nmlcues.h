#pragma once

#include <QList>
#include <optional>

#include "library/nml/nmldocument.h"
#include "library/nml/nmlpreparation.h"
#include "track/cueinfo.h"
#include "util/color/rgbcolor.h"

class Track;

// DJNote: turns the cues, colour and rating of an NmlEntry into Track state.
// The cue step is split in a pure part (entry -> CueInfo list, easy to test)
// and an apply part, so the keep/replace rule can call applyNmlCues() for
// tracks it replaces.

namespace mixxx {
namespace nml {

struct NmlCueImport {
    /// Positions in milliseconds, already shifted by the decoder delay.
    QList<CueInfo> cues;
    /// Buttonless cues beyond the main cue, buttonless loops, extra
    /// Load/Fade markers and clashes on a button.
    int dropped = 0;
};

/// Hotcues, loops, main cue, intro start and outro end of the entry.
/// A grid marker only becomes a hotcue when it sits on a button (VERSION 19
/// writes the AutoGrid marker on HOTCUE 0).
NmlCueImport cueInfosFromNmlEntry(const NmlEntry& entry, const NmlPositionConverter& converter);

/// Replaces ALL cues of the track with the entry's. Returns the dropped count.
int applyNmlCues(Track* pTrack, const NmlEntry& entry);

/// Traktor colour index 1-7 -> "Traktor Pro Track Colors" palette; others none.
RgbColor::optional_t trackColorFromNmlIndex(int index);

/// RANKING (0-255, 51 per star) -> 0-5 stars.
int starsFromNmlRanking(int ranking);

/// Track colour and rating.
void applyNmlColorAndRating(Track* pTrack, const NmlEntry& entry);

} // namespace nml
} // namespace mixxx
