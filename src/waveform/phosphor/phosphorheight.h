#pragma once

#include "waveform/waveformgains.h"

namespace mixxx::phosphor {

/// The gain side of the Phosphor lane, as plain functions. The density cache
/// does not depend on any of this: the shader stretches each column about the
/// lane center, so a knob, a kill or a Follows toggle never recomputes the
/// cache. The shader repeats the maths of stretchedRowDensity()
/// and eqHeightFactor().

/// What the shader receives per frame.
struct ShaderGains {
    /// All visual gain x total gain (when Follows gain is on).
    float height = 1.0f;
    /// EQ knob gains with kills (1.0 when Follows EQ is off or the deck's EQ
    /// is disabled), Low, Mid, High. Without the visual gains.
    float eq[3] = {1.0f, 1.0f, 1.0f};
    /// What multiplies the band levels for the color: EQ x visual gains.
    float color[3] = {1.0f, 1.0f, 1.0f};
};

ShaderGains shaderGains(const waveformgains::Inputs& in);

/// ‖levels * eq‖ / ‖levels‖; 1.0 for a column without levels.
float eqHeightFactor(const float levels[3], const float eq[3]);

/// The height factor g of a column.
float columnHeightFactor(const float levels[3], const ShaderGains& gains);

/// The smallest height factor that is drawn; the shader loops over at most
/// kMaxCoveredRows source rows per pixel row. Below it the column is not drawn
/// (it would be under 1/16 of the lane high).
constexpr float kMinHeightFactor = 1.0f / 16.0f;
constexpr int kMaxCoveredRows = 18; // ceil(1 / kMinHeightFactor) + 2

/// The density of lane pixel row `pixelRow` of a column stretched by g about
/// the lane center: the integral of the source density over the source
/// interval [c + (y - c) / g, c + (y + 1 - c) / g], c = height / 2, with
/// `density` the height source rows (row i covers [i, i + 1]). For g >= 1 that
/// is the source row's density / g; for g < 1 it sums the rows covered, so the
/// total density of a column is conserved for every g. The part of the
/// interval outside the lane is clipped; 0 when g < kMinHeightFactor.
float stretchedRowDensity(const float* density, int height, int pixelRow, float g);

} // namespace mixxx::phosphor
