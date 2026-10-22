#pragma once

#include <QAbstractItemModel>
#include <QColor>
#include <QVariant>
#include <functional>
#include <optional>

#include "library/columncache.h"
#include "proto/keys.pb.h"

// What a track list cell looks like under key matching (GLOSSARY.md, "Key
// matching"): the one answer every track list model asks from a small marked
// hook in its data().
namespace mixxx {
namespace keymatch {

// A column's identity, shared by all track list types. Later tickets let the DJ
// choose which identities are coloured; today only Key is.
enum class ColumnIdentity {
    Key,
    // Cover, Color, Rating, Played, Preview and the like: never coloured.
    NotColoured,
};

ColumnIdentity columnIdentity(ColumnCache::Column column);

// Everything the cell helper needs about the cell's row and column.
struct CellInput {
    ColumnIdentity column = ColumnIdentity::NotColoured;
    // The row's stored key, INVALID when it has none.
    mixxx::track::io::key::ChromaticKey key = mixxx::track::io::key::INVALID;
    bool missing = false;
    bool played = false;
};

// The colour and font the cell takes from key matching. Unset colour and no
// bold mean Mixxx's normal look.
struct CellStyle {
    std::optional<QColor> color;
    bool bold = false;
};

CellStyle cellStyle(const CellInput& input);

// The QVariant a model's data() returns for `role` (Qt::ForegroundRole or
// Qt::FontRole) given the cell's style; invalid means Mixxx's normal look.
QVariant roleData(const CellStyle& style, int role);

// The same for a track list model's cell, reading the row's raw values through
// `siblingValue` (the model's rawSiblingValue) only when the column can be
// coloured. The role is Qt::ForegroundRole (a QColor) or Qt::FontRole (a QFont
// that only sets what key matching changes); an invalid QVariant means Mixxx's
// normal look.
QVariant cellData(ColumnCache::Column column,
        int role,
        const std::function<QVariant(ColumnCache::Column)>& siblingValue);

// Makes the model repaint all its cells whenever key matching changes.
void repaintOnChange(QAbstractItemModel* pModel);

} // namespace keymatch
} // namespace mixxx
