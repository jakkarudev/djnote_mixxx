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

// DJNote: the export side of the preparation: the reverse of
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

/// One Grid marker per tempo section, ascending, each on HOTCUE -1 with the
/// BPM of its whole section (total beats over total frames). A constant grid
/// gives one marker at its stored anchor (Traktor's downbeat), moved by whole
/// bars only when the anchor is before the start of the track.
QList<NmlCue> nmlGridCuesFromBeats(const Beats& beats, const NmlPositionConverter& converter);

/// Whether the grid is the DJ's work (analysed, imported or BPM-locked) and so
/// worth writing as Traktor grid markers; a grid built from the file's BPM
/// tag is not.
bool nmlGridIsPrepared(const Track& track);

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
