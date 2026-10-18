// Beat analysis benchmark: runs beat analysis over a list of audio files and
// writes the resulting BPM and grid anchor of each file to a TSV file, for
// comparison against reference grids with tools/beat_bench.py.
//
// Disabled by default. Run with:
//   DJNOTE_BEATBENCH_LIST=<file with one audio path per line>
//   DJNOTE_BEATBENCH_OUT=<tsv path>
//   DJNOTE_BEATBENCH_THREADS=<optional thread count>
//   mixxx-test --gtest_also_run_disabled_tests
//       --gtest_filter=BeatAnalysisBenchTest.DISABLED_Run
//
// Grids written per file: "coarse" (Mixxx's Queen Mary detector, through
// BeatFactory) and "app" (the complete AnalyzerBeats path with the fixed tempo
// assumption and the detector named by DJNOTE_BEATBENCH_PLUGIN, by default
// jkr (Queen Mary tempo)).

#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>
#include <atomic>
#include <thread>
#include <vector>

#include "analyzer/analyzerbeats.h"
#include "analyzer/analyzertrack.h"
#include "analyzer/constants.h"
#include "analyzer/plugins/analyzerjkrbeats.h"
#include "analyzer/plugins/analyzerqueenmarybeats.h"
#include "sources/audiosourcestereoproxy.h"
#include "sources/soundsourceproxy.h"
#include "test/mixxxtest.h"
#include "test/soundsourceproviderregistration.h"
#include "track/beatfactory.h"
#include "track/beats.h"
#include "track/track.h"
#include "util/samplebuffer.h"

namespace {

struct GridResult {
    double bpm = 0;
    double firstBeat = -1;
};

struct BenchResult {
    QString path;
    double sampleRate = 0;
    double frames = 0;
    GridResult coarse;
    GridResult app;
    qint64 coarseNanos = 0;
    qint64 appNanos = 0;
};

std::vector<CSAMPLE> decodeStereo(const QString& path,
        mixxx::audio::SampleRate* pSampleRate) {
    auto pTrack = Track::newTemporary(path);
    mixxx::AudioSource::OpenParams openParams;
    openParams.setChannelCount(mixxx::kAnalysisChannels);
    auto pAudioSource = SoundSourceProxy(pTrack).openAudioSource(openParams);
    if (!pAudioSource) {
        return {};
    }
    *pSampleRate = pAudioSource->getSignalInfo().getSampleRate();
    mixxx::AudioSourceStereoProxy proxy(pAudioSource, mixxx::kAnalysisFramesPerChunk);
    mixxx::SampleBuffer buffer(mixxx::kAnalysisSamplesPerChunk);
    std::vector<CSAMPLE> samples;
    samples.reserve(static_cast<size_t>(pAudioSource->frameLength()) *
            mixxx::kAnalysisChannels);
    mixxx::IndexRange remaining = pAudioSource->frameIndexRange();
    while (!remaining.empty()) {
        auto chunk = remaining.splitAndShrinkFront(
                math_min(mixxx::kAnalysisFramesPerChunk, remaining.length()));
        const auto readable = proxy.readSampleFrames(mixxx::WritableSampleFrames(
                chunk, mixxx::SampleBuffer::WritableSlice(buffer)));
        remaining = intersect(remaining, proxy.frameIndexRange());
        const CSAMPLE* pData = readable.readableData();
        samples.insert(samples.end(), pData, pData + readable.readableLength());
    }
    return samples;
}

template<typename Fn>
void forEachChunk(const std::vector<CSAMPLE>& samples, Fn fn) {
    const SINT total = static_cast<SINT>(samples.size());
    for (SINT i = 0; i < total; i += mixxx::kAnalysisSamplesPerChunk) {
        fn(&samples[i], math_min(mixxx::kAnalysisSamplesPerChunk, total - i));
    }
}

GridResult toResult(const mixxx::BeatsPointer& pBeats) {
    GridResult result;
    if (pBeats) {
        result.bpm = pBeats->getLastMarkerBpm().value();
        result.firstBeat = (*pBeats->cfirstmarker()).value();
    }
    return result;
}

BenchResult analyze(UserSettingsPointer pConfig, const QString& path) {
    BenchResult result;
    result.path = path;
    mixxx::audio::SampleRate sampleRate;
    const std::vector<CSAMPLE> samples = decodeStereo(path, &sampleRate);
    if (samples.empty()) {
        return result;
    }
    const SINT frames = static_cast<SINT>(samples.size() / mixxx::kAnalysisChannels);
    result.sampleRate = sampleRate;
    result.frames = frames;

    QElapsedTimer timer;
    timer.start();
    mixxx::AnalyzerQueenMaryBeats plugin;
    plugin.initialize(sampleRate);
    forEachChunk(samples, [&](const CSAMPLE* pIn, SINT count) {
        plugin.processSamples(pIn, count);
    });
    plugin.finalize();
    result.coarse = toResult(BeatFactory::makePreferredBeats(
            plugin.getBeats(), {}, true, sampleRate));
    result.coarseNanos = timer.nsecsElapsed();

    timer.restart();
    auto pTrack = Track::newTemporary(path);
    pTrack->setAudioProperties(mixxx::kAnalysisChannels,
            sampleRate,
            mixxx::audio::Bitrate(),
            mixxx::Duration::fromSeconds(frames / static_cast<double>(sampleRate)));
    AnalyzerBeats analyzer(pConfig, true);
    if (analyzer.initialize(AnalyzerTrack(pTrack), sampleRate, frames)) {
        forEachChunk(samples, [&](const CSAMPLE* pIn, SINT count) {
            analyzer.processSamples(pIn, count);
        });
        analyzer.storeResults(pTrack);
        analyzer.cleanup();
        result.app = toResult(pTrack->getBeats());
    }
    result.appNanos = timer.nsecsElapsed();
    return result;
}

class BeatAnalysisBenchTest : public MixxxTest, SoundSourceProviderRegistration {
};

TEST_F(BeatAnalysisBenchTest, DISABLED_Run) {
    const QString listPath = qEnvironmentVariable("DJNOTE_BEATBENCH_LIST");
    const QString outPath = qEnvironmentVariable("DJNOTE_BEATBENCH_OUT");
    ASSERT_FALSE(listPath.isEmpty());
    ASSERT_FALSE(outPath.isEmpty());

    QFile listFile(listPath);
    ASSERT_TRUE(listFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QStringList paths;
    QTextStream in(&listFile);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty() && !paths.contains(line)) {
            paths.append(line);
        }
    }

