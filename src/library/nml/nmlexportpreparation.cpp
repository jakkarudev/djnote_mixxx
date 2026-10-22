#include "library/nml/nmlexportpreparation.h"

#include <algorithm>
#include <cmath>

#include "track/cue.h"
#include "track/keyutils.h"
#include "track/track.h"
#include "track/trackrecord.h"
#include "util/color/colorpalette.h"
#include "util/color/predefinedcolorpalettes.h"

namespace mixxx {
namespace nml {

namespace {

// Hotcue buttons 1-8 of Traktor; the rest are written without a button.
constexpr int kNmlHotcueButtons = 8;

// Traktor's "no name" and the names Traktor 4.0.2 gives its own markers.
const QString kNoName = QStringLiteral("n.n.");
const QString kGridName = QStringLiteral("Beat Marker");
const QString kLoadName = QStringLiteral("Load");
const QString kFadeInName = QStringLiteral("Fade-In");
const QString kFadeOutName = QStringLiteral("Fade-Out");

// Two beat lengths (in frames) that come from the same tempo differ by at most
// one frame: the database keeps beats as integer frames, so after a reload a
// section is split wherever the rounded length steps (1-3 beats per piece).
// A real tempo change (Connected: 137.00148 -> 136.97142 BPM) is 4 frames.
constexpr double kRoundingFrames = 1.0 + 1e-6;

int buttonOf(int hotCueIndex) {
    return (hotCueIndex >= 0 && hotCueIndex < kNmlHotcueButtons) ? hotCueIndex : -1;
}

NmlCue markerCue(NmlCueType type, const QString& name, double startMillis) {
    NmlCue cue;
    cue.type = type;
    cue.name = name;
    cue.startMillis = startMillis;
    return cue;
}

} // namespace

std::optional<int> nmlMusicalKeyFromKey(track::io::key::ChromaticKey key) {
    if (key == track::io::key::INVALID) {
        return std::nullopt;
    }
    return static_cast<int>(key) - 1;
}

std::optional<int> nmlColorIndexFromTrackColor(const RgbColor::optional_t& color) {
    if (!color) {
        return std::nullopt;
    }
    const ColorPalette& palette = PredefinedColorPalettes::kTraktorProTrackColorPalette;
    const QRgb wanted = static_cast<QRgb>(*color);
    int best = 0;
    long long bestDistance = -1;
    for (int i = 0; i < palette.size(); ++i) {
        const QRgb candidate = static_cast<QRgb>(palette.at(i));
        const long long dr = qRed(wanted) - qRed(candidate);
        const long long dg = qGreen(wanted) - qGreen(candidate);
        const long long db = qBlue(wanted) - qBlue(candidate);
        const long long distance = dr * dr + dg * dg + db * db;
        if (bestDistance < 0 || distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best + 1;
}

std::optional<int> nmlRankingFromStars(int stars) {
    if (stars <= 0) {
        return std::nullopt;
    }
    return std::min(stars, static_cast<int>(TrackRecord::kMaxRating)) * 51;
}

QList<NmlCue> nmlGridCuesFromBeats(const Beats& beats, const NmlPositionConverter& converter) {
    QList<NmlCue> grid;
    const auto marker = [&](audio::FramePos position, double bpm) {
        NmlCue cue = markerCue(NmlCueType::Grid, kGridName, converter.toMillis(position));
        cue.gridBpm = bpm;
        return cue;
    };
    const double sampleRate = beats.getSampleRate();
    const audio::FramePos lastMarkerPosition = beats.getLastMarkerPosition();
    const Bpm lastMarkerBpm = beats.getLastMarkerBpm();
    if (!lastMarkerPosition.isValid() || !lastMarkerBpm.isValid() || !(sampleRate > 0.0)) {
        return grid;
    }

    const std::vector<BeatMarker>& markers = beats.getMarkers();
    if (markers.empty()) {
        // A constant grid: the stored anchor is beat 1 of a bar in Traktor.
        // Only a negative anchor moves, by whole bars (4 beats) to the first
        // position >= 0, so the downbeat keeps its phase.
        audio::FramePos anchor = lastMarkerPosition;
        if (anchor.value() < 0.0) {
            const double barFrames = 4.0 * 60.0 * sampleRate / lastMarkerBpm.value();
            anchor = anchor + std::ceil(-anchor.value() / barFrames - 1e-9) * barFrames;
        }
        grid.append(marker(anchor, lastMarkerBpm.value()));
        return grid;
    }

    // Merge the pieces of one tempo into one section: its BPM is the whole
    // section's beats over its frames, not that of one rounded piece.
    struct Section {
        audio::FramePos start;
        double beats;
        double frames;
        std::optional<double> bpm; // set for the closing section only
        double beatFrames() const {
            return frames / beats;
        }
    };
    std::vector<Section> sections;
    const auto add = [&](audio::FramePos start, double beatCount, double frames) {
        const double length = frames / beatCount;
        if (!sections.empty() &&
                std::abs(length - sections.back().beatFrames()) <= kRoundingFrames) {
            sections.back().beats += beatCount;
            sections.back().frames += frames;
            return;
        }
        sections.push_back({start, beatCount, frames, std::nullopt});
    };
    for (size_t i = 0; i < markers.size(); ++i) {
        const audio::FramePos next = (i + 1 < markers.size())
                ? markers[i + 1].position()
                : lastMarkerPosition;
        const double frames = next - markers[i].position();
        if (!(frames > 0.0) || markers[i].beatsTillNextMarker() < 1) {
            continue;
        }
        add(markers[i].position(), markers[i].beatsTillNextMarker(), frames);
    }
    // The closing marker (the import appends one at the end of the last
    // section) carries the tempo after it: one more section only if it differs.
    const double tailFrames = 60.0 * sampleRate / lastMarkerBpm.value();
    if (sections.empty() ||
            std::abs(tailFrames - sections.back().beatFrames()) > kRoundingFrames) {
        sections.push_back({lastMarkerPosition, 1.0, tailFrames, lastMarkerBpm.value()});
    }
    for (const Section& section : sections) {
        grid.append(marker(section.start,
                section.bpm.value_or(60.0 * sampleRate * section.beats / section.frames)));
    }
    return grid;
}

bool nmlGridIsPrepared(const Track& track) {
    const BeatsPointer pBeats = track.getBeats();
    // An analysed or imported grid has a sub-version; the empty one is the
    // grid Mixxx builds from the BPM tag. Locking the BPM marks it as the DJ's.
    return pBeats && (!pBeats->getSubVersion().isEmpty() || track.isBpmLocked());
}

QList<NmlCue> nmlCuesFromTrack(const Track& track, const NmlPositionConverter& converter) {
    QList<NmlCue> cues;
    const BeatsPointer pBeats = track.getBeats();
    if (pBeats && nmlGridIsPrepared(track)) {
        cues = nmlGridCuesFromBeats(*pBeats, converter);
    }

    for (const CuePointer& pCue : track.getCuePoints()) {
        const audio::FramePos start = pCue->getPosition();
        switch (pCue->getType()) {
        case CueType::HotCue:
        case CueType::Loop: {
            if (!start.isValid()) {
                break;
            }
            NmlCue cue = markerCue(pCue->getType() == CueType::Loop ? NmlCueType::Loop
                                                                      : NmlCueType::Cue,
                    pCue->getLabel().isEmpty() ? kNoName : pCue->getLabel(),
                    converter.toMillis(start));
            cue.hotcue = buttonOf(pCue->getHotCue());
            // No COLOR for the colour DJNote gives cues imported without one.
            if (pCue->getColor() != PredefinedColorPalettes::kDefaultCueColor) {
                cue.color = static_cast<int>(static_cast<RgbColor::code_t>(pCue->getColor()));
            }
            if (pCue->getType() == CueType::Loop) {
                const audio::FramePos end = pCue->getEndPosition();
                if (!end.isValid()) {
                    break;
                }
                cue.lengthMillis = converter.toMillis(end) - cue.startMillis;
            }
            cues.append(cue);
            break;
        }
        case CueType::MainCue:
            if (start.isValid()) {
                cues.append(markerCue(NmlCueType::Load, kLoadName, converter.toMillis(start)));
            }
            break;
        case CueType::Intro:
            if (start.isValid()) {
                cues.append(markerCue(NmlCueType::FadeIn, kFadeInName, converter.toMillis(start)));
            }
            break;
        case CueType::Outro: {
            const audio::FramePos end = pCue->getEndPosition();
            if (end.isValid()) {
                cues.append(markerCue(NmlCueType::FadeOut, kFadeOutName, converter.toMillis(end)));
            }
            break;
        }
        default:
            break;
        }
    }

    std::stable_sort(cues.begin(), cues.end(), [](const NmlCue& a, const NmlCue& b) {
        return a.startMillis < b.startMillis;
    });
    return cues;
}

void fillNmlPreparation(
        NmlWriteEntry* pEntry, const Track& track, const NmlPositionConverter& converter) {
    pEntry->cues = nmlCuesFromTrack(track, converter);
    for (const NmlCue& cue : std::as_const(pEntry->cues)) {
        if (cue.type == NmlCueType::Grid && cue.gridBpm) {
            // TEMPO@BPM is the first marker's (ADR 0010); cues are sorted.
            pEntry->bpm = cue.gridBpm;
            break;
        }
    }
    if (!pEntry->bpm) {
        // Traktor writes TEMPO for a track without grid markers, too.
        const double bpm = track.getBpm();
        if (bpm > 0.0) {
            pEntry->bpm = bpm;
        }
    }
    pEntry->locked = track.isBpmLocked();

    const auto key = track.getKey();
    pEntry->musicalKey = nmlMusicalKeyFromKey(key);
    if (pEntry->musicalKey) {
        // Traktor's own form, whatever notation the DJ displays.
        pEntry->keyText = KeyUtils::keyToString(key, KeyUtils::KeyNotation::OpenKey);
    }

    pEntry->color = nmlColorIndexFromTrackColor(track.getColor());
    pEntry->ranking = nmlRankingFromStars(track.getRating());

    pEntry->playCount = track.getTimesPlayed();
    const QDateTime lastPlayed = track.getLastPlayedAt();
    if (lastPlayed.isValid()) {
        pEntry->lastPlayed = lastPlayed.toLocalTime().date();
    }
}

void fillNmlPreparation(NmlWriteEntry* pEntry, const Track& track) {
    fillNmlPreparation(pEntry, track, NmlPositionConverter::forTrack(track));
}

} // namespace nml
} // namespace mixxx
