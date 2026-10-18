#include "waveform/phosphor/phosphorcolortrack.h"

#include <algorithm>
#include <cmath>

#include "dsp/transforms/FFT.h"

namespace mixxx::phosphor {

namespace {

// Research 8.3: unit color of pure tones at -6 dBFS in Traktor's stripe.
constexpr int kTableSize = 10;
constexpr double kTableHz[kTableSize] = {21, 40, 100, 200, 400, 1000, 2000, 4000, 8000, 18500};
constexpr double kTableRgb[3][kTableSize] = {
    {.92, .88, .80, .67, .49, .29, .18, .12, .07, .05},
    {.39, .46, .58, .71, .80, .79, .68, .53, .41, .32},
    {.07, .09, .16, .23, .34, .53, .71, .84, .91, .94},
};

// Blocks the backward follower looks ahead before a block is final: 10 time
// constants of the look-ahead, so the cut-off error is below 5e-5.
constexpr size_t kBackwardMemoryBlocks = 1200;
// FFT size; the chunk is the size minus the filter length plus one.
constexpr int kFftSize = 32768;

double interpolate(const double* pTable, double logHz) {
    if (logHz <= std::log(kTableHz[0])) {
        return pTable[0];
    }
    for (int i = 1; i < kTableSize; ++i) {
        const double hi = std::log(kTableHz[i]);
        if (logHz <= hi) {
            const double lo = std::log(kTableHz[i - 1]);
            const double t = (logHz - lo) / (hi - lo);
            return pTable[i - 1] + t * (pTable[i] - pTable[i - 1]);
        }
    }
    return pTable[kTableSize - 1];
}

inline double followerCoefficient(double dtSeconds, double tau) {
    if (tau <= 0.0) {
        return 1.0; // no smoothing
    }
    return 1.0 - std::exp(-dtSeconds / tau);
}

inline uint16_t toU16(double level) {
    return static_cast<uint16_t>(std::clamp(std::floor(level * 65535.0 + 0.5), 0.0, 65535.0));
}

} // namespace

int colorFirLength(int sampleRate) {
    return static_cast<int>(std::lround(4096.0 * sampleRate / 44100.0 / 2)) * 2;
}

int colorBlockFrames(int sampleRate) {
    return std::max(1, static_cast<int>(std::lround(sampleRate / 1000.0)));
}

std::array<std::vector<double>, 3> designColorFilters(int sampleRate, double mixExponent) {
    const int m = colorFirLength(sampleRate);
    const int half = m / 2;
    // Response on the rfft grid of m points.
    std::array<std::vector<double>, 3> g;
    for (auto& v : g) {
        v.resize(static_cast<size_t>(half) + 1);
    }
    for (int k = 0; k <= half; ++k) {
        const double hz = std::clamp(
                static_cast<double>(k) * sampleRate / m, kTableHz[0], kTableHz[kTableSize - 1]);
        const double logHz = std::log(hz);
        double rgb[3];
        double norm = 0;
        for (int c = 0; c < 3; ++c) {
            rgb[c] = interpolate(kTableRgb[c], logHz);
            norm += rgb[c] * rgb[c];
        }
        norm = std::sqrt(norm);
        for (int c = 0; c < 3; ++c) {
            g[c][static_cast<size_t>(k)] = std::pow(rgb[c] / norm, 1.0 / mixExponent);
        }
    }
    // irfft of a real, even spectrum, rolled by m/2 and Hann-windowed.
    std::vector<double> cosTable(static_cast<size_t>(m));
    for (int i = 0; i < m; ++i) {
        cosTable[static_cast<size_t>(i)] = std::cos(2.0 * M_PI * i / m);
    }
    std::array<std::vector<double>, 3> taps;
    for (int c = 0; c < 3; ++c) {
        taps[c].resize(static_cast<size_t>(m));
        const auto& gc = g[c];
        for (int n = 0; n < m; ++n) {
            double sum = gc[0] + gc[static_cast<size_t>(half)] * ((n & 1) ? -1.0 : 1.0);
            for (int k = 1; k < half; ++k) {
                sum += 2.0 * gc[static_cast<size_t>(k)] *
                        cosTable[static_cast<size_t>((static_cast<int64_t>(k) * n) % m)];
            }
            const int rolled = (n + half) % m;
            const double window = 0.5 - 0.5 * std::cos(2.0 * M_PI * rolled / m);
            taps[c][static_cast<size_t>(rolled)] = sum / m * window;
        }
    }
    return taps;
}

struct ColorTrackBuilder::Impl {
    // A complex vector as separate real and imaginary parts (the FFT's layout).
    struct Spectrum {
        std::vector<double> re = std::vector<double>(kFftSize, 0.0);
        std::vector<double> im = std::vector<double>(kFftSize, 0.0);
        void clear() {
            std::fill(re.begin(), re.end(), 0.0);
            std::fill(im.begin(), im.end(), 0.0);
        }
    };

