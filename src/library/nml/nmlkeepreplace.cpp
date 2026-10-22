#include "library/nml/nmlkeepreplace.h"

#include <algorithm>
#include <cmath>
#include <tuple>

#include "library/nml/nmlcues.h"
#include "library/nml/nmlpreparation.h"
#include "track/cue.h"
#include "track/track.h"

namespace mixxx {
namespace nml {

namespace {

// The cues "group" of ADR 0008. Other cue kinds (beat, jump, analyzer range)
// are not part of what an NML can carry.
bool isGroupCue(CueType type) {
    switch (type) {
    case CueType::HotCue:
    case CueType::MainCue:
    case CueType::Loop:
    case CueType::Intro:
    case CueType::Outro:
        return true;
    default:
        return false;
    }
}

bool optionalMillisEqual(const std::optional<double>& a,
        const std::optional<double>& b,
        double toleranceMillis) {
    if (a.has_value() != b.has_value()) {
        return false;
    }
    return !a.has_value() || std::abs(*a - *b) <= toleranceMillis;
}

// Position, type, button and loop length are compared; labels and cue colours
// are not (the NML's are mostly defaults and would never match).
bool cueGroupsEqual(QList<CueInfo> a, QList<CueInfo> b, double toleranceMillis) {
    if (a.size() != b.size()) {
        return false;
    }
    const auto key = [](const CueInfo& c) {
        return std::make_tuple(static_cast<int>(c.getType()),
                c.getHotCueIndex().value_or(-1),
                c.getStartPositionMillis().value_or(c.getEndPositionMillis().value_or(0.0)));
    };
    const auto less = [&](const CueInfo& l, const CueInfo& r) { return key(l) < key(r); };
    std::sort(a.begin(), a.end(), less);
    std::sort(b.begin(), b.end(), less);
    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i).getType() != b.at(i).getType() ||
                a.at(i).getHotCueIndex() != b.at(i).getHotCueIndex() ||
                !optionalMillisEqual(a.at(i).getStartPositionMillis(),
                        b.at(i).getStartPositionMillis(),
                        toleranceMillis) ||
                !optionalMillisEqual(a.at(i).getEndPositionMillis(),
                        b.at(i).getEndPositionMillis(),
                        toleranceMillis)) {
            return false;
        }
    }
    return true;
}

bool gridsEqual(const Beats& a, const Beats& b, const NmlCompareTolerance& tolerance) {
    if (tolerance.bpm >= 0.0 && std::abs(a.getLastMarkerBpm().value() - b.getLastMarkerBpm().value()) > tolerance.bpm) {
        return false;
    }
    // Beats extend over the whole track, so equal nearest beats at spread-out
    // points (within 1.5 frames) mean equal grids.
    const double rate = a.getSampleRate().isValid() ? a.getSampleRate().value() : 44100.0;
    const double maxFrames = tolerance.positionMillis >= 0.0
            ? tolerance.positionMillis * rate / 1000.0
            : 1.5;
    for (double seconds : {0.0, 20.0, 90.0, 240.0}) {
        const audio::FramePos pos(seconds * rate);
        const audio::FramePos pa = a.findClosestBeat(pos);
        const audio::FramePos pb = b.findClosestBeat(pos);
        if (!pa.isValid() || !pb.isValid() || std::abs(pa.value() - pb.value()) > maxFrames) {
            return false;
        }
    }
    return true;
}

QList<CueInfo> trackGroupCueInfos(const Track& track, audio::SampleRate sampleRate) {
    QList<CueInfo> infos;
    for (const CuePointer& pCue : track.getCuePoints()) {
        if (isGroupCue(pCue->getType())) {
            infos.append(pCue->getCueInfo(sampleRate));
        }
    }
    return infos;
}

void applyCueGroup(Track* pTrack, const NmlCueImport& imported, audio::SampleRate sampleRate) {
    QList<CuePointer> cues;
    // Cues outside the group stay; the group is replaced as a whole (the
    // importCueInfos route would skip types that already exist).
    for (const CuePointer& pCue : pTrack->getCuePoints()) {
        if (!isGroupCue(pCue->getType())) {
            cues.append(pCue);
        }
    }
    for (const CueInfo& info : imported.cues) {
        cues.append(CuePointer(new Cue(info, sampleRate, true)));
    }
    pTrack->setCuePoints(cues);
}

} // namespace

