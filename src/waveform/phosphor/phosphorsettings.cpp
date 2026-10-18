#include "waveform/phosphor/phosphorsettings.h"

#include <algorithm>
#include <cmath>

#include "preferences/configobject.h"

namespace mixxx::phosphor {

namespace {

const ConfigKey kDetailCfgKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorDetail"));

const ConfigKey kSaturationCfgKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorSaturation"));
const ConfigKey kGainCfgKeys[3] = {
    ConfigKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorGainR")),
    ConfigKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorGainG")),
    ConfigKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorGainB")),
};
const ConfigKey kBrightnessExponentCfgKey(
        QStringLiteral("[Waveform]"), QStringLiteral("PhosphorBrightnessExponent"));
const ConfigKey kDensityBrightnessCfgKey(
        QStringLiteral("[Waveform]"), QStringLiteral("PhosphorDensityBrightness"));
const ConfigKey kBlendCfgKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorBlend"));
const ConfigKey kShapeWindowCfgKey(
        QStringLiteral("[Waveform]"), QStringLiteral("PhosphorShapeWindow"));
const ConfigKey kMixExponentCfgKey(
        QStringLiteral("[Waveform]"), QStringLiteral("PhosphorMixExponent"));
const ConfigKey kAttackCfgKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorAttackMs"));
const ConfigKey kReleaseCfgKeys[3] = {
    ConfigKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorReleaseLowMs")),
    ConfigKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorReleaseMidMs")),
    ConfigKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorReleaseHighMs")),
};
const ConfigKey kLookAheadCfgKey(
        QStringLiteral("[Waveform]"), QStringLiteral("PhosphorLookAheadMs"));

/// Out of range values are pulled into the range; unreadable ones (not a
/// number) give the default.
double clamped(double value, double min, double max, double fallback) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, min, max);
}

double readClamped(const UserSettingsPointer& pConfig,
        const ConfigKey& key,
        double min,
        double max,
        double fallback) {
    return clamped(pConfig->getValue(key, fallback), min, max, fallback);
}

} // namespace

LightBlend blendFromInt(int value) {
    return value == static_cast<int>(LightBlend::Opaque) ? LightBlend::Opaque : LightBlend::Add;
}

PhosphorSettings clampSettings(const PhosphorSettings& in) {
    const PhosphorSettings d;
    PhosphorSettings s = in;
    s.detail = detailFromInt(static_cast<int>(in.detail));
    s.blend = blendFromInt(static_cast<int>(in.blend));
    s.saturation = clamped(in.saturation, kMinSaturation, kMaxSaturation, d.saturation);
    for (int c = 0; c < 3; ++c) {
        s.channelGains[c] = clamped(in.channelGains[c], 0.0, kMaxChannelGain, d.channelGains[c]);
        s.color.releaseMs[c] =
                clamped(in.color.releaseMs[c], 0.0, kMaxReleaseMs, d.color.releaseMs[c]);
    }
    s.brightnessExponent = clamped(in.brightnessExponent,
            kMinBrightnessExponent,
            kMaxBrightnessExponent,
            d.brightnessExponent);
    s.densityBrightness = clamped(in.densityBrightness,
            kMinDensityBrightness,
            kMaxDensityBrightness,
            d.densityBrightness);
    s.shapeWindow = clamped(in.shapeWindow, kMinShapeWindow, kMaxShapeWindow, d.shapeWindow);
    s.color.mixExponent = clamped(
            in.color.mixExponent, kMinMixExponent, kMaxMixExponent, d.color.mixExponent);
    s.color.attackMs = clamped(in.color.attackMs, 0.0, kMaxAttackMs, d.color.attackMs);
    s.color.lookAheadMs =
            clamped(in.color.lookAheadMs, 0.0, kMaxLookAheadMs, d.color.lookAheadMs);
    return s;
}

