#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

#include "library/coverart.h"

class BrowseTableModel;
class CoverLookup;
class QAbstractItemDelegate;
class QModelIndex;
class RecordingManager;
class TrackModel;

/// DJNote: the cover column of Computer (including Quick Links) and
/// Recording, which share BrowseTableModel.
///
/// Covers come from the cover lookup, never by resolving a track, so drawing
/// a row never adds its file to the library. The file being recorded gets an
/// empty cover and is never touched.
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

  private:
    BrowseTableModel* const m_pModel;
    const int m_column;
    const QPointer<CoverLookup> m_pCoverLookup;
    RecordingManager* const m_pRecordingManager;
    // The row of each location whose cover was drawn, to repaint it when
    // the lookup's answer changes.
    mutable QHash<QString, int> m_rowByLocation;
};
