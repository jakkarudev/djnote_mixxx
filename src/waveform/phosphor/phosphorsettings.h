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

/// How the color is put on the lane. Stored in the config, never reorder.
enum class LightBlend {
    Add = 0, // the light is added to what is behind (the calibrated look)
    Opaque = 1, // the light replaces what is behind
};

/// A config value as a blend; anything unknown gives Add.
LightBlend blendFromInt(int value);

/// The settings that shape the color track. They are read when a track is
/// analyzed, so a change applies from the next loaded track.
struct ColorTrackParams {
    /// Levels are peak^mixExponent (how sounds of different bands mix).
    double mixExponent = 0.35;
    double attackMs = 5.0;
    /// Low, Mid, High.
    double releaseMs[3] = {350.0, 180.0, 40.0};
    /// How early red (bass) starts to rise before a hit.
    double lookAheadMs = 120.0;

    bool operator==(const ColorTrackParams&) const = default;
};

/// Ranges of the advanced settings (also the clamping limits).
constexpr double kMinSaturation = 1.0;
constexpr double kMaxSaturation = 10.0;
constexpr double kMaxChannelGain = 4.0;
constexpr double kMinBrightnessExponent = 0.1;
constexpr double kMaxBrightnessExponent = 1.5;
constexpr double kMinDensityBrightness = 0.1;
constexpr double kMaxDensityBrightness = 3.0;
constexpr double kMinShapeWindow = 1.0;
constexpr double kMaxShapeWindow = 6.0;
constexpr double kMinMixExponent = 0.1;
constexpr double kMaxMixExponent = 1.5;
constexpr double kMaxAttackMs = 50.0;
constexpr double kMaxReleaseMs = 2000.0;
constexpr double kMaxLookAheadMs = 500.0;

struct PhosphorSettings {
    WaveformDetail detail = kDefaultDetail;

    // Advanced settings: live (the next frame).
    double saturation = 5.0;
    /// Low (red), Mid (green), High (blue).
    double channelGains[3] = {1.0, 0.85, 1.8};
    double brightnessExponent = 0.87;
    double densityBrightness = 0.7;
    LightBlend blend = LightBlend::Add;
    // Advanced settings: after a recompute of the lane.
    double shapeWindow = 2.0;
    // Advanced settings: from the next loaded track.
    ColorTrackParams color;
};

/// Ranges of the Phosphor overview's settings (also the clamping limits).
constexpr double kMinOverviewBrightness = 0.1;
constexpr double kMaxOverviewBrightness = 4.0;
constexpr double kMinOverviewLevelExponent = 0.1;
constexpr double kMaxOverviewLevelExponent = 1.5;
constexpr double kMinOverviewDensityExponent = 0.05;
constexpr double kMaxOverviewDensityExponent = 1.0;
constexpr double kMaxOverviewGain = 4.0;

/// The Phosphor overview's own settings group. All of them apply live: the
/// overview image is drawn again from the stored columns. They share no values
/// with PhosphorSettings (the scrolling waveform).
struct PhosphorOverviewSettings {
    double brightness = 1.0;
    double levelExponent = 0.7;
    double densityExponent = 0.25;
    double gainLow = 1.0;
    double gainMid = 1.0;
    double gainHigh = 1.0;

    bool operator==(const PhosphorOverviewSettings&) const = default;
};

PhosphorOverviewSettings clampOverviewSettings(const PhosphorOverviewSettings& settings);
PhosphorOverviewSettings loadOverviewSettings(const UserSettingsPointer& pConfig);
void saveOverviewSettings(
        const UserSettingsPointer& pConfig, const PhosphorOverviewSettings& settings);

/// The settings with every value pulled into its range.
PhosphorSettings clampSettings(const PhosphorSettings& settings);

PhosphorSettings loadSettings(const UserSettingsPointer& pConfig);
void saveSettings(const UserSettingsPointer& pConfig, const PhosphorSettings& settings);

} // namespace mixxx::phosphor
