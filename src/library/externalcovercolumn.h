#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include "library/coverart.h"

class BaseSqlTableModel;
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
class ExternalCoverColumn : public QObject {
    Q_OBJECT
  public:
    /// The cover column in an external library's temporary view, right after
    /// the preview column as in Tracks. Its value is not used (yet): covers
    /// are answered by coverInfo().
    static QString viewColumn();

    /// Gives the cover column of `pModel` the cover lookup, if `pModel` is
    /// an external library model. Library calls it for every track model it
    /// shows; until then the column shows no covers.
    static void attachCoverLookup(QAbstractItemModel* pModel, CoverLookup* pCoverLookup);

    /// Becomes a child of `pModel`.
    explicit ExternalCoverColumn(BaseSqlTableModel* pModel);

    CoverInfo coverInfo(const QModelIndex& index) const;

    /// To call after the model's setTable(). A header state saved before the
    /// cover column existed gets it inserted right after the preview column,
    /// visible, as in Tracks. Without this the restored header would hide the
    /// new column. A state that has the column (shown or hidden) is kept.
    void addToSavedHeaderState();

  private slots:
    void slotCoverInfoChanged(const QString& location);

  private:
    void setCoverLookup(CoverLookup* pCoverLookup);

    BaseSqlTableModel* const m_pModel;
    QPointer<CoverLookup> m_pCoverLookup;
    // The rows of each location whose cover was drawn, to repaint them when
    // the lookup's answer changes. Cleared when the model's rows change.
    mutable QHash<QString, QList<int>> m_rowsByLocation;
};
