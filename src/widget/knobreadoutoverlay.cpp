#include "widget/knobreadoutoverlay.h"

#include <QDebug>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QWidget>
#include <algorithm>

#include "control/controlproxy.h"
#include "controllers/controllerinputscope.h"
#include "moc_knobreadoutoverlay.cpp"
#include "widget/controlwidgetconnection.h"
#include "widget/knobreadoutface.h"
#include "widget/knobreadouttext.h"
#include "widget/wbasewidget.h"
#include "widget/wknob.h"
#include "widget/wknobcomposed.h"

namespace {

const QString kOverlayObjectName = QStringLiteral("DJNoteKnobReadoutOverlay");
const QString kLabelObjectName = QStringLiteral("DJNoteKnobReadout");
// Marks the ControlProxy the overlay parents to each watched knob, so a knob
// is never watched twice.
const QString kWatchObjectName = QStringLiteral("DJNoteKnobReadoutWatch");

constexpr int kHideDelayMs = 1000;
// Gap between the bottom of the knob and the top of the label.
constexpr int kGapPx = 2;

// The control a knob displays: the skin parser's display connection is the
// last left-button or no-button connection.
const ControlParameterWidgetConnection* displayConnection(WBaseWidget* pWidget) {
    if (!pWidget->connections().isEmpty()) {
        return pWidget->connections().last();
    }
    if (!pWidget->leftConnections().isEmpty()) {
        return pWidget->leftConnections().last();
    }
    return nullptr;
}

bool isScreenInputEvent(const QEvent* pEvent) {
    switch (pEvent->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease:
    case QEvent::Wheel:
        return true;
    case QEvent::MouseMove:
        // A move without a pressed button is hovering, not turning.
        return static_cast<const QMouseEvent*>(pEvent)->buttons() != Qt::NoButton;
    default:
        return false;
    }
}

// The label's look: under the knob, or on the knob face (no border, so the
// padding matches the one knobreadoutface.h fits with).
QString labelStyleSheet(int pixelSize, bool onFace) {
    const QString border = onFace
            ? QStringLiteral("none")
            : QStringLiteral("1px solid rgba(255, 255, 255, 60)");
    const QString padding = onFace
            ? QStringLiteral("%1px %2px")
                      .arg(mixxx::knobreadout::kFaceVerticalPaddingPx)
                      .arg(mixxx::knobreadout::kFaceHorizontalPaddingPx)
            : QStringLiteral("1px 4px");
    return QStringLiteral(
            "QLabel#%1 {"
            " color: #f0f0f0;"
            " background-color: rgba(16, 16, 16, 220);"
            " border: %2;"
            " border-radius: 3px;"
            " padding: %3;"
            " font-size: %4px;"
            "}")
            .arg(kLabelObjectName, border, padding, QString::number(pixelSize));
}

// Under the knob, the label uses the full-size font.
constexpr int kUnderKnobPixelSize = 11;

} // namespace

