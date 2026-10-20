#include "waveform/scrollinglinesettings.h"

#include "preferences/configobject.h"

namespace mixxx {
namespace scrollinglinelayout {

namespace {

const QString kWaveformGroup = QStringLiteral("[Waveform]");

const ConfigKey kEnabledCfgKey(kWaveformGroup, QStringLiteral("scrollingLinesEnabled"));
const ConfigKey kBarOpacityCfgKey(kWaveformGroup, QStringLiteral("scrollingBarLineOpacity"));
const ConfigKey kBeatOpacityCfgKey(kWaveformGroup, QStringLiteral("scrollingBeatLineOpacity"));

} // namespace

LineSettings loadLineSettings(const UserSettingsPointer& pConfig) {
    LineSettings settings;
    settings.enabled = pConfig->getValue(kEnabledCfgKey, settings.enabled);
    settings.barOpacity = OpacityPercent(
            pConfig->getValue(kBarOpacityCfgKey, settings.barOpacity.percent()));
    settings.beatOpacity = OpacityPercent(
            pConfig->getValue(kBeatOpacityCfgKey, settings.beatOpacity.percent()));
    return settings;
}

void saveLineSettings(const UserSettingsPointer& pConfig, const LineSettings& settings) {
    pConfig->setValue(kEnabledCfgKey, settings.enabled);
    pConfig->setValue(kBarOpacityCfgKey, settings.barOpacity.percent());
    pConfig->setValue(kBeatOpacityCfgKey, settings.beatOpacity.percent());
}

} // namespace scrollinglinelayout
} // namespace mixxx
