#include "waveform/phosphor/phosphorsettings.h"

#include <algorithm>
#include <cmath>

#include "preferences/configobject.h"

namespace mixxx::phosphor {

namespace {

const QString kGroup = QStringLiteral("[Waveform]");

const ConfigKey kDetailCfgKey(kGroup, QStringLiteral("PhosphorDetail"));
const ConfigKey kBlendCfgKey(kGroup, QStringLiteral("PhosphorBlend"));

/// One numeric setting: its config key, range (the clamping limits) and where
/// it lives in PhosphorSettings. The default is the value of a default
/// constructed PhosphorSettings. clampSettings(), loadSettings() and
/// saveSettings() all go through this table.
struct NumberField {
    const char* key;
    double min;
    double max;
    double& (*value)(PhosphorSettings&);
    const double& (*constValue)(const PhosphorSettings&);
};

#define PHOSPHOR_FIELD(key, min, max, member)                                   \
    {                                                                           \
        key, min, max,                                                          \
                [](PhosphorSettings& s) -> double& { return s.member; },        \
                [](const PhosphorSettings& s) -> const double& { return s.member; } \
    }

const NumberField kNumberFields[] = {
    PHOSPHOR_FIELD("PhosphorSaturation", kMinSaturation, kMaxSaturation, saturation),
    PHOSPHOR_FIELD("PhosphorGainLow", 0.0, kMaxChannelGain, channelGains[0]),
    PHOSPHOR_FIELD("PhosphorGainMid", 0.0, kMaxChannelGain, channelGains[1]),
    PHOSPHOR_FIELD("PhosphorGainHigh", 0.0, kMaxChannelGain, channelGains[2]),
    PHOSPHOR_FIELD("PhosphorBrightnessExponent",
            kMinBrightnessExponent,
            kMaxBrightnessExponent,
            brightnessExponent),
    PHOSPHOR_FIELD("PhosphorDensityBrightness",
            kMinDensityBrightness,
            kMaxDensityBrightness,
            densityBrightness),
    PHOSPHOR_FIELD("PhosphorShapeWindow", kMinShapeWindow, kMaxShapeWindow, shapeWindow),
    PHOSPHOR_FIELD("PhosphorMixExponent", kMinMixExponent, kMaxMixExponent, color.mixExponent),
    PHOSPHOR_FIELD("PhosphorAttackMs", 0.0, kMaxAttackMs, color.attackMs),
    PHOSPHOR_FIELD("PhosphorReleaseLowMs", 0.0, kMaxReleaseMs, color.releaseMs[0]),
    PHOSPHOR_FIELD("PhosphorReleaseMidMs", 0.0, kMaxReleaseMs, color.releaseMs[1]),
    PHOSPHOR_FIELD("PhosphorReleaseHighMs", 0.0, kMaxReleaseMs, color.releaseMs[2]),
    PHOSPHOR_FIELD("PhosphorLookAheadMs", 0.0, kMaxLookAheadMs, color.lookAheadMs),
};

#undef PHOSPHOR_FIELD

/// Out of range values are pulled into the range; unreadable ones (not a
/// number) give the default.
double clamped(double value, const NumberField& field, double fallback) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, field.min, field.max);
}

ConfigKey keyOf(const NumberField& field) {
    return ConfigKey(kGroup, QString::fromLatin1(field.key));
}

} // namespace

LightBlend blendFromInt(int value) {
    return value == static_cast<int>(LightBlend::Opaque) ? LightBlend::Opaque : LightBlend::Add;
}

WaveformDetail detailFromInt(int value) {
    if (value < static_cast<int>(WaveformDetail::Full) ||
            value > static_cast<int>(kLowestDetail)) {
        return kDefaultDetail;
    }
    return static_cast<WaveformDetail>(value);
}

PhosphorSettings clampSettings(const PhosphorSettings& in) {
    PhosphorSettings defaults;
    PhosphorSettings s = in;
    s.detail = detailFromInt(static_cast<int>(in.detail));
    s.blend = blendFromInt(static_cast<int>(in.blend));
    for (const NumberField& field : kNumberFields) {
        field.value(s) = clamped(field.constValue(in), field, field.value(defaults));
    }
    return s;
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
    PhosphorSettings s;
    const PhosphorSettings defaults;
    s.detail = detailFromInt(pConfig->getValue(kDetailCfgKey, static_cast<int>(defaults.detail)));
    s.blend = blendFromInt(pConfig->getValue(kBlendCfgKey, static_cast<int>(defaults.blend)));
    for (const NumberField& field : kNumberFields) {
        const double fallback = field.constValue(defaults);
        field.value(s) = clamped(pConfig->getValue(keyOf(field), fallback), field, fallback);
    }
    return s;
}

void saveSettings(const UserSettingsPointer& pConfig, const PhosphorSettings& settings) {
    const PhosphorSettings s = clampSettings(settings);
    pConfig->setValue(kDetailCfgKey, static_cast<int>(s.detail));
    pConfig->setValue(kBlendCfgKey, static_cast<int>(s.blend));
    for (const NumberField& field : kNumberFields) {
        pConfig->setValue(keyOf(field), field.constValue(s));
    }
}

} // namespace mixxx::phosphor
