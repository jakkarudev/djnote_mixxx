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
            {ColumnIdentity::TimesPlayed, QStringLiteral("timesPlayed")},
            {ColumnIdentity::DateAdded, QStringLiteral("dateAdded")},
            {ColumnIdentity::LastPlayed, QStringLiteral("lastPlayed")},
    };
    return names;
}
const ConfigKey kShiftedKeyCfgKey(kDJNoteGroup, QStringLiteral("KeyMatchingShiftedKey"));

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
}

} // namespace keymatch
} // namespace mixxx
