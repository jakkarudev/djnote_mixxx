#include "library/nml/nmlfindmissingpanel.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QPushButton>

#include "library/nml/nmlfindmissing.h"

namespace mixxx {
namespace nml {

NmlFindMissingPanel::NmlFindMissingPanel(PlansProvider plansProvider,
        TrackCollectionManager* pManager,
        const QString& startFolder,
        QWidget* parent)
        : QWidget(parent),
          m_plansProvider(std::move(plansProvider)),
          m_pManager(pManager),
          m_startFolder(startFolder) {
    auto* pLayout = new QHBoxLayout(this);
    pLayout->setContentsMargins(0, 0, 0, 0);
    auto* pBrowse = new QPushButton(tr("Browse for the moved folder..."), this);
    connect(pBrowse, &QPushButton::clicked, this, &NmlFindMissingPanel::browse);
    pLayout->addWidget(pBrowse);
    if (m_pManager) {
        auto* pFind = new QPushButton(tr("Find in library"), this);
        connect(pFind, &QPushButton::clicked, this, &NmlFindMissingPanel::findInLibrary);
        pLayout->addWidget(pFind);
    }
    pLayout->addStretch(1);
}

void NmlFindMissingPanel::browse() {
    const QString folder = QFileDialog::getExistingDirectory(
            this, tr("Folder the tracks were moved to"), m_startFolder);
    if (folder.isEmpty()) {
        return;
    }
    browseNmlMissingInFolder(m_plansProvider(), folder);
    emit changed();
}

void NmlFindMissingPanel::findInLibrary() {
    findNmlMissingInLibrary(m_plansProvider(), m_pManager);
    emit changed();
}

} // namespace nml
} // namespace mixxx

#include "moc_nmlfindmissingpanel.cpp"
