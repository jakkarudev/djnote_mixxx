#include "widget/knobreadoutsettings.h"

#include "preferences/configobject.h"

namespace mixxx {
namespace knobreadout {

namespace {

const QString kDJNoteGroup = QStringLiteral("[DJNote]");

const ConfigKey kEnabledCfgKey(kDJNoteGroup, QStringLiteral("knobReadoutEnabled"));

} // namespace

Settings loadSettings(const UserSettingsPointer& pConfig) {
    Settings settings;
    settings.enabled = pConfig->getValue(kEnabledCfgKey, settings.enabled);
    return settings;
}

void saveSettings(const UserSettingsPointer& pConfig, const Settings& settings) {
    pConfig->setValue(kEnabledCfgKey, settings.enabled);
}

} // namespace knobreadout
} // namespace mixxx
