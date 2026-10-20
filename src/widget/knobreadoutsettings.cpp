#include "widget/knobreadoutsettings.h"

#include "preferences/configobject.h"

namespace mixxx {
namespace knobreadout {

namespace {

const QString kDJNoteGroup = QStringLiteral("[DJNote]");

const ConfigKey kEnabledCfgKey(kDJNoteGroup, QStringLiteral("knobReadoutEnabled"));
const ConfigKey kModeCfgKey(kDJNoteGroup, QStringLiteral("knobReadoutMode"));
const ConfigKey kStyleCfgKey(kDJNoteGroup, QStringLiteral("knobReadoutStyle"));

// Mode and style are stored as names, so the values stay readable and
// reorder-safe; unknown names read as the default.
const QString kModeSimple = QStringLiteral("simple");
const QString kModePrecise = QStringLiteral("precise");
const QString kStyleUnderKnob = QStringLiteral("underKnob");
const QString kStyleKnobFace = QStringLiteral("knobFace");

Mode modeFromString(const QString& name, Mode fallback) {
    if (name == kModeSimple) {
        return Mode::Simple;
    }
    if (name == kModePrecise) {
        return Mode::Precise;
    }
    return fallback;
}

QString modeToString(Mode mode) {
    return mode == Mode::Precise ? kModePrecise : kModeSimple;
}

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
    settings.mode = modeFromString(pConfig->getValueString(kModeCfgKey), settings.mode);
    settings.style = styleFromString(pConfig->getValueString(kStyleCfgKey), settings.style);
    return settings;
}

void saveSettings(const UserSettingsPointer& pConfig, const Settings& settings) {
    pConfig->setValue(kEnabledCfgKey, settings.enabled);
    pConfig->setValue(kModeCfgKey, modeToString(settings.mode));
    pConfig->setValue(kStyleCfgKey, styleToString(settings.style));
}

} // namespace knobreadout
} // namespace mixxx
