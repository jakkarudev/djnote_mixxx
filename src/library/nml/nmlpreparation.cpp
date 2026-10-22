#include "library/nml/nmlpreparation.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "library/nml/nmlconstants.h"
#include "library/traktor/traktorkey.h"
#include "track/keyutils.h"
#include "track/serato/tags.h"
#include "track/track.h"

namespace mixxx {
namespace nml {

NmlPositionConverter NmlPositionConverter::forTrack(const Track& track) {
    const audio::SignalInfo signalInfo(
            audio::ChannelCount(track.getChannels()), track.getSampleRate());
    const double offset = SeratoTags::guessTimingOffsetMillis(
            track.getLocation(), track.getType(), signalInfo);
    return NmlPositionConverter(signalInfo, offset, track.getDuration() * 1000.0);
}

audio::FramePos NmlPositionConverter::toFrame(double nmlMillis) const {
    return audio::FramePos(m_signalInfo.millis2frames(nmlMillis + m_timingOffsetMillis));
}

double NmlPositionConverter::toMillis(audio::FramePos frame) const {
    return m_signalInfo.frames2secsFractional(frame.value()) * 1000.0 - m_timingOffsetMillis;
}

namespace {
// A section needs a few beats for its tempo to be recoverable from positions.
constexpr int kMinLastSectionBeats = 16;
} // namespace

BeatsPointer beatsFromNmlEntry(const NmlEntry& entry, const NmlPositionConverter& converter) {
    const audio::SampleRate sampleRate = converter.signalInfo().getSampleRate();
    if (!sampleRate.isValid()) {
        return nullptr;
    }
    struct Marker {
        double startMillis;
        double bpm;
    };
    std::vector<Marker> markers;
    for (const NmlCue& cue : entry.cues) {
        if (cue.type != NmlCueType::Grid) {
            continue;
        }
        const double bpm = cue.gridBpm.value_or(entry.bpm.value_or(0.0));
        if (!(bpm > 0.0)) {
            continue;
        }
        markers.push_back({cue.startMillis, bpm});
    }
    if (markers.empty()) {
        return nullptr;
    }
    std::stable_sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) {
        return a.startMillis < b.startMillis;
    });

    if (markers.size() == 1) {
        return Beats::fromConstTempo(sampleRate,
                converter.toFrame(markers.front().startMillis),
                Bpm(markers.front().bpm),
                kBeatsSubVersion);
    }

    // The last section needs a closing marker, else Mixxx loses its tempo when
    // the grid is saved (it persists beat positions up to the last marker only).
    const Marker& last = markers.back();
    const double endMillis = std::max(converter.trackEndMillis(), last.startMillis);
    const int lastSectionBeats = std::max(kMinLastSectionBeats,
            static_cast<int>(std::lround((endMillis - last.startMillis) * last.bpm / 60000.0)));
    const audio::FramePos lastStart = converter.toFrame(last.startMillis).toLowerFrameBoundary();
    const audio::FramePos closingMarker = lastStart +
            lastSectionBeats * 60.0 * sampleRate / last.bpm;

    std::vector<BeatMarker> beatMarkers;
    for (size_t i = 0; i + 1 < markers.size(); ++i) {
        const double sectionMillis = markers[i + 1].startMillis - markers[i].startMillis;
        const int beats = static_cast<int>(std::lround(sectionMillis * markers[i].bpm / 60000.0));
        if (beats < 1) {
            continue; // coincident markers: the later one wins
        }
        beatMarkers.push_back(BeatMarker{
                converter.toFrame(markers[i].startMillis).toLowerFrameBoundary(), beats});
    }
    beatMarkers.push_back(BeatMarker{lastStart, lastSectionBeats});
    return Beats::fromBeatMarkers(sampleRate,
            beatMarkers,
            closingMarker.toLowerFrameBoundary(),
            Bpm(last.bpm),
            kBeatsSubVersion);
}

track::io::key::ChromaticKey keyFromNmlEntry(const NmlEntry& entry) {
    if (entry.musicalKey.has_value()) {
        const auto key = traktor::keyFromTraktorMusicalKeyValue(
                QString::number(*entry.musicalKey));
        if (key != track::io::key::INVALID) {
            return key;
        }
    }
    if (!entry.keyText.isEmpty()) {
        return KeyUtils::guessKeyFromText(entry.keyText);
    }
    return track::io::key::INVALID;
}

void applyNmlPreparation(Track* pTrack, const NmlEntry& entry) {
    const BeatsPointer pBeats = beatsFromNmlEntry(entry, NmlPositionConverter::forTrack(*pTrack));
    if (pBeats) {
        pTrack->trySetBeats(pBeats);
    }
    const auto key = keyFromNmlEntry(entry);
    if (key != track::io::key::INVALID) {
        pTrack->setKey(key, track::io::key::USER);
    }
    if (entry.locked) {
        pTrack->setBpmLocked(true);
    }
}

} // namespace nml
} // namespace mixxx
