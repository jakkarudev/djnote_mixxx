#include "widget/knobreadoutsettings.h"

#include "preferences/configobject.h"

namespace mixxx {
namespace knobreadout {

namespace {

const QString kDJNoteGroup = QStringLiteral("[DJNote]");

const ConfigKey kEnabledCfgKey(kDJNoteGroup, QStringLiteral("knobReadoutEnabled"));
const ConfigKey kStyleCfgKey(kDJNoteGroup, QStringLiteral("knobReadoutStyle"));

// Stored as names, so the values stay readable and reorder-safe.
const QString kStyleUnderKnob = QStringLiteral("underKnob");
const QString kStyleKnobFace = QStringLiteral("knobFace");

Style styleFromString(const QString& name, Style fallback) {
    if (name == kStyleUnderKnob) {
        return Style::UnderKnob;
    }
    if (name == kStyleKnobFace) {
        return Style::KnobFace;
    }
    return fallback;
}

QString styleToString(Style style) {
    return style == Style::KnobFace ? kStyleKnobFace : kStyleUnderKnob;
}

} // namespace

Settings loadSettings(const UserSettingsPointer& pConfig) {
    Settings settings;
    settings.enabled = pConfig->getValue(kEnabledCfgKey, settings.enabled);
    settings.style = styleFromString(pConfig->getValueString(kStyleCfgKey), settings.style);
    return settings;
}

void saveSettings(const UserSettingsPointer& pConfig, const Settings& settings) {
    pConfig->setValue(kEnabledCfgKey, settings.enabled);
    pConfig->setValue(kStyleCfgKey, styleToString(settings.style));
}

} // namespace knobreadout
} // namespace mixxx
