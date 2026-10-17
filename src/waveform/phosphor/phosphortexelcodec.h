#pragma once

#include <algorithm>
#include <cstdint>

#include "waveform/phosphor/phosphorlanemodel.h"

namespace mixxx::phosphor {

/// How a lane column is packed into RGBA8 texels for the ring texture, one
/// texel per row (the shader decodes the same layout):
///   R, B: density as a 16-bit fixed point value (R high byte), range
///         0..kTexelDensityMax, clipped
///   G:    255 on the row that must be drawn black (digital silence)
///   A:    255
/// The renderer's columns are rows of the texture, so a run of columns is a
/// contiguous block of memory.
constexpr float kTexelDensityMax = 4.0f;
constexpr int kBytesPerTexel = 4;

/// Writes height texels. An outside column (or null) becomes transparent black.
inline void encodeColumn(const LaneColumn* pColumn, int height, uint8_t* pOut) {
    std::fill(pOut, pOut + static_cast<size_t>(height) * kBytesPerTexel, uint8_t{0});
    if (!pColumn || pColumn->status == ColumnStatus::Outside ||
            static_cast<int>(pColumn->density.size()) != height) {
        return;
    }
    for (int row = 0; row < height; ++row) {
        const float d = std::clamp(pColumn->density[static_cast<size_t>(row)] / kTexelDensityMax,
                0.0f,
                1.0f);
        const unsigned v = static_cast<unsigned>(d * 65535.0f + 0.5f);
        uint8_t* texel = pOut + static_cast<size_t>(row) * kBytesPerTexel;
        texel[0] = static_cast<uint8_t>(v >> 8);
        texel[2] = static_cast<uint8_t>(v & 0xFF);
        texel[3] = 255;
    }
    if (pColumn->status == ColumnStatus::Silence) {
        // A thin black centre line, no axis.
        pOut[static_cast<size_t>(rowOfValue(0, height)) * kBytesPerTexel + 1] = 255;
    }
}

/// The density a texel stands for (what the shader computes).
inline float decodeDensity(const uint8_t* pTexel) {
    return ((pTexel[0] * 256u + pTexel[2]) / 65535.0f) * kTexelDensityMax;
}

} // namespace mixxx::phosphor
