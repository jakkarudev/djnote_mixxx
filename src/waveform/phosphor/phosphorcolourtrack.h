#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "util/types.h"
#include "waveform/phosphor/phosphorsettings.h"

namespace mixxx::phosphor {

/// Number of taps of the colour filters: about 4096 at 44.1 kHz, scaled with
/// the sample rate, even.
int colourFirLength(int sampleRate);

/// The block of the colour track in frames (1 ms).
int colourBlockFrames(int sampleRate);

/// Linear-phase FIR taps of the R, G, B colour filters (research 8.3's stripe
/// table: g_c(f) = unit_table_c(f)^(1/0.35), interpolated on log frequency,
/// each frequency's RGB vector normalised to length 1), Hann-windowed.
std::array<std::vector<double>, 3> designColourFilters(
        int sampleRate, double mixExponent = ColourTrackParams().mixExponent);

/// Builds the colour track (ADR-0002) of a track from its mono samples,
/// progressively: three zero-phase FIR filters applied by FFT overlap-add, the
/// peak per 1 ms block, level = peak^0.35, a one-pole follower per channel
/// (attack 5 ms, release R 350 / G 180 / B 40 ms), and red max'ed with a 120 ms
/// backward follower. These numbers are the defaults of ColourTrackParams.
///
/// Output is three uint16 per block (level * 65535). A block is emitted once
/// the filters' delay and the backward follower's memory (about 1.2 s) are
/// behind it, so the colour track trails the samples by about 2 s.
/// One writer thread; not thread-safe.
class ColourTrackBuilder {
  public:
    explicit ColourTrackBuilder(int sampleRate, const ColourTrackParams& params = {});
    ~ColourTrackBuilder();

    /// Feeds mono samples (int16 units). Appends the blocks that became final
    /// to *pOut (3 values each).
    void append(const int16_t* pSamples, SINT count, std::vector<uint16_t>* pOut);

    /// No more samples: appends all remaining whole blocks.
    void finish(std::vector<uint16_t>* pOut);

  private:
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};

} // namespace mixxx::phosphor
