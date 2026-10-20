#include "widget/knobreadoutoverlay.h"

#include <QDebug>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QWidget>
#include <algorithm>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "effects/effectchain.h"
#include "effects/effectparameterslotbase.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"
#include "moc_knobreadoutoverlay.cpp"
#include "widget/controlwidgetconnection.h"
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

} // namespace

namespace mixxx {

// static
void KnobReadoutOverlay::onSkinLoaded(QWidget* pMainWindow,
        QWidget* pSkinRoot,
        UserSettingsPointer pConfig,
        std::shared_ptr<EffectsManager> pEffectsManager) {
    if (!pMainWindow || !pSkinRoot || !pConfig) {
        return;
    }
    auto* pOverlay = pMainWindow->findChild<KnobReadoutOverlay*>(
            kOverlayObjectName, Qt::FindDirectChildrenOnly);
    if (!pOverlay) {
        pOverlay = new KnobReadoutOverlay(pMainWindow, pConfig, pEffectsManager);
    }
    pOverlay->watchKnobs(pSkinRoot);
}

KnobReadoutOverlay::KnobReadoutOverlay(QWidget* pMainWindow,
        UserSettingsPointer pConfig,
        std::shared_ptr<EffectsManager> pEffectsManager)
        : QObject(pMainWindow),
          m_pMainWindow(pMainWindow),
          m_pConfig(pConfig),
          m_pEffectsManager(pEffectsManager),
          m_pLabel(new QLabel(pMainWindow)) {
    setObjectName(kOverlayObjectName);

    m_pLabel->setObjectName(kLabelObjectName);
    m_pLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_pLabel->setAlignment(Qt::AlignCenter);
    m_pLabel->setStyleSheet(QStringLiteral(
            "QLabel#%1 {"
            " color: #f0f0f0;"
            " background-color: rgba(16, 16, 16, 220);"
            " border: 1px solid rgba(255, 255, 255, 60);"
            " border-radius: 3px;"
            " padding: 1px 4px;"
            " font-size: 11px;"
            "}")
                    .arg(kLabelObjectName));
    m_pLabel->hide();

    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(kHideDelayMs);
    connect(&m_hideTimer, &QTimer::timeout, m_pLabel, &QWidget::hide);
}

knobreadout::Settings KnobReadoutOverlay::settings() const {
    return knobreadout::loadSettings(m_pConfig);
}

knobreadout::KnobIdentity KnobReadoutOverlay::identify(const ConfigKey& key) const {
    knobreadout::KnobIdentity knob{key};
    // Effect parameter knobs: [<chain group without "]">_Effect<n>],parameter<m>,
    // e.g. [EffectRack1_EffectUnit1_Effect2],parameter3.
    static const QRegularExpression kEffectSlotGroup(
            QStringLiteral("^(\\[.*)_Effect(\\d+)\\]$"));
    static const QRegularExpression kKnobParameterItem(
            QStringLiteral("^parameter(\\d+)$"));
    const QRegularExpressionMatch groupMatch = kEffectSlotGroup.match(key.group);
    const QRegularExpressionMatch itemMatch = kKnobParameterItem.match(key.item);
    if (!groupMatch.hasMatch() || !itemMatch.hasMatch()) {
        return knob;
    }
    const auto pEffectsManager = m_pEffectsManager.lock();
    if (!pEffectsManager) {
        return knob;
    }
    // A parameter slot exists exactly when its "_loaded" control does; this
    // avoids the slot lookup's debug assertion on a number out of range.
    if (!ControlObject::exists(ConfigKey(key.group, key.item + QStringLiteral("_loaded")))) {
        return knob;
    }
    const EffectChainPointer pChain = pEffectsManager->getEffectChain(
            groupMatch.captured(1) + QStringLiteral("]"));
    if (!pChain) {
        return knob;
    }
    for (const EffectSlotPointer& pEffectSlot : pChain->getEffectSlots()) {
        if (!pEffectSlot || pEffectSlot->getGroup() != key.group) {
            continue;
        }
        const EffectParameterSlotBasePointer pParameterSlot =
                pEffectSlot->getEffectParameterSlot(
                        EffectManifestParameter::ParameterType::Knob,
                        itemMatch.captured(1).toUInt() - 1);
        if (pParameterSlot && pParameterSlot->isLoaded()) {
            knob.unitsHint = pParameterSlot->getManifest()->unitsHint();
        }
        break;
    }
    return knob;
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
    return m_pScreenInputKnob == pKnob;
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
    const knobreadout::KnobIdentity knob = identify(pControl->getKey());
    const knobreadout::KnobValue value{pControl->get(), pControl->getParameter()};
    knobreadout::Options options;
    options.mode = readoutSettings.mode;
    showReadout(pKnob, knobreadout::text(knob, value, options));
}

void KnobReadoutOverlay::showReadout(QWidget* pKnob, const QString& text) {
    if (!pKnob->isVisible()) {
        return;
    }
    m_pLabel->setText(text);
    m_pLabel->adjustSize();

    // Centred just under the knob, kept inside the main window.
    const QPoint knobBottomCentre = m_pMainWindow->mapFromGlobal(
            pKnob->mapToGlobal(QPoint(pKnob->width() / 2, pKnob->height())));
    const int maxX = std::max(0, m_pMainWindow->width() - m_pLabel->width());
    const int maxY = std::max(0, m_pMainWindow->height() - m_pLabel->height());
    const int x = std::clamp(knobBottomCentre.x() - m_pLabel->width() / 2, 0, maxX);
    const int y = std::clamp(knobBottomCentre.y() + kGapPx, 0, maxY);
    m_pLabel->move(x, y);
    m_pLabel->show();
    m_pLabel->raise();
    m_hideTimer.start();
}

} // namespace mixxx
