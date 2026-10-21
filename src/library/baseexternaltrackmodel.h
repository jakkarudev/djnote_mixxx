#pragma once

#include <QObject>

#include "library/trackmodel.h"
#include "library/basesqltablemodel.h"

class ExternalCoverColumn;
class TrackCollection;
class QModelIndex;
class QString;

class BaseExternalTrackModel : public BaseSqlTableModel {
    Q_OBJECT
  public:
    BaseExternalTrackModel(QObject* parent,
                           TrackCollectionManager* pTrackCollectionManager,
                           const char* settingsNamespace,
                           const QString& trackTable,
                           QSharedPointer<BaseTrackCache> trackSource);
    ~BaseExternalTrackModel() override;

    Capabilities getCapabilities() const override;
    TrackId getTrackId(const QModelIndex& index) const override;
    TrackPointer getTrack(const QModelIndex& index) const override;
    bool isColumnInternal(int column) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    CoverInfo getCoverInfo(const QModelIndex& index) const override;

  private:
    TrackId doGetTrackId(const TrackPointer& pTrack) const override;

    // DJNote
    bool prepareSort(int column, Qt::SortOrder order) override;
    ExternalCoverColumn* const m_pCoverColumn;
};
