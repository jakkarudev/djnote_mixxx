#include "waveform/overviewsettings.h"

#include "preferences/configobject.h"

namespace mixxx {
namespace overviewlayout {

namespace {

const QString kWaveformGroup = QStringLiteral("[Waveform]");

const ConfigKey kLinesModeCfgKey(kWaveformGroup, QStringLiteral("overviewLinesMode"));
const ConfigKey kPhraseBarsCfgKey(kWaveformGroup, QStringLiteral("overviewPhraseBars"));
const ConfigKey kLineOpacityCfgKey(kWaveformGroup, QStringLiteral("overviewLineOpacity"));

const ConfigKey kHoverShowCfgKey(kWaveformGroup, QStringLiteral("overviewHoverShow"));
const ConfigKey kHoverDisplayCfgKey(kWaveformGroup, QStringLiteral("overviewHoverDisplay"));
const ConfigKey kHoverModeCfgKey(kWaveformGroup, QStringLiteral("overviewHoverMode"));
const ConfigKey kHoverUnitCfgKey(kWaveformGroup, QStringLiteral("overviewHoverUnit"));
const ConfigKey kHoverPrecisionCfgKey(kWaveformGroup, QStringLiteral("overviewHoverPrecision"));
const ConfigKey kHoverLabelPositionCfgKey(
        kWaveformGroup, QStringLiteral("overviewHoverLabelPosition"));
const ConfigKey kHoverOverCueCfgKey(kWaveformGroup, QStringLiteral("overviewHoverOverCue"));

// Reads an enum stored as int. Anything outside first..last (the enum's
// consecutive values) yields defaultValue.
template<typename Enum>
Enum readEnum(const UserSettingsPointer& pConfig,
        const ConfigKey& key,
        Enum defaultValue,
        Enum first,
        Enum last) {
    const int value = pConfig->getValue(key, static_cast<int>(defaultValue));
    if (value < static_cast<int>(first) || value > static_cast<int>(last)) {
        return defaultValue;
    }
    return static_cast<Enum>(value);
}

template<typename Enum>
void writeEnum(const UserSettingsPointer& pConfig, const ConfigKey& key, Enum value) {
    pConfig->setValue(key, static_cast<int>(value));
}

} // namespace

LineSettings loadLineSettings(const UserSettingsPointer& pConfig) {
    LineSettings settings;
    settings.mode = readEnum(pConfig,
            kLinesModeCfgKey,
            settings.mode,
            LinesMode::Off,
            LinesMode::BarsAndPhrases);
    settings.phraseBars = validPhraseBarsOrDefault(
            pConfig->getValue(kPhraseBarsCfgKey, settings.phraseBars));
    settings.opacity = OpacityPercent(
            pConfig->getValue(kLineOpacityCfgKey, settings.opacity.percent()));
    return settings;
}

void saveLineSettings(const UserSettingsPointer& pConfig, const LineSettings& settings) {
    writeEnum(pConfig, kLinesModeCfgKey, settings.mode);
    pConfig->setValue(kPhraseBarsCfgKey, settings.phraseBars);
    pConfig->setValue(kLineOpacityCfgKey, settings.opacity.percent());
}

HoverReadoutSettings loadHoverReadoutSettings(const UserSettingsPointer& pConfig) {
    HoverReadoutSettings settings;
    settings.showHoverReadout = pConfig->getValue(kHoverShowCfgKey, settings.showHoverReadout);
    settings.display = readEnum(pConfig,
            kHoverDisplayCfgKey,
            settings.display,
            HoverDisplay::LineAndLabel,
            HoverDisplay::LabelOnly);
    settings.hoverMode = readEnum(pConfig,
            kHoverModeCfgKey,
            settings.hoverMode,
            HoverMode::ExtendTimeRuler,
            HoverMode::SeparateHoverLine);
    settings.unit = readEnum(pConfig,
            kHoverUnitCfgKey,
            settings.unit,
            BeatDistanceUnit::Bars,
            BeatDistanceUnit::Beats);
    settings.precision = readEnum(pConfig,
            kHoverPrecisionCfgKey,
            settings.precision,
            BeatDistancePrecision::Whole,
            BeatDistancePrecision::Fractional);
    settings.position = readEnum(pConfig,
            kHoverLabelPositionCfgKey,
            settings.position,
            HoverReadoutPosition::Auto,
            HoverReadoutPosition::AlwaysLeft);
    settings.overCue = readEnum(pConfig,
            kHoverOverCueCfgKey,
            settings.overCue,
            HoverOverCue::JoinCueTimes,
            HoverOverCue::MoveHoverReadout);
    return settings;
}

void saveHoverReadoutSettings(
        const UserSettingsPointer& pConfig, const HoverReadoutSettings& settings) {
    pConfig->setValue(kHoverShowCfgKey, settings.showHoverReadout);
    writeEnum(pConfig, kHoverDisplayCfgKey, settings.display);
    writeEnum(pConfig, kHoverModeCfgKey, settings.hoverMode);
    writeEnum(pConfig, kHoverUnitCfgKey, settings.unit);
    writeEnum(pConfig, kHoverPrecisionCfgKey, settings.precision);
    writeEnum(pConfig, kHoverLabelPositionCfgKey, settings.position);
    writeEnum(pConfig, kHoverOverCueCfgKey, settings.overCue);
}

} // namespace overviewlayout
} // namespace mixxx
