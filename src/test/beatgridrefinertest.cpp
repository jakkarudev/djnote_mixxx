#include "analyzer/beatgridrefiner.h"

#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <vector>

#include "analyzer/analyzerbeats.h"
#include "analyzer/analyzertrack.h"
#include "analyzer/constants.h"
#include "analyzer/plugins/analyzerjkrbeats.h"
#include "analyzer/plugins/analyzerqueenmarybeats.h"
#include "test/mixxxtest.h"
#include "track/beats.h"
#include "track/track.h"

namespace {

constexpr double kSampleRate = 44100;
// The refiner places the beat at the attack of the kick, calibrated against
// Traktor on real music; synthetic kicks start abruptly, so allow 3 ms.
constexpr double kToleranceSecs = 0.003;

class Synth {
  public:
    explicit Synth(double seconds)
            : m_mono(static_cast<size_t>(seconds * kSampleRate), 0.0),
              m_random(42) {
    }

    /// A kick: a 1.5 ms noise click and a decaying 150 to 50 Hz sweep.
    void kick(double at) {
        std::uniform_real_distribution<double> noise(-1, 1);
        double phase = 0;
        for (int i = 0; i < static_cast<int>(0.3 * kSampleRate); ++i) {
            const double t = i / kSampleRate;
            const double frequency = 50 + 100 * std::exp(-t / 0.02);
            phase += 2 * M_PI * frequency / kSampleRate;
            double value = 0.8 * std::exp(-t / 0.08) * std::sin(phase);
            if (t < 0.0015) {
                value += 0.5 * noise(m_random);
            }
            add(at + t, value);
        }
    }

    /// A bass note: an 80 Hz tone with a 3 ms attack, louder in the low band
    /// than the kick, as in a psytrance rolling bassline.
    void bass(double at) {
        for (int i = 0; i < static_cast<int>(0.09 * kSampleRate); ++i) {
            const double t = i / kSampleRate;
            const double attack = std::min(1.0, t / 0.003);
            add(at + t,
                    0.9 * attack * std::exp(-t / 0.05) * std::sin(2 * M_PI * 80 * t));
        }
    }

    /// A hi-hat: 30 ms of decaying noise.
    void hat(double at, double level = 0.3) {
        std::uniform_real_distribution<double> noise(-1, 1);
        for (int i = 0; i < static_cast<int>(0.03 * kSampleRate); ++i) {
            const double t = i / kSampleRate;
            add(at + t, level * std::exp(-t / 0.008) * noise(m_random));
        }
    }

    void noise(double level) {
        std::uniform_real_distribution<double> noise(-1, 1);
        for (double& value : m_mono) {
            value += level * noise(m_random);
        }
    }

    std::vector<CSAMPLE> stereo() const {
        std::vector<CSAMPLE> stereo(m_mono.size() * 2);
        for (size_t i = 0; i < m_mono.size(); ++i) {
            stereo[2 * i] = static_cast<CSAMPLE>(m_mono[i]);
            stereo[2 * i + 1] = static_cast<CSAMPLE>(m_mono[i]);
        }
        return stereo;
    }

    std::optional<mixxx::BeatGridRefiner::Grid> refine(double bpm) const {
        const std::vector<CSAMPLE> stereo = this->stereo();
        mixxx::BeatGridRefiner refiner{mixxx::audio::SampleRate(kSampleRate)};
        // In chunks, as the analyzer feeds it.
        constexpr SINT kChunk = 8192;
        for (size_t i = 0; i < stereo.size(); i += kChunk) {
            refiner.processStereoSamples(&stereo[i],
                    static_cast<SINT>(std::min<size_t>(kChunk, stereo.size() - i)));
        }
        return refiner.refine(mixxx::Bpm(bpm));
    }

    double seconds() const {
        return m_mono.size() / kSampleRate;
    }

  private:
    void add(double at, double value) {
        const auto index = static_cast<size_t>(std::lround(at * kSampleRate));
        if (index < m_mono.size()) {
            m_mono[index] += value;
        }
    }

