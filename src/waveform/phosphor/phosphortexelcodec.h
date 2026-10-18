#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "waveform/phosphor/phosphorlanemodel.h"

namespace mixxx::phosphor {

/// How a lane column is packed into RGBA8 texels for the ring texture, one
/// texel per row (the shader decodes the same layout):
///   R, B: sqrt(density / kTexelDensityMax) as a 16-bit fixed point value
///         (R high byte). The square root keeps the error of the final pixel
///         under 1/255 over the whole range. The density is not clipped here:
///         only the final pixel is.
///   G:    255 on the row that must be drawn black (digital silence)
///   A:    255
/// The renderer's columns are rows of the texture, so a run of columns is a
/// contiguous block of memory.
constexpr float kTexelDensityMax = 4096.0f; // above any lane height / 2
constexpr int kBytesPerTexel = 4;

/// Writes height texels. An outside column (or null) becomes transparent black.
inline void encodeColumn(const LaneColumn* pColumn, int height, uint8_t* pOut) {
    std::fill(pOut, pOut + static_cast<size_t>(height) * kBytesPerTexel, uint8_t{0});
    if (!pColumn || pColumn->status == ColumnStatus::Outside ||
            static_cast<int>(pColumn->density.size()) != height) {
        return;
    }
    for (int row = 0; row < height; ++row) {
        const float d = std::sqrt(std::clamp(
                pColumn->density[static_cast<size_t>(row)] / kTexelDensityMax, 0.0f, 1.0f));
        const unsigned v = static_cast<unsigned>(d * 65535.0f + 0.5f);
        uint8_t* texel = pOut + static_cast<size_t>(row) * kBytesPerTexel;
        texel[0] = static_cast<uint8_t>(v >> 8);
        texel[2] = static_cast<uint8_t>(v & 0xFF);
        texel[3] = 255;
    }
    if (pColumn->status == ColumnStatus::Silence) {
        // A thin black center line, no axis.
        pOut[static_cast<size_t>(rowOfValue(0, height)) * kBytesPerTexel + 1] = 255;
    }
}

/// The band levels of the columns live in a second, small RGBA8 texture with
/// one texel per column slot in two rows: row 0 holds the high bytes of the
/// R, G, B levels (16-bit fixed point, 0..1), row 1 the low bytes (A = 255).
/// An outside column is all zero.
constexpr int kColorRows = 2;

inline void encodeColumnLevels(const LaneColumn* pColumn, uint8_t* pHigh, uint8_t* pLow) {
    for (int c = 0; c < 4; ++c) {
        pHigh[c] = 0;
        pLow[c] = 0;
    }
    if (!pColumn || pColumn->status == ColumnStatus::Outside) {
        return;
    }
    for (int c = 0; c < 3; ++c) {
        const unsigned v = static_cast<unsigned>(std::clamp(pColumn->levels[c], 0.0f, 1.0f) * 65535.0f + 0.5f);
        pHigh[c] = static_cast<uint8_t>(v >> 8);
        pLow[c] = static_cast<uint8_t>(v & 0xFF);
    }
    pHigh[3] = 255;
    pLow[3] = 255;
}

/// The level a pair of bytes stands for.
inline float decodeLevel(uint8_t high, uint8_t low) {
    return (high * 256u + low) / 65535.0f;
}

/// The density a texel stands for (what the shader computes).
inline float decodeDensity(const uint8_t* pTexel) {
    const float root = (pTexel[0] * 256u + pTexel[2]) / 65535.0f;
    return root * root * kTexelDensityMax;
}

} // namespace mixxx::phosphor
