#pragma once

#include "preferences/usersettings.h"
#include "util/types.h"

namespace mixxx::phosphor {

/// The Phosphor settings module: keys under [Waveform], typed accessors,
/// defaults and clamping, following the overview settings module. Values that
/// are missing or invalid in the config fall back to the defaults.

/// How much of the sample buffer is kept. Every tier below Full keeps, per
/// block of N samples, the block's minimum and maximum in the order they
/// occurred. The values are stored in the config, so never reorder them.
/// Lowest (N = 32) is left out: the reference script's tier check found it
/// visibly different from Full (research section 8.6).
enum class WaveformDetail {
    Full = 0, // N = 1, every sample
    High = 1, // N = 4
    Medium = 2, // N = 8
    Low = 3, // N = 16
};

constexpr WaveformDetail kDefaultDetail = WaveformDetail::High;
constexpr WaveformDetail kLowestDetail = WaveformDetail::Low;

/// Samples per block of a tier.
constexpr int blockSizeOf(WaveformDetail detail) {
    switch (detail) {
    case WaveformDetail::Full:
        return 1;
    case WaveformDetail::High:
        return 4;
    case WaveformDetail::Medium:
        return 8;
    case WaveformDetail::Low:
        return 16;
    }
    return 4;
}

/// A config value as a tier; anything unknown gives the default (High).
WaveformDetail detailFromInt(int value);

/// No track keeps more than about this many bytes of sample data.
constexpr SINT kMaxDataBytes = 80'000'000;

/// The bytes of sample data of a track of the given length at a tier.
SINT dataBytes(WaveformDetail detail, SINT frames);

/// The tier a track of this length is really built at: the chosen tier is a
/// floor, and while it would exceed kMaxDataBytes the next lower tier is
/// used. Never lower than kLowestDetail.
WaveformDetail effectiveDetail(WaveformDetail chosen, SINT frames);

struct PhosphorSettings {
    WaveformDetail detail = kDefaultDetail;
};

PhosphorSettings loadSettings(const UserSettingsPointer& pConfig);
void saveSettings(const UserSettingsPointer& pConfig, const PhosphorSettings& settings);

} // namespace mixxx::phosphor