    Impl(int sampleRate, const ColorTrackParams& colorParams)
            : mixExponent(colorParams.mixExponent),
              blockFrames(colorBlockFrames(sampleRate)),
              firLength(colorFirLength(sampleRate)),
              chunk(kFftSize - firLength + 1),
              fft(kFftSize),
              attackCoefficient(0),
              lookAheadCoefficient(0),
              skipOutput(firLength / 2) {
        const double dt = static_cast<double>(blockFrames) / sampleRate;
        attackCoefficient = followerCoefficient(dt, colorParams.attackMs / 1000.0);
        lookAheadCoefficient = followerCoefficient(dt, colorParams.lookAheadMs / 1000.0);
        for (int c = 0; c < 3; ++c) {
            releaseCoefficient[c] = followerCoefficient(dt, colorParams.releaseMs[c] / 1000.0);
        }
        // Spectra of the taps. Red and green share one inverse FFT (red in
        // the real part, green in the imaginary part), blue has its own.
        const auto taps = designColorFilters(sampleRate, mixExponent);
        Spectrum tap;
        Spectrum spectrum;
        for (int c = 0; c < 3; ++c) {
            tap.clear();
            std::copy(taps[c].begin(), taps[c].end(), tap.re.begin());
            fft.process(false, tap.re.data(), tap.im.data(), spectrum.re.data(), spectrum.im.data());
            for (size_t k = 0; k < kFftSize; ++k) {
                if (c == 0) {
                    spectrumRG.re[k] += spectrum.re[k];
                    spectrumRG.im[k] += spectrum.im[k];
                } else if (c == 1) {
                    // times i
                    spectrumRG.re[k] -= spectrum.im[k];
                    spectrumRG.im[k] += spectrum.re[k];
                } else {
                    spectrumB.re[k] = spectrum.re[k];
                    spectrumB.im[k] = spectrum.im[k];
                }
            }
        }
        input.reserve(static_cast<size_t>(chunk));
    }

    // out += in * h (complex), result of the inverse FFT added to 'pending'.
    void filterInto(const Spectrum& h, Spectrum* pPending) {
        for (size_t k = 0; k < kFftSize; ++k) {
            work1.re[k] = spectrum.re[k] * h.re[k] - spectrum.im[k] * h.im[k];
            work1.im[k] = spectrum.re[k] * h.im[k] + spectrum.im[k] * h.re[k];
        }
        // The inverse transform is scaled by 1 / size.
        fft.process(true, work1.re.data(), work1.im.data(), work2.re.data(), work2.im.data());
        for (size_t k = 0; k < kFftSize; ++k) {
            pPending->re[k] += work2.re[k];
            pPending->im[k] += work2.im[k];
        }
    }

    // Runs the filters over the buffered input (zero-padded to a chunk) and
    // moves the finished outputs on to the blocks.
    void processChunk(bool last, std::vector<uint16_t>* pOut) {
        const size_t r = input.size();
        work1.clear();
        std::copy(input.begin(), input.end(), work1.re.begin());
        fft.process(false, work1.re.data(), work1.im.data(), spectrum.re.data(), spectrum.im.data());
        filterInto(spectrumRG, &pendingRG);
        filterInto(spectrumB, &pendingB);
        // The first finalCount samples of the accumulators are final now.
        const size_t finalCount = last ? r + static_cast<size_t>(firLength / 2)
                                       : static_cast<size_t>(chunk);
        for (size_t j = 0; j < finalCount; ++j) {
            if (skipOutput > 0) {
                --skipOutput;
                continue;
            }
            ready[0].push_back(pendingRG.re[j]);
            ready[1].push_back(pendingRG.im[j]);
            ready[2].push_back(pendingB.re[j]);
        }
        if (!last) {
            const auto shift = static_cast<std::ptrdiff_t>(chunk);
            for (Spectrum* p : {&pendingRG, &pendingB}) {
                for (auto* v : {&p->re, &p->im}) {
                    std::copy(v->begin() + shift, v->end(), v->begin());
                    std::fill(v->end() - shift, v->end(), 0.0);
                }
            }
        }
        input.clear();
        makeBlocks();
        commit(last, pOut);
    }

