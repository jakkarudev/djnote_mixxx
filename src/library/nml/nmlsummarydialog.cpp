#include "library/nml/nmlsummarydialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>
#include <algorithm>

namespace mixxx {
namespace nml {

NmlSummaryParts splitNmlSummary(const QString& text) {
    NmlSummaryParts parts;
    const int split = text.indexOf(QStringLiteral("\n\n"));
    if (split < 0) {
        parts.headline = text;
        return parts;
    }
    parts.headline = text.left(split);
    parts.details = text.mid(split + 2);
    return parts;
}

int nmlSummaryMaxHeight(int screenHeight) {
    return std::max(300, screenHeight / 2);
}

void showNmlSummary(QWidget* pParent, const QString& title, const QString& text, bool warning) {
    const NmlSummaryParts parts = splitNmlSummary(text);

    QDialog dialog(pParent);
    dialog.setWindowTitle(title);
    auto* pLayout = new QVBoxLayout(&dialog);

    auto* pTop = new QHBoxLayout();
    auto* pIcon = new QLabel(&dialog);
    pIcon->setPixmap(dialog.style()
                             ->standardIcon(warning ? QStyle::SP_MessageBoxWarning
                                                    : QStyle::SP_MessageBoxInformation)
                             .pixmap(32, 32));
    pIcon->setAlignment(Qt::AlignTop);
    auto* pHeadline = new QLabel(parts.headline, &dialog);
    pHeadline->setWordWrap(true);
    pHeadline->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pTop->addWidget(pIcon);
    pTop->addWidget(pHeadline, 1);
    pLayout->addLayout(pTop);

    int maxHeight = 600;
    int maxWidth = 800;
    QScreen* pScreen = (pParent && pParent->screen()) ? pParent->screen()
                                                      : QGuiApplication::primaryScreen();
    if (pScreen) {
        maxHeight = nmlSummaryMaxHeight(pScreen->availableGeometry().height());
        maxWidth = pScreen->availableGeometry().width() * 2 / 3;
    }

    if (!parts.details.isEmpty()) {
        auto* pDetails = new QPlainTextEdit(parts.details, &dialog);
        pDetails->setReadOnly(true);
        pDetails->setLineWrapMode(QPlainTextEdit::NoWrap);
        pDetails->setMinimumHeight(120);
        pLayout->addWidget(pDetails, 1);
        dialog.setMinimumWidth(std::min(560, maxWidth));
    }

    auto* pButtons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    QObject::connect(pButtons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    pLayout->addWidget(pButtons);

    dialog.setMaximumHeight(maxHeight);
    dialog.setMaximumWidth(maxWidth);
    dialog.adjustSize();
    dialog.exec();
}

} // namespace nml
} // namespace mixxx