NmlPreparationComparison compareNmlPreparation(const Track& track,
        const NmlEntry& entry,
        const NmlCompareTolerance& tolerance) {
    NmlPreparationComparison c;
    const NmlPositionConverter converter = NmlPositionConverter::forTrack(track);
    const audio::SampleRate sampleRate = converter.signalInfo().getSampleRate();

    // Grid and BPM
    const BeatsPointer pTrackBeats = track.getBeats();
    c.grid.trackHas = static_cast<bool>(pTrackBeats);
    const BeatsPointer pNmlBeats = beatsFromNmlEntry(entry, converter);
    c.grid.nmlCarries = static_cast<bool>(pNmlBeats);
    c.grid.same = (c.grid.trackHas == c.grid.nmlCarries) &&
            (!c.grid.trackHas || gridsEqual(*pTrackBeats, *pNmlBeats, tolerance));
    c.gridLocked = c.grid.trackHas && track.isBpmLocked();

    // Key
    const auto trackKey = track.getKey();
    const auto nmlKey = keyFromNmlEntry(entry);
    c.key.trackHas = trackKey != track::io::key::INVALID;
    c.key.nmlCarries = nmlKey != track::io::key::INVALID;
    c.key.same = (c.key.trackHas == c.key.nmlCarries) &&
            (!c.key.trackHas || trackKey == nmlKey);

    // Cues
    QList<CueInfo> nmlCues;
    QList<CueInfo> trackCues;
    if (sampleRate.isValid()) {
        nmlCues = cueInfosFromNmlEntry(entry, converter).cues;
        trackCues = trackGroupCueInfos(track, sampleRate);
    }
    c.cues.trackHas = !trackCues.isEmpty();
    c.cues.nmlCarries = !nmlCues.isEmpty();
    const double toleranceMillis = tolerance.positionMillis >= 0.0
            ? tolerance.positionMillis
            : (sampleRate.isValid() ? 1500.0 / sampleRate.value() : 0.1);
    c.cues.same = (c.cues.trackHas == c.cues.nmlCarries) &&
            (!c.cues.trackHas || cueGroupsEqual(trackCues, nmlCues, toleranceMillis));

    // Colour
    const auto trackColor = track.getColor();
    const RgbColor::optional_t nmlColor =
            entry.color ? trackColorFromNmlIndex(*entry.color) : std::nullopt;
    c.color.trackHas = trackColor.has_value();
    c.color.nmlCarries = nmlColor.has_value();
    c.color.same = (c.color.trackHas == c.color.nmlCarries) &&
            (!c.color.trackHas || *trackColor == *nmlColor);

    // Rating
    const int nmlStars = entry.ranking ? starsFromNmlRanking(*entry.ranking) : 0;
    c.rating.trackHas = track.getRating() > 0;
    c.rating.nmlCarries = nmlStars > 0;
    c.rating.same = (c.rating.trackHas == c.rating.nmlCarries) &&
            (!c.rating.trackHas || track.getRating() == nmlStars);
    return c;
}

NmlKeepReplaceOutcome applyNmlToExistingTrack(
        Track* pTrack, const NmlEntry& entry, NmlPreparationMode mode) {
    NmlKeepReplaceOutcome outcome;
    const NmlPreparationComparison c = compareNmlPreparation(*pTrack, entry);
    const bool replace = mode == NmlPreparationMode::Replace;
    // A part is written when Keep fills a gap, or Replace has an NML value for it.
    const auto write = [replace](const NmlPartComparison& part) {
        return replace ? part.nmlCarries : part.takenInKeep();
    };
    outcome.lockedGridKept = c.lockedGridBlocksNml();

    const NmlPositionConverter converter = NmlPositionConverter::forTrack(*pTrack);
    if (write(c.grid) && !c.gridLocked) { // a BPM-locked grid is never replaced
        const BeatsPointer pBeats = beatsFromNmlEntry(entry, converter);
        if (pBeats && pTrack->trySetBeats(pBeats)) {
            outcome.changed = true;
            if (entry.locked) {
                pTrack->setBpmLocked(true);
            }
        }
    }
    if (write(c.key)) {
        pTrack->setKey(keyFromNmlEntry(entry), track::io::key::USER);
        outcome.changed = true;
    }
    if (write(c.cues)) {
        const NmlCueImport imported = cueInfosFromNmlEntry(entry, converter);
        applyCueGroup(pTrack, imported, converter.signalInfo().getSampleRate());
        outcome.droppedCues = imported.dropped;
        outcome.changed = true;
    }
    if (write(c.color)) {
        pTrack->setColor(trackColorFromNmlIndex(*entry.color));
        outcome.changed = true;
    }
    if (write(c.rating)) {
        pTrack->setRating(starsFromNmlRanking(*entry.ranking));
        outcome.changed = true;
    }
    return outcome;
}

} // namespace nml
} // namespace mixxx
