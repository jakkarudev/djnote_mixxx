#include "waveform/phosphor/phosphorsettings.h"

#include <algorithm>
#include <cmath>

#include "preferences/configobject.h"

namespace mixxx::phosphor {

namespace {

const QString kGroup = QStringLiteral("[Waveform]");

const ConfigKey kDetailCfgKey(kGroup, QStringLiteral("PhosphorDetail"));
const ConfigKey kBlendCfgKey(kGroup, QStringLiteral("PhosphorBlend"));

/// One numeric setting of a settings struct S: its config key, range (the
/// clamping limits) and where it lives in S. The default is the value of a
/// default constructed S. The clamp, load and save functions of every settings
/// struct go through a table of these.
template<class S>
struct NumberField {
    const char* key;
    double min;
    double max;
    double& (*value)(S&);
    const double& (*constValue)(const S&);
};

#define NUMBER_FIELD(S, key, min, max, member)                                     {                                                                                   key, min, max,                                                                          [](S& s) -> double& { return s.member; },                                       [](const S& s) -> const double& { return s.member; }                }

#define PHOSPHOR_FIELD(key, min, max, member)     NUMBER_FIELD(PhosphorSettings, key, min, max, member)

#define OVERVIEW_FIELD(key, min, max, member)     NUMBER_FIELD(PhosphorOverviewSettings, key, min, max, member)

const NumberField<PhosphorSettings> kNumberFields[] = {
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

const NumberField<PhosphorOverviewSettings> kOverviewFields[] = {
    OVERVIEW_FIELD("PhosphorOverviewBrightness",
            kMinOverviewBrightness,
            kMaxOverviewBrightness,
            brightness),
    OVERVIEW_FIELD("PhosphorOverviewLevelExponent",
            kMinOverviewLevelExponent,
            kMaxOverviewLevelExponent,
            levelExponent),
    OVERVIEW_FIELD("PhosphorOverviewDensityExponent",
            kMinOverviewDensityExponent,
            kMaxOverviewDensityExponent,
            densityExponent),
    OVERVIEW_FIELD("PhosphorOverviewGainLow", 0.0, kMaxOverviewGain, gainLow),
    OVERVIEW_FIELD("PhosphorOverviewGainMid", 0.0, kMaxOverviewGain, gainMid),
    OVERVIEW_FIELD("PhosphorOverviewGainHigh", 0.0, kMaxOverviewGain, gainHigh),
};

#undef OVERVIEW_FIELD
#undef PHOSPHOR_FIELD
#undef NUMBER_FIELD

/// Out of range values are pulled into the range; unreadable ones (not a
/// number) give the default.
template<class S>
double clamped(double value, const NumberField<S>& field, double fallback) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, field.min, field.max);
}

template<class S>
ConfigKey keyOf(const NumberField<S>& field) {
    return ConfigKey(kGroup, QString::fromLatin1(field.key));
}

template<class S, size_t N>
void clampNumbers(const NumberField<S> (&fields)[N], S& s) {
    const S defaults;
    for (const NumberField<S>& field : fields) {
        field.value(s) = clamped(field.constValue(s), field, field.constValue(defaults));
    }
}

template<class S, size_t N>
void loadNumbers(const NumberField<S> (&fields)[N], const UserSettingsPointer& pConfig, S& s) {
    const S defaults;
    for (const NumberField<S>& field : fields) {
        const double fallback = field.constValue(defaults);
        field.value(s) = clamped(pConfig->getValue(keyOf(field), fallback), field, fallback);
    }
}

template<class S, size_t N>
void saveNumbers(
        const NumberField<S> (&fields)[N], const UserSettingsPointer& pConfig, const S& s) {
    for (const NumberField<S>& field : fields) {
        pConfig->setValue(keyOf(field), field.constValue(s));
    }
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
    PhosphorSettings s = in;
    s.detail = detailFromInt(static_cast<int>(in.detail));
    s.blend = blendFromInt(static_cast<int>(in.blend));
    clampNumbers(kNumberFields, s);
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
    loadNumbers(kNumberFields, pConfig, s);
    return s;
}

void saveSettings(const UserSettingsPointer& pConfig, const PhosphorSettings& settings) {
    const PhosphorSettings s = clampSettings(settings);
    pConfig->setValue(kDetailCfgKey, static_cast<int>(s.detail));
    pConfig->setValue(kBlendCfgKey, static_cast<int>(s.blend));
    saveNumbers(kNumberFields, pConfig, s);
}

PhosphorOverviewSettings clampOverviewSettings(const PhosphorOverviewSettings& in) {
    PhosphorOverviewSettings s = in;
    clampNumbers(kOverviewFields, s);
    return s;
}

PhosphorOverviewSettings loadOverviewSettings(const UserSettingsPointer& pConfig) {
    PhosphorOverviewSettings s;
    loadNumbers(kOverviewFields, pConfig, s);
    return s;
}

void saveOverviewSettings(
        const UserSettingsPointer& pConfig, const PhosphorOverviewSettings& settings) {
    saveNumbers(kOverviewFields, pConfig, clampOverviewSettings(settings));
}

} // namespace mixxx::phosphor
