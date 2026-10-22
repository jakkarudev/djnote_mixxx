#pragma once

#include <QString>
#include <QVariant>

#include "library/keymatch/keymatchcell.h"

// Key matching for Computer and Quick Links (BrowseTableModel), which is not a
// track list model: it has the key as tag text only and its own columns.
namespace mixxx {
namespace keymatch {

// Maps a BrowseTableModel column to the shared column identity.
ColumnIdentity browseColumnIdentity(int column);

// What a Computer cell takes from key matching. `keyText` is the row's key tag
// in any notation Mixxx's key parser understands. Role as in cellData().
QVariant browseCellData(int column, const QString& keyText, int role);

} // namespace keymatch
} // namespace mixxx
