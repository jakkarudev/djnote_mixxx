#include "waveform/phosphor/phosphorheight.h"

#include <algorithm>
#include <cmath>

namespace mixxx::phosphor {

ShaderGains shaderGains(const waveformgains::Inputs& in) {
    ShaderGains out;
    const waveformgains::Gains full = waveformgains::calculateGains(in);
    out.height = full.all;
    out.color[0] = full.low;
    out.color[1] = full.mid;
    out.color[2] = full.high;

    // The same rules with neutral visual gains leave the EQ alone.
    waveformgains::Inputs eqOnly = in;
    eqOnly.visualLow = 1.0f;
    eqOnly.visualMid = 1.0f;
    eqOnly.visualHigh = 1.0f;
    const waveformgains::Gains eq = waveformgains::calculateGains(eqOnly);
    out.eq[0] = eq.low;
    out.eq[1] = eq.mid;
    out.eq[2] = eq.high;
    return out;
}

float eqHeightFactor(const float levels[3], const float eq[3]) {
    float sum = 0.0f;
    float gainedSum = 0.0f;
    for (int i = 0; i < 3; ++i) {
        sum += levels[i] * levels[i];
        const float gained = levels[i] * eq[i];
        gainedSum += gained * gained;
    }
    if (sum <= 1e-12f) {
        return 1.0f;
    }
    return std::sqrt(gainedSum / sum);
}

float columnHeightFactor(const float levels[3], const ShaderGains& gains) {
    return gains.height * eqHeightFactor(levels, gains.eq);
}

float stretchedRowDensity(const float* density, int height, int pixelRow, float g) {
    if (g < kMinHeightFactor || height < 1) {
        return 0.0f;
    }
    const double center = height / 2.0;
    const double low = std::max(0.0, center + (pixelRow - center) / g);
    const double high = std::min<double>(height, center + (pixelRow + 1 - center) / g);
    double total = 0.0;
    const int first = static_cast<int>(std::floor(low));
    for (int i = first; i < first + kMaxCoveredRows && i < height && i < high; ++i) {
        const double overlap = std::min<double>(high, i + 1) - std::max<double>(low, i);
        if (overlap > 0.0) {
            total += overlap * density[i];
        }
    }
    return static_cast<float>(total);
}

} // namespace mixxx::phosphor
