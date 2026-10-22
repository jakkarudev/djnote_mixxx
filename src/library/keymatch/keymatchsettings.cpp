#include "library/keymatch/keymatchsettings.h"

#include "preferences/configobject.h"

namespace mixxx {
namespace keymatch {

namespace {

const QString kDJNoteGroup = QStringLiteral("[DJNote]");

const ConfigKey kHighlightCfgKey(kDJNoteGroup, QStringLiteral("KeyMatchingHighlight"));
const ConfigKey kBoldMatchedTextCfgKey(kDJNoteGroup, QStringLiteral("KeyMatchingBold"));

// Stored as names, so the values stay readable and reorder-safe; unknown names
// read as the default.
const QString kHighlightOff = QStringLiteral("off");
const QString kHighlightStrictOnly = QStringLiteral("strictOnly");
const QString kHighlightStrictAndFuzzy = QStringLiteral("strictAndFuzzy");

Highlight highlightFromString(const QString& name, Highlight fallback) {
    if (name == kHighlightOff) {
        return Highlight::Off;
    }
    if (name == kHighlightStrictOnly) {
        return Highlight::StrictOnly;
    }
    if (name == kHighlightStrictAndFuzzy) {
        return Highlight::StrictAndFuzzy;
    }
    return fallback;
}

QString highlightToString(Highlight highlight) {
    switch (highlight) {
    case Highlight::Off:
        return kHighlightOff;
    case Highlight::StrictOnly:
        return kHighlightStrictOnly;
    case Highlight::StrictAndFuzzy:
        break;
    }
    return kHighlightStrictAndFuzzy;
}

} // namespace

Settings loadSettings(const UserSettingsPointer& pConfig) {
    Settings settings;
    settings.highlight = highlightFromString(
            pConfig->getValueString(kHighlightCfgKey), settings.highlight);
    const QString bold = pConfig->getValueString(kBoldMatchedTextCfgKey);
    if (bold == QLatin1String("true")) {
        settings.boldMatchedText = true;
    } else if (bold == QLatin1String("false")) {
        settings.boldMatchedText = false;
    }
    return settings;
}

void saveSettings(const UserSettingsPointer& pConfig, const Settings& settings) {
    pConfig->setValue(kHighlightCfgKey, highlightToString(settings.highlight));
    pConfig->setValue(kBoldMatchedTextCfgKey,
            settings.boldMatchedText ? QStringLiteral("true") : QStringLiteral("false"));
}

} // namespace keymatch
} // namespace mixxx
