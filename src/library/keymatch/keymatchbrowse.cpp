#include "library/keymatch/keymatchbrowse.h"

#include "library/browse/browsetablemodel.h"
#include "track/keyutils.h"

namespace mixxx {
namespace keymatch {

ColumnIdentity browseColumnIdentity(int column) {
    switch (column) {
    case COLUMN_ARTIST:
        return ColumnIdentity::Artist;
    case COLUMN_TITLE:
        return ColumnIdentity::Title;
    case COLUMN_ALBUM:
        return ColumnIdentity::Album;
    case COLUMN_TRACK_NUMBER:
        return ColumnIdentity::TrackNumber;
    case COLUMN_YEAR:
        return ColumnIdentity::Year;
    case COLUMN_GENRE:
        return ColumnIdentity::Genre;
    case COLUMN_COMPOSER:
        return ColumnIdentity::Composer;
    case COLUMN_COMMENT:
        return ColumnIdentity::Comment;
    case COLUMN_DURATION:
        return ColumnIdentity::Duration;
    case COLUMN_BPM:
        return ColumnIdentity::Bpm;
    case COLUMN_KEY:
        return ColumnIdentity::Key;
    case COLUMN_TYPE:
        return ColumnIdentity::FileType;
    case COLUMN_BITRATE:
        return ColumnIdentity::Bitrate;
    case COLUMN_NATIVELOCATION:
        return ColumnIdentity::Location;
    case COLUMN_ALBUMARTIST:
        return ColumnIdentity::AlbumArtist;
    case COLUMN_GROUPING:
        return ColumnIdentity::Grouping;
    case COLUMN_REPLAYGAIN:
        return ColumnIdentity::ReplayGain;
    default:
        // Preview, Filename, file times and Cover Art.
        return ColumnIdentity::NotColoured;
    }
}

QVariant browseCellData(int column, const QString& keyText, int role) {
    CellInput input;
    input.column = browseColumnIdentity(column);
    if (input.column == ColumnIdentity::NotColoured) {
        return QVariant();
    }
    input.key = KeyUtils::guessKeyFromText(keyText);
    return roleData(cellStyle(input), role);
}

} // namespace keymatch
} // namespace mixxx
