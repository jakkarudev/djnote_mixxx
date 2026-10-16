#pragma once

#include <vector>

#include "track/beats.h"

// Bar and phrase line positions for the track overview. Part of the overview
// layout module (see overviewlayout.h): pure, no widget or painting
// dependencies.
namespace mixxx {
namespace overviewlayout {

enum class LinesMode {
    Off = 0,
    PhrasesOnly = 1,
    BarsAndPhrases = 2,
};

constexpr int kBeatsPerBar = 4;

struct LineSettings {
    LinesMode mode = LinesMode::PhrasesOnly;
    // Phrase length in bars (4, 8, 16 or 32).
    int phraseBars = 8;
};

struct LinePositions {
    // Pixel positions along the track axis, ascending. A bar that starts a
    // phrase is only listed in phraseLines.
    std::vector<double> barLines;
    std::vector<double> phraseLines;
};

// Bars are 4 beats and phrases are phraseBars bars, counted from the beat
// grid's first beat and continuing back through the intro (ADR 0001).
// trackSamples is in engine samples, overviewLength in pixels. A null grid
// yields no lines.
LinePositions layoutLines(const Beats* pBeats,
        double trackSamples,
        double overviewLength,
        const LineSettings& settings);

} // namespace overviewlayout
} // namespace mixxx