    std::vector<double> m_mono;
    std::mt19937 m_random;
};

double firstBeatSecs(const mixxx::BeatGridRefiner::Grid& grid) {
    return grid.firstBeat.value() / kSampleRate;
}

/// The distance from `position` to the nearest beat of a grid starting at
/// `firstBeat`.
double distanceToGrid(double position, double firstBeat, double beatLength) {
    double distance = std::fmod(position - firstBeat, beatLength);
    if (distance < 0) {
        distance += beatLength;
    }
    return std::min(distance, beatLength - distance);
}

TEST(BeatGridRefinerTest, PlacesTheGridOnTheKick) {
    Synth synth(60);
    const double beatLength = 60.0 / 140;
    const double start = 0.25;
    for (double t = start; t < synth.seconds(); t += beatLength) {
        synth.kick(t);
        synth.hat(t + beatLength / 2);
    }

    const auto grid = synth.refine(140.3);
    ASSERT_TRUE(grid);
    EXPECT_DOUBLE_EQ(140.0, grid->bpm.value());
    EXPECT_LT(distanceToGrid(start, firstBeatSecs(*grid), beatLength), kToleranceSecs);
}

TEST(BeatGridRefinerTest, PrefersTheKickOverARollingBassline) {
    Synth synth(60);
    const double beatLength = 60.0 / 150;
    const double start = 0.1;
    for (double t = start; t < synth.seconds(); t += beatLength) {
        synth.kick(t);
        for (int sixteenth = 1; sixteenth < 4; ++sixteenth) {
            synth.bass(t + sixteenth * beatLength / 4);
        }
        synth.hat(t + beatLength / 2);
    }

    const auto grid = synth.refine(150);
    ASSERT_TRUE(grid);
    EXPECT_DOUBLE_EQ(150.0, grid->bpm.value());
    EXPECT_LT(distanceToGrid(start, firstBeatSecs(*grid), beatLength), kToleranceSecs);
}

TEST(BeatGridRefinerTest, CorrectsTwoThirdsOfTheTempo) {
    Synth synth(60);
    const double beatLength = 60.0 / 150;
    const double start = 0.1;
    for (double t = start; t < synth.seconds(); t += beatLength) {
        synth.kick(t);
        for (int sixteenth = 1; sixteenth < 4; ++sixteenth) {
            synth.bass(t + sixteenth * beatLength / 4);
        }
    }

    const auto grid = synth.refine(100);
    ASSERT_TRUE(grid);
    EXPECT_DOUBLE_EQ(150.0, grid->bpm.value());
    EXPECT_LT(distanceToGrid(start, firstBeatSecs(*grid), beatLength), kToleranceSecs);
}

TEST(BeatGridRefinerTest, AnchorsOnTheFirstDownbeat) {
    // A pickup beat, then bars whose hi-hats come and go every four bars.
    Synth synth(90);
    const double beatLength = 60.0 / 130;
    const double start = 0.1;
    int beat = 0;
    for (double t = start; t < synth.seconds(); t += beatLength, ++beat) {
        synth.kick(t);
        const int bar = (beat - 1) / 4;
        if (beat >= 1 && bar % 8 >= 4) {
            for (int sixteenth = 0; sixteenth < 4; ++sixteenth) {
                synth.hat(t + sixteenth * beatLength / 4, 0.2);
            }
        }
    }

    const auto grid = synth.refine(130);
    ASSERT_TRUE(grid);
    EXPECT_DOUBLE_EQ(130.0, grid->bpm.value());
    EXPECT_NEAR(start + beatLength, firstBeatSecs(*grid), kToleranceSecs);
}

TEST(BeatGridRefinerTest, KeepsThePluginGridWithoutABeat) {
    Synth synth(60);
    synth.noise(0.3);
    EXPECT_FALSE(synth.refine(120));
}

TEST(BeatGridRefinerTest, KeepsThePluginGridForShortTracks) {
    Synth synth(5);
    for (double t = 0; t < synth.seconds(); t += 0.5) {
        synth.kick(t);
    }
    EXPECT_FALSE(synth.refine(120));
}

class AnalyzerJkrBeatsTest : public MixxxTest {
  protected:
    /// Analyzes a 140 BPM kick track whose first kick is at 0.25 s, the way
    /// the app does, with the detector `pluginId`.
    mixxx::BeatsPointer analyze(const QString& pluginId) {
        Synth synth(60);
        for (double t = 0.25; t < synth.seconds(); t += 60.0 / 140) {
            synth.kick(t);
        }
        const std::vector<CSAMPLE> samples = synth.stereo();
        const SINT frames = static_cast<SINT>(samples.size() / 2);

        BeatDetectionSettings settings(config());
        settings.setBeatPluginId(pluginId);
        settings.setFixedTempoAssumption(true);
        settings.setFastAnalysis(false);
        auto pTrack = Track::newTemporary();
        pTrack->setAudioProperties(mixxx::audio::ChannelCount(2),
                mixxx::audio::SampleRate(kSampleRate),
                mixxx::audio::Bitrate(),
                mixxx::Duration::fromSeconds(synth.seconds()));
        AnalyzerBeats analyzer(config(), true);
        EXPECT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack),
                mixxx::audio::SampleRate(kSampleRate),
                frames));
        for (size_t i = 0; i < samples.size(); i += mixxx::kAnalysisSamplesPerChunk) {
            analyzer.processSamples(&samples[i],
                    static_cast<SINT>(std::min<size_t>(
                            mixxx::kAnalysisSamplesPerChunk, samples.size() - i)));
        }
        analyzer.storeResults(pTrack);
        analyzer.cleanup();
        return pTrack->getBeats();
    }
};

