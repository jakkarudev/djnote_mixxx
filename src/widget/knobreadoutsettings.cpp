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
    settings.mode = pConfig->getValueString(kModeCfgKey) == kPreciseModeName
            ? Mode::Precise
            : Mode::Simple;
    settings.style = styleFromString(pConfig->getValueString(kStyleCfgKey), settings.style);
    return settings;
}

void saveSettings(const UserSettingsPointer& pConfig, const Settings& settings) {
    pConfig->setValue(kEnabledCfgKey, settings.enabled);
    pConfig->setValue(kModeCfgKey,
            settings.mode == Mode::Precise ? kPreciseModeName : kSimpleModeName);
    pConfig->setValue(kStyleCfgKey, styleToString(settings.style));
}

} // namespace knobreadout
} // namespace mixxx
