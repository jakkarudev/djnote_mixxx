#include "library/nml/nmlcues.h"

#include <QSet>
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

// Traktor's "no name".
QString labelFromNmlName(const QString& name) {
    return name == QLatin1String("n.n.") ? QString() : name;
}

RgbColor::optional_t colorFromNmlCue(const NmlCue& cue) {
    if (!cue.color) {
        return RgbColor::nullopt();
    }
    return RgbColor::optional(RgbColor(static_cast<unsigned>(*cue.color) & 0xFFFFFFu));
}

CueInfo markerInfo(CueType type,
        std::optional<double> startMillis,
        std::optional<double> endMillis) {
    return CueInfo(type, startMillis, endMillis, std::nullopt, QString(), std::nullopt);
}

} // namespace

NmlCueImport cueInfosFromNmlEntry(const NmlEntry& entry, const NmlPositionConverter& converter) {
    NmlCueImport out;
    const double offset = converter.timingOffsetMillis();
    QSet<int> usedButtons;
    bool haveMain = false;
    bool haveIntro = false;
    bool haveOutro = false;

    auto addButtonCue = [&](CueType type, const NmlCue& cue, bool withEnd) {
        if (usedButtons.contains(cue.hotcue)) {
            ++out.dropped;
            return;
        }
        usedButtons.insert(cue.hotcue);
        const double start = cue.startMillis + offset;
        out.cues.append(CueInfo(type,
                start,
                withEnd ? std::optional<double>(start + cue.lengthMillis) : std::nullopt,
                cue.hotcue,
                labelFromNmlName(cue.name),
                colorFromNmlCue(cue)));
    };

    QList<const NmlCue*> buttonlessCues;
    for (const NmlCue& cue : entry.cues) {
        switch (cue.type) {
        case NmlCueType::Cue:
            if (cue.hotcue >= 0) {
                addButtonCue(CueType::HotCue, cue, false);
            } else {
                buttonlessCues.append(&cue);
            }
            break;
        case NmlCueType::Loop:
            if (cue.hotcue >= 0) {
                addButtonCue(CueType::Loop, cue, true);
            } else {
                ++out.dropped;
            }
            break;
        case NmlCueType::Load:
            if (haveMain) {
                ++out.dropped;
            } else {
                haveMain = true;
                out.cues.append(markerInfo(CueType::MainCue, cue.startMillis + offset, std::nullopt));
            }
            break;
        case NmlCueType::FadeIn:
            if (haveIntro) {
                ++out.dropped;
            } else {
                haveIntro = true;
                out.cues.append(markerInfo(CueType::Intro, cue.startMillis + offset, std::nullopt));
            }
            break;
        case NmlCueType::FadeOut:
            if (haveOutro) {
                ++out.dropped;
            } else {
                haveOutro = true;
                out.cues.append(markerInfo(CueType::Outro, std::nullopt, cue.startMillis + offset));
            }
            break;
        case NmlCueType::Grid:
        case NmlCueType::Unknown:
            break;
        }
    }
    // Grid markers on a button (VERSION 19) come last so a real hotcue on the
    // same button wins.
    for (const NmlCue& cue : entry.cues) {
        if (cue.type == NmlCueType::Grid && cue.hotcue >= 0 &&
                !usedButtons.contains(cue.hotcue)) {
            addButtonCue(CueType::HotCue, cue, false);
        }
    }

    // Without a Load marker the first buttonless cue is the main cue.
    int usedAsMain = 0;
    if (!haveMain && !buttonlessCues.isEmpty()) {
        out.cues.append(markerInfo(
                CueType::MainCue, buttonlessCues.first()->startMillis + offset, std::nullopt));
        usedAsMain = 1;
    }
    out.dropped += buttonlessCues.size() - usedAsMain;
    return out;
}

int applyNmlCues(Track* pTrack, const NmlEntry& entry) {
    const NmlPositionConverter converter = NmlPositionConverter::forTrack(*pTrack);
    const audio::SampleRate sampleRate = converter.signalInfo().getSampleRate();
    if (!sampleRate.isValid()) {
        return 0;
    }
    const NmlCueImport imported = cueInfosFromNmlEntry(entry, converter);
    QList<CuePointer> cues;
    for (const CueInfo& info : imported.cues) {
        cues.append(CuePointer(new Cue(info, sampleRate, true)));
    }
    pTrack->setCuePoints(cues);
    return imported.dropped;
}

RgbColor::optional_t trackColorFromNmlIndex(int index) {
    const ColorPalette& palette = PredefinedColorPalettes::kTraktorProTrackColorPalette;
    if (index < 1 || index > palette.size()) {
        return std::nullopt;
    }
    return RgbColor::optional(palette.at(index - 1));
}

int starsFromNmlRanking(int ranking) {
    const int stars = static_cast<int>(std::lround(ranking / 51.0));
    return std::clamp(stars, 0, static_cast<int>(TrackRecord::kMaxRating));
}

void applyNmlColorAndRating(Track* pTrack, const NmlEntry& entry) {
    if (entry.color) {
        const auto color = trackColorFromNmlIndex(*entry.color);
        if (color) {
            pTrack->setColor(color);
        }
    }
    if (entry.ranking) {
        pTrack->setRating(starsFromNmlRanking(*entry.ranking));
    }
}

} // namespace nml
} // namespace mixxx
