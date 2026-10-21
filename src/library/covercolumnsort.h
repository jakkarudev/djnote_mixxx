#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <memory>

class CoverBatch;
class CoverLookup;
class QAbstractItemModel;

/// DJNote: sorting a track list by a cover column whose covers come from the
/// cover lookup (Computer and Recording, later the external libraries).
///
/// Sorting by cover needs the cover of every row. Covers not known yet are
/// read in the background first, with the progress shown in the column's
/// header ("Cover Art (reading 340/2000)"). The list keeps its order while
/// they are read and is sorted once at the end: readyToSort() asks for it.
/// Asking to sort by the cover column again with the other order (its header
/// clicked again) or by another column cancels the read.
///
/// The list itself writes the sort keys (the covers' image digests, as in
/// Tracks) and sorts; this class only decides when.
class CoverColumnSort : public QObject {
    Q_OBJECT
  public:
    /// `pModel` is the model whose header shows the progress for `column`.
    CoverColumnSort(
            QAbstractItemModel* pModel,
            int column,
            CoverLookup* pCoverLookup,
            QObject* pParent);
    ~CoverColumnSort() override;

    /// To call when the list is asked to sort by the cover column, with the
    /// locations of all its rows. Returns whether to sort now, i.e. all
    /// covers are known. Otherwise readyToSort() follows, unless cancelled.
    bool sortByCover(Qt::SortOrder order, const QStringList& locations);

    /// To call when the list is asked to sort by another column. Cancels the
    /// read, if any.
    void sortByOtherColumn();

    /// To call when the rows changed while sorted by cover, with the
    /// locations of all rows. Returns whether to sort now, as sortByCover().
    bool rowsChanged(const QStringList& locations);

    /// To call when the rows are removed: cancels the read, if any, and
    /// stays sorted by cover.
    void rowsCleared();

    /// The list is sorted by cover, or will be once the covers are read.
    bool isSortedByCover() const {
        return m_sortedByCover;
    }
    Qt::SortOrder sortOrder() const {
        return m_order;
    }
    bool isReading() const {
        return m_pBatch != nullptr;
    }

    /// The cover sort key of a cover's image digest: rows that share a cover
    /// share it, rows without a cover come first in ascending order.
    static QString sortKey(const QByteArray& imageDigest);

  signals:
    /// All covers are read: sort the list by cover now.
    void readyToSort(Qt::SortOrder order);

  private slots:
    void slotProgress(int done, int total);
    void slotFinished();

  private:
    /// Returns whether all covers are known.
    bool read(const QStringList& locations);
    void cancelReading();
    void showProgress(int done, int total);
    void restoreHeader();

    const QPointer<QAbstractItemModel> m_pModel;
    const int m_column;
    const QPointer<CoverLookup> m_pCoverLookup;
    std::unique_ptr<CoverBatch> m_pBatch;
    // The header label while the progress is shown.
    QString m_headerLabel;
    bool m_sortedByCover;
    Qt::SortOrder m_order;
};
