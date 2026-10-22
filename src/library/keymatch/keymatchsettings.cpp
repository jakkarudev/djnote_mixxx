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

// Columns are stored by name, comma separated; unknown names are skipped.
const QList<QPair<ColumnIdentity, QString>>& columnNames() {
    static const QList<QPair<ColumnIdentity, QString>> names = {
            {ColumnIdentity::Key, QStringLiteral("key")},
            {ColumnIdentity::Title, QStringLiteral("title")},
            {ColumnIdentity::Artist, QStringLiteral("artist")},
            {ColumnIdentity::Album, QStringLiteral("album")},
            {ColumnIdentity::AlbumArtist, QStringLiteral("albumArtist")},
            {ColumnIdentity::Year, QStringLiteral("year")},
            {ColumnIdentity::Genre, QStringLiteral("genre")},
            {ColumnIdentity::Composer, QStringLiteral("composer")},
            {ColumnIdentity::Grouping, QStringLiteral("grouping")},
            {ColumnIdentity::TrackNumber, QStringLiteral("trackNumber")},
            {ColumnIdentity::FileType, QStringLiteral("fileType")},
            {ColumnIdentity::Comment, QStringLiteral("comment")},
            {ColumnIdentity::Duration, QStringLiteral("duration")},
            {ColumnIdentity::Bitrate, QStringLiteral("bitrate")},
            {ColumnIdentity::Bpm, QStringLiteral("bpm")},
            {ColumnIdentity::ReplayGain, QStringLiteral("replayGain")},
            {ColumnIdentity::Location, QStringLiteral("location")},
            {ColumnIdentity::DateAdded, QStringLiteral("dateAdded")},
            {ColumnIdentity::LastPlayed, QStringLiteral("lastPlayed")},
    };
    return names;
}

// Stored as names, so the values stay readable and reorder-safe; unknown names
// read as the default.
const QString kHighlightOff = QStringLiteral("off");
const QString kHighlightStrictOnly = QStringLiteral("strictOnly");
const QString kHighlightStrictAndFuzzy = QStringLiteral("strictAndFuzzy");

constexpr ColourSlot kColourSlots[] = {ColourSlot::Strict,
        ColourSlot::Fuzzy,
        ColourSlot::ShiftedStrict,
        ColourSlot::ShiftedFuzzy};

ConfigKey colourKey(ColourSlot slot) {
    switch (slot) {
    case ColourSlot::Strict:
        return ConfigKey(kDJNoteGroup, QStringLiteral("KeyMatchingStrictColor"));
    case ColourSlot::Fuzzy:
        return ConfigKey(kDJNoteGroup, QStringLiteral("KeyMatchingFuzzyColor"));
    case ColourSlot::ShiftedStrict:
        return ConfigKey(kDJNoteGroup, QStringLiteral("KeyMatchingShiftedStrictColor"));
    case ColourSlot::ShiftedFuzzy:
        break;
    }
    return ConfigKey(kDJNoteGroup, QStringLiteral("KeyMatchingShiftedFuzzyColor"));
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

QColor defaultColor(ColourSlot slot) {
    switch (slot) {
    case ColourSlot::Strict:
        return QColor(0x4a, 0xde, 0x80);
    case ColourSlot::Fuzzy:
        return QColor(0xfa, 0xcc, 0x15);
    case ColourSlot::ShiftedStrict:
        return QColor(0xc0, 0x84, 0xfc);
    case ColourSlot::ShiftedFuzzy:
        break;
    }
    return QColor(0xe9, 0xd5, 0xff);
}

QColor& Settings::colour(ColourSlot slot) {
    switch (slot) {
    case ColourSlot::Strict:
        return strictColor;
    case ColourSlot::Fuzzy:
        return fuzzyColor;
    case ColourSlot::ShiftedStrict:
        return shiftedStrictColor;
    case ColourSlot::ShiftedFuzzy:
        break;
    }
    return shiftedFuzzyColor;
}

const QColor& Settings::colour(ColourSlot slot) const {
    return const_cast<Settings*>(this)->colour(slot);
}

void Settings::resetColours() {
    for (const auto slot : kColourSlots) {
        colour(slot) = defaultColor(slot);
    }
}

QList<ColumnIdentity> choosableColumns() {
    QList<ColumnIdentity> columns;
    for (const auto& entry : columnNames()) {
        columns.append(entry.first);
    }
    return columns;
}

Settings loadSettings(const UserSettingsPointer& pConfig) {
    Settings settings;
    settings.highlight = highlightFromString(
            pConfig->getValueString(kHighlightCfgKey), settings.highlight);
    if (pConfig->exists(kColumnsCfgKey)) {
        settings.columns.clear();
        const QStringList names = pConfig->getValueString(kColumnsCfgKey)
                                          .split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const auto& entry : columnNames()) {
            if (names.contains(entry.second)) {
                settings.columns.insert(entry.first);
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
    for (const auto slot : kColourSlots) {
        const QString text = pConfig->getValueString(colourKey(slot));
        const QColor color(text);
        if (text.startsWith(QLatin1Char('#')) && color.isValid()) {
            settings.colour(slot) = color;
        }
    }
    return settings;
}

void saveSettings(const UserSettingsPointer& pConfig, const Settings& settings) {
    pConfig->setValue(kHighlightCfgKey, highlightToString(settings.highlight));
    QStringList names;
    for (const auto& entry : columnNames()) {
        if (settings.columns.count(entry.first) > 0) {
            names.append(entry.second);
        }
    }
    // "-" keeps "no column" distinct from a missing (defaulted) key.
    pConfig->setValue(kColumnsCfgKey,
            names.isEmpty() ? QStringLiteral("-") : names.join(QLatin1Char(',')));
    pConfig->setValue(kWholeRowCfgKey, settings.wholeRow);
    pConfig->setValue(kBoldMatchedTextCfgKey,
            settings.boldMatchedText ? QStringLiteral("true") : QStringLiteral("false"));
    pConfig->setValue(kShiftedKeyCfgKey, settings.matchShiftedKey);
    for (const auto slot : kColourSlots) {
        pConfig->setValue(colourKey(slot), settings.colour(slot).name(QColor::HexRgb));
    }
}

} // namespace keymatch
} // namespace mixxx