    // Whole blocks of the ready outputs -> raw levels and forward followers.
    void makeBlocks() {
        const size_t blocks = ready[0].size() / static_cast<size_t>(blockFrames);
        for (size_t b = 0; b < blocks; ++b) {
            for (int c = 0; c < 3; ++c) {
                double peak = 0;
                const double* p = ready[c].data() + b * static_cast<size_t>(blockFrames);
                for (int i = 0; i < blockFrames; ++i) {
                    peak = std::max(peak, std::abs(p[i]));
                }
                const double level = std::pow(peak / 32768.0, mixExponent);
                raw.push_back(level);
                const double coefficient =
                        level > forwardState[c] ? attackCoefficient : releaseCoefficient[c];
                forwardState[c] += (level - forwardState[c]) * coefficient;
                smoothed.push_back(forwardState[c]);
            }
        }
        for (auto& v : ready) {
            v.erase(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(blocks * blockFrames));
        }
    }

    // Emits the blocks that are final. Red also needs the backward follower,
    // which depends on later blocks.
    void commit(bool last, std::vector<uint16_t>* pOut) {
        const size_t total = raw.size() / 3;
        const size_t end = last ? total : (total > kBackwardMemoryBlocks ? total - kBackwardMemoryBlocks : 0);
        if (end == 0) {
            return;
        }
        // Backward follower over the uncommitted blocks, started at rest.
        backward.assign(total, 0.0);
        double state = 0;
        for (size_t b = total; b-- > 0;) {
            const double level = raw[b * 3];
            state += (level - state) * (level > state ? attackCoefficient : lookAheadCoefficient);
            backward[b] = state;
        }
        for (size_t b = 0; b < end; ++b) {
            pOut->push_back(toU16(std::max(smoothed[b * 3], backward[b])));
            pOut->push_back(toU16(smoothed[b * 3 + 1]));
            pOut->push_back(toU16(smoothed[b * 3 + 2]));
        }
        raw.erase(raw.begin(), raw.begin() + static_cast<std::ptrdiff_t>(end * 3));
        smoothed.erase(smoothed.begin(), smoothed.begin() + static_cast<std::ptrdiff_t>(end * 3));
    }

    const double mixExponent;
    const int blockFrames;
    const int firLength;
    const int chunk;
    FFT fft;
    double attackCoefficient;
    double lookAheadCoefficient;
    double releaseCoefficient[3] = {0, 0, 0};

    Spectrum spectrumRG; // tap spectra: R + iG, B
    Spectrum spectrumB;
    Spectrum pendingRG; // overlap-add accumulators
    Spectrum pendingB;
    Spectrum spectrum;  // spectrum of the current chunk
    Spectrum work1;
    Spectrum work2;
    std::vector<double> input;
    int skipOutput;
    std::vector<double> ready[3];
    double forwardState[3] = {0, 0, 0};
    std::vector<double> raw;      // level per block and channel, not emitted yet
    std::vector<double> smoothed; // forward follower, same layout
    std::vector<double> backward;
};

ColorTrackBuilder::ColorTrackBuilder(int sampleRate, const ColorTrackParams& params)
        : m_pImpl(std::make_unique<Impl>(sampleRate, params)) {
}

ColorTrackBuilder::~ColorTrackBuilder() = default;

void ColorTrackBuilder::append(const int16_t* pSamples, SINT count, std::vector<uint16_t>* pOut) {
    Impl& impl = *m_pImpl;
    SINT pos = 0;
    while (pos < count) {
        const SINT room = impl.chunk - static_cast<SINT>(impl.input.size());
        const SINT n = std::min(room, count - pos);
        impl.input.insert(impl.input.end(), pSamples + pos, pSamples + pos + n);
        pos += n;
        if (static_cast<int>(impl.input.size()) == impl.chunk) {
            impl.processChunk(false, pOut);
        }
    }
}

void ColorTrackBuilder::finish(std::vector<uint16_t>* pOut) {
    m_pImpl->processChunk(true, pOut);
}

} // namespace mixxx::phosphor