    BeatDetectionSettings settings(config());
    settings.setBeatPluginId(qEnvironmentVariable("DJNOTE_BEATBENCH_PLUGIN",
            mixxx::AnalyzerJkrBeats::queenMaryPluginInfo().id()));
    settings.setFixedTempoAssumption(true);
    settings.setFastAnalysis(false);

    std::vector<BenchResult> results(paths.size());
    std::atomic<int> next{0};
    const int threadCount = qEnvironmentVariableIntValue("DJNOTE_BEATBENCH_THREADS") > 0
            ? qEnvironmentVariableIntValue("DJNOTE_BEATBENCH_THREADS")
            : static_cast<int>(std::max(1u, std::thread::hardware_concurrency() / 2));
    std::vector<std::thread> threads;
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&]() {
            for (int i = next++; i < paths.size(); i = next++) {
                results[i] = analyze(config(), paths[i]);
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    QFile outFile(outPath);
    ASSERT_TRUE(outFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&outFile);
    out << "path\tsampleRate\tframes\tbpm_coarse\tfirstBeat_coarse\tbpm_app\tfirstBeat_app"
           "\tcoarseMs\tappMs\n";
    qint64 coarseNanos = 0;
    qint64 appNanos = 0;
    double audioSecs = 0;
    for (const auto& r : results) {
        out << r.path << '\t' << QString::number(r.sampleRate, 'f', 0) << '\t'
            << QString::number(r.frames, 'f', 0);
        for (const auto& grid : {r.coarse, r.app}) {
            out << '\t' << QString::number(grid.bpm, 'f', 6) << '\t'
                << QString::number(grid.firstBeat, 'f', 3);
        }
        out << '\t' << r.coarseNanos / 1000000 << '\t' << r.appNanos / 1000000 << '\n';
        coarseNanos += r.coarseNanos;
        appNanos += r.appNanos;
        if (r.sampleRate > 0) {
            audioSecs += r.frames / r.sampleRate;
        }
    }
    qInfo() << "Analyzed" << audioSecs / 3600 << "hours of audio."
            << "Plugin only:" << coarseNanos / 1e9 << "s,"
            << "AnalyzerBeats with grid refinement:" << appNanos / 1e9 << "s";
}

} // namespace
