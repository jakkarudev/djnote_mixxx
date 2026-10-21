#include "library/trackset/historysessionplaylist.h"

#include <QAction>
#include <QCoreApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>

#include "library/dao/playlistdao.h"

namespace {

class HistorySessionPlaylist {
    Q_DECLARE_TR_FUNCTIONS(HistorySessionPlaylist)
};

void askNameAndSaveSessionAsPlaylist(PlaylistDAO* pDao, int sessionId) {
    const QString sessionName = pDao->getPlaylistName(sessionId);
    QString name = suggestedPlaylistNameForSession(*pDao, sessionName);
    while (true) {
        bool ok = false;
        name = QInputDialog::getText(nullptr,
                HistorySessionPlaylist::tr("Save as Playlist"),
                HistorySessionPlaylist::tr("Enter name for new playlist:"),
                QLineEdit::Normal,
                name,
                &ok)
                       .trimmed();
        if (!ok) {
            return;
        }
        if (pDao->getPlaylistIdFromName(name) != kInvalidPlaylistId) {
            QMessageBox::warning(nullptr,
                    HistorySessionPlaylist::tr("Playlist Creation Failed"),
                    HistorySessionPlaylist::tr("A playlist by that name already exists."));
        } else if (name.isEmpty()) {
            QMessageBox::warning(nullptr,
                    HistorySessionPlaylist::tr("Playlist Creation Failed"),
                    HistorySessionPlaylist::tr("A playlist cannot have a blank name."));
        } else {
            break;
        }
    }
    if (saveSessionAsPlaylist(*pDao, sessionId, name) == kInvalidPlaylistId) {
        QMessageBox::warning(nullptr,
                HistorySessionPlaylist::tr("Playlist Creation Failed"),
                HistorySessionPlaylist::tr("The playlist could not be saved."));
    }
}

} // namespace

QString suggestedPlaylistNameForSession(const PlaylistDAO& dao, const QString& sessionName) {
    //: Default name when saving a History session as a Playlist. %1 is the session name.
    QString name = HistorySessionPlaylist::tr("%1 (Playlist)").arg(sessionName);
    for (int n = 2; dao.getPlaylistIdFromName(name) != kInvalidPlaylistId; ++n) {
        //: Default name when "%1 (Playlist)" is taken. %1 is the session name, %2 a number.
        name = HistorySessionPlaylist::tr("%1 (Playlist %2)").arg(sessionName).arg(n);
    }
    return name;
}

int saveSessionAsPlaylist(PlaylistDAO& dao, int sessionId, const QString& name) {
    const int newId = dao.createPlaylist(name);
    if (newId == kInvalidPlaylistId) {
        return kInvalidPlaylistId;
    }
    if (!dao.copyPlaylistTracks(sessionId, newId)) {
        dao.deletePlaylist(newId);
        return kInvalidPlaylistId;
    }
    return newId;
}

void addSaveSessionAsPlaylistAction(QMenu* pMenu, PlaylistDAO* pDao, int sessionId) {
    QAction* pAction = pMenu->addAction(HistorySessionPlaylist::tr("Save as Playlist"));
    pAction->setEnabled(pDao->tracksInPlaylist(sessionId) > 0);
    QObject::connect(pAction, &QAction::triggered, pMenu, [pDao, sessionId]() {
        askNameAndSaveSessionAsPlaylist(pDao, sessionId);
    });
}
