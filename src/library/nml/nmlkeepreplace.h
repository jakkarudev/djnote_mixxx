#pragma once

#include <QString>

#include "library/nml/nmldocument.h"

class Track;

// DJNote (#109, ADR 0008): the keep/replace rule for NML import rows that land
// on a track DJNote already has (same path, or a library match the DJ accepted).
// A track keeps ONE preparation; the NML's parts are taken per part, all or
// nothing:
//   grid+BPM, key, cues (hotcues, main cue, loops, intro/outro as one group),
//   colour, rating.
// Keep (default): a part is taken from the NML only if the track has none of it.
// Replace: every part the NML carries overwrites the track's (cues: the whole
// group), except that a BPM-locked grid is never replaced.
// #112 reuses NmlPreparationComparison::identical(); #114 reuses
// applyNmlToExistingTrack() as the whole rule.

namespace mixxx {
namespace nml {

enum class NmlPreparationMode {
    Keep,    // default
    Replace, // not undoable
};

/// One part (grid, key, cues, colour, rating) of track vs. NML.
struct NmlPartComparison {
    bool trackHas = false;   // the track has this part
    bool nmlCarries = false; // the NML entry carries it (and it can be imported)
    bool same = false;       // both absent, or both present and equal
    /// Both have it and they differ: Keep keeps the track's, Replace swaps it.
    bool conflicts() const {
        return trackHas && nmlCarries && !same;
    }
    /// Keep takes this part from the NML.
    bool takenInKeep() const {
        return !trackHas && nmlCarries;
    }
};

struct NmlPreparationComparison {
    NmlPartComparison grid; // grid and BPM
    NmlPartComparison key;
    NmlPartComparison cues; // hotcues, main cue, loops, intro/outro together
    NmlPartComparison color;
    NmlPartComparison rating;
    bool gridLocked = false; // the track's BPM lock is set (and it has a grid)

    /// The track has any preparation at all (decides if the keep/replace
    /// choice is shown).
    bool trackHasPreparation() const {
        return grid.trackHas || key.trackHas || cues.trackHas || color.trackHas ||
                rating.trackHas;
    }
    bool anyConflict() const {
        return grid.conflicts() || key.conflicts() || cues.conflicts() ||
                color.conflicts() || rating.conflicts();
    }
    /// Every part the NML can carry is identical to the track's (#112).
    bool identical() const {
        return grid.same && key.same && cues.same && color.same && rating.same;
    }
    /// A locked grid blocks the NML's different grid, in either mode.
    bool lockedGridBlocksNml() const {
        return gridLocked && grid.conflicts();
    }
};

/// DJNote (#112): tolerances of a comparison. The defaults (negative) keep the
/// keep/replace behaviour: positions within 1.5 frames, BPM not compared on its own.
struct NmlCompareTolerance {
    double bpm = -1.0;            // >= 0: grid BPMs must also be within this
    double positionMillis = -1.0; // >= 0: beats and cues within this instead of 1.5 frames
};

/// DJNote (#112): "identical" for reusing a library track: BPM within 0.01,
/// beats and cues within 10 ms.
constexpr NmlCompareTolerance kNmlIdenticalTolerance{0.01, 10.0};

/// Compares a library track with an NML entry. Writes nothing.
NmlPreparationComparison compareNmlPreparation(const Track& track,
        const NmlEntry& entry,
        const NmlCompareTolerance& tolerance = NmlCompareTolerance());

struct NmlKeepReplaceOutcome {
    bool changed = false;          // at least one part was written
    bool lockedGridKept = false;   // the NML's grid differs but the track is BPM-locked
    int droppedCues = 0;           // as applyNmlCues
};

/// Applies the rule to an existing track.
NmlKeepReplaceOutcome applyNmlToExistingTrack(
        Track* pTrack, const NmlEntry& entry, NmlPreparationMode mode);

} // namespace nml
} // namespace mixxx
