#pragma once

#include <QList>
#include <optional>

#include "library/nml/nmldocument.h"
#include "library/nml/nmlpreparation.h"
#include "library/nml/nmlwriter.h"
#include "proto/keys.pb.h"
#include "track/beats.h"
#include "util/color/rgbcolor.h"

class Track;

// DJNote: the export side of the preparation (#116): the reverse of
// nmlpreparation.h (grid, key, lock) and nmlcues.h (cues, colour, rating).
// Positions go through the same NmlPositionConverter as the import, so the
// decoder delay is subtracted here and added back there.

namespace mixxx {
namespace nml {

/// MUSICAL_KEY value (Mixxx key - 1); nullopt for an invalid key.
std::optional<int> nmlMusicalKeyFromKey(track::io::key::ChromaticKey key);

/// Index 1-7 of the Traktor colour nearest to `color` (RGB distance);
/// nullopt when the track has no colour.
std::optional<int> nmlColorIndexFromTrackColor(const RgbColor::optional_t& color);

/// RANKING for a 0-5 star rating (51 per star); nullopt for no rating.
std::optional<int> nmlRankingFromStars(int stars);

/// One Grid marker per tempo section, ascending. A constant grid gives one
/// marker at the first beat. Every marker is on HOTCUE -1 with its own BPM.
QList<NmlCue> nmlGridCuesFromBeats(const Beats& beats, const NmlPositionConverter& converter);

/// Grid markers, hotcues (buttons 1-8 on their button, the rest buttonless),
/// loops, Load, Fade-In and Fade-Out of the track, in ascending START order.
QList<NmlCue> nmlCuesFromTrack(const Track& track, const NmlPositionConverter& converter);

/// Fills the preparation slots of `pEntry` from the track: tempo, key, colour,
/// rating, lock, play count, last played and cues.
void fillNmlPreparation(
        NmlWriteEntry* pEntry, const Track& track, const NmlPositionConverter& converter);
/// Same, with the converter of the track itself.
void fillNmlPreparation(NmlWriteEntry* pEntry, const Track& track);

} // namespace nml
} // namespace mixxx
