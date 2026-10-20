#include "waveform/scrollinglinesettings.h"

#include "preferences/configobject.h"

namespace mixxx {
namespace scrollinglinelayout {

namespace {

const QString kWaveformGroup = QStringLiteral("[Waveform]");

const ConfigKey kEnabledCfgKey(kWaveformGroup, QStringLiteral("scrollingLinesEnabled"));
const ConfigKey kBarOpacityCfgKey(kWaveformGroup, QStringLiteral("scrollingBarLineOpacity"));
const ConfigKey kBeatOpacityCfgKey(kWaveformGroup, QStringLiteral("scrollingBeatLineOpacity"));
const ConfigKey kBarWidthCfgKey(kWaveformGroup, QStringLiteral("scrollingBarLineWidth"));
const ConfigKey kBeatWidthCfgKey(kWaveformGroup, QStringLiteral("scrollingBeatLineWidth"));
// "#rrggbb", or empty for the skin's beat colour.
const ConfigKey kBarColorCfgKey(kWaveformGroup, QStringLiteral("scrollingBarLineColor"));
const ConfigKey kBeatColorCfgKey(kWaveformGroup, QStringLiteral("scrollingBeatLineColor"));
const ConfigKey kStepsEnabledCfgKey(kWaveformGroup, QStringLiteral("scrollingStepLinesEnabled"));
const ConfigKey kStepMinSpacingCfgKey(
        kWaveformGroup, QStringLiteral("scrollingStepLineMinSpacing"));
const ConfigKey kStepOpacityCfgKey(kWaveformGroup, QStringLiteral("scrollingStepLineOpacity"));
const ConfigKey kStepWidthCfgKey(kWaveformGroup, QStringLiteral("scrollingStepLineWidth"));
const ConfigKey kStepColorCfgKey(kWaveformGroup, QStringLiteral("scrollingStepLineColor"));

std::optional<QColor> loadColor(const UserSettingsPointer& pConfig, const ConfigKey& key) {
    const QColor color(pConfig->getValueString(key));
    if (!color.isValid()) {
        return std::nullopt;
    }
    return color;
}

void saveColor(const UserSettingsPointer& pConfig,
        const ConfigKey& key,
        const std::optional<QColor>& color) {
    pConfig->setValue(key, color ? color->name(QColor::HexRgb) : QString());
}

} // namespace

LineSettings loadLineSettings(const UserSettingsPointer& pConfig) {
    LineSettings settings;
    settings.enabled = pConfig->getValue(kEnabledCfgKey, settings.enabled);
    settings.barOpacity = OpacityPercent(
            pConfig->getValue(kBarOpacityCfgKey, settings.barOpacity.percent()));
    settings.beatOpacity = OpacityPercent(
            pConfig->getValue(kBeatOpacityCfgKey, settings.beatOpacity.percent()));
    settings.barWidth = LineWidthPx(pConfig->getValue(kBarWidthCfgKey, settings.barWidth.px()));
    settings.beatWidth = LineWidthPx(pConfig->getValue(kBeatWidthCfgKey, settings.beatWidth.px()));
    settings.barColor = loadColor(pConfig, kBarColorCfgKey);
    settings.beatColor = loadColor(pConfig, kBeatColorCfgKey);
    settings.stepsEnabled = pConfig->getValue(kStepsEnabledCfgKey, settings.stepsEnabled);
    settings.stepMinSpacing = StepSpacingPx(
            pConfig->getValue(kStepMinSpacingCfgKey, settings.stepMinSpacing.px()));
    settings.stepOpacity = OpacityPercent(
            pConfig->getValue(kStepOpacityCfgKey, settings.stepOpacity.percent()));
    settings.stepWidth = LineWidthPx(pConfig->getValue(kStepWidthCfgKey, settings.stepWidth.px()));
    settings.stepColor = loadColor(pConfig, kStepColorCfgKey);
    return settings;
}

void saveLineSettings(const UserSettingsPointer& pConfig, const LineSettings& settings) {
    pConfig->setValue(kEnabledCfgKey, settings.enabled);
    pConfig->setValue(kBarOpacityCfgKey, settings.barOpacity.percent());
    pConfig->setValue(kBeatOpacityCfgKey, settings.beatOpacity.percent());
    pConfig->setValue(kBarWidthCfgKey, settings.barWidth.px());
    pConfig->setValue(kBeatWidthCfgKey, settings.beatWidth.px());
    saveColor(pConfig, kBarColorCfgKey, settings.barColor);
    saveColor(pConfig, kBeatColorCfgKey, settings.beatColor);
    pConfig->setValue(kStepsEnabledCfgKey, settings.stepsEnabled);
    pConfig->setValue(kStepMinSpacingCfgKey, settings.stepMinSpacing.px());
    pConfig->setValue(kStepOpacityCfgKey, settings.stepOpacity.percent());
    pConfig->setValue(kStepWidthCfgKey, settings.stepWidth.px());
    saveColor(pConfig, kStepColorCfgKey, settings.stepColor);
}

} // namespace scrollinglinelayout
} // namespace mixxx
