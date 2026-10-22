#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <set>

#include "preferences/usersettings.h"

// The key matching settings (see GLOSSARY.md, "Key matching"), shown on the
// DJNote preferences page and stored under [DJNote] in `KeyMatching...` keys.
namespace mixxx {
namespace keymatch {

// Which tiers are highlighted.
enum class Highlight {
    Off,
    StrictOnly,
    StrictAndFuzzy,
};

// A column's identity, shared by all track list types: each model maps its own
// columns to these, so one choice applies to every list. Only text columns have
// an identity; Cover, Color, Rating, Played, Preview and the like are
// NotColoured and never offered.
enum class ColumnIdentity {
    Key,
    Title,
    Artist,
    Album,
    AlbumArtist,
    Year,
    Genre,
    Composer,
    Grouping,
    TrackNumber,
    FileType,
    Comment,
    Duration,
    Bitrate,
    Bpm,
    ReplayGain,
    Location,
    DateAdded,
    LastPlayed,
    NotColoured,
};

// The identities the DJ can choose from, Key first.
QList<ColumnIdentity> choosableColumns();

// The four settable match colours, one per tier.
enum class ColourSlot {
    Strict,
    Fuzzy,
    ShiftedStrict,
    ShiftedFuzzy,
};

QColor defaultColor(ColourSlot slot);

// Add a field here, its key in keymatchsettings.cpp and its control on
// DlgPrefDJNote; the key matcher reloads the whole struct when the page changes
// something, so a new setting applies live without more plumbing.
struct Settings {
    Highlight highlight = Highlight::StrictAndFuzzy;
    // Which columns get the match colour; wholeRow means every text column.
    std::set<ColumnIdentity> columns = {ColumnIdentity::Key};
    bool wholeRow = false;

    bool colours(ColumnIdentity column) const {
        return column != ColumnIdentity::NotColoured &&
                (wholeRow || columns.count(column) > 0);
    }
    // Coloured cells are bold.
    bool boldMatchedText = true;
    // Also match the shifted key of the reference deck (purple).
    bool matchShiftedKey = true;
    // The tier colours.
    QColor strictColor = defaultColor(ColourSlot::Strict);
    QColor fuzzyColor = defaultColor(ColourSlot::Fuzzy);
    QColor shiftedStrictColor = defaultColor(ColourSlot::ShiftedStrict);
    QColor shiftedFuzzyColor = defaultColor(ColourSlot::ShiftedFuzzy);

    QColor& colour(ColourSlot slot);
    const QColor& colour(ColourSlot slot) const;
    void resetColours();
};

// Missing keys fall back to the defaults above.
Settings loadSettings(const UserSettingsPointer& pConfig);
void saveSettings(const UserSettingsPointer& pConfig, const Settings& settings);

} // namespace keymatch
} // namespace mixxx
