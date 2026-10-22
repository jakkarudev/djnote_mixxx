#include "library/keymatch/keymatchsettings.h"

#include "preferences/configobject.h"

namespace mixxx {
namespace keymatch {

namespace {

const QString kDJNoteGroup = QStringLiteral("[DJNote]");

const ConfigKey kHighlightCfgKey(kDJNoteGroup, QStringLiteral("KeyMatchingHighlight"));
const ConfigKey kBoldMatchedTextCfgKey(kDJNoteGroup, QStringLiteral("KeyMatchingBold"));

const ConfigKey kColumnsCfgKey(kDJNoteGroup, QStringLiteral("KeyMatchingColumns"));
const ConfigKey kWholeRowCfgKey(kDJNoteGroup, QStringLiteral("KeyMatchingWholeRow"));
const ConfigKey kShiftedKeyCfgKey(kDJNoteGroup, QStringLiteral("KeyMatchingShiftedKey"));

// Columns are stored by name, comma separated; unknown names (such as a
// column that is no longer offered) are skipped.
const QList<ChoosableColumn> kChoosableColumns = {
        {ColumnIdentity::Key, QStringLiteral("key"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Key")},
        {ColumnIdentity::Title, QStringLiteral("title"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Title")},
        {ColumnIdentity::Artist, QStringLiteral("artist"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Artist")},
        {ColumnIdentity::Album, QStringLiteral("album"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Album")},
        {ColumnIdentity::AlbumArtist, QStringLiteral("albumArtist"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Album Artist")},
        {ColumnIdentity::Year, QStringLiteral("year"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Year")},
        {ColumnIdentity::Genre, QStringLiteral("genre"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Genre")},
        {ColumnIdentity::Composer, QStringLiteral("composer"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Composer")},
        {ColumnIdentity::Grouping, QStringLiteral("grouping"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Grouping")},
        {ColumnIdentity::TrackNumber, QStringLiteral("trackNumber"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Track #")},
        {ColumnIdentity::FileType, QStringLiteral("fileType"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Type")},
        {ColumnIdentity::Comment, QStringLiteral("comment"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Comment")},
        {ColumnIdentity::Duration, QStringLiteral("duration"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Duration")},
        {ColumnIdentity::Bitrate, QStringLiteral("bitrate"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Bitrate")},
        {ColumnIdentity::Bpm, QStringLiteral("bpm"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "BPM")},
        {ColumnIdentity::ReplayGain, QStringLiteral("replayGain"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "ReplayGain")},
        {ColumnIdentity::Location, QStringLiteral("location"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Location")},
        {ColumnIdentity::DateAdded, QStringLiteral("dateAdded"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Date Added")},
        {ColumnIdentity::LastPlayed, QStringLiteral("lastPlayed"), QT_TRANSLATE_NOOP("DlgPrefDJNote", "Last Played")},
};

// Stored as names, so the values stay readable and reorder-safe; unknown names
// read as the default.
const QString kHighlightOff = QStringLiteral("off");
const QString kHighlightStrictOnly = QStringLiteral("strictOnly");
const QString kHighlightStrictAndFuzzy = QStringLiteral("strictAndFuzzy");

// Per slot: the name its config key ends with, its default colour and its
// field in Settings.
struct ColorSlotInfo {
    const char* cfgName;
    QRgb defaultRgb;
    QColor Settings::*field;
};

const ColorSlotInfo& slotInfo(ColorSlot slot) {
    static constexpr ColorSlotInfo kInfo[kColorSlotCount] = {
            {"KeyMatchingStrictColor", 0x4ade80, &Settings::strictColor},
            {"KeyMatchingFuzzyColor", 0xfacc15, &Settings::fuzzyColor},
            {"KeyMatchingShiftedStrictColor", 0xc084fc, &Settings::shiftedStrictColor},
            {"KeyMatchingShiftedFuzzyColor", 0xe9d5ff, &Settings::shiftedFuzzyColor},
    };
    return kInfo[static_cast<int>(slot)];
}

ConfigKey colorKey(ColorSlot slot) {
    return ConfigKey(kDJNoteGroup, QString::fromLatin1(slotInfo(slot).cfgName));
}

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

QColor defaultColor(ColorSlot slot) {
    return QColor(slotInfo(slot).defaultRgb);
}

QColor& Settings::color(ColorSlot slot) {
    return this->*slotInfo(slot).field;
}

const QColor& Settings::color(ColorSlot slot) const {
    return this->*slotInfo(slot).field;
}

void Settings::resetColors() {
    for (const auto slot : kColorSlots) {
        color(slot) = defaultColor(slot);
    }
}

const QList<ChoosableColumn>& choosableColumns() {
    return kChoosableColumns;
}

Settings loadSettings(const UserSettingsPointer& pConfig) {
    Settings settings;
    settings.highlight = highlightFromString(
            pConfig->getValueString(kHighlightCfgKey), settings.highlight);
    if (pConfig->exists(kColumnsCfgKey)) {
        settings.columns.clear();
        const QStringList names = pConfig->getValueString(kColumnsCfgKey)
                                          .split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const auto& column : kChoosableColumns) {
            if (names.contains(column.name)) {
                settings.columns.insert(column.identity);
            }
        }
    }
    settings.wholeRow = pConfig->getValue(kWholeRowCfgKey, settings.wholeRow);
    const QString bold = pConfig->getValueString(kBoldMatchedTextCfgKey);
    if (bold == QLatin1String("true")) {
        settings.boldMatchedText = true;
    } else if (bold == QLatin1String("false")) {
        settings.boldMatchedText = false;
    }
    settings.matchShiftedKey = pConfig->getValue<bool>(kShiftedKeyCfgKey, settings.matchShiftedKey);
    // Colours are stored as #rrggbb; anything else reads as the default.
    for (const auto slot : kColorSlots) {
        const QString text = pConfig->getValueString(colorKey(slot));
        const QColor color(text);
        if (text.startsWith(QLatin1Char('#')) && color.isValid()) {
            settings.color(slot) = color;
        }
    }
    return settings;
}

void saveSettings(const UserSettingsPointer& pConfig, const Settings& settings) {
    pConfig->setValue(kHighlightCfgKey, highlightToString(settings.highlight));
    QStringList names;
    for (const auto& column : kChoosableColumns) {
        if (settings.columns.count(column.identity) > 0) {
            names.append(column.name);
        }
    }
    // "-" keeps "no column" distinct from a missing (defaulted) key.
    pConfig->setValue(kColumnsCfgKey,
            names.isEmpty() ? QStringLiteral("-") : names.join(QLatin1Char(',')));
    pConfig->setValue(kWholeRowCfgKey, settings.wholeRow);
    pConfig->setValue(kBoldMatchedTextCfgKey,
            settings.boldMatchedText ? QStringLiteral("true") : QStringLiteral("false"));
    pConfig->setValue(kShiftedKeyCfgKey, settings.matchShiftedKey);
    for (const auto slot : kColorSlots) {
        pConfig->setValue(colorKey(slot), settings.color(slot).name(QColor::HexRgb));
    }
}

} // namespace keymatch
} // namespace mixxx
