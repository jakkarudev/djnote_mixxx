#include "analyzer/beatgridrefiner.h"

#include <algorithm>
#include <cmath>

#include "track/beatutils.h"
#include "util/math.h"

namespace mixxx {

namespace {

constexpr double kHopSecs = 0.001;
constexpr double kLowCutoffHz = 150;
constexpr double kHighCutoffHz = 1000;

// Tracks shorter than this, or tempos outside this range, keep the plugin's grid.
constexpr double kMinimumSecs = 10;
constexpr double kMinimumBpm = 30;
constexpr double kMaximumBpm = 300;

// The tempo is searched this far around each candidate, relative to its beat
// length. The plugins are usually within 0.1 BPM.
constexpr double kTempoSearchRange = 0.005;

// A tempo level of 3/2 or 2/3 of the plugin's tempo is taken when its onsets
// fold more peaked and its low band (the kicks) folds this much sharper.
constexpr double kTempoLevelLowSharpnessRatio = 1.15;

// Below this peakedness of the folded onsets the track has no beat to place a
// grid on (spoken word, ambient, field recordings); real music is above 0.1.
constexpr double kMinimumPeakiness = 0.05;

// A rounder tempo (as BeatUtils rounds it) within this many BPM is taken when
// its onsets fold at least this peaked relative to the exact tempo.
constexpr double kRoundingRangeBpm = 0.05;
constexpr double kRoundingMinimumPeakiness = 0.9;

// Kick detection, in hops (ms): the attack is the rise of the high band's log
// level from the quietest point this far before to the loudest point this
// far after, the body is the mean low-band level over the following span.
constexpr int kAttackBeforeHops = 20;
constexpr int kAttackAfterHops = 10;
constexpr int kBodyHops = 200;
// The beat is the steepest high-band rise within this distance of the kick.
constexpr int kAttackSearchHops = 15;
// Where Traktor places the beat relative to the steepest rise of the attack.
constexpr double kBeatOffsetSecs = 0.0015;

void designBiquad(bool highPass,
        double cutoffHz,
        double q,
        double sampleRate,
        double* pCoefficients) {
    const double w0 = 2 * M_PI * cutoffHz / sampleRate;
    const double alpha = std::sin(w0) / (2 * q);
    const double cosW0 = std::cos(w0);
    const double a0 = 1 + alpha;
    const double b1 = highPass ? -(1 + cosW0) : 1 - cosW0;
    pCoefficients[0] = std::abs(b1) / 2 / a0;
    pCoefficients[1] = b1 / a0;
    pCoefficients[2] = std::abs(b1) / 2 / a0;
    pCoefficients[3] = -2 * cosW0 / a0;
    pCoefficients[4] = (1 - alpha) / a0;
}

inline double processBiquad(const double* c, double* s, double x) {
    // Transposed direct form II
    const double y = c[0] * x + s[0];
    s[0] = c[1] * x - c[3] * y + s[1];
    s[1] = c[2] * x - c[4] * y;
    return y;
}

inline int wrap(int i, int size) {
    return (i % size + size) % size;
}

/// The mean envelope at each position within the beat, in one-hop bins. Hop i
/// is centered at i + 0.5 hops, so bin b holds positions [b, b + 1).
std::vector<double> foldEnvelope(const std::vector<float>& envelope, double beatLengthHops) {
    const int bins = static_cast<int>(std::ceil(beatLengthHops));
    std::vector<double> sum(bins, 0.0);
    std::vector<int> count(bins, 0);
    double position = 0.5;
    for (const float energy : envelope) {
        const int bin = static_cast<int>(position);
        sum[bin] += energy;
        ++count[bin];
        position += 1;
        if (position >= beatLengthHops) {
            position -= beatLengthHops;
        }
    }
    for (int i = 0; i < bins; ++i) {
        if (count[i] > 0) {
            sum[i] /= count[i];
        }
    }
    return sum;
}

/// Triangular smoothing of a circular profile.
std::vector<double> smoothCircular(const std::vector<double>& profile, int halfWidth) {
    const int bins = static_cast<int>(profile.size());
    std::vector<double> smooth(bins, 0.0);
    for (int i = 0; i < bins; ++i) {
        double weightSum = 0;
        for (int k = -halfWidth; k <= halfWidth; ++k) {
            const double weight = halfWidth + 1 - std::abs(k);
            smooth[i] += weight * profile[wrap(i + k, bins)];
            weightSum += weight;
        }
        smooth[i] /= weightSum;
    }
    return smooth;
}

struct Rise {
    double slope = 0;
    /// Position of the steepest rise, in hops from the start of the beat.
    double positionHops = 0;
};

/// The steepest rise of a circular profile, searched in the bins
/// [from, from + length), interpolated between bins.
Rise steepestRise(const std::vector<double>& profile, int from, int length) {
    const int bins = static_cast<int>(profile.size());
    const auto slopeAt = [&](int i) {
        return (profile[wrap(i + 1, bins)] - profile[wrap(i - 1, bins)]) / 2;
    };
    int steepest = from;
    for (int i = from; i < from + length; ++i) {
        if (slopeAt(i) > slopeAt(steepest)) {
            steepest = i;
        }
    }
    const double before = slopeAt(steepest - 1);
    const double peak = slopeAt(steepest);
    const double after = slopeAt(steepest + 1);
    const double curvature = before - 2 * peak + after;
    const double offset = curvature < 0 ? 0.5 * (before - after) / curvature : 0;
    return {peak, steepest + 0.5 + offset};
}

double mean(const std::vector<double>& values) {
    double sum = 0;
    for (const double value : values) {
        sum += value;
    }
    return values.empty() ? 0 : sum / values.size();
}

/// How peaked the onsets fold at this beat length: the variance of the
/// folded profile relative to its squared mean.
double peakiness(const std::vector<float>& onsets, double beatLengthHops) {
    const auto profile = foldEnvelope(onsets, beatLengthHops);
    const double average = mean(profile);
    if (average <= 0) {
        return 0;
    }
    double variance = 0;
    for (const double value : profile) {
        variance += (value - average) * (value - average);
    }
    return variance / profile.size() / (average * average);
}

/// Rises in log level over 5 ms of a band's 5 ms mean energy, normalized to a
/// mean of 1.
std::vector<float> bandOnsets(const std::vector<float>& envelope) {
    constexpr size_t kSpanHops = 5;
    const size_t size = envelope.size();
    std::vector<float> onsets(size, 0.0f);
    if (size <= kSpanHops) {
        return onsets;
    }
    double total = 0;
    for (const float energy : envelope) {
        total += energy;
    }
    const double floor = 1e-4 * total / size + 1e-12;
    std::vector<double> level(size);
    double window = 0;
    for (size_t i = 0; i < size; ++i) {
        window += envelope[i];
        if (i >= kSpanHops) {
            window -= envelope[i - kSpanHops];
        }
        level[i] = std::log10(window / kSpanHops + floor);
    }
    double sum = 0;
    for (size_t i = kSpanHops; i < size; ++i) {
        onsets[i] = static_cast<float>(std::max(0.0, level[i] - level[i - kSpanHops]));
        sum += onsets[i];
    }
    if (sum > 0) {
        const double scale = size / sum;
        for (float& onset : onsets) {
            onset = static_cast<float>(onset * scale);
        }
    }
    return onsets;
}

} // namespace

BeatGridRefiner::BeatGridRefiner(audio::SampleRate sampleRate)
        : m_sampleRate(sampleRate),
          m_hopFrames(math_max<SINT>(1, static_cast<SINT>(std::lround(sampleRate * kHopSecs)))),
          m_lowState{},
          m_highState{},
          m_lowHopEnergy(0),
          m_highHopEnergy(0),
          m_hopPosition(0) {
    designBiquad(false, kLowCutoffHz, 0.5412, sampleRate, m_lowCoefficients[0]);
    designBiquad(false, kLowCutoffHz, 1.3066, sampleRate, m_lowCoefficients[1]);
    designBiquad(true, kHighCutoffHz, M_SQRT1_2, sampleRate, m_highCoefficients);
}

void BeatGridRefiner::processStereoSamples(const CSAMPLE* pIn, SINT sampleCount) {
    for (SINT i = 0; i + 1 < sampleCount; i += 2) {
        const double mono = 0.5 * (pIn[i] + pIn[i + 1]);
        double low = processBiquad(m_lowCoefficients[0], m_lowState[0], mono);
        low = processBiquad(m_lowCoefficients[1], m_lowState[1], low);
        const double high = processBiquad(m_highCoefficients, m_highState, mono);
        m_lowHopEnergy += low * low;
        m_highHopEnergy += high * high;
        if (++m_hopPosition == m_hopFrames) {
            m_lowEnvelope.push_back(static_cast<float>(m_lowHopEnergy / m_hopFrames));
            m_highEnvelope.push_back(static_cast<float>(m_highHopEnergy / m_hopFrames));
            m_lowHopEnergy = 0;
            m_highHopEnergy = 0;
            m_hopPosition = 0;
        }
    }
}

std::vector<float> BeatGridRefiner::onsetStrength() const {
    std::vector<float> onsets = bandOnsets(m_highEnvelope);
    const std::vector<float> lowOnsets = bandOnsets(m_lowEnvelope);
    for (size_t i = 0; i < onsets.size(); ++i) {
        onsets[i] += lowOnsets[i];
    }
    return onsets;
}

double BeatGridRefiner::findBeatLength(
        const std::vector<float>& onsets, double beatLengthHops) const {
    // Coarse pass on 4 ms onsets, in steps that move the last beat of the
    // track by about 12 ms, then a fine pass at full resolution in 0.5 ms steps.
    constexpr int kDecimation = 4;
    std::vector<float> coarse(onsets.size() / kDecimation);
    for (size_t i = 0; i < coarse.size(); ++i) {
        float sum = 0;
        for (int k = 0; k < kDecimation; ++k) {
            sum += onsets[i * kDecimation + k];
        }
        coarse[i] = sum;
    }
    const double hops = static_cast<double>(onsets.size());
    const double coarseStep = 12.0 * beatLengthHops / hops;
    double best = beatLengthHops;
    double bestPeakiness = -1;
    for (double length = beatLengthHops * (1 - kTempoSearchRange);
            length <= beatLengthHops * (1 + kTempoSearchRange);
            length += coarseStep) {
        const double value = peakiness(coarse, length / kDecimation);
        if (value > bestPeakiness) {
            bestPeakiness = value;
            best = length;
        }
    }
    const double fineStep = 0.5 * beatLengthHops / hops;
    const double center = best;
    bestPeakiness = -1;
    for (double length = center - coarseStep; length <= center + coarseStep;
            length += fineStep) {
        const double value = peakiness(onsets, length);
        if (value > bestPeakiness) {
            bestPeakiness = value;
            best = length;
        }
    }
    return best;
}

double BeatGridRefiner::lowSharpness(double beatLengthHops) const {
    // The steepest rise of the folded low band, relative to its mean.
    const auto profile = smoothCircular(foldEnvelope(m_lowEnvelope, beatLengthHops), 2);
    const double average = mean(profile);
    if (average <= 0) {
        return 0;
    }
    return steepestRise(profile, 0, static_cast<int>(profile.size())).slope / average;
}

double BeatGridRefiner::findKickHops(double beatLengthHops) const {
    // The kick is the biggest attack in the high band's log level that is
    // followed by low-band energy. The low band alone does not find it: in
    // psytrance the rolling bass fills it, and a kick's low end arrives tens
    // of ms after its click.
    const auto lowProfile = smoothCircular(foldEnvelope(m_lowEnvelope, beatLengthHops), 2);
    std::vector<double> highProfile = foldEnvelope(m_highEnvelope, beatLengthHops);
    const double floor = 1e-4 * mean(highProfile) + 1e-12;
    for (double& value : highProfile) {
        value = std::log10(value + floor);
    }
    highProfile = smoothCircular(highProfile, 1);

    const int bins = static_cast<int>(highProfile.size());
    std::vector<double> attack(bins);
    std::vector<double> body(bins);
    double maxAttack = 1e-12;
    double maxBody = 1e-12;
    for (int i = 0; i < bins; ++i) {
        double quietest = highProfile[i];
        double loudest = highProfile[i];
        for (int k = 1; k <= kAttackBeforeHops; ++k) {
            quietest = std::min(quietest, highProfile[wrap(i - k, bins)]);
        }
        for (int k = 1; k <= kAttackAfterHops; ++k) {
            loudest = std::max(loudest, highProfile[wrap(i + k, bins)]);
        }
        attack[i] = loudest - quietest;
        double sum = 0;
        for (int k = 0; k < kBodyHops; ++k) {
            sum += lowProfile[wrap(i + k, bins)];
        }
        body[i] = sum / kBodyHops;
        maxAttack = std::max(maxAttack, attack[i]);
        maxBody = std::max(maxBody, body[i]);
    }
    int kick = 0;
    double bestScore = -1;
    for (int i = 0; i < bins; ++i) {
        const double score = (attack[i] / maxAttack) * (body[i] / maxBody);
        if (score > bestScore) {
            bestScore = score;
            kick = i;
        }
    }
    return steepestRise(highProfile, kick - kAttackSearchHops, 2 * kAttackSearchHops + 1)
                   .positionHops +
            kBeatOffsetSecs / kHopSecs;
}

double BeatGridRefiner::findDownbeatHops(double beatLengthHops, double kickHops) const {
    // Arrangements change on bar lines: elements enter and leave on the first
    // beat of a bar. The downbeats are the beats (of the four positions in a
    // bar) where the levels of the bands change the most from the previous
    // beat, beyond the change that repeats every bar (kick and snare).
    const auto beatLevel = [&](const std::vector<float>& envelope, double position) {
        const auto start = static_cast<size_t>(position);
        const auto end = static_cast<size_t>(position + beatLengthHops);
        double sum = 0;
        for (size_t i = start; i < end; ++i) {
            sum += envelope[i];
        }
        return std::log10(sum / std::max<size_t>(end - start, 1) + 1e-10);
    };
    std::vector<double> lowChanges;
    std::vector<double> highChanges;
    double lowMean[4] = {0, 0, 0, 0};
    double highMean[4] = {0, 0, 0, 0};
    double count[4] = {0, 0, 0, 0};
    double previousLow = 0;
    double previousHigh = 0;
    int beat = 0;
    for (double position = kickHops;
            position + beatLengthHops < static_cast<double>(m_lowEnvelope.size());
            position += beatLengthHops, ++beat) {
        const double low = beatLevel(m_lowEnvelope, position);
        const double high = beatLevel(m_highEnvelope, position);
        if (beat > 0) {
            lowChanges.push_back(low - previousLow);
            highChanges.push_back(high - previousHigh);
            lowMean[beat % 4] += lowChanges.back();
            highMean[beat % 4] += highChanges.back();
            ++count[beat % 4];
        }
        previousLow = low;
        previousHigh = high;
    }
    for (int i = 0; i < 4; ++i) {
        if (count[i] > 0) {
            lowMean[i] /= count[i];
            highMean[i] /= count[i];
        }
    }
    double change[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < lowChanges.size(); ++i) {
        const int position = static_cast<int>((i + 1) % 4);
        change[position] += std::abs(lowChanges[i] - lowMean[position]) +
                std::abs(highChanges[i] - highMean[position]);
    }
    const auto downbeat = std::max_element(change, change + 4) - change;
    return kickHops + downbeat * beatLengthHops;
}

std::optional<BeatGridRefiner::Grid> BeatGridRefiner::refine(Bpm bpm) const {
    if (!bpm.isValid() || bpm.value() < kMinimumBpm || bpm.value() > kMaximumBpm ||
            m_lowEnvelope.size() < kMinimumSecs / kHopSecs) {
        return std::nullopt;
    }
    const double framesPerMinute = 60.0 * m_sampleRate;
    const auto toBeatLengthHops = [&](double tempo) {
        return framesPerMinute / tempo / m_hopFrames;
    };
    const auto toBpm = [&](double beatLengthHops) {
        return framesPerMinute / (beatLengthHops * m_hopFrames);
    };

    const std::vector<float> onsets = onsetStrength();
    double beatLength = findBeatLength(onsets, toBeatLengthHops(bpm.value()));
    double bestPeakiness = peakiness(onsets, beatLength);

    // The trackers sometimes lock onto 2/3 of the tempo (or 3/2), where every
    // 16th note of a rolling bassline still lines up but only every other beat
    // lands on a kick.
    for (const double ratio : {1.5, 2.0 / 3.0}) {
        const double candidateLength = findBeatLength(
                onsets, toBeatLengthHops(bpm.value() * ratio));
        const double candidatePeakiness = peakiness(onsets, candidateLength);
        if (candidatePeakiness > bestPeakiness &&
                lowSharpness(candidateLength) >
                        kTempoLevelLowSharpnessRatio * lowSharpness(beatLength)) {
            beatLength = candidateLength;
            bestPeakiness = candidatePeakiness;
        }
    }
    if (bestPeakiness < kMinimumPeakiness) {
        return std::nullopt;
    }

    // Prefer a round tempo when it fits (nearly) as well.
    const Bpm exactBpm(toBpm(beatLength));
    Bpm finalBpm = exactBpm;
    const Bpm roundBpm = BeatUtils::roundBpmWithinRange(
            exactBpm - kRoundingRangeBpm, exactBpm, exactBpm + kRoundingRangeBpm);
    if (roundBpm != exactBpm) {
        const double roundLength = toBeatLengthHops(roundBpm.value());
        if (peakiness(onsets, roundLength) >= kRoundingMinimumPeakiness * bestPeakiness) {
            finalBpm = roundBpm;
            beatLength = roundLength;
        }
    }

    const double kick = std::fmod(findKickHops(beatLength) + 2 * beatLength, beatLength);
    const double firstBeat = findDownbeatHops(beatLength, kick);
    return Grid{finalBpm, audio::FramePos(firstBeat * m_hopFrames)};
}

} // namespace mixxx
