#pragma once

#include <algorithm>
#include <array>
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

// The phrase lengths (in bars) offered in the preferences.
constexpr std::array<int, 4> kPhraseBarsChoices = {4, 8, 16, 32};
constexpr int kDefaultPhraseBars = 8;

// Returns bars if it is one of kPhraseBarsChoices, else the default.
constexpr int validPhraseBarsOrDefault(int bars) {
    for (const int choice : kPhraseBarsChoices) {
        if (choice == bars) {
            return bars;
        }
    }
    return kDefaultPhraseBars;
}

// Opacity of the bar and phrase lines, as a percentage. Always within 0..100:
// out-of-range values are clamped on construction.
class OpacityPercent {
  public:
    static constexpr int kDefault = 80;

    constexpr explicit OpacityPercent(int percent = kDefault)
            : m_percent(std::clamp(percent, 0, 100)) {
    }
    constexpr int percent() const {
        return m_percent;
    }
    // For QPainter::setOpacity().
    constexpr double fraction() const {
        return m_percent / 100.0;
    }

  private:
    int m_percent;
};

struct LineSettings {
    LinesMode mode = LinesMode::PhrasesOnly;
    // Phrase length in bars, one of kPhraseBarsChoices.
    int phraseBars = kDefaultPhraseBars;
    OpacityPercent opacity;
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
