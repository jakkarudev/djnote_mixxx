#include "library/keymatch/keymatchbrowse.h"

#include "library/browse/browsetablemodel.h"
#include "track/keyutils.h"

namespace mixxx {
namespace keymatch {

ColumnIdentity browseColumnIdentity(int column) {
    return column == COLUMN_KEY ? ColumnIdentity::Key : ColumnIdentity::NotColoured;
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
