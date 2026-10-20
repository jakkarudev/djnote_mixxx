#include "widget/knobreadoutsettings.h"

#include "preferences/configobject.h"

namespace mixxx {
namespace knobreadout {

namespace {

const QString kDJNoteGroup = QStringLiteral("[DJNote]");

const ConfigKey kEnabledCfgKey(kDJNoteGroup, QStringLiteral("knobReadoutEnabled"));
// Stored by name ("simple" / "precise"); unknown values read as Simple.
const ConfigKey kModeCfgKey(kDJNoteGroup, QStringLiteral("knobReadoutMode"));

const QString kSimpleModeName = QStringLiteral("simple");
const QString kPreciseModeName = QStringLiteral("precise");

} // namespace

Settings loadSettings(const UserSettingsPointer& pConfig) {
    Settings settings;
    settings.enabled = pConfig->getValue(kEnabledCfgKey, settings.enabled);
    settings.mode = pConfig->getValueString(kModeCfgKey) == kPreciseModeName
            ? Mode::Precise
            : Mode::Simple;
    return settings;
}

void saveSettings(const UserSettingsPointer& pConfig, const Settings& settings) {
    pConfig->setValue(kEnabledCfgKey, settings.enabled);
    pConfig->setValue(kModeCfgKey,
            settings.mode == Mode::Precise ? kPreciseModeName : kSimpleModeName);
}

} // namespace knobreadout
} // namespace mixxx
