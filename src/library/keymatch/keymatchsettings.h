#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <iterator>
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

// An identity the DJ can choose from, with the name it is stored under and its
// label (a DlgPrefDJNote translation source string) in the checklist.
struct ChoosableColumn {
    ColumnIdentity identity;
    QString name;
    const char* label;
};

// The identities the DJ can choose from, Key first.
const QList<ChoosableColumn>& choosableColumns();

// The settable match colours, one per tier.
enum class ColorSlot {
    Strict,
    Fuzzy,
    ShiftedStrict,
    ShiftedFuzzy,
};

// Every slot, in order; a slot's number is static_cast<int>(slot).
inline constexpr ColorSlot kColorSlots[] = {ColorSlot::Strict,
        ColorSlot::Fuzzy,
        ColorSlot::ShiftedStrict,
        ColorSlot::ShiftedFuzzy};
inline constexpr int kColorSlotCount = static_cast<int>(std::size(kColorSlots));

QColor defaultColor(ColorSlot slot);

// Add a field here, its key in keymatchsettings.cpp and its control on
// DlgPrefDJNote; the key matcher reloads the whole struct when the page changes
// something, so a new setting applies live without more plumbing.
struct Settings {
    Highlight highlight = Highlight::StrictAndFuzzy;
    // Which columns get the match colour; wholeRow means every text column.
    std::set<ColumnIdentity> columns = {ColumnIdentity::Key};
    bool wholeRow = false;

    // Coloured cells are bold.
    bool boldMatchedText = true;
    // Also match the shifted key of the reference deck (purple).
    bool matchShiftedKey = true;
    // The tier colours.
    QColor strictColor = defaultColor(ColorSlot::Strict);
    QColor fuzzyColor = defaultColor(ColorSlot::Fuzzy);
    QColor shiftedStrictColor = defaultColor(ColorSlot::ShiftedStrict);
    QColor shiftedFuzzyColor = defaultColor(ColorSlot::ShiftedFuzzy);

    QColor& color(ColorSlot slot);
    const QColor& color(ColorSlot slot) const;
    void resetColors();

    // Whether cells of this column get the match colour.
    bool isColumnColored(ColumnIdentity column) const {
        return column != ColumnIdentity::NotColoured &&
                (wholeRow || columns.count(column) > 0);
    }
};

// Missing keys fall back to the defaults above.
Settings loadSettings(const UserSettingsPointer& pConfig);
void saveSettings(const UserSettingsPointer& pConfig, const Settings& settings);

} // namespace keymatch
} // namespace mixxx
