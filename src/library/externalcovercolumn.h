#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include "library/coverart.h"

class BaseSqlTableModel;
class CoverColumnSort;
class CoverLookup;
class QAbstractItemModel;
class QModelIndex;

/// DJNote: the cover column of the external libraries (Rekordbox, Serato,
/// iTunes, Traktor, and Rhythmbox), whose track lists and playlists share
/// BaseExternalTrackModel and BaseExternalPlaylistModel.
///
/// Covers come from the cover lookup by track location, never by resolving
/// a track, so drawing a row never adds its file to the library, and the
/// external library tables are never written.
///
/// Sorting by the cover column works as in Computer (CoverColumnSort): the
/// covers not known yet are read in the background first, then the list is
/// sorted once. The sort goes through the model's SQL ordering like any view
/// column: the sort keys of the covers read this session are kept in a
/// session-only temporary table keyed by track location, which the view's
/// cover column looks up.
class ExternalCoverColumn : public QObject {
    Q_OBJECT
  public:
    /// Gives the cover column of `pModel` the cover lookup, if `pModel` is
    /// an external library model. Library calls it for every track model it
    /// shows; until then the column shows no covers and sorts as it is.
    static void attachCoverLookup(QAbstractItemModel* pModel, CoverLookup* pCoverLookup);

    /// Becomes a child of `pModel`, whose views are in `database`.
    ExternalCoverColumn(BaseSqlTableModel* pModel, const QSqlDatabase& database);

    /// The cover column in an external library's temporary view, right after
    /// the preview column as in Tracks: the cover sort key of the track at
    /// `locationExpression` (SQL, the location as stored in the external
    /// library table), NULL while not known. Creates the session's sort key
    /// table first, so call it before creating the view.
    QString viewColumn(const QString& locationExpression) const;

    CoverInfo coverInfo(const QModelIndex& index) const;

    /// To call after each setTable() of the model (e.g. another playlist).
    ///
    /// A header state saved before the cover column existed gets it inserted
    /// right after the preview column, visible, as in Tracks. Without this
    /// the restored header would hide the new column. A state that has the
    /// column (shown or hidden) is kept.
    void tableSet();

    /// To call when the model is asked to sort. Returns whether to sort now.
    /// Otherwise the model keeps its rows' order while the covers are read,
    /// then this sorts it by cover.
    bool sortRequested(int column, Qt::SortOrder order);

  private slots:
    void slotCoverInfoChanged(const QString& location);
    void slotReadyToSort(Qt::SortOrder order);
    void slotRowsChanged();

  private:
    void setCoverLookup(CoverLookup* pCoverLookup);
    void addToSavedHeaderState();
    int column() const;
    QStringList rowLocations() const;
    /// Writes the sort keys of the rows whose cover sort value is not their
    /// cover's (yet). Returns their locations: if any, the rows need a
    /// select() to be sorted by them.
    QSet<QString> writeSortKeys();

    BaseSqlTableModel* const m_pModel;
    const QSqlDatabase m_database;
    QPointer<CoverLookup> m_pCoverLookup;
    CoverColumnSort* m_pSort;
    // The locations being read for a cover sort.
    QSet<QString> m_readingLocations;
    // The locations whose sort keys the last select() was for.
    QSet<QString> m_reselectedFor;
    // The rows are of the previous table (playlist) until the next select().
    bool m_rowsOfPreviousTable;
    bool m_rowsChangePending;
    // The rows of each location whose cover was drawn, to repaint them when
    // the lookup's answer changes. Cleared when the model's rows change.
    mutable QHash<QString, QList<int>> m_rowsByLocation;
};