WaveformDetail detailFromInt(int value) {
    if (value < static_cast<int>(WaveformDetail::Full) ||
            value > static_cast<int>(kLowestDetail)) {
        return kDefaultDetail;
    }
    return static_cast<WaveformDetail>(value);
}

SINT dataBytes(WaveformDetail detail, SINT frames) {
    const SINT n = blockSizeOf(detail);
    if (n == 1) {
        return frames * 2;
    }
    // Two int16 values per block.
    return (frames + n - 1) / n * 4;
}

WaveformDetail effectiveDetail(WaveformDetail chosen, SINT frames) {
    int tier = static_cast<int>(chosen);
    while (tier < static_cast<int>(kLowestDetail) &&
            dataBytes(static_cast<WaveformDetail>(tier), frames) > kMaxDataBytes) {
        ++tier;
    }
    return static_cast<WaveformDetail>(tier);
}

PhosphorSettings loadSettings(const UserSettingsPointer& pConfig) {
    const PhosphorSettings d;
    PhosphorSettings s;
    s.detail = detailFromInt(pConfig->getValue(kDetailCfgKey, static_cast<int>(d.detail)));
    s.saturation = readClamped(
            pConfig, kSaturationCfgKey, kMinSaturation, kMaxSaturation, d.saturation);
    for (int c = 0; c < 3; ++c) {
        s.channelGains[c] = readClamped(
                pConfig, kGainCfgKeys[c], 0.0, kMaxChannelGain, d.channelGains[c]);
        s.color.releaseMs[c] = readClamped(
                pConfig, kReleaseCfgKeys[c], 0.0, kMaxReleaseMs, d.color.releaseMs[c]);
    }
    s.brightnessExponent = readClamped(pConfig,
            kBrightnessExponentCfgKey,
            kMinBrightnessExponent,
            kMaxBrightnessExponent,
            d.brightnessExponent);
    s.densityBrightness = readClamped(pConfig,
            kDensityBrightnessCfgKey,
            kMinDensityBrightness,
            kMaxDensityBrightness,
            d.densityBrightness);
    s.blend = blendFromInt(pConfig->getValue(kBlendCfgKey, static_cast<int>(d.blend)));
    s.shapeWindow = readClamped(
            pConfig, kShapeWindowCfgKey, kMinShapeWindow, kMaxShapeWindow, d.shapeWindow);
    s.color.mixExponent = readClamped(pConfig,
            kMixExponentCfgKey,
            kMinMixExponent,
            kMaxMixExponent,
            d.color.mixExponent);
    s.color.attackMs = readClamped(pConfig, kAttackCfgKey, 0.0, kMaxAttackMs, d.color.attackMs);
    s.color.lookAheadMs = readClamped(
            pConfig, kLookAheadCfgKey, 0.0, kMaxLookAheadMs, d.color.lookAheadMs);
    return s;
}

void saveSettings(const UserSettingsPointer& pConfig, const PhosphorSettings& settings) {
    const PhosphorSettings s = clampSettings(settings);
    pConfig->setValue(kDetailCfgKey, static_cast<int>(s.detail));
    pConfig->setValue(kSaturationCfgKey, s.saturation);
    for (int c = 0; c < 3; ++c) {
        pConfig->setValue(kGainCfgKeys[c], s.channelGains[c]);
        pConfig->setValue(kReleaseCfgKeys[c], s.color.releaseMs[c]);
    }
    pConfig->setValue(kBrightnessExponentCfgKey, s.brightnessExponent);
    pConfig->setValue(kDensityBrightnessCfgKey, s.densityBrightness);
    pConfig->setValue(kBlendCfgKey, static_cast<int>(s.blend));
    pConfig->setValue(kShapeWindowCfgKey, s.shapeWindow);
    pConfig->setValue(kMixExponentCfgKey, s.color.mixExponent);
    pConfig->setValue(kAttackCfgKey, s.color.attackMs);
    pConfig->setValue(kLookAheadCfgKey, s.color.lookAheadMs);
}

} // namespace mixxx::phosphor
