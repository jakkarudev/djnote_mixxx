#pragma once

#include "waveform/waveformgains.h"

namespace mixxx::phosphor {

/// The gain side of the Phosphor lane, as plain functions. The density cache
/// does not depend on any of this: the shader stretches each column about the
/// lane centre, so a knob, a kill or a Follows toggle never recomputes the
/// cache. The shader repeats the maths of stretchSourceRow() and
/// stretchedDensity() and eqHeightFactor().

/// What the shader receives per frame.
struct ShaderGains {
    /// All visual gain x total gain (when Follows gain is on).
    float height = 1.0f;
    /// EQ knob gains with kills (1.0 when Follows EQ is off or the deck's EQ
    /// is disabled), Low, Mid, High. Without the visual gains.
    float eq[3] = {1.0f, 1.0f, 1.0f};
    /// What multiplies the band levels for the colour: EQ x visual gains.
    float colour[3] = {1.0f, 1.0f, 1.0f};
};

ShaderGains shaderGains(const waveformgains::Inputs& in);

/// ‖levels * eq‖ / ‖levels‖; 1.0 for a column without levels.
float eqHeightFactor(const float levels[3], const float eq[3]);

/// The height factor g of a column.
float columnHeightFactor(const float levels[3], const ShaderGains& gains);

/// The source row a lane pixel row reads when its column is stretched by g
/// about the lane centre (sample at y / g), or -1 when that is outside the
/// lane (clipped, or g is 0).
int stretchSourceRow(int pixelRow, int height, float g);

/// The density after the stretch: divided by g.
float stretchedDensity(float density, float g);

} // namespace mixxx::phosphor