TEST_F(AnalyzerJkrBeatsTest, JkrIsTheDefault) {
    EXPECT_EQ(mixxx::AnalyzerJkrBeats::queenMaryPluginInfo().id(),
            AnalyzerBeats::defaultPlugin().id());
}

TEST_F(AnalyzerJkrBeatsTest, MixxxDetectorKeepsItsGrid) {
    const auto pBeats = analyze(mixxx::AnalyzerQueenMaryBeats::pluginInfo().id());
    ASSERT_TRUE(pBeats);
    EXPECT_FALSE(pBeats->getSubVersion().contains("grid_refiner"));
}

TEST_F(AnalyzerJkrBeatsTest, JkrPlacesTheGridOnTheKick) {
    const auto pBeats = analyze(mixxx::AnalyzerJkrBeats::queenMaryPluginInfo().id());
    ASSERT_TRUE(pBeats);
    EXPECT_TRUE(pBeats->getSubVersion().contains("vamp_plugin_id=jkr-qm-tempotracker"));
    EXPECT_TRUE(pBeats->getSubVersion().contains("grid_refiner=1"));
    EXPECT_DOUBLE_EQ(140.0, pBeats->getLastMarkerBpm().value());
    EXPECT_LT(distanceToGrid(0.25,
                      (*pBeats->cfirstmarker()).value() / kSampleRate,
                      60.0 / 140),
            kToleranceSecs);
}

TEST_F(AnalyzerJkrBeatsTest, JkrPlacesSoundTouchGridsToo) {
    const auto pBeats = analyze(mixxx::AnalyzerJkrBeats::soundTouchPluginInfo().id());
    ASSERT_TRUE(pBeats);
    EXPECT_TRUE(pBeats->getSubVersion().contains("grid_refiner=1"));
    EXPECT_DOUBLE_EQ(140.0, pBeats->getLastMarkerBpm().value());
    EXPECT_LT(distanceToGrid(0.25,
                      (*pBeats->cfirstmarker()).value() / kSampleRate,
                      60.0 / 140),
            kToleranceSecs);
}

} // namespace
