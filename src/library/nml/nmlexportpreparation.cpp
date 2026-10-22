#include "library/nml/nmlexportpreparation.h"

#include <algorithm>
#include <cmath>

#include "track/cue.h"
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

// Two consecutive grid sections closer than this are one tempo.
constexpr double kSameTempoBpm = 0.05;

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
    const auto addMarker = [&](audio::FramePos position, double bpm) {
        // One marker per tempo CHANGE: the closing marker the import appends
        // (and frame-rounding splits after a database reload) repeat the tempo.
        if (!grid.isEmpty() && std::abs(*grid.last().gridBpm - bpm) < kSameTempoBpm) {
            return;
        }
        NmlCue cue = markerCue(NmlCueType::Grid, kGridName, converter.toMillis(position));
        cue.gridBpm = bpm;
        grid.append(cue);
    };

    const std::vector<BeatMarker>& markers = beats.getMarkers();
    if (markers.empty()) {
        const audio::FramePos first = beats.firstBeat();
        if (first.isValid() && beats.getLastMarkerBpm().isValid()) {
            addMarker(first, beats.getLastMarkerBpm().value());
        }
        return grid;
    }
    const double sampleRate = beats.getSampleRate();
    for (size_t i = 0; i < markers.size(); ++i) {
        const audio::FramePos next = (i + 1 < markers.size())
                ? markers[i + 1].position()
                : beats.getLastMarkerPosition();
        const double sectionFrames = next - markers[i].position();
        if (!(sectionFrames > 0.0)) {
            continue;
        }
        const double bpm = markers[i].beatsTillNextMarker() * 60.0 * sampleRate / sectionFrames;
        addMarker(markers[i].position(), bpm);
    }
    addMarker(beats.getLastMarkerPosition(), beats.getLastMarkerBpm().value());
    return grid;
}

QList<NmlCue> nmlCuesFromTrack(const Track& track, const NmlPositionConverter& converter) {
    QList<NmlCue> cues;
    const BeatsPointer pBeats = track.getBeats();
    if (pBeats) {
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
            cue.color = static_cast<int>(static_cast<RgbColor::code_t>(pCue->getColor()));
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
    pEntry->locked = track.isBpmLocked();

    const auto key = track.getKey();
    pEntry->musicalKey = nmlMusicalKeyFromKey(key);
    if (pEntry->musicalKey) {
        pEntry->keyText = track.getKeyText();
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
