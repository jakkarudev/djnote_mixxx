#include "waveform/overviewlinelayout.h"

#include "audio/frame.h"

namespace mixxx {
namespace overviewlayout {

namespace {

int positiveMod(int a, int n) {
    return ((a % n) + n) % n;
}

} // namespace

LinePositions layoutLines(const Beats* pBeats,
        double trackSamples,
        double overviewLength,
        const LineSettings& settings) {
    LinePositions lines;
    if (!pBeats || settings.mode == LinesMode::Off || trackSamples <= 0.0 ||
            settings.phraseBars <= 0) {
        return lines;
    }

    const int phraseBeats = kBeatsPerBar * settings.phraseBars;
    const bool withBars = settings.mode == LinesMode::BarsAndPhrases;
    // Step by whole bars (or phrases) instead of beats to keep this cheap on
    // long tracks.
    const int stepBeats = withBars ? kBeatsPerBar : phraseBeats;

    const auto first = pBeats->cfirstmarker();
    const auto end = audio::FramePos::fromEngineSamplePos(trackSamples);
    auto it = pBeats->iteratorFrom(audio::kStartFramePos);
    if (it == pBeats->cend() || it == pBeats->cbegin()) {
        return lines;
    }
    // Align to the first bar/phrase start at or after the current beat.
    const int index = it - first;
    it += positiveMod(-index, stepBeats);

    const double scale = (overviewLength - 2.0) / trackSamples;
    for (; it != pBeats->cend() && *it <= end; it += stepBeats) {
        const double pos = 1.0 + it->toEngineSamplePos() * scale;
        if (positiveMod(it - first, phraseBeats) == 0) {
            lines.phraseLines.push_back(pos);
        } else {
            lines.barLines.push_back(pos);
        }
    }
    return lines;
}

} // namespace overviewlayout
} // namespace mixxx
