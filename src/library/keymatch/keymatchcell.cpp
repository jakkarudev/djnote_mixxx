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

bool isColoured(ColumnIdentity column) {
    return column == ColumnIdentity::Key;
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
    return column == ColumnCache::COLUMN_LIBRARYTABLE_KEY ? ColumnIdentity::Key
                                                          : ColumnIdentity::NotColoured;
}

CellStyle cellStyle(const CellInput& input) {
    const KeyMatcher* pMatcher = KeyMatcher::instance();
    // A missing track keeps Mixxx's missing colour and is never highlighted.
    if (!pMatcher || input.missing || !isColoured(input.column)) {
        return {};
    }
    CellStyle style;
    const std::optional<QColor> color = colorOf(pMatcher->match(input.key));
    style.color = color;
    // Every coloured cell is bold, played or not.
    style.bold = color.has_value() && pMatcher->settings().boldMatchedText;
    return style;
}

QVariant cellData(ColumnCache::Column column,
        int role,
        const std::function<QVariant(ColumnCache::Column)>& siblingValue) {
    const ColumnIdentity identity = columnIdentity(column);
    if (!isColoured(identity) || !KeyMatcher::instance()) {
        return QVariant();
    }
    CellInput input;
    input.column = identity;
    input.key = rowKey(siblingValue);
    const QVariant missing = siblingValue(ColumnCache::COLUMN_TRACKLOCATIONSTABLE_FSDELETED);
    input.missing = !missing.isNull() && missing.toBool();
    const QVariant played = siblingValue(ColumnCache::COLUMN_LIBRARYTABLE_PLAYED);
    input.played = !played.isNull() && played.toBool();

    const CellStyle style = cellStyle(input);
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