namespace mixxx {

// static
void KnobReadoutOverlay::onSkinLoaded(QWidget* pMainWindow,
        QWidget* pSkinRoot,
        UserSettingsPointer pConfig) {
    if (!pMainWindow || !pSkinRoot || !pConfig) {
        return;
    }
    auto* pOverlay = pMainWindow->findChild<KnobReadoutOverlay*>(
            kOverlayObjectName, Qt::FindDirectChildrenOnly);
    if (!pOverlay) {
        pOverlay = new KnobReadoutOverlay(pMainWindow, pConfig);
    }
    pOverlay->watchKnobs(pSkinRoot);
}

KnobReadoutOverlay::KnobReadoutOverlay(QWidget* pMainWindow, UserSettingsPointer pConfig)
        : QObject(pMainWindow),
          m_pMainWindow(pMainWindow),
          m_pConfig(pConfig),
          m_pLabel(new QLabel(pMainWindow)) {
    setObjectName(kOverlayObjectName);

    m_pLabel->setObjectName(kLabelObjectName);
    m_pLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_pLabel->setAlignment(Qt::AlignCenter);
    setLabelStyleSheet(labelStyleSheet(kUnderKnobPixelSize, false));
    m_pLabel->hide();

    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(kHideDelayMs);
    connect(&m_hideTimer, &QTimer::timeout, m_pLabel, &QWidget::hide);
}

knobreadout::Settings KnobReadoutOverlay::settings() const {
    return knobreadout::loadSettings(m_pConfig);
}

void KnobReadoutOverlay::watchKnobs(QWidget* pSkinRoot) {
    // The previous skin's knobs are gone, and their watches with them.
    m_hideTimer.stop();
    m_pLabel->hide();
    // findChildren also returns the effect parameter and meta knob subclasses.
    const auto knobs = pSkinRoot->findChildren<WKnob*>();
    for (WKnob* pKnob : knobs) {
        watchKnob(pKnob);
    }
    const auto composedKnobs = pSkinRoot->findChildren<WKnobComposed*>();
    for (WKnobComposed* pKnob : composedKnobs) {
        watchKnob(pKnob);
    }
    qDebug() << "KnobReadoutOverlay: found" << knobs.size() << "knobs and"
             << composedKnobs.size() << "composed knobs";
}

void KnobReadoutOverlay::watchKnob(QWidget* pKnob) {
    if (pKnob->findChild<ControlProxy*>(kWatchObjectName, Qt::FindDirectChildrenOnly)) {
        return;
    }
    auto* pBaseWidget = dynamic_cast<WBaseWidget*>(pKnob);
    if (!pBaseWidget) {
        return;
    }
    const ControlParameterWidgetConnection* pConnection = displayConnection(pBaseWidget);
    if (!pConnection) {
        return;
    }
    // Parented to the knob, so the watch dies with the knob on skin reload.
    auto* pControl = new ControlProxy(pConnection->getKey(), pKnob);
    pControl->setObjectName(kWatchObjectName);
    const QPointer<QWidget> pKnobGuard(pKnob);
    pControl->connectValueChanged(this, [this, pKnobGuard, pControl](double) {
        if (pKnobGuard) {
            onKnobControlChanged(pKnobGuard, pControl);
        }
    });
    // Controller input: the watch samples the controller's input scope in the
    // controller thread; the connection is queued to this (GUI) thread and
    // dies with pControl, i.e. with the knob.
    if (auto* pWatch = ControllerInputWatch::forKey(pConnection->getKey())) {
        connect(pWatch,
                &ControllerInputWatch::changedByController,
                pControl,
                [this, pKnobGuard, pControl](double) {
                    if (!pKnobGuard) {
                        return;
                    }
                    m_pControllerInputKnob = pKnobGuard;
                    onKnobControlChanged(pKnobGuard, pControl);
                    m_pControllerInputKnob.clear();
                });
    }
    pKnob->installEventFilter(this);
}

bool KnobReadoutOverlay::eventFilter(QObject* pWatched, QEvent* pEvent) {
    // Runs before the knob handles the event; the knob then sets its control
    // synchronously. The mark is cleared on the next event-loop pass.
    if (isScreenInputEvent(pEvent)) {
        m_pScreenInputKnob = qobject_cast<QWidget*>(pWatched);
        QTimer::singleShot(0, this, [this]() {
            m_pScreenInputKnob.clear();
        });
    }
    return QObject::eventFilter(pWatched, pEvent);
}

bool KnobReadoutOverlay::isOwnInput(QWidget* pKnob) const {
    return m_pScreenInputKnob == pKnob || m_pControllerInputKnob == pKnob;
}

void KnobReadoutOverlay::onKnobControlChanged(QWidget* pKnob, ControlProxy* pControl) {
    const knobreadout::Settings readoutSettings = settings();
    if (!readoutSettings.enabled) {
        // Turned off while a readout was showing: take it down now.
        m_hideTimer.stop();
        m_pLabel->hide();
        return;
    }
    if (!isOwnInput(pKnob)) {
        return;
    }
    const knobreadout::KnobIdentity knob{pControl->getKey()};
    const knobreadout::KnobValue value{pControl->get(), pControl->getParameter()};
    knobreadout::Options options;
    if (readoutSettings.style == knobreadout::Style::KnobFace) {
        options.compact = true;
        const QString compactText = knobreadout::text(knob, value, options);
        const std::optional<int> pixelSize = knobreadout::faceFontPixelSize(
                compactText, m_pLabel->font(), pKnob->size());
        if (pixelSize) {
            showReadout(pKnob, compactText, *pixelSize);
            return;
        }
        // Even the compact text doesn't fit: full text under the knob.
        options.compact = false;
    }
    showReadout(pKnob, knobreadout::text(knob, value, options));
}

void KnobReadoutOverlay::setLabelStyleSheet(const QString& styleSheet) {
    // Re-polishing on every change would be wasted work while a knob turns.
    if (m_pLabel->styleSheet() != styleSheet) {
        m_pLabel->setStyleSheet(styleSheet);
    }
}

void KnobReadoutOverlay::showReadout(
        QWidget* pKnob, const QString& text, std::optional<int> facePixelSize) {
    if (!pKnob->isVisible()) {
        return;
    }
    setLabelStyleSheet(facePixelSize
                    ? labelStyleSheet(*facePixelSize, true)
                    : labelStyleSheet(kUnderKnobPixelSize, false));
    m_pLabel->setText(text);
    m_pLabel->adjustSize();

    // Centred on the knob face, or centred just under the knob; kept inside
    // the main window.
    const QPoint anchor = m_pMainWindow->mapFromGlobal(pKnob->mapToGlobal(
            facePixelSize ? pKnob->rect().center()
                          : QPoint(pKnob->width() / 2, pKnob->height())));
    const int maxX = std::max(0, m_pMainWindow->width() - m_pLabel->width());
    const int maxY = std::max(0, m_pMainWindow->height() - m_pLabel->height());
    const int x = std::clamp(anchor.x() - m_pLabel->width() / 2, 0, maxX);
    const int top = facePixelSize ? anchor.y() - m_pLabel->height() / 2 : anchor.y() + kGapPx;
    const int y = std::clamp(top, 0, maxY);
    m_pLabel->move(x, y);
    m_pLabel->show();
    m_pLabel->raise();
    m_hideTimer.start();
}

} // namespace mixxx
