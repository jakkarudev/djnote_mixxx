#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "library/coverart.h"

class BrowseTableModel;
class CoverColumnSort;
class CoverLookup;
class QAbstractItemDelegate;
class QModelIndex;
class QSortFilterProxyModel;
class RecordingManager;
class TrackModel;

/// DJNote: the cover column of Computer (including Quick Links) and
/// Recording, which share BrowseTableModel.
///
/// Covers come from the cover lookup, never by resolving a track, so drawing
/// a row never adds its file to the library. The file being recorded gets an
/// empty cover and is never touched.
///
/// Sorting by the cover column reads the covers not known yet first, then
/// sorts once (see CoverColumnSort). The sort keys go into the cover
/// column's items, in the sort proxy's sort role, all at once.
class BrowseCoverColumn : public QObject {
    Q_OBJECT
  public:
    BrowseCoverColumn(
            BrowseTableModel* pModel,
            int column,
            CoverLookup* pCoverLookup,
            RecordingManager* pRecordingManager);

    CoverInfo coverInfo(const QModelIndex& index) const;

    /// The cover delegate, same as in Tracks. `pParent` is the table view.
    QAbstractItemDelegate* createDelegate(QObject* pParent) const;

    /// To call when the model's rows are removed.
    void clearRows();

    /// To call when `pProxy` is asked to sort by `column`. Returns whether
    /// to sort now; sorting by cover may first have to read covers.
    bool sortRequested(QSortFilterProxyModel* pProxy, int column, Qt::SortOrder order);

    /// Header states saved before the cover column existed get it inserted
    /// right after the preview column, visible, as in Tracks. Without this
    /// the restored header would hide the new column.
    static void addToSavedHeaderState(
            TrackModel* pTrackModel,
            int column,
            int columnCount,
            int width);

  private slots:
    void slotCoverInfoChanged(const QString& location);
    void slotRowsInserted();
    void slotReadyToSort(Qt::SortOrder order);

  private:
    /// The location whose cover a row shows, empty for none.
    QString coverLocation(const QModelIndex& index) const;
    QStringList coverLocations() const;
    /// Writes every row's sort key, all at once.
    void writeSortKeys();

    BrowseTableModel* const m_pModel;
    const int m_column;
    const QPointer<CoverLookup> m_pCoverLookup;
    RecordingManager* const m_pRecordingManager;
    // The row of each location whose cover was drawn, to repaint it when
    // the lookup's answer changes.
    mutable QHash<QString, int> m_rowByLocation;
    CoverColumnSort* const m_pSort;
    // The proxy that last asked to sort.
    QPointer<QSortFilterProxyModel> m_pProxy;
    // Rows arrive in chunks while a folder is read: handle them together.
    QTimer m_rowsInsertedTimer;
};
