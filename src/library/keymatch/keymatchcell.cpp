#include "library/keymatch/keymatchcell.h"

#include <QFont>

#include "library/keymatch/keymatcher.h"
#include "track/keyutils.h"

namespace mixxx {
namespace keymatch {

namespace {

// The default tier colours. Settable colours come with their own settings.
const QColor kStrictColor(0x4a, 0xde, 0x80);
const QColor kFuzzyColor(0xfa, 0xcc, 0x15);
const QColor kShiftedStrictColor(0xc0, 0x84, 0xfc);
const QColor kShiftedFuzzyColor(0xe9, 0xd5, 0xff);

std::optional<QColor> colorOf(KeyMatchTier tier) {
    switch (tier) {
    case KeyMatchTier::Strict:
        return kStrictColor;
    case KeyMatchTier::Fuzzy:
        return kFuzzyColor;
    case KeyMatchTier::ShiftedStrict:
        return kShiftedStrictColor;
    case KeyMatchTier::ShiftedFuzzy:
        return kShiftedFuzzyColor;
    case KeyMatchTier::None:
        break;
    }
    return std::nullopt;
}

mixxx::track::io::key::ChromaticKey rowKey(
        const std::function<QVariant(ColumnCache::Column)>& siblingValue) {
    const QVariant keyId = siblingValue(ColumnCache::COLUMN_LIBRARYTABLE_KEY_ID);
    bool ok = false;
    const int id = keyId.toInt(&ok);
    if (ok) {
        const auto key = KeyUtils::keyFromNumericValue(id);
        if (key != mixxx::track::io::key::INVALID) {
            return key;
        }
    }
    // Library lists store the key id; lists without one only have the key's
    // text, in any notation.
    const QVariant keyText = siblingValue(ColumnCache::COLUMN_LIBRARYTABLE_KEY);
    return keyText.isNull() ? mixxx::track::io::key::INVALID
                            : KeyUtils::guessKeyFromText(keyText.toString());
}

} // namespace

ColumnIdentity columnIdentity(ColumnCache::Column column) {
    switch (column) {
    case ColumnCache::COLUMN_LIBRARYTABLE_KEY:
        return ColumnIdentity::Key;
    case ColumnCache::COLUMN_LIBRARYTABLE_TITLE:
        return ColumnIdentity::Title;
    case ColumnCache::COLUMN_LIBRARYTABLE_ARTIST:
        return ColumnIdentity::Artist;
    case ColumnCache::COLUMN_LIBRARYTABLE_ALBUM:
        return ColumnIdentity::Album;
    case ColumnCache::COLUMN_LIBRARYTABLE_ALBUMARTIST:
        return ColumnIdentity::AlbumArtist;
    case ColumnCache::COLUMN_LIBRARYTABLE_YEAR:
        return ColumnIdentity::Year;
    case ColumnCache::COLUMN_LIBRARYTABLE_GENRE:
        return ColumnIdentity::Genre;
    case ColumnCache::COLUMN_LIBRARYTABLE_COMPOSER:
        return ColumnIdentity::Composer;
    case ColumnCache::COLUMN_LIBRARYTABLE_GROUPING:
        return ColumnIdentity::Grouping;
    case ColumnCache::COLUMN_LIBRARYTABLE_TRACKNUMBER:
        return ColumnIdentity::TrackNumber;
    case ColumnCache::COLUMN_LIBRARYTABLE_FILETYPE:
        return ColumnIdentity::FileType;
    case ColumnCache::COLUMN_LIBRARYTABLE_COMMENT:
        return ColumnIdentity::Comment;
    case ColumnCache::COLUMN_LIBRARYTABLE_DURATION:
        return ColumnIdentity::Duration;
    case ColumnCache::COLUMN_LIBRARYTABLE_BITRATE:
        return ColumnIdentity::Bitrate;
    case ColumnCache::COLUMN_LIBRARYTABLE_BPM:
        return ColumnIdentity::Bpm;
    case ColumnCache::COLUMN_LIBRARYTABLE_REPLAYGAIN:
        return ColumnIdentity::ReplayGain;
    case ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION:
        return ColumnIdentity::Location;
    case ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED:
        return ColumnIdentity::TimesPlayed;
    case ColumnCache::COLUMN_LIBRARYTABLE_DATETIMEADDED:
        return ColumnIdentity::DateAdded;
    case ColumnCache::COLUMN_LIBRARYTABLE_LAST_PLAYED_AT:
        return ColumnIdentity::LastPlayed;
    default:
        return ColumnIdentity::NotColoured;
    }
}

CellStyle cellStyle(const CellInput& input) {
    const KeyMatcher* pMatcher = KeyMatcher::instance();
    // A missing track keeps Mixxx's missing colour and is never highlighted.
    if (!pMatcher || input.missing || !pMatcher->settings().colours(input.column)) {
        return {};
    }
    CellStyle style;
    const std::optional<QColor> color = colorOf(pMatcher->match(input.key));
    style.color = color;
    // A played match is mixed 50/50 with the played colour.
    if (style.color && input.played && input.playedColor) {
        const QColor& a = *style.color;
        const QColor& b = *input.playedColor;
        style.color = QColor((a.red() + b.red() + 1) / 2,
                (a.green() + b.green() + 1) / 2,
                (a.blue() + b.blue() + 1) / 2);
    }
    // Every coloured cell is bold, played or not.
    style.bold = color.has_value() && pMatcher->settings().boldMatchedText;
    return style;
}

QVariant roleData(const CellStyle& style, int role) {
    if (role == Qt::ForegroundRole) {
        return style.color ? QVariant::fromValue(*style.color) : QVariant();
    }
    if (role == Qt::FontRole && style.bold) {
        // Only bold is set, so the view's own font is kept otherwise.
        QFont font;
        font.setBold(true);
        return font;
    }
    return QVariant();
}

QVariant cellData(ColumnCache::Column column,
        int role,
        const std::function<QVariant(ColumnCache::Column)>& siblingValue,
        const std::optional<QColor>& playedColor) {
    const ColumnIdentity identity = columnIdentity(column);
    const KeyMatcher* pMatcher = KeyMatcher::instance();
    if (!pMatcher || !pMatcher->settings().colours(identity)) {
        return QVariant();
    }
    CellInput input;
    input.column = identity;
    input.key = rowKey(siblingValue);
    const QVariant missing = siblingValue(ColumnCache::COLUMN_TRACKLOCATIONSTABLE_FSDELETED);
    input.missing = !missing.isNull() && missing.toBool();
    const QVariant played = siblingValue(ColumnCache::COLUMN_LIBRARYTABLE_PLAYED);
    input.played = !played.isNull() && played.toBool();
    input.playedColor = playedColor;

    return roleData(cellStyle(input), role);
}

void repaintOnChange(QAbstractItemModel* pModel) {
    const KeyMatcher* pMatcher = KeyMatcher::instance();
    if (!pMatcher) {
        return;
    }
    QObject::connect(pMatcher, &KeyMatcher::changed, pModel, [pModel] {
        const int rows = pModel->rowCount();
        const int columns = pModel->columnCount();
        if (rows > 0 && columns > 0) {
            emit pModel->dataChanged(pModel->index(0, 0),
                    pModel->index(rows - 1, columns - 1),
                    {Qt::ForegroundRole, Qt::FontRole});
        }
    });
}

} // namespace keymatch
} // namespace mixxx
