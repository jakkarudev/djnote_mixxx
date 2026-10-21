#pragma once

#include <QString>

#include "proto/headers.pb.h"

class TrackModel;

/// DJNote: the header layout that WTrackTableViewHeader saved for a track
/// model (its "header_state_pb" model setting), to place a column DJNote
/// adds to an upstream track list, e.g. the cover column.
///
/// The layout lists the columns in visual order, each with its model
/// (logical) index, and the model's column name (kHeaderNameRole) if it has
/// one. The header restores a column by name when it has one, else by its
/// logical index.
class SavedHeaderState {
  public:
    /// Reads the layout saved for `pModel`, if any.
    explicit SavedHeaderState(TrackModel* pModel);

    /// A layout was saved and could be read.
    bool isSaved() const {
        return m_saved;
    }
    int columnCount() const {
        return m_state.header_state_size();
    }
    /// The visual index of the column named `name`, -1 if none.
    int visualIndexOfName(const QString& name) const;
    /// The visual index of the column with `logicalIndex`, -1 if none.
    int visualIndexOfLogicalIndex(int logicalIndex) const;

    /// Appends a column at the right.
    void appendColumn(int logicalIndex, int size, bool hidden);
    /// Inserts a visible column at `visualIndex`, for a column the model
    /// inserted at `logicalIndex`: the saved columns from there on, and the
    /// sort column, move one to the right.
    void insertColumn(int visualIndex, int logicalIndex, int size, const QString& name);
    /// The column saved as `fromLogicalIndex` is the model's column
    /// `toLogicalIndex` now, and the ones in between moved by one to close
    /// the gap. Moves the sort column alike.
    void moveColumn(int fromLogicalIndex, int toLogicalIndex);
    void setName(int visualIndex, const QString& name);

    /// Saves the layout for the model.
    void save();

  private:
    TrackModel* const m_pModel;
    mixxx::library::HeaderViewState m_state;
    bool m_saved;
};
